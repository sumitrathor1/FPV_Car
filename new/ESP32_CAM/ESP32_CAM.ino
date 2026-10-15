/******************************************************************************
 * 🏎️ FPV Car - ESP32-CAM Unified Controller (Local AP + Server Mode)
 *
 * Boot Default:
 *   - Boots in LOCAL (AP) MODE (Hotspot: "FPV-Car", IP: 192.168.4.1)
 *   - No router, no internet needed to drive immediately!
 *
 * Mode Switching from UI:
 *   - "SWITCH TO SERVER": Scans Wi-Fi, connects to user Wi-Fi, and registers
 *     local IP with Azure Cloud VM (http://172.198.73.188/)
 *   - "SWITCH TO LOCAL": Disconnects from Wi-Fi and returns to FPV-Car AP Hotspot
 *
 * Identical UI:
 *   - Embedded HTML is 100% identical to the cloud dashboard!
 ******************************************************************************/

#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <DNSServer.h>
#include "esp_http_server.h"

// ==========================================
// AI-Thinker ESP32-CAM Pin Configuration
// ==========================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define FLASH_LED_PIN      4

// Global States
bool cameraReady = false;
bool flashState = false;
bool isApMode = true; // Always true on boot!
char currentCmd = 'S';
int currentSpeed = 255;
String connectedSsid = "";

Preferences prefs;
DNSServer dnsServer;
const byte DNS_PORT = 53;

httpd_handle_t serverHttpd = NULL;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace; boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// URL Decoder helper
String urldecode(const String& str) {
  String decoded = "";
  char a, b;
  for (size_t i = 0; i < str.length(); ++i) {
    if (str[i] == '%') {
      if (i + 2 < str.length()) {
        a = str[i + 1];
        b = str[i + 2];
        if (isxdigit(a) && isxdigit(b)) {
          if (a >= 'a') a -= 'a' - 'A';
          if (a >= 'A') a -= ('A' - 10);
          else a -= '0';
          if (b >= 'a') b -= 'a' - 'A';
          if (b >= 'A') b -= ('A' - 10);
          else b -= '0';
          decoded += (char)(16 * a + b);
          i += 2;
        }
      }
    } else if (str[i] == '+') {
      decoded += ' ';
    } else {
      decoded += str[i];
    }
  }
  return decoded;
}

