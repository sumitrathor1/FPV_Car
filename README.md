# 🏎️ FPV Surveillance Car & AI Autonomous Driving System

An end-to-end IoT, Robotics, Computer Vision, and Web Engineering project featuring real-time first-person-view (FPV) video streaming, zero-delay local AP driving, cloud telemetry over Azure VM, and an ultra-fast sub-millisecond AI obstacle avoidance engine.

---

## 📁 Clean Repository Structure

All duplicate, legacy, and temporary files have been eliminated. The project contains strictly the 4 core components:

```text
FPV_Car/
├── Arduino_UNO/            # 🚗 Arduino UNO Motor & Sound Driver
│   └── Arduino_UNO.ino     # Final Arduino UNO sketch (L298N, Buzzer, Boot Sound)
│
├── ESP32_CAM/              # 📷 ESP32-CAM Master Controller & Web Server
│   └── ESP32_CAM.ino       # Final ESP32 sketch (Camera, AP Hotspot, Wi-Fi Mode Switch)
│
├── Backend/                # ⚡ Cloud Backend & AI Engine (Azure VM)
│   ├── main.py             # High-speed FastAPI server & OpenCV AI obstacle detector (<3ms)
│   ├── requirements.txt    # Python dependencies (fastapi, uvicorn, opencv, websockets)
│   ├── deploy.sh           # 1-click auto deployment script for Ubuntu/Azure
│   ├── fpv-car.service     # Systemd production background service
│   └── nginx.conf          # Nginx reverse proxy & static frontend configuration
│
├── Frontend/               # 🕹️ Unified Cyber-Dark Web Controller UI
│   ├── index.html          # Responsive controller (AP 192.168.4.1 + Cloud 172.198.73.188)
│   ├── manifest.json       # Progressive Web App (PWA) manifest
│   └── sw.js               # Service Worker for offline/PWA caching
│
└── .github/workflows/      # Automated CI/CD pipelines
```

---

## 🚀 Quick Setup & Usage

### 1. 🚗 Arduino UNO (Motor & Buzzer Driver)
- **Path**: `Arduino_UNO/Arduino_UNO.ino`
- Open in **Arduino IDE**.
- Select Board: `Arduino Uno`, select your COM Port, and click **Upload**.
- **Features**:
  - Full PWM speed control (80-255).
  - Physical motor kick test and ascending chimes on boot.
  - 1.5-second fail-safe auto-stop if signal is interrupted.

### 2. 📷 ESP32-CAM (Camera, Hotspot & Controller)
- **Path**: `ESP32_CAM/ESP32_CAM.ino`
- Open in **Arduino IDE**.
- Board Configuration:
  - **Board**: `AI Thinker ESP32-CAM`
  - **CPU Frequency**: `240MHz`
  - **Flash Frequency**: `80MHz`
  - **Partition Scheme**: `Huge APP (3MB No OTA/1MB SPIFFS)`
  - **PSRAM**: `Enabled`
- Click **Upload**.
- **Operation Modes**:
  - **Default Boot: Local Mode (AP Hotspot)**:
    - Starts open Wi-Fi: **`FPV-Car`**
    - Connect your phone/laptop to `FPV-Car` and open **`http://192.168.4.1/`** in your browser.
    - Zero delay, direct LAN control (<2ms response time).
  - **Server Mode (Cloud Wi-Fi)**:
    - Click **`📻 LOCAL (AP)`** in the web dashboard header.
    - Click **Scan** to find nearby Wi-Fi networks, enter your password, and click **Connect**.
    - Car connects to your Wi-Fi, registers with the Azure Cloud VM, and provides the live dashboard URL: **`http://172.198.73.188/`**.

### 3. ⚡ Cloud Backend (Azure VM `172.198.73.188`)
- **Path**: `Backend/`
- Production FastAPI server with in-memory state, WebSockets, and real-time OpenCV obstacle avoidance.
- Auto-deploy on Azure:
  ```bash
  cd /home/azureuser/FPV_Car/Backend
  ./deploy.sh
  ```

### 4. 🕹️ Frontend Dashboard
- **Path**: `Frontend/index.html`
- Unified cyber-dark interface embedded directly in ESP32-CAM Flash memory and served by Nginx on Azure VM.
- Features:
  - Tactile touch D-Pad and keyboard controls (`W, A, S, D, Space, H, L`).
  - Hold Mode and Step Pulse driving modes.
  - Live AI Obstacle HUD and telemetry strip.
  - Camera Offline (0x106) auto-protection with on-demand `🔄 RE-DETECT CAMERA` button.
