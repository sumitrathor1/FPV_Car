# 🌐 FPV Car - Frontend Web Dashboard

A state-of-the-art, responsive Web Command Center for real-time remote surveillance car operation, live video streaming, AI telemetry inspection, and recorded footage playback.

---

## ✨ Key Features

- **🎮 Tri-Mode Driving Control:**
  - **STEP Mode:** Sends momentary pulse on tap.
  - **HOLD Mode:** Continuous drive while button/key is held down.
  - **LATCH Mode:** Tap to latch direction until explicit Stop or opposite command.
- **⚡ Dual Controller Themes:**
  - Standard 5-button D-Pad with Emergency Stop.
  - Split-stick layout (Left column: Steering, Right column: Throttle).
- **📹 Live FPV Stream:**
  - Sub-second MJPEG/JPEG continuous stream directly from the car.
  - Snapshot Capture & full-screen view.
- **🧠 Real-Time AI Telemetry Panel:**
  - Obstacle detection status, risk score, depth/near estimation, and edge density.
  - Auto-Drive AI toggle with live worker heartbeat.
- **🏎️ Dynamic PWM Speed Tuning:**
  - Independent Forward Speed (`fs`) and Backward Speed (`bs`) sliders (0–255).
- **💡 Headlight Flash Control:**
  - Toggle onboard high-power ESP32-CAM flash LED remotely.
- **📼 Recording & Gallery Player:**
  - Start/Stop session recording directly to the server.
  - Frame-by-frame scrubbing, speed multiplier playback, and MP4 download conversion.

---

## 🚀 Deployment & Usage

### Local Usage:
Simply open `index.html` in any modern web browser or serve it using a lightweight HTTP server:
```bash
npx serve ./Frontend
```

### Production Hosting:
This frontend is configured with GitHub Actions to automatically deploy to the hosting server over FTP whenever changes are pushed to `main`.