// ==========================================
// Embedded Unified Web Controller HTML
// ==========================================
static const char PROGMEM INDEX_HTML[] = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no" />
  <meta name="theme-color" content="#090d16" />
  <title>🏎️ FPV Car Unified Controller</title>
  <style>
    :root {
      --bg: #080c14;
      --card-bg: rgba(15, 23, 42, 0.75);
      --border: rgba(255, 255, 255, 0.1);
      --primary: #00f2fe;
      --primary-glow: rgba(0, 242, 254, 0.4);
      --accent: #4facfe;
      --danger: #f43f5e;
      --danger-glow: rgba(244, 63, 94, 0.4);
      --success: #10b981;
      --warning: #f59e0b;
      --text: #f8fafc;
      --text-muted: #94a3b8;
    }
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      user-select: none;
      -webkit-user-select: none;
      -webkit-touch-callout: none;
    }
    body {
      background: var(--bg);
      background-image: radial-gradient(circle at 50% 0%, rgba(0, 242, 254, 0.08) 0%, transparent 70%);
      color: var(--text);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 10px;
    }
    .app-container {
      width: 100%;
      max-width: 920px;
      display: flex;
      flex-direction: column;
      gap: 10px;
    }
    .header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      background: var(--card-bg);
      border: 1px solid var(--border);
      padding: 8px 14px;
      border-radius: 14px;
      backdrop-filter: blur(10px);
    }
    .brand {
      display: flex;
      align-items: center;
      gap: 8px;
      font-size: 1rem;
      font-weight: 800;
      letter-spacing: 0.5px;
      color: var(--primary);
    }
    .badge-group {
      display: flex;
      gap: 6px;
      align-items: center;
      flex-wrap: wrap;
    }
    .badge {
      font-size: 0.7rem;
      font-weight: 700;
      padding: 4px 9px;
      border-radius: 999px;
      background: rgba(255, 255, 255, 0.06);
      border: 1px solid var(--border);
      color: var(--text-muted);
      cursor: pointer;
      transition: all 0.15s;
      display: inline-flex;
      align-items: center;
      gap: 4px;
    }
    .badge.mode-btn {
      background: rgba(0, 242, 254, 0.15);
      border-color: var(--primary);
      color: var(--primary);
      font-weight: 800;
    }
    .badge.mode-btn:active {
      transform: scale(0.95);
    }
    .badge.online {
      background: rgba(16, 185, 129, 0.2);
      border-color: var(--success);
      color: #34d399;
    }
    .badge.offline {
      background: rgba(244, 63, 94, 0.2);
      border-color: var(--danger);
      color: #fb7185;
    }
    .dashboard-grid {
      display: grid;
      grid-template-columns: 1fr;
      gap: 12px;
    }
    @media (min-width: 768px) {
      .dashboard-grid {
        grid-template-columns: 1.15fr 1fr;
      }
    }
    .video-card {
      background: var(--card-bg);
      border: 1px solid var(--border);
      border-radius: 16px;
      padding: 10px;
      display: flex;
      flex-direction: column;
      gap: 10px;
    }
    .video-wrapper {
      position: relative;
      width: 100%;
      aspect-ratio: 4/3;
      background: #000;
      border: 2px solid #1e293b;
      border-radius: 12px;
      overflow: hidden;
      box-shadow: 0 8px 24px rgba(0, 0, 0, 0.6);
    }
    .video-wrapper img {
      width: 100%;
      height: 100%;
      object-fit: cover;
      display: block;
    }
    .ai-hud-overlay {
      position: absolute;
      top: 8px;
      left: 8px;
      right: 8px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      pointer-events: none;
    }
    .ai-pill {
      font-size: 0.7rem;
      font-weight: 800;
      padding: 3px 8px;
      border-radius: 6px;
      backdrop-filter: blur(8px);
      letter-spacing: 0.5px;
    }
    .ai-pill.clear {
      background: rgba(16, 185, 129, 0.75);
      color: #fff;
    }
    .ai-pill.obstacle {
      background: rgba(244, 63, 94, 0.85);
      color: #fff;
      animation: pulseAlert 1s infinite alternate;
    }
    .ai-pill.idle {
      background: rgba(15, 23, 42, 0.75);
      color: var(--text-muted);
    }
    @keyframes pulseAlert {
      from { transform: scale(1); box-shadow: 0 0 10px rgba(244, 63, 94, 0.5); }
      to { transform: scale(1.04); box-shadow: 0 0 20px rgba(244, 63, 94, 0.9); }
    }
    .video-placeholder {
      position: absolute;
      inset: 0;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      background: #020617;
      color: var(--text-muted);
      gap: 6px;
      padding: 14px;
      text-align: center;
    }
    .telemetry-strip {
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      gap: 6px;
      background: rgba(255, 255, 255, 0.03);
      padding: 8px;
      border-radius: 10px;
      border: 1px solid var(--border);
      text-align: center;
    }
    .tele-item .label {
      font-size: 0.65rem;
      color: var(--text-muted);
      font-weight: 700;
      display: block;
      margin-bottom: 2px;
    }
    .tele-item .val {
      font-size: 0.8rem;
      font-weight: 800;
      color: var(--primary);
    }
    .tele-item .val.danger {
      color: var(--danger);
    }
    .actions-bar {
      display: flex;
      gap: 6px;
      flex-wrap: wrap;
    }
    .action-btn {
      flex: 1;
      min-width: 75px;
      background: rgba(255, 255, 255, 0.05);
      border: 1px solid var(--border);
      color: var(--text);
      font-size: 0.75rem;
      font-weight: 700;
      padding: 8px 10px;
      border-radius: 10px;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 5px;
      touch-action: manipulation;
      transition: all 0.15s;
    }
    .action-btn:active, .action-btn.active {
      background: var(--primary);
      color: #000;
      border-color: var(--primary);
      box-shadow: 0 0 14px var(--primary-glow);
    }
    .action-btn.horn:active, .action-btn.horn.active {
      background: var(--danger);
      color: #fff;
      border-color: var(--danger);
    }
    .ctrl-card {
      background: var(--card-bg);
      border: 1px solid var(--border);
      border-radius: 16px;
      padding: 14px;
      display: flex;
      flex-direction: column;
      justify-content: space-between;
      gap: 12px;
    }
    .mode-selector {
      display: flex;
      background: rgba(0, 0, 0, 0.4);
      padding: 3px;
      border-radius: 10px;
      border: 1px solid var(--border);
    }
    .mode-tab {
      flex: 1;
      text-align: center;
      padding: 6px;
      font-size: 0.75rem;
      font-weight: 700;
      border-radius: 8px;
      color: var(--text-muted);
      cursor: pointer;
    }
    .mode-tab.active {
      background: var(--primary);
      color: #000;
      box-shadow: 0 2px 8px var(--primary-glow);
    }
    .dpad {
      display: grid;
      grid-template-columns: repeat(3, 76px);
      grid-template-rows: repeat(3, 76px);
      gap: 10px;
      justify-content: center;
      margin: 8px auto;
    }
    .d-btn {
      background: #1e293b;
      border: 2px solid #334155;
      border-radius: 18px;
      color: #fff;
      font-size: 1.8rem;
      display: flex;
      align-items: center;
      justify-content: center;
      cursor: pointer;
      touch-action: none;
      box-shadow: 0 4px 14px rgba(0, 0, 0, 0.5);
      transition: all 0.08s;
    }
    .d-btn:active, .d-btn.pressed {
      background: var(--primary);
      border-color: #fff;
      color: #000;
      transform: scale(0.92);
      box-shadow: 0 0 20px var(--primary-glow);
    }
    .d-btn.stop {
      background: var(--danger);
      border-color: #fda4af;
      font-size: 1rem;
      font-weight: 900;
    }
    .d-btn.stop:active {
      background: #b91c1c;
    }
    .slider-wrap {
      background: rgba(255, 255, 255, 0.03);
      border: 1px solid var(--border);
      padding: 8px 12px;
      border-radius: 10px;
    }
    .slider-wrap label {
      display: flex;
      justify-content: space-between;
      font-size: 0.75rem;
      color: var(--text-muted);
      font-weight: 700;
      margin-bottom: 4px;
    }
    input[type=range] {
      width: 100%;
      accent-color: var(--primary);
      cursor: pointer;
    }
    .modal-backdrop {
      display: none;
      position: fixed;
      inset: 0;
      background: rgba(2, 6, 23, 0.85);
      backdrop-filter: blur(8px);
      z-index: 1000;
      align-items: center;
      justify-content: center;
      padding: 16px;
    }
    .modal-backdrop.active {
      display: flex;
    }
    .modal-card {
      background: #0f172a;
      border: 1px solid rgba(255, 255, 255, 0.15);
      border-radius: 18px;
      width: 100%;
      max-width: 400px;
      padding: 18px;
      box-shadow: 0 10px 35px rgba(0, 0, 0, 0.8);
      display: flex;
      flex-direction: column;
      gap: 12px;
    }
    .modal-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .modal-title {
      font-size: 1rem;
      font-weight: 800;
      color: var(--primary);
    }
    .modal-close {
      background: none;
      border: none;
      color: var(--text-muted);
      font-size: 1.3rem;
      cursor: pointer;
    }
    .modal-body {
      display: flex;
      flex-direction: column;
      gap: 10px;
    }
    .input-group {
      display: flex;
      flex-direction: column;
      gap: 4px;
    }
    .input-group label {
      font-size: 0.75rem;
      font-weight: 700;
      color: var(--text-muted);
    }
    .input-control {
      background: rgba(255, 255, 255, 0.05);
      border: 1px solid var(--border);
      color: #fff;
      padding: 10px 12px;
      border-radius: 10px;
      font-size: 0.85rem;
      outline: none;
    }
    .input-control:focus {
      border-color: var(--primary);
    }
    .wifi-list-box {
      max-height: 110px;
      overflow-y: auto;
      display: flex;
      flex-direction: column;
      gap: 4px;
      background: rgba(0, 0, 0, 0.35);
      padding: 6px;
      border-radius: 8px;
      border: 1px solid var(--border);
    }
    .wifi-item {
      padding: 6px 10px;
      font-size: 0.8rem;
      background: rgba(255, 255, 255, 0.04);
      border-radius: 6px;
      cursor: pointer;
      display: flex;
      justify-content: space-between;
    }
    .wifi-item:hover, .wifi-item.selected {
      background: rgba(0, 242, 254, 0.2);
      color: var(--primary);
      font-weight: 700;
    }
    .btn-submit {
      background: var(--primary);
      color: #000;
      font-weight: 800;
      font-size: 0.85rem;
      padding: 11px;
      border-radius: 10px;
      border: none;
      cursor: pointer;
      transition: all 0.15s;
      margin-top: 4px;
    }
    .btn-submit:active {
      transform: scale(0.98);
    }
    .footer {
      font-size: 0.75rem;
      color: var(--text-muted);
      text-align: center;
      margin-top: 6px;
    }
  </style>
