import os
import time
import json
import asyncio
import zipfile
from io import BytesIO
from typing import Set, Dict, Any, Optional
from pathlib import Path

from fastapi import FastAPI, Request, WebSocket, WebSocketDisconnect, UploadFile, File, Form, Query
from fastapi.responses import HTMLResponse, PlainTextResponse, JSONResponse, Response, StreamingResponse, FileResponse
from fastapi.staticfiles import StaticFiles
from fastapi.middleware.cors import CORSMiddleware

app = FastAPI(title="FPV Car Cloud Controller", version="2.0.0")

# Enable full CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

BASE_DIR = Path(__file__).resolve().parent.parent
WEB_DIR = BASE_DIR / "FPV_Car" / "web"
RECORDINGS_DIR = BASE_DIR / "recordings"
RECORDINGS_DIR.mkdir(parents=True, exist_ok=True)
STATE_FILE = BASE_DIR / "state.json"

# In-memory car state for microsecond access
car_state: Dict[str, Any] = {
    "cmd": "S",
    "mode": "0",
    "cam": "1",
    "flash": "0",
    "fs": "255",
    "bs": "255",
    "esp_hb": 0,
    "car_ip": "",
    "horn": "0"
}

current_recording_folder: Optional[str] = None
is_recording_active: bool = False

# Load persistent state if exists
if STATE_FILE.exists():
    try:
        with open(STATE_FILE, "r", encoding="utf-8") as f:
            saved = json.load(f)
            if isinstance(saved, dict):
                car_state.update(saved)
    except Exception:
        pass

def save_state_to_disk():
    try:
        with open(STATE_FILE, "w", encoding="utf-8") as f:
            json.dump(car_state, f)
    except Exception:
        pass

# WebSocket Connection Manager
class ConnectionManager:
    def __init__(self):
        self.active_connections: Set[WebSocket] = set()

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.add(websocket)
        # Send current state immediately on connect
        await websocket.send_text(json.dumps(get_effective_state()))

    def disconnect(self, websocket: WebSocket):
        self.active_connections.discard(websocket)

    async def broadcast_state(self):
        if not self.active_connections:
            return
        payload = json.dumps(get_effective_state())
        dead = []
        for connection in list(self.active_connections):
            try:
                await connection.send_text(payload)
            except Exception:
                dead.append(connection)
        for d in dead:
            self.active_connections.discard(d)

manager = ConnectionManager()

def get_effective_state() -> Dict[str, Any]:
    state = dict(car_state)
    hb = state.get("esp_hb", 0)
    try:
        hb_val = float(hb)
    except Exception:
        hb_val = 0
    # Auto-stop fail-safe if heartbeat is missing for > 5 seconds
    if hb_val > 0 and (time.time() - hb_val > 5.0):
        state["cmd"] = "S"
    return state

# 1x1 Transparent GIF for latency measurement
PROBE_GIF = bytes([
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x21, 0xf9, 0x04, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x00, 0x02, 0x02, 0x44, 0x01, 0x00, 0x3b
])

@app.get("/ping.gif")
def probe_gif():
    return Response(
        content=PROBE_GIF,
        media_type="image/gif",
        headers={"Cache-Control": "no-cache, no-store, must-revalidate"}
    )

@app.get("/ping")
def ping():
    return {"status": "pong", "time": time.time()}

# State Read Endpoint (Compatible with get.php, /api/get, /status)
@app.get("/get.php")
@app.get("/api/get")
@app.get("/api/status")
@app.get("/status")
def get_state_endpoint():
    return JSONResponse(
        content=get_effective_state(),
        headers={"Cache-Control": "no-store, no-cache, must-revalidate, max-age=0"}
    )

# State Update Endpoint (Compatible with set.php, /api/set, /cmd)
@app.get("/set.php")
@app.get("/api/set")
@app.get("/cmd")
async def set_state_endpoint(
    cmd: Optional[str] = None,
    dir: Optional[str] = None,
    horn: Optional[str] = None,
    mode: Optional[str] = None,
    cam: Optional[str] = None,
    flash: Optional[str] = None,
    fs: Optional[int] = None,
    bs: Optional[int] = None,
    esp_hb: Optional[str] = None,
    car_ip: Optional[str] = None,
    val: Optional[str] = None
):
    command = cmd or dir
    if command and command in ["F", "B", "L", "R", "S", "H", "h"]:
        car_state["cmd"] = command

    if horn is not None:
        car_state["horn"] = "1" if str(horn) in ["1", "true", "H"] else "0"
    elif val is not None and command in ["H", "h"]:
        car_state["horn"] = "1" if str(val) in ["1", "true"] else "0"

    if mode is not None and str(mode) in ["0", "1", "2"]:
        car_state["mode"] = str(mode)

    if cam is not None:
        car_state["cam"] = "0" if str(cam) == "0" else "1"

    if flash is not None:
        car_state["flash"] = "1" if str(flash) in ["1", "true"] else "0"

    if fs is not None:
        car_state["fs"] = str(max(0, min(255, int(fs))))

    if bs is not None:
        car_state["bs"] = str(max(0, min(255, int(bs))))

    if esp_hb is not None:
        car_state["esp_hb"] = time.time()

    if car_ip is not None:
        clean_ip = "".join(c for c in str(car_ip) if c.isdigit() or c == ".")
        if len(clean_ip) >= 7:
            car_state["car_ip"] = clean_ip

    save_state_to_disk()
    asyncio.create_task(manager.broadcast_state())
    return PlainTextResponse("OK")

