/*************************************************
 * ESP32-CAM Server Mode (Cloud PHP Server Polling)
 * 
 * Flow:
 * 1. ESP32 connects to WiFi.
 * 2. ESP32 polls Azure PHP Server (get.php) every 30ms to get latest command ('F','B','L','R','S').
 * 3. ESP32 sends command over Serial (GPIO 1 TX) to Arduino UNO (RX Pin 0).
 * 4. Arduino UNO drives motors accordingly.
 * 5. ESP32 uploads live camera frames to Azure PHP Server (upload.php) every ~110ms.
 *************************************************/

#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

// ======================================================
// WiFi Credentials (Apna WiFi SSID aur Password yahan daalein)
// ======================================================
const char* ssid     = "sumit";
const char* password = "12345678";

// ======================================================
// Azure PHP Server Endpoints
// ======================================================
const char* controlUrl = "http://20.244.113.234/fpv_car/get.php";
const char* uploadUrl  = "http://20.244.113.234/fpv_car/cam/upload.php";


// Timing & State variables
const uint32_t CONTROL_INTERVAL_MS = 30;  // 30ms polling for ultra-low latency steering
const uint32_t UPLOAD_INTERVAL_MS  = 110; // ~9 FPS video stream upload
uint32_t lastControlAt = 0;
uint32_t lastUploadAt  = 0;

char lastCmd = 'S';
bool cameraPowerOn = true;
bool cameraReady   = false;

// ======================================================
// ESP32-CAM AI-Thinker Pin Definitions
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

// ======================================================
// Helper: Parse Command from JSON Response
// ======================================================
char parseJsonCommand(const String& payload) {
  int keyPos = payload.indexOf("\"cmd\":\"");
  if (keyPos == -1) return 'S';

  int valuePos = keyPos + 7;
  if (valuePos >= payload.length()) return 'S';

  char cmd = payload.charAt(valuePos);
  if (cmd == 'F' || cmd == 'B' || cmd == 'L' || cmd == 'R' || cmd == 'S') {
    return cmd;
  }
  return 'S';
}

// ======================================================
// Initialize ESP32 Camera
// ======================================================
void initCamera() {
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

  config.frame_size   = FRAMESIZE_QVGA; // 320x240 for fast network transfer
  config.jpeg_quality = 14;            // Good balance of quality and size
  config.fb_count     = 1;

  if (esp_camera_init(&config) == ESP_OK) {
    cameraReady = true;
  } else {
    cameraReady = false;
  }
}

// ======================================================
// Setup
// ======================================================
void setup() {
  // Serial Baud Rate must match Arduino UNO (115200)
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.setSleep(false); // Disables WiFi power save for ultra-low latency
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }

  // Initialize Camera
  initCamera();
}

// ======================================================
// Loop
// ======================================================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    delay(50);
    return;
  }

  uint32_t now = millis();

  // ----------------------------------------------------
  // 1. Poll Server for Motor Commands (Every 30ms)
  // ----------------------------------------------------
  if (now - lastControlAt >= CONTROL_INTERVAL_MS) {
    lastControlAt = now;

    HTTPClient http;
    http.begin(controlUrl);
    http.setTimeout(90); // Short timeout for low latency

    int httpCode = http.GET();
    if (httpCode == 200) {
      String response = http.getString();
      response.trim();

      char cmd = parseJsonCommand(response);

      // Send command character to Arduino UNO over Serial
      Serial.write(cmd);
      lastCmd = cmd;
    }
    http.end();
  }

  // ----------------------------------------------------
  // 2. Upload Camera Frame to Server (Every 110ms)
  // ----------------------------------------------------
  if (cameraReady && (now - lastUploadAt >= UPLOAD_INTERVAL_MS)) {
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

  delay(5);
}
