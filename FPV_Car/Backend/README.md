# 🚗 FPV Car - Backend Architecture & API Documentation

This directory contains the central control APIs (PHP) and the Computer Vision/AI engine (Python) that bridge the Frontend Web Dashboard and the ESP32-CAM / Arduino hardware.

---

## 📁 Directory Structure

```text
Backend/
├── ai_bot/                     # 🐍 Computer Vision & Autonomous Navigation Engine (Python)
│   ├── ai.py                   # Real-time obstacle detection & steering prediction algorithm
│   ├── ai_watch.py             # Watchdog supervisor (launches/kills ai.py based on cam state)
│   └── requirements.txt        # Python dependencies (opencv-python, requests, etc.)
│
├── cam/                        # 📷 Camera Stream & Session Recording Handlers (PHP)
│   ├── upload.php              # Receives incoming JPEG frames from ESP32-CAM (saves latest.jpg)
│   ├── start_record.php        # Initiates frame-by-frame recording session
│   ├── stop_record.php         # Ends recording session
│   ├── list.php                # Lists all recorded sessions in JSON
│   ├── list_frames.php         # Lists timestamped JPEG frames in a session
│   ├── download_recording.php  # Zips and downloads frame recordings
│   ├── convert_to_mp4.php      # Converts recorded frames into MP4 video (FFmpeg)
│   └── delete.php              # Deletes recording sessions
│
├── get.php                     # 📡 State Reader API (Returns full car state, telemetry & AI metrics)
├── set.php                     # 🕹️ Command Controller API (Sets car direction, speed, flash, mode)
└── README.md                   # This documentation file
```

---

## 📡 REST API Reference

### 1. `get.php` — Fetch System State & Telemetry
- **Method:** `GET`
- **Response Format:** `JSON`
- **Example Response:**
```json
{
  "cmd": "S",
  "mode": "0",
  "cam": "1",
  "flash": "0",
  "ai": "0",
  "ai_obstacle": "0",
  "ai_action": "IDLE",
  "ai_turn": "-",
  "ai_brightness": "120.4",
  "ai_edge": "15.2",
  "ai_near": "0.12",
  "ai_near_ema": "0.10",
  "ai_vclose": "0.0",
  "ai_pred_near": "0.14",
  "ai_pred_risk": "0.05",
  "ai_hit": "0",
  "ai_cmd": "S",
  "ai_score": "0.85",
  "ai_latency": "22",
  "ai_worker": "1",
  "fs": "255",
  "bs": "255"
}
```

---

### 2. `set.php` — Update Control Commands & Settings
- **Method:** `GET` / `POST`
- **Parameters:**
  - `cmd`: `F` (Forward), `B` (Backward), `L` (Left), `R` (Right), `S` (Stop)
  - `mode`: `0` (Step), `1` (Hold), `2` (Latch)
  - `cam`: `1` (Camera ON), `0` (Camera OFF / Hardware Sleep)
  - `flash`: `1` (Headlight ON), `0` (Headlight OFF)
  - `fs`: Forward PWM speed (`0` to `255`)
  - `bs`: Backward PWM speed (`0` to `255`)
  - `ai`: `1` (AI Auto-Drive Enabled), `0` (Disabled)
  - `ai_obstacle`, `ai_action`, `ai_turn`, `ai_score`, etc. (Updated by Python AI bot)

- **Example Usage:**
  ```http
  GET /fpv_car/set.php?cmd=F&fs=220
  GET /fpv_car/set.php?flash=1
  GET /fpv_car/set.php?cam=0
  ```

---

## 🐍 Python AI Engine Setup (`ai_bot/`)

### Requirements:
- Python 3.8+
- Dependencies: `pip install -r ai_bot/requirements.txt`

### Running the AI Engine:
1. **Watchdog Mode (Recommended):**
   ```bash
   python ai_bot/ai_watch.py
   ```
   Automatically starts `ai.py` when camera power is enabled and shuts it down to save CPU when camera is turned off.

2. **Standalone AI Worker:**
   ```bash
   python ai_bot/ai.py
   ```