</head>
<body>
  <div class="app-container">
    <header class="header">
      <div class="brand">
        <span>🏎️</span>
        <span>FPV CONTROLLER</span>
      </div>
      <div class="badge-group">
        <span class="badge mode-btn" id="modeSwitchBtn" onclick="handleModeSwitchClick()">📻 LOCAL (AP)</span>
        <span class="badge offline" id="statusBadge">● OFFLINE</span>
        <span class="badge" id="ipBadge" onclick="editCarIp()">IP: AUTO</span>
        <span class="badge" id="latencyBadge">⚡ --ms</span>
      </div>
    </header>

    <div class="dashboard-grid">
      <section class="video-card">
        <div class="video-wrapper">
          <img id="liveCam" src="" alt="FPV Live Camera Feed" />
          
          <div class="ai-hud-overlay">
            <span class="ai-pill idle" id="aiStatusPill">🛡️ AI: IDLE</span>
            <span class="ai-pill idle" id="aiLatencyPill">0ms</span>
          </div>

          <div class="video-placeholder" id="placeholder">
            <span style="font-size: 2.2rem;">📷</span>
            <span style="font-weight: 800; font-size: 0.95rem; color:#f87171;" id="placeholderTitle">Camera Sensor Offline (0x106)</span>
            <span style="font-size: 0.75rem; color:#94a3b8; line-height:1.4;" id="placeholderDesc">Check 24-pin ribbon cable seating & 5V power.<br><b style="color:#00f2fe;">Motor controls are 100% active below!</b></span>
            <button class="action-btn" onclick="retryCamera()" style="flex:none; padding:6px 14px; margin-top:6px; background:#0284c7; border-color:#38bdf8;">🔄 RE-DETECT CAMERA</button>
          </div>
        </div>

        <div class="telemetry-strip">
          <div class="tele-item">
            <span class="label">ACTION</span>
            <span class="val" id="teleAction">IDLE</span>
          </div>
          <div class="tele-item">
            <span class="label">OBSTACLE</span>
            <span class="val" id="teleObstacle">CLEAR</span>
          </div>
          <div class="tele-item">
            <span class="label">AI MODE</span>
            <span class="val" id="teleAiMode">ASSIST</span>
          </div>
          <div class="tele-item">
            <span class="label">SPEED</span>
            <span class="val" id="teleSpeed">255</span>
          </div>
        </div>

        <div class="actions-bar">
          <button class="action-btn active" id="camBtn" onclick="toggleCam()"><span>📹</span><span id="camBtnTxt">CAM ON</span></button>
          <button class="action-btn active" id="aiBtn" onclick="toggleAi()"><span>🤖</span><span id="aiBtnTxt">AI ON</span></button>
          <button class="action-btn" id="lightBtn" onclick="toggleLight()"><span>💡</span><span>LIGHT</span></button>
          <button class="action-btn horn" id="hornBtn" onpointerdown="sendHorn(1)" onpointerup="sendHorn(0)" onpointerleave="sendHorn(0)"><span>📢</span><span>HORN</span></button>
        </div>
      </section>

      <section class="ctrl-card">
        <div class="mode-selector">
          <div class="mode-tab active" id="modeHold" onclick="setDriveMode(0)">HOLD MODE</div>
          <div class="mode-tab" id="modeStep" onclick="setDriveMode(1)">STEP PULSE</div>
          <div class="mode-tab" id="modeAuto" onclick="toggleAutoMode()">AI ASSIST</div>
        </div>

        <div class="dpad">
          <div></div>
          <button class="d-btn" onpointerdown="handlePress('F')" onpointerup="handleRelease('F')" onpointerleave="handleRelease('F')">▲</button>
          <div></div>

          <button class="d-btn" onpointerdown="handlePress('L')" onpointerup="handleRelease('L')" onpointerleave="handleRelease('L')">◀</button>
          <button class="d-btn stop" onclick="sendCmd('S')">STOP</button>
          <button class="d-btn" onpointerdown="handlePress('R')" onpointerup="handleRelease('R')" onpointerleave="handleRelease('R')">▶</button>

          <div></div>
          <button class="d-btn" onpointerdown="handlePress('B')" onpointerup="handleRelease('B')" onpointerleave="handleRelease('B')">▼</button>
          <div></div>
        </div>

        <div class="slider-wrap">
          <label><span>⚡ MOTOR SPEED PWM</span><span id="sliderVal">255</span></label>
          <input type="range" min="80" max="255" value="255" step="5" oninput="changeSpeed(this.value)" />
        </div>
      </section>
    </div>

    <!-- Wi-Fi Setup Modal -->
    <div class="modal-backdrop" id="wifiModal">
      <div class="modal-card">
        <div class="modal-header">
          <span class="modal-title">📡 Connect to Wi-Fi (Server Mode)</span>
          <button class="modal-close" onclick="closeWifiModal()">✕</button>
        </div>
        <div class="modal-body">
          <div style="font-size:0.75rem; color:#94a3b8;">
            Connect car to your Home/Mobile Wi-Fi to access it over Cloud at <b>http://172.198.73.188/</b>
          </div>

          <div class="input-group">
            <div style="display:flex; justify-content:space-between; align-items:center;">
              <label>Select Nearby Wi-Fi</label>
              <button onclick="scanWifi()" style="background:none; border:none; color:var(--primary); font-size:0.75rem; cursor:pointer; font-weight:700;">🔄 Scan</button>
            </div>
            <div class="wifi-list-box" id="wifiListBox">
              <div style="padding:10px; text-align:center; color:#64748b; font-size:0.75rem;">Click 'Scan' to find networks</div>
            </div>
          </div>

          <div class="input-group">
            <label>Wi-Fi SSID Name</label>
            <input type="text" class="input-control" id="ssidInput" placeholder="e.g. sumit or Home-Wifi" />
          </div>

          <div class="input-group">
            <label>Wi-Fi Password</label>
            <input type="password" class="input-control" id="passInput" placeholder="Enter Wi-Fi password" />
          </div>

          <div id="connectStatusMsg" style="font-size:0.8rem; font-weight:700; text-align:center; min-height:18px;"></div>

          <button class="btn-submit" id="connectBtn" onclick="submitWifiConnect()">🚀 Connect & Switch to Server</button>
        </div>
      </div>
    </div>

    <footer class="footer">
      ⌨️ Controls: W, A, S, D / Arrow Keys to Drive | Space = STOP | H = Horn | L = Light
    </footer>
  </div>

  <script>
    const CLOUD_SERVER_URL = "http://172.198.73.188/";
    const LOCAL_AP_IP = "192.168.4.1";

    const isRunningOnLocalAp = (window.location.hostname === LOCAL_AP_IP || window.location.hostname === "fpv.car");
    let carHost = isRunningOnLocalAp ? LOCAL_AP_IP : (localStorage.getItem("fpv_car_ip") || "10.172.34.222");

    let isCamOn = true;
    let isAiOn = true;
    let isLightOn = false;
    let isAutoMode = false;
    let driveMode = 0;
    let activeDirection = 'S';
    let currentSpeed = 255;
    let lastPingTime = 0;

    const liveCam = document.getElementById("liveCam");
    const placeholder = document.getElementById("placeholder");
    const placeholderTitle = document.getElementById("placeholderTitle");
    const placeholderDesc = document.getElementById("placeholderDesc");
    const statusBadge = document.getElementById("statusBadge");
    const ipBadge = document.getElementById("ipBadge");
    const modeSwitchBtn = document.getElementById("modeSwitchBtn");
    const latencyBadge = document.getElementById("latencyBadge");
    const aiStatusPill = document.getElementById("aiStatusPill");
    const aiLatencyPill = document.getElementById("aiLatencyPill");
    const teleAction = document.getElementById("teleAction");
    const teleObstacle = document.getElementById("teleObstacle");
    const teleAiMode = document.getElementById("teleAiMode");
    const teleSpeed = document.getElementById("teleSpeed");
    const sliderVal = document.getElementById("sliderVal");
    const wifiModal = document.getElementById("wifiModal");

    updateModeBadgeUI();

    function updateModeBadgeUI() {
      if (isRunningOnLocalAp || carHost === LOCAL_AP_IP) {
        modeSwitchBtn.textContent = "📻 LOCAL (AP)";
        modeSwitchBtn.title = "Click to switch Car to Server Mode (Wi-Fi)";
        ipBadge.textContent = "IP: 192.168.4.1";
      } else {
        modeSwitchBtn.textContent = "☁️ SERVER (WIFI)";
        modeSwitchBtn.title = "Click to switch Car back to Local AP Hotspot";
        ipBadge.textContent = "IP: " + carHost;
      }
    }

    function editCarIp() {
      const val = prompt("Enter Car Local IP Address (e.g. 10.172.34.222 or 192.168.4.1):", carHost);
      if (val && val.trim()) {
        carHost = val.trim().replace(/^https?:\/\//, '').replace(/\/.*$/, '');
        localStorage.setItem("fpv_car_ip", carHost);
        updateModeBadgeUI();
        sendServerReq(`/set.php?car_ip=${carHost}`);
        startStream();
      }
    }

    function handleModeSwitchClick() {
      if (isRunningOnLocalAp || carHost === LOCAL_AP_IP) {
        openWifiModal();
      } else {
        if (confirm("Switch Car back to Local Hotspot mode (192.168.4.1)?\n\nYour phone/laptop will need to connect to Wi-Fi 'FPV-Car'.")) {
          switchToLocalAp();
        }
      }
    }

    function openWifiModal() {
      wifiModal.classList.add("active");
      scanWifi();
    }

    function closeWifiModal() {
      wifiModal.classList.remove("active");
    }

    function scanWifi() {
      const box = document.getElementById("wifiListBox");
      box.innerHTML = `<div style="padding:10px; text-align:center; color:#00f2fe; font-size:0.75rem;">Scanning nearby Wi-Fi networks...</div>`;
      
      const target = isRunningOnLocalAp ? "" : `http://${carHost}`;
      fetch(`${target}/scan`)
        .then(r => r.json())
        .then(list => {
          if (!list || list.length === 0) {
            box.innerHTML = `<div style="padding:10px; text-align:center; color:#94a3b8; font-size:0.75rem;">No networks found</div>`;
            return;
          }
          box.innerHTML = "";
          list.forEach(item => {
            const div = document.createElement("div");
            div.className = "wifi-item";
            div.innerHTML = `<span><b>${item.ssid}</b></span><span style="color:#64748b;">${item.rssi} dBm</span>`;
            div.onclick = () => {
              document.querySelectorAll(".wifi-item").forEach(el => el.classList.remove("selected"));
              div.classList.add("selected");
              document.getElementById("ssidInput").value = item.ssid;
              document.getElementById("passInput").focus();
            };
            box.appendChild(div);
          });
        })
        .catch(() => {
          box.innerHTML = `<div style="padding:8px; text-align:center; color:#f87171; font-size:0.75rem;">Scan failed. You can type SSID manually below.</div>`;
        });
    }

    function submitWifiConnect() {
      const ssid = document.getElementById("ssidInput").value.trim();
      const pass = document.getElementById("passInput").value.trim();
      const msg = document.getElementById("connectStatusMsg");
      const btn = document.getElementById("connectBtn");

      if (!ssid) {
        alert("Please enter a Wi-Fi SSID");
        return;
      }

      btn.disabled = true;
      msg.style.color = "#00f2fe";
      msg.textContent = `Connecting car to "${ssid}"... Please wait up to 10s...`;

      const target = isRunningOnLocalAp ? "" : `http://${carHost}`;
      const url = `${target}/save-wifi?ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}`;

      fetch(url)
        .then(r => r.json())
        .then(res => {
          btn.disabled = false;
          if (res.status === "ok") {
            msg.style.color = "#34d399";
            msg.innerHTML = `✅ Connected! Car IP: <b>${res.ip}</b><br><br><a href="${CLOUD_SERVER_URL}" target="_blank" style="color:#00f2fe; text-decoration:underline; font-weight:800;">🚀 Open Cloud Dashboard (http://172.198.73.188/)</a>`;
            carHost = res.ip;
            localStorage.setItem("fpv_car_ip", carHost);
            updateModeBadgeUI();
          } else {
            msg.style.color = "#f87171";
            msg.textContent = `❌ Connection failed. Check Wi-Fi password and try again.`;
          }
        })
        .catch(() => {
          btn.disabled = false;
          msg.style.color = "#f59e0b";
          msg.innerHTML = `Signal sent! Car is connecting to Wi-Fi.<br>Connect your phone to <b>"${ssid}"</b> and visit <a href="${CLOUD_SERVER_URL}" style="color:#00f2fe;">${CLOUD_SERVER_URL}</a>`;
        });
    }

    function switchToLocalAp() {
      const target = isRunningOnLocalAp ? "" : `http://${carHost}`;
      fetch(`${target}/switch-mode?mode=ap`)
        .then(() => {
          alert("Car is now in Local Hotspot mode!\n\n1. Connect phone to Wi-Fi: FPV-Car\n2. Open http://192.168.4.1/");
          window.location.href = `http://${LOCAL_AP_IP}/`;
        })
        .catch(() => {
          alert("Mode switch command sent. Connect phone to Wi-Fi 'FPV-Car' and visit http://192.168.4.1/");
        });
    }

    function startStream() {
      if (!isCamOn) {
        liveCam.src = "";
        placeholder.style.display = "flex";
        placeholderTitle.textContent = "Camera Turned Off";
        placeholderDesc.textContent = "Camera is sleeping (0% CPU / Zero Latency)";
        aiStatusPill.className = "ai-pill idle";
        aiStatusPill.textContent = "🛡️ AI: CAM OFF";
        return;
      }

      clearTimeout(streamRetryTimer);
      clearTimeout(fallbackCaptureTimer);

      placeholder.style.display = "flex";
      placeholderTitle.textContent = "Connecting to Video Stream...";
      placeholderDesc.textContent = "Establishing live stream link...";

      const streamUrl = isRunningOnLocalAp ? "/stream" : `http://${carHost}/stream`;
      liveCam.src = streamUrl;

      liveCam.onload = () => {
        placeholder.style.display = "none";
        setOnline(true);
      };

      liveCam.onerror = () => {
        function pollFallback() {
          if (!isCamOn) return;
          const img = new Image();
          img.onload = () => {
            liveCam.src = img.src;
            placeholder.style.display = "none";
            setOnline(true);
            setTimeout(pollFallback, 45);
          };
          img.onerror = () => {
            placeholder.style.display = "flex";
            placeholderTitle.textContent = "Camera Sensor Offline (0x106)";
            placeholderDesc.innerHTML = "Check 24-pin ribbon cable seating & 5V power.<br><b style='color:#00f2fe;'>Motor controls are 100% active below!</b>";
            setTimeout(pollFallback, 3000);
          };
          const base = isRunningOnLocalAp ? "" : `http://${carHost}`;
          img.src = `${base}/capture?t=${Date.now()}`;
        }
        pollFallback();

        streamRetryTimer = setTimeout(startStream, 6000);
      };
    }

    function retryCamera() {
      const base = isRunningOnLocalAp ? "" : `http://${carHost}`;
      fetch(`${base}/reprobe`, { mode: 'no-cors' }).catch(() => {});
      setTimeout(() => {
        startStream();
      }, 500);
    }

    function sendServerReq(url) {
      if (isRunningOnLocalAp) return;
      lastPingTime = performance.now();
      fetch(url, { keepalive: true, cache: 'no-store' })
        .then(r => {
          const lat = Math.round(performance.now() - lastPingTime);
          latencyBadge.textContent = `⚡ ${lat}ms`;
          setOnline(true);
        })
        .catch(() => {});
    }

    function sendCmd(cmd) {
      activeDirection = cmd;
      teleAction.textContent = cmd === 'S' ? 'STOP' : cmd;

      const base = isRunningOnLocalAp ? "" : `http://${carHost}`;
      const u = `${base}/cmd?dir=${cmd}`;
      fetch(u, { mode: 'no-cors', keepalive: true }).catch(() => {
        new Image().src = u;
      });

      if (!isRunningOnLocalAp) {
        sendServerReq(`/set.php?cmd=${cmd}`);
      }
    }

    function handlePress(dir) {
      if (driveMode === 0) {
        sendCmd(dir);
      } else {
        sendCmd(dir);
        setTimeout(() => sendCmd('S'), 200);
      }
    }

    function handleRelease(dir) {
      if (driveMode === 0 && activeDirection === dir) {
        sendCmd('S');
      }
    }

    function sendHorn(v) {
      const base = isRunningOnLocalAp ? "" : `http://${carHost}`;
      fetch(`${base}/horn?val=${v}`, { mode: 'no-cors' }).catch(() => {});
      if (!isRunningOnLocalAp) sendServerReq(`/set.php?horn=${v}`);
      document.getElementById("hornBtn").classList.toggle("active", v === 1);
    }

    function toggleLight() {
      isLightOn = !isLightOn;
      const v = isLightOn ? 1 : 0;
      const base = isRunningOnLocalAp ? "" : `http://${carHost}`;
      fetch(`${base}/flash?val=${v}`, { mode: 'no-cors' }).catch(() => {});
      if (!isRunningOnLocalAp) sendServerReq(`/set.php?flash=${v}`);
      document.getElementById("lightBtn").classList.toggle("active", isLightOn);
    }

    function toggleCam() {
      isCamOn = !isCamOn;
      document.getElementById("camBtn").classList.toggle("active", isCamOn);
      document.getElementById("camBtnTxt").textContent = isCamOn ? "CAM ON" : "CAM OFF";
      if (!isRunningOnLocalAp) sendServerReq(`/set.php?cam=${isCamOn ? 1 : 0}`);
      startStream();
    }

    function toggleAi() {
      isAiOn = !isAiOn;
      document.getElementById("aiBtn").classList.toggle("active", isAiOn);
      document.getElementById("aiBtnTxt").textContent = isAiOn ? "AI ON" : "AI OFF";
      if (!isRunningOnLocalAp) sendServerReq(`/set.php?ai=${isAiOn ? 1 : 0}`);
      updateAiUI();
    }

    function changeSpeed(val) {
      currentSpeed = val;
      sliderVal.textContent = val;
      teleSpeed.textContent = val;
      const base = isRunningOnLocalAp ? "" : `http://${carHost}`;
      fetch(`${base}/speed?s=${val}&fs=${val}&bs=${val}`, { mode: 'no-cors' }).catch(() => {});
      if (!isRunningOnLocalAp) sendServerReq(`/set.php?speed=${val}`);
    }

    function setDriveMode(m) {
      driveMode = m;
      document.getElementById("modeHold").classList.toggle("active", m === 0);
      document.getElementById("modeStep").classList.toggle("active", m === 1);
    }

    function toggleAutoMode() {
      isAutoMode = !isAutoMode;
      document.getElementById("modeAuto").classList.toggle("active", isAutoMode);
      teleAiMode.textContent = isAutoMode ? "AUTO-STOP" : "MANUAL";
      if (!isRunningOnLocalAp) sendServerReq(`/set.php?mode=${isAutoMode ? 1 : 0}`);
    }

    function setOnline(online) {
      statusBadge.className = online ? "badge online" : "badge offline";
      statusBadge.textContent = online ? "● ONLINE" : "● OFFLINE";
    }

    function updateAiUI(state) {
      if (!isCamOn) {
        aiStatusPill.className = "ai-pill idle";
        aiStatusPill.textContent = "🛡️ AI: CAM OFF";
        teleObstacle.textContent = "IDLE";
        teleObstacle.className = "val";
        return;
      }
      if (!isAiOn) {
        aiStatusPill.className = "ai-pill idle";
        aiStatusPill.textContent = "🛡️ AI: OFF";
        teleObstacle.textContent = "DISABLED";
        teleObstacle.className = "val";
        return;
      }

      if (state && state.ai_obstacle === "1") {
        aiStatusPill.className = "ai-pill obstacle";
        aiStatusPill.textContent = `🚨 OBSTACLE DETECTED!`;
        teleObstacle.textContent = "DETECTED";
        teleObstacle.className = "val danger";
      } else {
        aiStatusPill.className = "ai-pill clear";
        aiStatusPill.textContent = "🛡️ PATH CLEAR";
        teleObstacle.textContent = "CLEAR";
        teleObstacle.className = "val";
      }

      if (state && state.ai_latency) {
        aiLatencyPill.textContent = state.ai_latency;
      }
    }

    let ws = null;
    function connectWS() {
      if (isRunningOnLocalAp) return;
      const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
      const wsUrl = `${proto}//${window.location.host}/ws`;
      try {
        ws = new WebSocket(wsUrl);
        ws.onopen = () => setOnline(true);
        ws.onmessage = (e) => {
          try {
            const data = JSON.parse(e.data);
            if (data.car_ip && data.car_ip.length >= 7 && data.car_ip !== carHost) {
              carHost = data.car_ip;
              localStorage.setItem("fpv_car_ip", carHost);
              updateModeBadgeUI();
            }
            updateAiUI(data);
          } catch (err) {}
        };
        ws.onclose = () => setTimeout(connectWS, 2500);
      } catch (err) {
        setTimeout(connectWS, 3000);
      }
    }

    setInterval(() => {
      const endpoint = isRunningOnLocalAp ? "/status" : `./get.php?t=${Date.now()}`;
      fetch(endpoint)
        .then(r => r.json())
        .then(data => {
          if (data.car_ip && data.car_ip.length >= 7 && data.car_ip !== carHost) {
            carHost = data.car_ip;
            localStorage.setItem("fpv_car_ip", carHost);
            updateModeBadgeUI();
          }
          updateAiUI(data);
          setOnline(true);
        })
        .catch(() => {});
    }, 2000);

    window.addEventListener("keydown", e => {
      if (e.repeat) return;
      const k = e.key.toLowerCase();
      if (k === 'w' || k === 'arrowup') handlePress('F');
      else if (k === 's' || k === 'arrowdown') handlePress('B');
      else if (k === 'a' || k === 'arrowleft') handlePress('L');
      else if (k === 'd' || k === 'arrowright') handlePress('R');
      else if (k === ' ' || k === 'escape') sendCmd('S');
      else if (k === 'h') sendHorn(1);
      else if (k === 'l') toggleLight();
    });

    window.addEventListener("keyup", e => {
      const k = e.key.toLowerCase();
      if (['w', 's', 'a', 'd', 'arrowup', 'arrowdown', 'arrowleft', 'arrowright'].includes(k)) {
        handleRelease(activeDirection);
      } else if (k === 'h') {
        sendHorn(0);
      }
    });

    connectWS();
    startStream();
  </script>
</body>
</html>)rawliteral";

