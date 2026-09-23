#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

Preferences prefs;
WebServer server(80);
WiFiServer streamServer(81);
DNSServer dnsServer;
const byte DNS_PORT = 53;

String saved_ssid = "";
String saved_password = "";
bool isApMode = false;
bool localClientStreaming = false;
bool clientConnectedNoticeSent = false;
uint32_t lastClientPingAt = 0;

const char* controlUrl = "http://sumitrathor.rf.gd/FPV_Car/get.php";
const char* uploadUrl = "http://sumitrathor.rf.gd/FPV_Car/cam/upload.php";
const char* heartbeatUrl = "http://sumitrathor.rf.gd/FPV_Car/set.php?esp_hb=1";

const uint32_t CLOUD_CONTROL_INTERVAL_MS = 250;
const uint32_t CLOUD_UPLOAD_INTERVAL_MS = 350;
const uint32_t CLOUD_HEARTBEAT_INTERVAL_MS = 3000;

uint32_t lastControlAt = 0;
uint32_t lastUploadAt = 0;
uint32_t lastHeartbeatAt = 0;

char lastCmd = 'S';
bool cameraPowerOn = false;
bool cameraReady = false;
bool flashState = false;
int lastForwardSpeed = 255;
int lastBackwardSpeed = 255;

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

void flashBlink(int count, int durationMs = 80) {
  for (int i = 0; i < count; i++) {
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(durationMs);
    digitalWrite(FLASH_LED_PIN, LOW);
    if (i < count - 1) delay(durationMs);
  }
}

bool startCamera() {
  if (cameraReady) return true;
  pinMode(PWDN_GPIO_NUM, OUTPUT);
  digitalWrite(PWDN_GPIO_NUM, HIGH);
  delay(50);
  digitalWrite(PWDN_GPIO_NUM, LOW);
  delay(150);

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

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count     = 2;
    config.grab_mode    = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size   = FRAMESIZE_HVGA;
    config.jpeg_quality = 12;
    config.fb_count     = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    digitalWrite(PWDN_GPIO_NUM, HIGH);
    delay(50);
    digitalWrite(PWDN_GPIO_NUM, LOW);
    delay(100);
    config.xclk_freq_hz = 10000000;
    err = esp_camera_init(&config);
  }

  if (err == ESP_OK) {
    cameraReady = true;
    cameraPowerOn = true;
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
      s->set_brightness(s, 1);
      s->set_contrast(s, 1);
      s->set_saturation(s, 0);
      s->set_whitebal(s, 1);
      s->set_awb_gain(s, 1);
      s->set_wb_mode(s, 0);
    }
    return true;
  } else {
    cameraReady = false;
    cameraPowerOn = false;
    return false;
  }
}

