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
| **U0TXD (GPIO 1)** | **Pin 0 (RX)** *(via level shift/divider if needed)* | Serial Data (Commands & Status Feedback) |
| **U0RXD (GPIO 3)** | **Pin 1 (TX)** | Serial Telemetry |
| **GND** | **GND** | Common Ground *(Mandatory)* |
| **5V (External)** | **5V / VIN** | Regulated Power Supply |

### Arduino UNO to Peripherals:
| Arduino UNO Pin | Peripheral / Target | Function |
| :--- | :--- | :--- |
| **Pin 4** | **Buzzer (+)** *(Buzzer (-) to GND)* | Audio Horn & State Beeps / Melodies |
| **Pin 5** | L298N `ENA` | Left Motors PWM Speed Control |
| **Pin 6** | L298N `ENB` | Right Motors PWM Speed Control |
| **Pin 7** | L298N `IN1` | Motor Direction Control |
| **Pin 8** | L298N `IN2` | Motor Direction Control |
| **Pin 9** | L298N `IN3` | Motor Direction Control |
| **Pin 10** | L298N `IN4` | Motor Direction Control |

---

## 🎶 Audio-Visual Feedback & State Matrix

The car combines the **ESP32-CAM Ultra-Bright Flashlight (GPIO 4)** and **Arduino UNO Buzzer (Pin 4)** to give crystal-clear feedback for every car state:

| Event / State | Flashlight (ESP32-CAM) | Buzzer (Arduino UNO Pin 4) | Meaning |
| :--- | :--- | :--- | :--- |
| **Power-On (3s Self-Test)** | 3 Synchronized Blinks | 3 Ascending Chimes + 80ms Motor Pulse | Confirms CPU, L298N driver, Flash LED & Buzzer are all operational! |
| **Wi-Fi Connected** | 2 Quick Flashes | 2 Cheerful Beeps (`Beep-Beep!`) | Successfully joined local Wi-Fi / Router |
| **AP Fallback Mode** | 3 Slow Warning Blinks | 3 Warning Pips | Router unreachable; Car started Hotspot `FPV-CAR-WIFI` |
| **Browser Dashboard Connected** | 1 Solid Flash | Ascending Chime (`Di-Doot!`) | Web UI active & communicating directly |
| **Browser Inactive / Disconnected** | 2 Warning Flashes | Descending Tone (`Boo-Boo`) | Web client left or lost connection (>6s) |
| **Horn Button Pressed** | Steady (if Flash enabled) | Continuous Horn Sound (1000 Hz) | Interactive Car Horn from UI or 'H' key |