// ==========================================
// Camera Hardware Initialization
// ==========================================
bool startCameraDriver() {
  if (cameraReady) return true;

  pinMode(PWDN_GPIO_NUM, OUTPUT);
  digitalWrite(PWDN_GPIO_NUM, HIGH); // Power off
  delay(120);
  digitalWrite(PWDN_GPIO_NUM, LOW);  // Power on
  delay(120);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_HVGA; // 480x320: Crisp + Ultra Low Lag (<30ms)
    config.jpeg_quality = 12;
    config.fb_count     = 2;
#ifdef CAMERA_GRAB_LATEST
    config.grab_mode    = CAMERA_GRAB_LATEST;
#endif
  } else {
    config.frame_size   = FRAMESIZE_QVGA; // 320x240
    config.jpeg_quality = 14;
    config.fb_count     = 1;
  }

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    esp_camera_deinit();
    delay(50);
    config.xclk_freq_hz = 10000000;
    config.frame_size   = FRAMESIZE_QVGA;
    config.fb_count     = 1;
    err = esp_camera_init(&config);
  }

  if (err != ESP_OK) {
    esp_camera_deinit();
    delay(50);
    config.pin_pwdn     = -1;
    config.xclk_freq_hz = 10000000;
    err = esp_camera_init(&config);
  }

  if (err == ESP_OK) {
    cameraReady = true;
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
      s->set_brightness(s, 1);
      s->set_contrast(s, 1);
      s->set_saturation(s, 0);
      s->set_whitebal(s, 1);
      s->set_awb_gain(s, 1);
      s->set_wb_mode(s, 0);
    }
    Serial.println("[CAMERA] Sensor Initialized OK");
    return true;
  } else {
    cameraReady = false;
    Serial.printf("[CAMERA] Probe failed (0x%x) -> Check ribbon cable seating!\n", err);
    return false;
  }
}

