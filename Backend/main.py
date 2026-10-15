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

app = FastAPI(title="FPV Car Ultra-Fast Cloud Controller", version="2.5.0")

# Enable full CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

BASE_DIR = Path(__file__).resolve().parent.parent
WEB_DIR = BASE_DIR / "Frontend"
if not WEB_DIR.exists():
    WEB_DIR = BASE_DIR / "FPV_Car" / "web"
RECORDINGS_DIR = BASE_DIR / "recordings"
RECORDINGS_DIR.mkdir(parents=True, exist_ok=True)
STATE_FILE = BASE_DIR / "state.json"
LATEST_FRAME_FILE = RECORDINGS_DIR / "latest.jpg"

# In-memory car state for sub-millisecond execution
car_state: Dict[str, Any] = {
    "cmd": "S",
    "mode": "0",          # 0 = Manual, 1 = Auto-Stop AI Assist
    "cam": "1",           # 1 = On, 0 = Off
    "flash": "0",
    "fs": "255",
    "bs": "255",
    "esp_hb": 0,
    "car_ip": "10.172.34.222",
    "horn": "0",
    "ai": "1",            # 1 = AI Active, 0 = AI Disabled
    "ai_obstacle": "0",   # 0 = Clear, 1 = Obstacle Detected
    "ai_score": "0.0",
    "ai_near": "0.0",
    "ai_action": "IDLE",  # IDLE, CLEAR, OBSTACLE_DETECTED, AUTO_STOP, CAM_OFF, AI_OFF
    "ai_latency": "0ms"
}

current_recording_folder: Optional[str] = None
is_recording_active: bool = False
last_user_cmd_time: float = time.time()

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

# ======================================================
# Lightweight Computer Vision / Obstacle Detection (<3ms)
# ======================================================
def analyze_frame_obstacle(image_bytes: bytes) -> Dict[str, Any]:
    # If camera or AI is OFF, return immediately with zero CPU consumption
    if str(car_state.get("cam", "1")) == "0":
        return {
            "obstacle": False,
            "score": 0.0,
            "action": "CAM_OFF",
            "latency_ms": 0
        }
    if str(car_state.get("ai", "1")) == "0":
        return {
            "obstacle": False,
            "score": 0.0,
            "action": "AI_OFF",
            "latency_ms": 0
        }

    t0 = time.time()
    try:
        import cv2
        import numpy as np

        arr = np.frombuffer(image_bytes, np.uint8)
        img = cv2.imdecode(arr, cv2.IMREAD_GRAYSCALE)
        if img is None:
            return {"obstacle": False, "score": 0.0, "action": "DECODE_FAIL", "latency_ms": 0}

        # Fast downsample to 160x120 for instant sub-millisecond edge analysis
        small = cv2.resize(img, (160, 120), interpolation=cv2.INTER_NEAREST)
        h, w = small.shape

        # Near-field region of interest: lower 50% center
        roi = small[int(h * 0.45):, int(w * 0.15):int(w * 0.85)]

        # Fast Canny Edge Detection
        edges = cv2.Canny(roi, 50, 150)
        edge_ratio = float(np.count_nonzero(edges)) / float(edges.size) * 100.0

        # Darkness / Shadow analysis
        avg_brightness = float(np.mean(roi))
        dark_ratio = float(np.mean(roi < 45.0)) * 100.0

        # Multi-signal obstacle score
        score = (edge_ratio * 2.2) + (dark_ratio * 0.25) + max(0.0, (85.0 - avg_brightness) * 0.15)
        is_obstacle = score > 42.0

        latency_ms = max(1, int((time.time() - t0) * 1000))
        action = "OBSTACLE_DETECTED" if is_obstacle else "CLEAR"

        return {
            "obstacle": is_obstacle,
            "score": round(score, 1),
            "edge_ratio": round(edge_ratio, 1),
            "brightness": round(avg_brightness, 1),
            "action": action,
            "latency_ms": latency_ms
        }
    except Exception as e:
        return {"obstacle": False, "score": 0.0, "action": f"ERROR_{str(e)[:15]}", "latency_ms": 0}

