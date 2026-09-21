/*************************************************
 * FPV Car - Advanced ESP32-CAM Dual-Mode Firmware
 * 
 * MODES:
 * 1. LOCAL DIRECT (LAN / AP Hotspot Mode):
 *    - Works with ZERO internet connection.
 *    - Hotspot: "FPV-Car-Setup" (IP 192.168.4.1) or Local Home Wi-Fi Router.
 *    - Native Real-Time MJPEG Video Stream at http://<IP>/stream (15-25 FPS, <50ms latency).
 *    - Direct instant REST controls: /cmd?dir=F|B|L|R|S, /speed, /flash, /cam.
 * 
 * 2. CLOUD MODE (InfinityFree Hosting):
 *    - Connects to Wi-Fi and synchronizes with InfinityFree PHP backend:
 *      http://sumitrathor.rf.gd/FPV_Car/
 *    - Polls get.php for remote commands and uploads JPEG frames to cam/upload.php.
 * 
 * Hardware:
 * - ESP32-CAM AI-Thinker
 * - Flash LED Light: GPIO 4
 * - Camera PWDN: GPIO 32 (Hardware sensor power down)
 * - Serial to Arduino UNO at 115200 baud
 * - Preferences (NVS) for persistent Wi-Fi credentials
 *************************************************/

#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>

// ======================================================
// WebServer & Preferences Storage
// ======================================================
Preferences prefs;
WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

String saved_ssid     = "";
String saved_password = "";
bool isApMode         = false;
bool localClientStreaming = false;

// ======================================================
// InfinityFree Cloud Server Endpoints
// ======================================================
const char* controlUrl   = "http://sumitrathor.rf.gd/FPV_Car/get.php";
const char* uploadUrl    = "http://sumitrathor.rf.gd/FPV_Car/cam/upload.php";
const char* heartbeatUrl = "http://sumitrathor.rf.gd/FPV_Car/set.php?esp_hb=1";

const uint32_t CLOUD_CONTROL_INTERVAL_MS   = 60;   // 60ms cloud command polling
const uint32_t CLOUD_UPLOAD_INTERVAL_MS    = 150;  // Cloud frame upload interval
const uint32_t CLOUD_HEARTBEAT_INTERVAL_MS = 2500; // 2.5s heartbeat sync

uint32_t lastControlAt   = 0;
uint32_t lastUploadAt    = 0;
uint32_t lastHeartbeatAt = 0;

char lastCmd          = 'S';
bool cameraPowerOn    = false;
bool cameraReady      = false;
bool flashState       = false;
int lastForwardSpeed  = 255;
int lastBackwardSpeed = 255;

// ======================================================
// ESP32-CAM AI-Thinker Camera Pin Definitions
// ======================================================
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

// ======================================================
// Camera Hardware Power Management
// ======================================================
void startCamera() {
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Frame size: QVGA (320x240) gives 20+ FPS high frame rate & low latency
  config.frame_size   = FRAMESIZE_QVGA;
  config.jpeg_quality = 12; // 10-14 is crisp with fast transmission
  config.fb_count     = 2;  // Double buffering for smooth frames

  if (esp_camera_init(&config) == ESP_OK) {
    cameraReady = true;
  } else {
    cameraReady = false;
  }
}

void setCameraHardwarePower(bool on) {
  if (on) {
    if (!cameraReady) startCamera();
    if (cameraReady) {
      pinMode(PWDN_GPIO_NUM, OUTPUT);
      digitalWrite(PWDN_GPIO_NUM, LOW); // LOW powers ON camera chip
      cameraPowerOn = true;
    }
    return;
  }

  // De-init and cut power to sensor to eliminate heat and battery drain
  cameraPowerOn = false;
  if (cameraReady) {
    esp_camera_deinit();
    cameraReady = false;
  }
  pinMode(PWDN_GPIO_NUM, OUTPUT);
  digitalWrite(PWDN_GPIO_NUM, HIGH); // HIGH powers down camera sensor chip
}

// ======================================================
// CORS Helper
// ======================================================
void handleCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ======================================================
// Native MJPEG High-Speed Stream Server (/stream)
// Delivers 15-25 FPS live video feed to browser
// ======================================================
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

void handleStream() {
  handleCORS();

  if (!cameraPowerOn || !cameraReady) {
    server.send(503, "text/plain", "Camera is Powered OFF");
    return;
  }

  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: " + String(_STREAM_CONTENT_TYPE) + "\r\n";
  response += "Access-Control-Allow-Origin: *\r\n";
  response += "Connection: close\r\n\r\n";
  server.sendContent(response);

  localClientStreaming = true;

  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      delay(10);
      continue;
    }

    char part_buf[128];
    size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, fb->len);
    client.write(_STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    client.write(part_buf, hlen);
    client.write((const char*)fb->buf, fb->len);
    client.write("\r\n", 2);

    esp_camera_fb_return(fb);
    delay(2); // Yield to ESP32 Wi-Fi stack
  }

  localClientStreaming = false;
}