// ==========================================
// HTTP Handlers
// ==========================================
esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

esp_err_t stream_handler(httpd_req_t *req) {
  if (!cameraReady) {
    httpd_resp_set_status(req, "503 Camera Offline");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "Camera Offline", HTTPD_RESP_USE_STRLEN);
  }

  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[128];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");

  while (true) {
    if (!cameraReady) break;
    fb = esp_camera_fb_get();
    if (!fb) {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, fb->len);
    res = httpd_resp_send_chunk(req, part_buf, hlen);
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    esp_camera_fb_return(fb);

    if (res != ESP_OK) break;
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
  return res;
}

esp_err_t cmd_handler(httpd_req_t *req) {
  char buf[32];
  if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
    char dir[8];
    if (httpd_query_key_value(buf, "dir", dir, sizeof(dir)) == ESP_OK) {
      char c = dir[0];
      if (c == 'F' || c == 'B' || c == 'L' || c == 'R' || c == 'S') {
        currentCmd = c;
        Serial.println(c);
        Serial.flush();
      }
    }
  }
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, "OK", 2);
}

esp_err_t horn_handler(httpd_req_t *req) {
  char buf[32];
  if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
    char val[8];
    if (httpd_query_key_value(buf, "val", val, sizeof(val)) == ESP_OK) {
      if (val[0] == '1') {
        Serial.println("H");
      } else {
        Serial.println("h");
      }
      Serial.flush();
    }
  }
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, "OK", 2);
}