void setCameraHardwarePower(bool on) {
  if (on) {
    if (!cameraReady) startCamera();
    else {
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

void handleCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

const uint8_t PROBE_GIF[] PROGMEM = {
  0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0x80, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x21, 0xf9, 0x04, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
  0x00, 0x02, 0x02, 0x44, 0x01, 0x00, 0x3b
};

void handleProbeGif() {
  handleCORS();
  lastClientPingAt = millis();
  if (!clientConnectedNoticeSent) {
    clientConnectedNoticeSent = true;
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(70);
    digitalWrite(FLASH_LED_PIN, LOW);
    Serial.println("Z:CLIENT_ON");
    Serial.flush();
  }
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send_P(200, "image/gif", (const char*)PROBE_GIF, sizeof(PROBE_GIF));
}

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

void streamTask(void* pvParameters) {
  streamServer.begin();
  while (true) {
    WiFiClient sClient = streamServer.available();
    if (sClient) {
      sClient.print("HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace;boundary=" PART_BOUNDARY "\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n");
      localClientStreaming = true;
      while (sClient.connected()) {
        if (!cameraPowerOn || !cameraReady) {
          vTaskDelay(100 / portTICK_PERIOD_MS);
          continue;
        }
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
          vTaskDelay(10 / portTICK_PERIOD_MS);
          continue;
        }
        char part_buf[128];
        size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, fb->len);
        sClient.write((const uint8_t*)("\r\n--" PART_BOUNDARY "\r\n"), strlen("\r\n--" PART_BOUNDARY "\r\n"));
        sClient.write((const uint8_t*)part_buf, hlen);
        sClient.write((const uint8_t*)fb->buf, fb->len);
        sClient.write((const uint8_t*)"\r\n", 2);
        esp_camera_fb_return(fb);
        vTaskDelay(15 / portTICK_PERIOD_MS);
      }
      sClient.stop();
      localClientStreaming = false;
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

void handleStream() {
  handleCORS();
  if (!cameraPowerOn || !cameraReady) {
    server.send(503, "text/plain", "Camera OFF");
    return;
  }
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Capture failed");
    return;
  }
  server.sendHeader("Content-Type", "image/jpeg");
  server.sendHeader("Content-Length", String(fb->len));
  server.sendHeader("Access-Control-Allow-Origin", "*");
  WiFiClient client = server.client();
  client.write((const uint8_t*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void handleCmd() {
  handleCORS();
  lastClientPingAt = millis();
  String dir = server.arg("dir");
  if (dir.length() > 0) {
    char c = dir.charAt(0);
    if (c == 'F' || c == 'B' || c == 'L' || c == 'R' || c == 'S' || c == 'H' || c == 'h') {
      Serial.println(c);
      Serial.flush();
      lastCmd = c;
      server.send(200, "application/json", "{\"status\":\"ok\",\"cmd\":\"" + String(c) + "\"}");
      return;
    }
  }
  server.send(400, "application/json", "{\"error\":\"invalid command\"}");
}

void handleHorn() {
  handleCORS();
  lastClientPingAt = millis();
  String val = server.arg("val");
  if (val == "1" || val == "H" || val == "true") {
    Serial.println("H");
    Serial.flush();
    server.send(200, "application/json", "{\"horn\":1}");
  } else {
    Serial.println("h");
    Serial.flush();
    server.send(200, "application/json", "{\"horn\":0}");
  }
}

int clampSpeed(long val) {
  if (val < 0) return 0;
  if (val > 255) return 255;
  return (int)val;
}

void handleSpeed() {
  handleCORS();
  if (server.hasArg("fs")) {
    int fs = clampSpeed(server.arg("fs").toInt());
    Serial.print("FSP:");
    Serial.println(fs);
    lastForwardSpeed = fs;
  }
  if (server.hasArg("bs")) {
    int bs = clampSpeed(server.arg("bs").toInt());
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
  setCameraHardwarePower(server.arg("power") == "1");
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
    server.send(200, "application/json", "{\"status\":\"saved\"}");
    delay(1000);
    ESP.restart();
  } else {
    server.send(400, "application/json", "{\"error\":\"SSID empty\"}");
  }
}

void setupLocalServer() {
  server.on("/ping.gif", HTTP_GET, handleProbeGif);
  server.on("/probe.gif", HTTP_GET, handleProbeGif);
  server.on("/status.gif", HTTP_GET, handleProbeGif);
  server.on("/ping", HTTP_GET, []() {
    handleCORS();
    server.send(200, "text/plain", "pong");
  });
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/cmd", HTTP_GET, handleCmd);
  server.on("/set.php", HTTP_GET, handleCmd);
  server.on("/horn", HTTP_GET, handleHorn);
  server.on("/speed", HTTP_GET, handleSpeed);
  server.on("/flash", HTTP_GET, handleFlash);
  server.on("/cam", HTTP_GET, handleCamPower);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/get.php", HTTP_GET, handleStatus);
  server.on("/scan", HTTP_GET, handleScanWifi);
  server.on("/save-wifi", HTTP_GET, handleSaveWifi);
  server.on("/", HTTP_GET, []() {
    handleCORS();
    server.send(200, "text/html", "<html><head><meta http-equiv='refresh' content='0;url=http://sumitrathor.rf.gd/FPV_Car/'></head><body>Redirecting...</body></html>");
  });
  server.on("/FPV_Car", HTTP_GET, []() {
    handleCORS();
    server.send(200, "text/plain", "OK");
  });
  server.on("/FPV_Car/", HTTP_GET, []() {
    handleCORS();
    server.send(200, "text/plain", "OK");
  });
  server.onNotFound([]() {
    handleCORS();
    server.send(200, "text/plain", "OK");
  });
  server.begin();
}

void startApMode() {
  isApMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("FPV-Car-Setup", "");
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  setupLocalServer();
  if (MDNS.begin("fpvcar")) {
    MDNS.addService("http", "tcp", 80);
  }
  flashBlink(3, 160);
  Serial.println("Z:AP_MODE");
  Serial.flush();
}

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
  return clampSpeed(payload.substring(valueStart, valueEnd).toInt());
}

void setup() {
  Serial.begin(115200);
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  startCamera();
  flashBlink(3, 140);

  prefs.begin("fpv_wifi", true);
  saved_ssid = prefs.getString("ssid", "");
  saved_password = prefs.getString("pass", "");
  prefs.end();

  WiFi.setSleep(false);

  bool connected = false;
  if (saved_ssid.length() > 0) {
    WiFi.begin(saved_ssid.c_str(), saved_password.c_str());
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 25) {
      delay(300);
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      flashBlink(2, 90);
      Serial.println("Z:WIFI_OK");
      Serial.flush();
    }
  }

  if (!connected) {
    startApMode();
  } else {
    setupLocalServer();
    if (MDNS.begin("fpvcar")) {
      MDNS.addService("http", "tcp", 80);
    }
  }

  xTaskCreatePinnedToCore(streamTask, "streamTask", 4096, NULL, 1, NULL, 0);
}

void loop() {
  if (isApMode) {
    dnsServer.processNextRequest();
  }
  server.handleClient();

  uint32_t now = millis();

  if (clientConnectedNoticeSent && (now - lastClientPingAt > 6000)) {
    clientConnectedNoticeSent = false;
    flashBlink(2, 60);
    Serial.println("Z:CLIENT_OFF");
    Serial.flush();
  }

  if (isApMode || WiFi.status() != WL_CONNECTED || localClientStreaming) {
    delay(2);
    return;
  }

  if (now - lastHeartbeatAt >= CLOUD_HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatAt = now;
    HTTPClient http;
    String myIp = isApMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    http.begin(String(heartbeatUrl) + "&car_ip=" + myIp);
    http.setTimeout(2500);
    http.GET();
    http.end();
  }

  if (now - lastControlAt >= CLOUD_CONTROL_INTERVAL_MS) {
    lastControlAt = now;
    HTTPClient http;
    http.begin(controlUrl);
    http.setTimeout(2500);
    int code = http.GET();
    if (code == 200) {
      String res = http.getString();
      res.trim();
      char cmd = parseJsonCommand(res);
      bool shouldCam = parseJsonCamPower(res);
      bool shouldFlash = parseJsonFlashLight(res);
      int desiredFs = parseJsonSpeed(res, "fs", lastForwardSpeed);
      int desiredBs = parseJsonSpeed(res, "bs", lastBackwardSpeed);

      if (shouldCam != cameraPowerOn) {
        setCameraHardwarePower(shouldCam);
      }
      if (shouldFlash != flashState) {
        flashState = shouldFlash;
        digitalWrite(FLASH_LED_PIN, flashState ? HIGH : LOW);
      }
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
      if (cmd != lastCmd || cmd != 'S') {
        Serial.println(cmd);
        Serial.flush();
        lastCmd = cmd;
      }
    }
    http.end();
  }

  if (cameraPowerOn && cameraReady && (now - lastUploadAt >= CLOUD_UPLOAD_INTERVAL_MS)) {
    lastUploadAt = now;
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb != nullptr) {
      HTTPClient http;
      http.begin(uploadUrl);
      http.setTimeout(2500);
      http.addHeader("Content-Type", "application/octet-stream");
      http.POST(fb->buf, fb->len);
      http.end();
      esp_camera_fb_return(fb);
    }
  }

  delay(2);
}
