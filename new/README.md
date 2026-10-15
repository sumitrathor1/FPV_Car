# 🏎️ FPV Car - Ultra-Lightweight Standalone AP Edition

Pure local, zero-cloud, direct-connect setup.
No router, no internet, no Azure, no dynamic IP changes!
The ESP32 creates its own Wi-Fi Hotspot (**`FPV-Car`**), and you control it directly from any Mobile Phone or Laptop at **`http://192.168.4.1`**.

---

## 📁 Folder Structure

```
new/
├── Arduino_UNO/
│   └── Arduino_UNO.ino     <- Flash to Arduino UNO (Motor & Buzzer Driver)
├── ESP32_CAM/
│   └── ESP32_CAM.ino       <- Flash to AI-Thinker ESP32-CAM (Wi-Fi AP & Video Stream)
└── README.md               <- This complete guide
```

---

## 🔌 Hardware Circuit & Wiring

### 1. ESP32-CAM to Arduino UNO (Serial Bridge)
| ESP32-CAM Pin | Arduino UNO Pin | Purpose |
|---|---|---|
| **U0T (GPIO 1)** | **Pin 0 (RX)** | ESP32 transmits commands (`F`, `B`, `L`, `R`, `S`, `H`, etc.) |
| **GND** | **GND** | **COMMON GROUND (MANDATORY)** |
| **5V** | **5V Power Source** | Dedicated 5V 1A+ power (Battery or Buck Converter) |

> ⚠️ **IMPORTANT**: Disconnect ESP32-CAM TX from Arduino Pin 0 (RX) **while uploading code to Arduino UNO**, then reconnect it after uploading!

---

### 2. Arduino UNO to L298N / Motor Driver
| Motor Driver Pin | Arduino UNO Pin | Purpose |
|---|---|---|
| **IN1** | **Pin 5** | Left Motor Forward (PWM) |
| **IN2** | **Pin 6** | Left Motor Reverse (PWM) |
| **IN3** | **Pin 9** | Right Motor Forward (PWM) |
| **IN4** | **Pin 10** | Right Motor Reverse (PWM) |
| **GND** | **GND** | Common Ground |

---

### 3. Buzzer Pin
| Buzzer Pin | Arduino UNO Pin |
|---|---|
| **Positive (+)** | **Pin 4** |
| **Negative (-)** | **GND** |

---

## 💻 Uploading Code

### Step 1: Upload to Arduino UNO
1. Open [`Arduino_UNO.ino`](file:///c:/Users/Sumit/Desktop/FPV_Car/FPV_Car/new/Arduino_UNO/Arduino_UNO.ino) in Arduino IDE.
2. Select **Tools** -> **Board** -> **Arduino Uno**.
3. Select your Arduino COM Port.
4. Click **Upload**.

### Step 2: Upload to ESP32-CAM
1. Open [`ESP32_CAM.ino`](file:///c:/Users/Sumit/Desktop/FPV_Car/FPV_Car/new/ESP32_CAM/ESP32_CAM.ino) in Arduino IDE.
2. Select **Tools** -> **Board** -> **AI Thinker ESP32-CAM**.
3. Select **Tools** -> **PSRAM** -> **Enabled**.
4. Select **Tools** -> **Flash Frequency** -> **80MHz**.
5. Connect **GPIO 0 to GND** (on FTDI/programmer to enter flash mode).
6. Press the ESP32 **RST** button, then click **Upload**.
7. Once upload completes, **disconnect GPIO 0 from GND** and press **RST**.

---

## 📱 How to Connect & Drive in 10 Seconds

1. Power on the car.
   - You will hear the boot chime and motors give a quick test kick.
2. On your **Mobile Phone or Laptop**, open Wi-Fi settings.
3. Connect to the Wi-Fi network:
   - **SSID**: `FPV-Car`
   - **Password**: *(None - Open Network)*
4. Open Chrome / Safari / Edge browser and visit:
   👉 **http://192.168.4.1**
5. The full FPV controller loads instantly:
   - Touch D-Pad / Arrows to drive (Zero delay).
   - Keyboard: `W`, `A`, `S`, `D` / Arrows, `Space` = STOP, `H` = Horn, `L` = Headlight.
   - Motor speed slider (100–255).

---

## 🔧 Camera Probe Error 0x106 Troubleshooting

If the screen says **"Camera Sensor Offline (0x106)"**:
- **Motor controls are still 100% active and responsive!**
- To fix the camera video feed:
  1. Gently lift the **black/brown clip** on the camera ribbon connector on the ESP32-CAM.
  2. Pull out the yellow ribbon cable and insert it **straight and all the way in** (gold pins facing the board contacts).
  3. Snap the black clip firmly back down.
  4. Ensure ESP32-CAM is powered with a stable 5V (weak USB ports drop voltage below 3.0V, causing camera probe to fail).
  5. Tap the **"🔄 Re-Detect Camera"** button on the screen!