esp_err_t speed_handler(httpd_req_t *req) {
  char buf[48];
  if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
    char s[8];
    if (httpd_query_key_value(buf, "s", s, sizeof(s)) == ESP_OK ||
        httpd_query_key_value(buf, "fs", s, sizeof(s)) == ESP_OK) {
      int val = atoi(s);
      if (val >= 0 && val <= 255) {
        currentSpeed = val;
        Serial.print("FSP:");
        Serial.println(val);
        Serial.print("BSP:");
        Serial.println(val);
        Serial.flush();
      }
    }
  }
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, "OK", 2);
}

esp_err_t flash_handler(httpd_req_t *req) {
  char buf[32];
  if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
    char val[8];
    if (httpd_query_key_value(buf, "val", val, sizeof(val)) == ESP_OK) {
      flashState = (val[0] == '1');
      digitalWrite(FLASH_LED_PIN, flashState ? HIGH : LOW);
    }
  }
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, "OK", 2);
}

esp_err_t status_handler(httpd_req_t *req) {
  char json[220];
  snprintf(json, sizeof(json),
    "{\"mode\":\"%s\",\"ip\":\"%s\",\"ssid\":\"%s\",\"cam\":%s,\"flash\":%s,\"cmd\":\"%c\",\"speed\":%d}",
    isApMode ? "AP" : "STA",
    isApMode ? WiFi.softAPIP().toString().c_str() : WiFi.localIP().toString().c_str(),
    isApMode ? "FPV-Car" : connectedSsid.c_str(),
    cameraReady ? "true" : "false",
    flashState ? "true" : "false",
    currentCmd,
    currentSpeed);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json, strlen(json));
}