// ======================================================
// Direct Local REST Control Endpoints
// ======================================================
void handleCmd() {
  handleCORS();
  String dir = server.arg("dir");
  if (dir.length() > 0) {
    char c = dir.charAt(0);
    if (c == 'F' || c == 'B' || c == 'L' || c == 'R' || c == 'S') {
      Serial.println(c);
      lastCmd = c;
      server.send(200, "application/json", "{\"status\":\"ok\",\"cmd\":\"" + String(c) + "\"}");
      return;
    }
  }
  server.send(400, "application/json", "{\"error\":\"invalid command\"}");
}

void handleSpeed() {
  handleCORS();
  if (server.hasArg("fs")) {
    int fs = server.arg("fs").toInt();
    fs = max(0, min(255, fs));
    Serial.print("FSP:");
    Serial.println(fs);
    lastForwardSpeed = fs;
  }
  if (server.hasArg("bs")) {
    int bs = server.arg("bs").toInt();
    bs = max(0, min(255, bs));
    Serial.print("BSP:");
    Serial.println(bs);
    lastBackwardSpeed = bs;
  }
  server.send(200, "application/json", "{\"status\":\"ok\",\"fs\":" + String(lastForwardSpeed) + ",\"bs\":" + String(lastBackwardSpeed) + "}");
}

void handleFlash() {
  handleCORS();
  String val = server.arg("val");
  flashState = (val == "1");
  digitalWrite(FLASH_LED_PIN, flashState ? HIGH : LOW);
  server.send(200, "application/json", "{\"flash\":" + String(flashState ? 1 : 0) + "}");
}

void handleCamPower() {
  handleCORS();
  String val = server.arg("power");
  bool on = (val == "1");
  setCameraHardwarePower(on);
  server.send(200, "application/json", "{\"cam\":" + String(cameraPowerOn ? 1 : 0) + "}");
}