# ======================================================
# WebSocket Connection Manager
# ======================================================
class ConnectionManager:
    def __init__(self):
        self.active_connections: Set[WebSocket] = set()

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.add(websocket)
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
    # Check if AI or Camera is off
    if str(state.get("cam", "1")) == "0":
        state["ai_action"] = "CAM_OFF"
        state["ai_obstacle"] = "0"
    elif str(state.get("ai", "1")) == "0":
        state["ai_action"] = "AI_OFF"
        state["ai_obstacle"] = "0"
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
@app.post("/set.php")
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
    speed: Optional[int] = None,
    esp_hb: Optional[str] = None,
    car_ip: Optional[str] = None,
    val: Optional[str] = None,
    ai: Optional[str] = None,
    ai_obstacle: Optional[str] = None,
    ai_action: Optional[str] = None,
    ai_score: Optional[str] = None
):
    global last_user_cmd_time

    command = cmd or dir
    if command and command in ["F", "B", "L", "R", "S", "H", "h"]:
        car_state["cmd"] = command
        last_user_cmd_time = time.time()

    if horn is not None:
        car_state["horn"] = "1" if str(horn) in ["1", "true", "H"] else "0"
    elif val is not None and command in ["H", "h"]:
        car_state["horn"] = "1" if str(val) in ["1", "true"] else "0"

    if mode is not None and str(mode) in ["0", "1", "2"]:
        car_state["mode"] = str(mode)

    if cam is not None:
        car_state["cam"] = "0" if str(cam) == "0" else "1"
        if car_state["cam"] == "0":
            car_state["ai_action"] = "CAM_OFF"
            car_state["ai_obstacle"] = "0"

    if flash is not None:
        car_state["flash"] = "1" if str(flash) in ["1", "true"] else "0"

    spd = speed if speed is not None else fs
    if spd is not None:
        car_state["fs"] = str(max(0, min(255, int(spd))))
    if bs is not None:
        car_state["bs"] = str(max(0, min(255, int(bs))))

    if esp_hb is not None:
        car_state["esp_hb"] = time.time()

    if car_ip is not None:
        clean_ip = "".join(c for c in str(car_ip) if c.isdigit() or c == ".")
        if len(clean_ip) >= 7:
            car_state["car_ip"] = clean_ip

    if ai is not None:
        car_state["ai"] = "0" if str(ai) == "0" else "1"

    if ai_obstacle is not None:
        car_state["ai_obstacle"] = "1" if str(ai_obstacle) == "1" else "0"
        # If auto-stop mode is active and obstacle hit, auto-stop car
        if car_state["ai_obstacle"] == "1" and str(car_state.get("mode")) == "1":
            car_state["cmd"] = "S"
            car_state["ai_action"] = "AUTO_STOP"

    if ai_action is not None:
        car_state["ai_action"] = str(ai_action)

    if ai_score is not None:
        car_state["ai_score"] = str(ai_score)

    save_state_to_disk()
    asyncio.create_task(manager.broadcast_state())
    return PlainTextResponse("OK")

# ======================================================
# Real-Time AI Obstacle Detection Endpoint (<5ms)
# ======================================================
@app.post("/api/detect")
async def detect_endpoint(request: Request, file: Optional[UploadFile] = File(None)):
    if file:
        contents = await file.read()
    else:
        contents = await request.body()

    if not contents:
        return JSONResponse({"obstacle": False, "score": 0.0, "action": "NO_IMAGE", "latency_ms": 0})

    result = analyze_frame_obstacle(contents)
    
    # Update state
    car_state["ai_obstacle"] = "1" if result["obstacle"] else "0"
    car_state["ai_score"] = str(result["score"])
    car_state["ai_action"] = result["action"]
    car_state["ai_latency"] = f"{result['latency_ms']}ms"

    # Auto-stop fail-safe if in AI Assist mode
    if result["obstacle"] and str(car_state.get("mode")) == "1":
        car_state["cmd"] = "S"
        car_state["ai_action"] = "AUTO_STOP"

    asyncio.create_task(manager.broadcast_state())
    return JSONResponse(result)

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
                    if "ai" in msg:
                        car_state["ai"] = "1" if str(msg["ai"]) == "1" else "0"
                    if "mode" in msg:
                        car_state["mode"] = str(msg["mode"])

                    save_state_to_disk()
                    await manager.broadcast_state()
            except Exception:
                pass
    except WebSocketDisconnect:
        manager.disconnect(websocket)
    except Exception:
        manager.disconnect(websocket)

# ======================================================
# Camera Frame Upload & Stream Servicing
# ======================================================
@app.post("/cam/upload_frame.php")
@app.post("/api/frame")
async def upload_frame(file: UploadFile = File(...)):
    global current_recording_folder, is_recording_active

    contents = await file.read()
    if not contents:
        return PlainTextResponse("EMPTY")

    # Save as latest frame for AI / live display
    try:
        with open(LATEST_FRAME_FILE, "wb") as f:
            f.write(contents)
    except Exception:
        pass

    # If camera and AI are enabled, run ultra-fast background obstacle check
    if str(car_state.get("cam", "1")) == "1" and str(car_state.get("ai", "1")) == "1":
        res = analyze_frame_obstacle(contents)
        car_state["ai_obstacle"] = "1" if res["obstacle"] else "0"
        car_state["ai_score"] = str(res["score"])
        car_state["ai_action"] = res["action"]
        car_state["ai_latency"] = f"{res['latency_ms']}ms"

        if res["obstacle"] and str(car_state.get("mode")) == "1":
            car_state["cmd"] = "S"
            car_state["ai_action"] = "AUTO_STOP"

        asyncio.create_task(manager.broadcast_state())

    # If recording session active, save to session folder
    if is_recording_active and current_recording_folder:
        target_dir = RECORDINGS_DIR / current_recording_folder
        target_dir.mkdir(parents=True, exist_ok=True)
        frame_name = f"frame_{int(time.time() * 1000)}.jpg"
        with open(target_dir / frame_name, "wb") as f:
            f.write(contents)

    return PlainTextResponse("OK")

@app.get("/cam/latest.jpg")
@app.get("/api/latest.jpg")
def get_latest_frame():
    if LATEST_FRAME_FILE.exists():
        return FileResponse(LATEST_FRAME_FILE, media_type="image/jpeg", headers={"Cache-Control": "no-cache, max-age=0"})
    return Response(content=PROBE_GIF, media_type="image/gif")

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