esp_err_t reprobe_handler(httpd_req_t *req) {
  startCameraDriver();
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, cameraReady ? "OK" : "FAIL", HTTPD_RESP_USE_STRLEN);
}

// Scan nearby Wi-Fi networks
esp_err_t scan_handler(httpd_req_t *req) {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  json += "]";
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json.c_str(), HTTPD_RESP_USE_STRLEN);
}

// Connect to Wi-Fi and switch to Server Mode
esp_err_t save_wifi_handler(httpd_req_t *req) {
  char buf[160];
  String ssid = "", pass = "";
  if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
    char s[64], p[64];
    if (httpd_query_key_value(buf, "ssid", s, sizeof(s)) == ESP_OK) ssid = urldecode(String(s));
    if (httpd_query_key_value(buf, "pass", p, sizeof(p)) == ESP_OK) pass = urldecode(String(p));
  }

  if (ssid.length() > 0) {
    prefs.begin("fpv_cfg", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();

    // Enable Station Mode alongside AP while connecting
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
      isApMode = false;
      connectedSsid = ssid;
      String localIp = WiFi.localIP().toString();

      Serial.println("Z:WIFI_OK");

      // Auto-register IP with Azure Cloud VM
      HTTPClient http;
      http.begin("http://172.198.73.188/set.php?car_ip=" + localIp + "&esp_hb=1");
      http.setTimeout(1800);
      http.GET();
      http.end();

      String res = "{\"status\":\"ok\",\"ip\":\"" + localIp + "\",\"cloud\":\"http://172.198.73.188/\"}";
      httpd_resp_set_type(req, "application/json");
      httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
      return httpd_resp_send(req, res.c_str(), HTTPD_RESP_USE_STRLEN);
    }
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, "{\"status\":\"fail\"}", HTTPD_RESP_USE_STRLEN);
}

