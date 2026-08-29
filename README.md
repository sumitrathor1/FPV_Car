# 🏎️ FPV Surveillance Car & AI Autonomous Driving System

An end-to-end IoT, Robotics, Computer Vision, and Web Engineering project featuring real-time first-person-view (FPV) video streaming, multi-mode remote car control, cloud telemetry, session recording playback, and an autonomous AI obstacle avoidance worker.

---

## 🌟 System Overview

```text
                                  +-----------------------+
                                  |   Web Command Center  |
                                  |   (Frontend HTML/JS)  |
                                  +-----------+-----------+
                                              |
                                              | HTTP (AJAX / REST)
                                              v
                              +---------------+---------------+
                              |       Cloud PHP Server        |
                              |  (State Sync & Cam Handlers)  |
                              +-------+---------------+-------+
                                      |               |
             HTTP Stream & Telemetry  |               |  Image Frames / AI Decision
                                      v               v
                +---------------------+--+     +------+------------------+
                | ESP32-CAM (AI-Thinker) |     |  Python AI Engine       |
                | (WiFi Video & Bridge)  |     |  (Obstacle Avoidance)   |
                +-------------+----------+     +-------------------------+
                              |
                              | UART Serial (115200 Baud)
                              v
                +-------------+----------+
                |      Arduino UNO       |
                | (PWM Motor Controller) |
                +-------------+----------+
                              |
                              v
                      [DC Motors & L298N]
```

---

## 📂 Repository Structure

| Directory | Description | Documentation |
| :--- | :--- | :--- |
| **`Frontend/`** | Modern Responsive Web Dashboard (Live video, D-Pad/Split-stick controls, speed slider, AI metrics). | [Frontend Docs](Frontend/README.md) |
| **`Backend/`** | Cloud REST API (`get.php`, `set.php`, `cam/*`) and Python Computer Vision engine (`ai_bot/`). | [Backend Docs](Backend/README.md) |
| **`Hardware/`** | Embedded C/C++ firmware for ESP32-CAM and Arduino UNO with UART serial bridge. | [Hardware Docs](Hardware/README.md) |
| **`.github/`** | Automated CI/CD workflows for frontend deployment over FTP to InfinityFree. | [deploy.yml](.github/workflows/deploy.yml) |

---

## 🚀 Quick Start Guide

### 1. Hardware Setup
- Flash `Hardware/ESP32_CAM/ESP32_Server_Client/ESP32_Server_Client.ino` to your ESP32-CAM (configure your WiFi credentials).
- Flash `Hardware/Arduino_UNO/Arduino_UNO.ino` to your Arduino UNO.
- Connect ESP32 `TX -> Arduino RX`, `RX -> Arduino TX`, and common `GND`.

### 2. Backend & Cloud Setup
- Upload `Backend/` PHP scripts to your web server (e.g. Apache/PHP or cloud hosting).
- Set write permissions (`chmod 777`) on the `/cam/` directory for saving frame uploads and recordings.

### 3. AI Computer Vision Bot (Optional)
- Install Python requirements:
  ```bash
  pip install -r Backend/ai_bot/requirements.txt
  ```
- Run the watchdog:
  ```bash
  python Backend/ai_bot/ai_watch.py
  ```

### 4. Frontend Web Dashboard
- Open `Frontend/index.html` in your browser or access it via your hosted URL.
