#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

// ================= WIFI =================
const char* ssid = "sumit";
const char* password = "12345678";

// ================= SERVER =================
const char* controlUrl = "http://20.244.113.234/robot/get.php";
const char* uploadUrl  = "http://20.244.113.234/robot/cam/upload.php";

char lastCmd = 'X';
bool cameraPowerOn = false;
bool cameraReady = false;

const uint32_t CONTROL_INTERVAL_MS = 35;
const uint32_t UPLOAD_INTERVAL_MS = 90;
uint32_t lastControlAt = 0;
uint32_t lastUploadAt = 0;

// ================= CAMERA PINS =================
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

// ================= CAMERA INIT =================
void startCamera() {
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // 🔥 SPEED OPTIMIZATION
  config.frame_size = FRAMESIZE_QQVGA;   // faster
  config.jpeg_quality = 20;              // smaller size
  config.fb_count = 1;

  if (esp_camera_init(&config) == ESP_OK) {
    cameraReady = true;
  } else {
    cameraReady = false;
  }
}

void setCameraHardwarePower(bool on) {
  if (on) {
    if (!cameraReady) {
      startCamera();
    }

    if (cameraReady) {
      pinMode(PWDN_GPIO_NUM, OUTPUT);
      digitalWrite(PWDN_GPIO_NUM, LOW);
      cameraPowerOn = true;
    }
    return;
  }

  cameraPowerOn = false;

  if (cameraReady) {
    esp_camera_deinit();
    cameraReady = false;
  }

  pinMode(PWDN_GPIO_NUM, OUTPUT);
  digitalWrite(PWDN_GPIO_NUM, HIGH);
}

char readJsonCommand(const String& payload) {
  int keyPos = payload.indexOf("\"cmd\":\"");
  if (keyPos == -1) {
    return 'S';
  }

  int valuePos = keyPos + 7;
  if (valuePos >= payload.length()) {
    return 'S';
  }

  char cmd = payload.charAt(valuePos);
  if (cmd == 'F' || cmd == 'B' || cmd == 'L' || cmd == 'R' || cmd == 'S') {
    return cmd;
  }

  return 'S';
}

bool readJsonCameraPower(const String& payload) {
  int keyPos = payload.indexOf("\"cam\":\"");
  if (keyPos == -1) {
    return true;
  }

  int valuePos = keyPos + 7;
  if (valuePos >= payload.length()) {
    return true;
  }

  return payload.charAt(valuePos) == '1';
}

// ================= SETUP =================
void setup() {
  Serial.begin(9600);

  Serial.println("START");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nCONNECTED");

  startCamera();
  setCameraHardwarePower(true);
}

// ================= LOOP =================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    delay(50);
    return;
  }

  uint32_t now = millis();

  // Poll control more often than camera upload to reduce steering latency.
  if (now - lastControlAt >= CONTROL_INTERVAL_MS) {
    lastControlAt = now;

    HTTPClient http;
    http.begin(controlUrl);
    http.setTimeout(120);

    int code = http.GET();
    if (code == 200) {
      String res = http.getString();
      res.trim();

      char cmd = readJsonCommand(res);
      bool shouldCamBeOn = readJsonCameraPower(res);

      if (shouldCamBeOn != cameraPowerOn) {
        setCameraHardwarePower(shouldCamBeOn);
      }

      if (cmd != lastCmd) {
        Serial.println(cmd);
        lastCmd = cmd;
      }
    }

    http.end();
  }

  if (cameraPowerOn && cameraReady && (now - lastUploadAt >= UPLOAD_INTERVAL_MS)) {
    lastUploadAt = now;

    camera_fb_t* fb = esp_camera_fb_get();
    if (fb != nullptr) {
      HTTPClient http;
      http.begin(uploadUrl);
      http.setTimeout(200);
      http.addHeader("Content-Type", "application/octet-stream");
      http.POST(fb->buf, fb->len);
      http.end();

      esp_camera_fb_return(fb);
    }
  }

  delay(5);
}