// Switch between AP and STA mode
esp_err_t switch_mode_handler(httpd_req_t *req) {
  char buf[32];
  if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
    char m[8];
    if (httpd_query_key_value(buf, "mode", m, sizeof(m)) == ESP_OK) {
      if (String(m) == "ap") {
        WiFi.disconnect();
        WiFi.mode(WIFI_AP);
        WiFi.softAP("FPV-Car", "");
        dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
        isApMode = true;
        Serial.println("Z:AP");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        return httpd_resp_send(req, "{\"status\":\"ap\",\"ip\":\"192.168.4.1\"}", HTTPD_RESP_USE_STRLEN);
      }
    }
  }
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, "{\"status\":\"error\"}", HTTPD_RESP_USE_STRLEN);
}

// Captive Portal redirect
esp_err_t redirect_handler(httpd_req_t *req) {
  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
  return httpd_resp_send(req, NULL, 0);
}

// ==========================================
// Start Web Server
// ==========================================
void startWebServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 32768;
  config.max_open_sockets = 7;
  config.lru_purge_enable = true;

  httpd_uri_t uri_index = { .uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL };
  httpd_uri_t uri_stream = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };
  httpd_uri_t uri_cmd = { .uri = "/cmd", .method = HTTP_GET, .handler = cmd_handler, .user_ctx = NULL };
  httpd_uri_t uri_horn = { .uri = "/horn", .method = HTTP_GET, .handler = horn_handler, .user_ctx = NULL };
  httpd_uri_t uri_speed = { .uri = "/speed", .method = HTTP_GET, .handler = speed_handler, .user_ctx = NULL };
  httpd_uri_t uri_flash = { .uri = "/flash", .method = HTTP_GET, .handler = flash_handler, .user_ctx = NULL };
  httpd_uri_t uri_status = { .uri = "/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = NULL };
  httpd_uri_t uri_reprobe = { .uri = "/reprobe", .method = HTTP_GET, .handler = reprobe_handler, .user_ctx = NULL };
  httpd_uri_t uri_scan = { .uri = "/scan", .method = HTTP_GET, .handler = scan_handler, .user_ctx = NULL };
  httpd_uri_t uri_save_wifi = { .uri = "/save-wifi", .method = HTTP_GET, .handler = save_wifi_handler, .user_ctx = NULL };
  httpd_uri_t uri_switch_mode = { .uri = "/switch-mode", .method = HTTP_GET, .handler = switch_mode_handler, .user_ctx = NULL };
  httpd_uri_t uri_cp1 = { .uri = "/generate_204", .method = HTTP_GET, .handler = redirect_handler, .user_ctx = NULL };
  httpd_uri_t uri_cp2 = { .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = redirect_handler, .user_ctx = NULL };

  if (httpd_start(&serverHttpd, &config) == ESP_OK) {
    httpd_register_uri_handler(serverHttpd, &uri_index);
    httpd_register_uri_handler(serverHttpd, &uri_stream);
    httpd_register_uri_handler(serverHttpd, &uri_cmd);
    httpd_register_uri_handler(serverHttpd, &uri_horn);
    httpd_register_uri_handler(serverHttpd, &uri_speed);
    httpd_register_uri_handler(serverHttpd, &uri_flash);
    httpd_register_uri_handler(serverHttpd, &uri_status);
    httpd_register_uri_handler(serverHttpd, &uri_reprobe);
    httpd_register_uri_handler(serverHttpd, &uri_scan);
    httpd_register_uri_handler(serverHttpd, &uri_save_wifi);
    httpd_register_uri_handler(serverHttpd, &uri_switch_mode);
    httpd_register_uri_handler(serverHttpd, &uri_cp1);
    httpd_register_uri_handler(serverHttpd, &uri_cp2);
    Serial.println("[HTTP] Server started on port 80");
  }
}

// ==========================================
// Setup
// ==========================================
void setup() {
  Serial.begin(115200);
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  delay(600);
  startCameraDriver();

  // Boot Melody & Motor Kick on Arduino
  Serial.println();
  Serial.println("Z:BOOT");
  Serial.flush();

  // ALWAYS BOOT IN LOCAL (AP) MODE BY DEFAULT!
  isApMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("FPV-Car", ""); // Open network for 1-tap connection
  WiFi.setSleep(false);

  // Captive Portal DNS on Port 53
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // Tell Arduino that AP Hotspot is ready
  delay(100);
  Serial.println("Z:AP");
  Serial.flush();

  // Start Asynchronous Web Server
  startWebServer();

  Serial.println("=========================================");
  Serial.println("🏎️ FPV CAR READY IN LOCAL MODE (AP)!");
  Serial.println("Wi-Fi SSID : FPV-Car");
  Serial.println("Browser URL: http://192.168.4.1");
  Serial.println("=========================================");
}

// ==========================================
// Main Loop
// ==========================================
void loop() {
  if (isApMode) {
    dnsServer.processNextRequest();
  }
  delay(2);
}