void handleStatus() {
  handleCORS();
  String json = "{";
  json += "\"mode\":\"" + String(isApMode ? "AP" : "STA") + "\",";
  json += "\"ip\":\"" + (isApMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "\",";
  json += "\"cam\":" + String(cameraPowerOn ? 1 : 0) + ",";
  json += "\"flash\":" + String(flashState ? 1 : 0) + ",";
  json += "\"lastCmd\":\"" + String(lastCmd) + "\",";
  json += "\"fs\":" + String(lastForwardSpeed) + ",";
  json += "\"bs\":" + String(lastBackwardSpeed) + ",";
  json += "\"rssi\":" + String(isApMode ? 0 : WiFi.RSSI());
  json += "}";
  server.send(200, "application/json", json);
}

void handleScanWifi() {
  handleCORS();
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleSaveWifi() {
  handleCORS();
  String newSsid = server.arg("ssid");
  String newPass = server.arg("password");

  if (newSsid.length() > 0) {
    prefs.begin("fpv_wifi", false);
    prefs.putString("ssid", newSsid);
    prefs.putString("pass", newPass);
    prefs.end();

    server.send(200, "application/json", "{\"status\":\"saved\",\"message\":\"Restarting and connecting...\"}");
    delay(1000);
    ESP.restart();
  } else {
    server.send(400, "application/json", "{\"error\":\"SSID cannot be empty\"}");
  }
}

void setupLocalServer() {
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/cmd", HTTP_GET, handleCmd);
  server.on("/speed", HTTP_GET, handleSpeed);
  server.on("/flash", HTTP_GET, handleFlash);
  server.on("/cam", HTTP_GET, handleCamPower);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/scan", HTTP_GET, handleScanWifi);
  server.on("/save-wifi", HTTP_GET, handleSaveWifi);
  server.on("/FPV_Car", HTTP_GET, []() {
    handleCORS();
    server.send(200, "text/plain", "FPV Car Local Gateway Ready");
  });
  server.on("/FPV_Car/", HTTP_GET, []() {
    handleCORS();
    server.send(200, "text/plain", "FPV Car Local Gateway Ready");
  });
  server.onNotFound([]() {
    handleCORS();
    server.send(200, "text/plain", "FPV Car Ready");
  });
  server.begin();
}

void startApMode() {
  isApMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("FPV-Car-Setup", ""); // Open setup & direct drive hotspot
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP()); // Captive portal DNS redirects any host (e.g. sumitrathor.rf.gd) to car
  setupLocalServer();
  Serial.println("[WIFI] AP Hotspot Started: FPV-Car-Setup (IP: 192.168.4.1)");
}

// ======================================================
// JSON Parser Helpers for Cloud Responses
// ======================================================
char parseJsonCommand(const String& payload) {
  int keyPos = payload.indexOf("\"cmd\":\"");
  if (keyPos == -1) return 'S';
  int valuePos = keyPos + 7;
  if (valuePos >= payload.length()) return 'S';
  char cmd = payload.charAt(valuePos);
  if (cmd == 'F' || cmd == 'B' || cmd == 'L' || cmd == 'R' || cmd == 'S') return cmd;
  return 'S';
}

bool parseJsonCamPower(const String& payload) {
  int keyPos = payload.indexOf("\"cam\":\"");
  if (keyPos == -1) return true;
  int valuePos = keyPos + 7;
  if (valuePos >= payload.length()) return true;
  return payload.charAt(valuePos) == '1';
}

bool parseJsonFlashLight(const String& payload) {
  int keyPos = payload.indexOf("\"flash\":\"");
  if (keyPos == -1) return false;
  int valuePos = keyPos + 9;
  if (valuePos >= payload.length()) return false;
  return payload.charAt(valuePos) == '1';
}

int parseJsonSpeed(const String& payload, const char* key, int fallback) {
  String token = String("\"") + key + "\":\"";
  int keyPos = payload.indexOf(token);
  if (keyPos == -1) return fallback;
  int valueStart = keyPos + token.length();
  int valueEnd = payload.indexOf('"', valueStart);
  if (valueEnd == -1) return fallback;
  int parsed = payload.substring(valueStart, valueEnd).toInt();
  return max(0, min(255, parsed));
}

// ======================================================
// Setup
// ======================================================
void setup() {
  // Serial Baud rate must match Arduino UNO (115200)
  Serial.begin(115200);

  // Flashlight GPIO 4 Setup
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  // Load Saved WiFi from Flash (NVS)
  prefs.begin("fpv_wifi", true);
  saved_ssid     = prefs.getString("ssid", "");
  saved_password = prefs.getString("pass", "");
  prefs.end();

  WiFi.setSleep(false); // Disables Wi-Fi power save for ultra-low latency

  bool connected = false;
  if (saved_ssid.length() > 0) {
    Serial.print("[WIFI] Connecting to saved WiFi: ");
    Serial.println(saved_ssid);
    WiFi.begin(saved_ssid.c_str(), saved_password.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 25) {
      delay(300);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      Serial.println("\n[WIFI] Connected! IP: " + WiFi.localIP().toString());
    }
  }

  if (!connected) {
    Serial.println("\n[WIFI] Starting AP Hotspot (FPV-Car-Setup)...");
    startApMode();
  } else {
    setupLocalServer();
  }

  // Initialize camera
  startCamera();
  setCameraHardwarePower(true);
}

// ======================================================
// Main Loop
// ======================================================
void loop() {
  if (isApMode) {
    dnsServer.processNextRequest();
  }

  // Handle local HTTP and streaming requests
  server.handleClient();

  // If in AP Mode or local client is actively streaming /stream, yield priority to local
  if (isApMode || WiFi.status() != WL_CONNECTED || localClientStreaming) {
    delay(2);
    return;
  }

  uint32_t now = millis();

  // 1. Cloud Heartbeat Sync (Every 2.5 seconds)
  if (now - lastHeartbeatAt >= CLOUD_HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatAt = now;
    HTTPClient http;
    http.begin(heartbeatUrl);
    http.setTimeout(180);
    http.GET();
    http.end();
  }

  // 2. Cloud Command Polling (Every 60ms)
  if (now - lastControlAt >= CLOUD_CONTROL_INTERVAL_MS) {
    lastControlAt = now;

    HTTPClient http;
    http.begin(controlUrl);
    http.setTimeout(120);

    int code = http.GET();
    if (code == 200) {
      String res = http.getString();
      res.trim();

      char cmd          = parseJsonCommand(res);
      bool shouldCam    = parseJsonCamPower(res);
      bool shouldFlash  = parseJsonFlashLight(res);
      int desiredFs     = parseJsonSpeed(res, "fs", lastForwardSpeed);
      int desiredBs     = parseJsonSpeed(res, "bs", lastBackwardSpeed);

      // Camera Power Toggle
      if (shouldCam != cameraPowerOn) {
        setCameraHardwarePower(shouldCam);
      }

      // Flashlight Toggle
      if (shouldFlash != flashState) {
        flashState = shouldFlash;
        digitalWrite(FLASH_LED_PIN, flashState ? HIGH : LOW);
      }

      // Motor Speeds Sync
      if (desiredFs != lastForwardSpeed) {
        Serial.print("FSP:");
        Serial.println(desiredFs);
        lastForwardSpeed = desiredFs;
      }
      if (desiredBs != lastBackwardSpeed) {
        Serial.print("BSP:");
        Serial.println(desiredBs);
        lastBackwardSpeed = desiredBs;
      }

      // Motor Direction Command to Arduino UNO
      if (cmd != lastCmd || cmd != 'S') {
        Serial.println(cmd);
        lastCmd = cmd;
      }
    } else {
      // Cloud disconnect fail-safe: Auto-stop car
      if (lastCmd != 'S') {
        Serial.println('S');
        lastCmd = 'S';
      }
    }
    http.end();
  }

  // 3. Cloud Frame Upload (Every 150ms if Camera is ON)
  if (cameraPowerOn && cameraReady && (now - lastUploadAt >= CLOUD_UPLOAD_INTERVAL_MS)) {
    lastUploadAt = now;

    camera_fb_t* fb = esp_camera_fb_get();
    if (fb != nullptr) {
      HTTPClient http;
      http.begin(uploadUrl);
      http.setTimeout(160);
      http.addHeader("Content-Type", "application/octet-stream");
      http.POST(fb->buf, fb->len);
      http.end();

      esp_camera_fb_return(fb);
    }
  }

  delay(2);
}