# Real-time WebSocket Endpoint
@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        while True:
            data = await websocket.receive_text()
            try:
                msg = json.loads(data)
                if isinstance(msg, dict):
                    if "cmd" in msg and msg["cmd"] in ["F", "B", "L", "R", "S"]:
                        car_state["cmd"] = msg["cmd"]
                    if "horn" in msg:
                        car_state["horn"] = "1" if str(msg["horn"]) == "1" else "0"
                    if "fs" in msg:
                        car_state["fs"] = str(max(0, min(255, int(msg["fs"]))))
                    if "bs" in msg:
                        car_state["bs"] = str(max(0, min(255, int(msg["bs"]))))
                    if "car_ip" in msg:
                        car_state["car_ip"] = str(msg["car_ip"])
                    if "esp_hb" in msg:
                        car_state["esp_hb"] = time.time()

                    save_state_to_disk()
                    await manager.broadcast_state()
            except Exception:
                pass
    except WebSocketDisconnect:
        manager.disconnect(websocket)
    except Exception:
        manager.disconnect(websocket)

# Recording API endpoints
@app.get("/cam/start_record.php")
def start_record():
    global current_recording_folder, is_recording_active
    is_recording_active = True
    session_name = time.strftime("session_%Y%m%d_%H%M%S")
    folder_path = RECORDINGS_DIR / session_name
    folder_path.mkdir(parents=True, exist_ok=True)
    current_recording_folder = session_name
    return {"status": "recording_started", "folder": session_name}

@app.get("/cam/stop_record.php")
def stop_record():
    global is_recording_active
    is_recording_active = False
    return {"status": "recording_stopped"}

@app.post("/cam/upload_frame.php")
async def upload_frame(file: UploadFile = File(...)):
    global current_recording_folder, is_recording_active
    if not is_recording_active or not current_recording_folder:
        return PlainTextResponse("NOT_RECORDING")
    
    target_dir = RECORDINGS_DIR / current_recording_folder
    target_dir.mkdir(parents=True, exist_ok=True)
    frame_name = f"frame_{int(time.time() * 1000)}.jpg"
    frame_path = target_dir / frame_name

    contents = await file.read()
    with open(frame_path, "wb") as f:
        f.write(contents)

    return PlainTextResponse("OK")

@app.get("/cam/list.php")
def list_recordings():
    if not RECORDINGS_DIR.exists():
        return []
    folders = [f.name for f in RECORDINGS_DIR.iterdir() if f.is_dir()]
    folders.sort(reverse=True)
    return folders

@app.get("/cam/list_frames.php")
def list_frames(folder: str = Query(...)):
    safe_folder = Path(folder).name
    folder_path = RECORDINGS_DIR / safe_folder
    if not folder_path.exists() or not folder_path.is_dir():
        return []
    frames = [f.name for f in folder_path.iterdir() if f.is_file() and f.suffix.lower() in [".jpg", ".jpeg"]]
    frames.sort()
    return frames

@app.get("/cam/delete.php")
def delete_recording(folder: str = Query(...)):
    import shutil
    safe_folder = Path(folder).name
    folder_path = RECORDINGS_DIR / safe_folder
    if folder_path.exists() and folder_path.is_dir():
        shutil.rmtree(folder_path)
        return {"status": "deleted"}
    return {"status": "not_found"}

@app.get("/cam/download_recording.php")
def download_recording(folder: str = Query(...)):
    safe_folder = Path(folder).name
    folder_path = RECORDINGS_DIR / safe_folder
    if not folder_path.exists() or not folder_path.is_dir():
        return PlainTextResponse("Folder not found", status_code=404)

    zip_buffer = BytesIO()
    with zipfile.ZipFile(zip_buffer, "w", zipfile.ZIP_DEFLATED) as zf:
        for file_path in folder_path.glob("*.jpg"):
            zf.write(file_path, arcname=file_path.name)
    zip_buffer.seek(0)

    return StreamingResponse(
        zip_buffer,
        media_type="application/zip",
        headers={"Content-Disposition": f'attachment; filename="{safe_folder}.zip"'}
    )

# Serve individual recording images
@app.get("/cam/{folder}/{filename}")
def get_recorded_frame(folder: str, filename: str):
    safe_folder = Path(folder).name
    safe_file = Path(filename).name
    file_path = RECORDINGS_DIR / safe_folder / safe_file
    if file_path.exists() and file_path.is_file():
        return FileResponse(file_path, media_type="image/jpeg")
    return PlainTextResponse("File not found", status_code=404)

# Serve frontend web app directly
if WEB_DIR.exists():
    @app.get("/")
    @app.get("/index.html")
    def serve_index():
        index_file = WEB_DIR / "index.html"
        if index_file.exists():
            return FileResponse(index_file)
        return PlainTextResponse("FPV Car Server Running")

    @app.get("/manifest.json")
    def serve_manifest():
        m_file = WEB_DIR / "manifest.json"
        if m_file.exists():
            return FileResponse(m_file, media_type="application/json")
        return PlainTextResponse("Not Found", status_code=404)

    @app.get("/sw.js")
    def serve_sw():
        sw_file = WEB_DIR / "sw.js"
        if sw_file.exists():
            return FileResponse(sw_file, media_type="application/javascript")
        return PlainTextResponse("Not Found", status_code=404)

    app.mount("/", StaticFiles(directory=str(WEB_DIR)), name="web_static")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=False, workers=2)
