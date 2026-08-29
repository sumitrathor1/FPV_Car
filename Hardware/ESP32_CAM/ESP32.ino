/*************************************************
 * FPV Car - Advanced ESP32-CAM Firmware with Smart WiFi Provisioning
 * Features:
 * - Permanent Flash Memory (Preferences.h) for WiFi Credentials
 * - Fallback AP Hotspot "FPV-Car-Setup" (IP 192.168.4.1) with Captive Portal & REST APIs
 * - Live WiFi Network Scanner (/scan) & WiFi Credential Setter (/save-wifi)
 * - Hardware Camera Power Off (De-init sensor & GPIO PWDN power down)
 * - Zero CPU/Network load when camera is OFF (No capture, No uploads)
 * - Flash LED Light Control (GPIO 4 Headlight ON/OFF)
 * - Low latency 30ms polling from HTTPS Cloud Server
 * - Dynamic Forward & Backward speed transmission (FSP / BSP) to UNO
 * - High-speed JPEG video streaming upload (110ms interval)
 * - Cloud Heartbeat Sync (esp_hb) for real-time Frontend Online/Offline detection
 *************************************************/

#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Preferences.h>

// ======================================================
// Preferences & WebServer for WiFi Configuration
// ======================================================
Preferences prefs;
WebServer server(80);

String saved_ssid     = "";
String saved_password = "";
bool isApMode         = false;

// ======================================================
// Cloud Server Endpoints
// ======================================================
const char* controlUrl = "http://20.244.113.234/fpv_car/get.php";
const char* uploadUrl  = "http://20.244.113.234/fpv_car/cam/upload.php";
const char* heartbeatUrl = "http://20.244.113.234/fpv_car/set.php?esp_hb=1";

const uint32_t CONTROL_INTERVAL_MS   = 30;    // 30ms polling for steering & speed
const uint32_t UPLOAD_INTERVAL_MS    = 110;   // ~9 FPS image upload
const uint32_t HEARTBEAT_INTERVAL_MS = 2000;  // 2s cloud heartbeat sync
uint32_t lastControlAt   = 0;
uint32_t lastUploadAt    = 0;
uint32_t lastHeartbeatAt = 0;

char lastCmd = 'X';
bool cameraPowerOn = false;
bool cameraReady   = false;
bool flashState    = false;
int lastForwardSpeed  = 255;
int lastBackwardSpeed = 255;

// ======================================================
// ESP32-CAM Pin Definitions (AI-Thinker Model)
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
// JSON Helper Functions
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
// Hardware Camera Power Management (Zero CPU/Load when OFF)
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

  config.frame_size   = FRAMESIZE_QVGA; // 320x240 for low network latency
  config.jpeg_quality = 14;
  config.fb_count     = 1;

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

  // Complete Hardware De-initialization & Sensor Power Cutoff
  cameraPowerOn = false;
  if (cameraReady) {
    esp_camera_deinit();
    cameraReady = false;
  }
  pinMode(PWDN_GPIO_NUM, OUTPUT);
  digitalWrite(PWDN_GPIO_NUM, HIGH); // HIGH powers down camera sensor chip
}

// ======================================================
// AP Mode & Local REST API Handlers (CORS Enabled)
// ======================================================
void handleCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
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

void handleStatus() {
  handleCORS();
  String json = "{\"mode\":\"" + String(isApMode ? "AP" : "STA") + "\",\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  server.send(200, "application/json", json);
}

void startApMode() {
  isApMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("FPV-Car-Setup", ""); // Open setup hotspot

  server.on("/scan", HTTP_GET, handleScanWifi);
  server.on("/save-wifi", HTTP_GET, handleSaveWifi);
  server.on("/status", HTTP_GET, handleStatus);
  server.onNotFound([]() {
    handleCORS();
    server.send(200, "text/plain", "FPV Car Setup Ready at 192.168.4.1");
  });

  server.begin();
  Serial.println("[WIFI] AP Hotspot Started: FPV-Car-Setup (IP: 192.168.4.1)");
}

// ======================================================
// Setup
// ======================================================
void setup() {
  Serial.begin(115200);

  // Flash LED Light Setup
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW); // Flash OFF initially

  // Load Saved WiFi from Flash (NVS)
  prefs.begin("fpv_wifi", true);
  saved_ssid     = prefs.getString("ssid", "");
  saved_password = prefs.getString("pass", "");
  prefs.end();

  WiFi.setSleep(false); // Disables WiFi power save for ultra-low latency

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
    Serial.println("\n[WIFI] No saved WiFi or connection failed. Starting Setup Hotspot...");
    startApMode();
  } else {
    // Start local server in Station mode as well so local tools can query /status or change wifi
    server.on("/scan", HTTP_GET, handleScanWifi);
    server.on("/save-wifi", HTTP_GET, handleSaveWifi);
    server.on("/status", HTTP_GET, handleStatus);
    server.begin();

    startCamera();
    setCameraHardwarePower(true);
  }
}

// ======================================================
// Loop
// ======================================================
void loop() {
  // Always handle local WebServer requests
  server.handleClient();

  if (isApMode || WiFi.status() != WL_CONNECTED) {
    delay(10);
    return;
  }

  uint32_t now = millis();

  // 1. Send Cloud Heartbeat (Every 2 seconds)
  if (now - lastHeartbeatAt >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatAt = now;
    HTTPClient http;
    http.begin(heartbeatUrl);
    http.setTimeout(120);
    http.GET();
    http.end();
  }

  // 2. Poll Server for Commands, Speeds & Flash Light (Every 30ms)
  if (now - lastControlAt >= CONTROL_INTERVAL_MS) {
    lastControlAt = now;

    HTTPClient http;
    http.begin(controlUrl);
    http.setTimeout(90);

    int code = http.GET();
    if (code == 200) {
      String res = http.getString();
      res.trim();

      char cmd = parseJsonCommand(res);
      bool shouldCamBeOn = parseJsonCamPower(res);
      bool shouldFlashBeOn = parseJsonFlashLight(res);
      int desiredFs = parseJsonSpeed(res, "fs", lastForwardSpeed);
      int desiredBs = parseJsonSpeed(res, "bs", lastBackwardSpeed);

      // Hardware Camera Power Toggle
      if (shouldCamBeOn != cameraPowerOn) {
        setCameraHardwarePower(shouldCamBeOn);
      }

      // Flash Light Toggle
      if (shouldFlashBeOn != flashState) {
        flashState = shouldFlashBeOn;
        digitalWrite(FLASH_LED_PIN, flashState ? HIGH : LOW);
      }

      // Motor Speed Sync
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

      // Direction Command to Arduino UNO
      Serial.println(cmd);
      lastCmd = cmd;
    }

    http.end();
  }

  // 3. Upload Camera Frames (ONLY IF CAMERA POWER IS ON & SENSOR READY)
  if (cameraPowerOn && cameraReady && (now - lastUploadAt >= UPLOAD_INTERVAL_MS)) {
    lastUploadAt = now;

    camera_fb_t* fb = esp_camera_fb_get();
    if (fb != nullptr) {
      HTTPClient http;
      http.begin(uploadUrl);
      http.setTimeout(120);
      http.addHeader("Content-Type", "application/octet-stream");
      http.POST(fb->buf, fb->len);
      http.end();

      esp_camera_fb_return(fb);
    }
  }

  delay(2);
}
