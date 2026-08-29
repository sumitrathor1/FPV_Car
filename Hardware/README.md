# ⚡ FPV Car - Hardware Firmware & Embedded Systems

This directory contains the embedded C/C++ (Arduino) source code running on the dual-microcontroller setup: **ESP32-CAM (AI-Thinker)** and **Arduino UNO**.

---

## 🏗️ Architecture & Inter-Board Communication

```text
               +-------------------------------------+
               |           Cloud PHP Server          |
               |          (get.php / upload.php)     |
               +------------------+------------------+
                                  ^
                                  | WiFi (HTTP / 30ms Polling & JPEG Stream)
                                  v
                    +-------------+-------------+
                    |    ESP32-CAM (AI-Thinker) |
                    |   (Master Transceiver)    |
                    +-------------+-------------+
                                  |
                                  | UART Serial (115200 Baud)
                                  v
                    +-------------+-------------+
                    |        Arduino UNO        |
                    |    (Motor Driver Shield)  |
                    +-------------+-------------+
                                  |
                                  +--> L298N / Motor Driver -> DC Motors
```

---

## 📁 Subdirectories

### 1. `ESP32_CAM/`
- **`ESP32.ino`** *(Production Firmware)*:
  - Connects to local WiFi.
  - Polls commands from the cloud backend (`/fpv_car/get.php`) at ultra-low latency (~30ms).
  - Streams camera frames to `/fpv_car/cam/upload.php`.
  - Supports deep hardware camera de-initialization & GPIO PWDN power-off to eliminate CPU load when camera is switched off.
  - Relays parsed speed and directional commands over Hardware Serial (115200 baud) to Arduino UNO.

---

### 2. `Arduino_UNO/`
- **`Arduino_UNO.ino`** *(Production Firmware)*:
  - Receives directional (`F`, `B`, `L`, `R`, `S`) and dynamic PWM speed commands (`FSP:<val>`, `BSP:<val>`).
  - Directly drives the dual H-Bridge / L298N motor driver pins with hardware PWM.

---

## 🔌 Pin Connections

### ESP32-CAM to Arduino UNO:
| ESP32-CAM Pin | Arduino UNO Pin | Function |
| :--- | :--- | :--- |
| **U0TXD (GPIO 1)** | **Pin 0 (RX)** *(via level shift/divider if needed)* | Serial Data (Commands) |
| **U0RXD (GPIO 3)** | **Pin 1 (TX)** | Serial Telemetry |
| **GND** | **GND** | Common Ground *(Mandatory)* |
| **5V (External)** | **5V / VIN** | Regulated Power Supply |
