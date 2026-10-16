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

// Asynchronous Wi-Fi Connect & Cloud Polling States
bool pendingWifiConnect = false;
String pendingSsid = "";
String pendingPass = "";
unsigned long pendingConnectStartTime = 0;
unsigned long lastCloudPoll = 0;
unsigned long lastHeartbeat = 0;

// Wi-Fi Scanning & Connection Progress Tracking
String cachedScanResults = "[]";
volatile bool isScanningAsync = false;
String wifiConnectState = "idle"; // "idle", "connecting", "connected", "failed"
String wifiConnectError = "";
unsigned long wifiConnectStartMs = 0;

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
#include "index_html.h"


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

// Helper to parse scanned networks into JSON cache
void updateScanCacheFromCount(int n) {
  if (n <= 0) {
    cachedScanResults = "[]";
    return;
  }
  String json = "[";
  bool first = true;
  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    ssid.trim();
    if (ssid.length() == 0) continue; // skip hidden
    ssid.replace("\\", "\\\\");
    ssid.replace("\"", "\\\"");
    int rssi = WiFi.RSSI(i);
    if (!first) json += ",";
    json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(rssi) + "}";
    first = false;
  }
  json += "]";
  cachedScanResults = json;
  WiFi.scanDelete();
}

// Single Frame Snapshot Capture Handler
esp_err_t capture_handler(httpd_req_t *req) {
  if (!cameraReady) {
    httpd_resp_set_status(req, "503 Camera Offline");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "Camera Offline", HTTPD_RESP_USE_STRLEN);
  }
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_set_status(req, "500 Capture Failed");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "Capture Failed", HTTPD_RESP_USE_STRLEN);
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

// Wi-Fi Connection Progress Status Handler
esp_err_t wifi_status_handler(httpd_req_t *req) {
  char json[220];
  snprintf(json, sizeof(json),
    "{\"state\":\"%s\",\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"err\":\"%s\"}",
    wifiConnectState.c_str(),
    connectedSsid.c_str(),
    WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "",
    WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0,
    wifiConnectError.c_str()
  );
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  return httpd_resp_send(req, json, strlen(json));
}

// Scan nearby 2.4GHz Wi-Fi networks (Instant response from cache + async refresh)
esp_err_t scan_handler(httpd_req_t *req) {
  char buf[32];
  bool refresh = false;
  if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
    char ref[8];
    if (httpd_query_key_value(buf, "refresh", ref, sizeof(ref)) == ESP_OK && ref[0] == '1') {
      refresh = true;
    }
  }

  // If refresh requested and not currently scanning, trigger background async scan
  if (refresh && !isScanningAsync) {
    isScanningAsync = true;
    WiFi.scanNetworks(true, false, false, 80);
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  return httpd_resp_send(req, cachedScanResults.c_str(), HTTPD_RESP_USE_STRLEN);
}

// Connect to Wi-Fi and switch to Server Mode (Non-blocking response to client)
esp_err_t save_wifi_handler(httpd_req_t *req) {
  char buf[256];
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

    pendingSsid = ssid;
    pendingPass = pass;
    pendingWifiConnect = true;
    pendingConnectStartTime = millis() + 400; // Allow 400ms for phone to finish receiving response
    wifiConnectState = "connecting";
    wifiConnectError = "";

    String res = "{\"status\":\"ok\",\"msg\":\"connecting\",\"ssid\":\"" + ssid + "\",\"cloud\":\"http://172.198.73.188/\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, res.c_str(), HTTPD_RESP_USE_STRLEN);
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, "{\"status\":\"fail\",\"error\":\"empty_ssid\"}", HTTPD_RESP_USE_STRLEN);
}

// Revert to Local AP Hotspot Mode
void switchToLocalAp() {
  isApMode = true;
  connectedSsid = "";
  wifiConnectState = "idle";

  WiFi.disconnect(false, true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("FPV-Car", "");
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  Serial.println("Z:AP");
  Serial.flush();

  // Notify Cloud Server that car has reverted to AP mode and stopped
  HTTPClient http;
  http.begin("http://172.198.73.188/set.php?car_mode=sta&mode=0&cmd=S&car_ip=192.168.4.1");
  http.setTimeout(1200);
  http.GET();
  http.end();

  Serial.println("[MODE] Reverted to Local AP Hotspot: FPV-Car (192.168.4.1)");
}

// Switch between AP and STA mode
esp_err_t switch_mode_handler(httpd_req_t *req) {
  char buf[32];
  if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
    char m[8];
    if (httpd_query_key_value(buf, "mode", m, sizeof(m)) == ESP_OK) {
      if (String(m) == "ap") {
        switchToLocalAp();
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
  httpd_uri_t uri_capture = { .uri = "/capture", .method = HTTP_GET, .handler = capture_handler, .user_ctx = NULL };
  httpd_uri_t uri_cmd = { .uri = "/cmd", .method = HTTP_GET, .handler = cmd_handler, .user_ctx = NULL };
  httpd_uri_t uri_horn = { .uri = "/horn", .method = HTTP_GET, .handler = horn_handler, .user_ctx = NULL };
  httpd_uri_t uri_speed = { .uri = "/speed", .method = HTTP_GET, .handler = speed_handler, .user_ctx = NULL };
  httpd_uri_t uri_flash = { .uri = "/flash", .method = HTTP_GET, .handler = flash_handler, .user_ctx = NULL };
  httpd_uri_t uri_status = { .uri = "/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = NULL };
  httpd_uri_t uri_reprobe = { .uri = "/reprobe", .method = HTTP_GET, .handler = reprobe_handler, .user_ctx = NULL };
  httpd_uri_t uri_scan = { .uri = "/scan", .method = HTTP_GET, .handler = scan_handler, .user_ctx = NULL };
  httpd_uri_t uri_save_wifi = { .uri = "/save-wifi", .method = HTTP_GET, .handler = save_wifi_handler, .user_ctx = NULL };
  httpd_uri_t uri_wifi_status = { .uri = "/wifi-status", .method = HTTP_GET, .handler = wifi_status_handler, .user_ctx = NULL };
  httpd_uri_t uri_switch_mode = { .uri = "/switch-mode", .method = HTTP_GET, .handler = switch_mode_handler, .user_ctx = NULL };
  httpd_uri_t uri_cp1 = { .uri = "/generate_204", .method = HTTP_GET, .handler = redirect_handler, .user_ctx = NULL };
  httpd_uri_t uri_cp2 = { .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = redirect_handler, .user_ctx = NULL };

  if (httpd_start(&serverHttpd, &config) == ESP_OK) {
    httpd_register_uri_handler(serverHttpd, &uri_index);
    httpd_register_uri_handler(serverHttpd, &uri_stream);
    httpd_register_uri_handler(serverHttpd, &uri_capture);
    httpd_register_uri_handler(serverHttpd, &uri_cmd);
    httpd_register_uri_handler(serverHttpd, &uri_horn);
    httpd_register_uri_handler(serverHttpd, &uri_speed);
    httpd_register_uri_handler(serverHttpd, &uri_flash);
    httpd_register_uri_handler(serverHttpd, &uri_status);
    httpd_register_uri_handler(serverHttpd, &uri_reprobe);
    httpd_register_uri_handler(serverHttpd, &uri_scan);
    httpd_register_uri_handler(serverHttpd, &uri_save_wifi);
    httpd_register_uri_handler(serverHttpd, &uri_wifi_status);
    httpd_register_uri_handler(serverHttpd, &uri_switch_mode);
    httpd_register_uri_handler(serverHttpd, &uri_cp1);
    httpd_register_uri_handler(serverHttpd, &uri_cp2);
    Serial.println("[HTTP] Server started on port 80");
  }
}

// ==========================================
// Cloud Server Polling & Execution
// ==========================================
String getJsonVal(const String& json, const String& key) {
  int kIdx = json.indexOf("\"" + key + "\"");
  if (kIdx < 0) return "";
  int colonIdx = json.indexOf(':', kIdx + key.length() + 2);
  if (colonIdx < 0) return "";
  int start = colonIdx + 1;
  while (start < json.length() && (json[start] == ' ' || json[start] == '\"')) start++;
  int end = start;
  while (end < json.length() && json[end] != '\"' && json[end] != ',' && json[end] != '}' && json[end] != '\r' && json[end] != '\n') end++;
  return json.substring(start, end);
}

void parseAndExecuteCloudCommand(const String& payload) {
  // A. Check if user requested switch to Local AP
  String carMode = getJsonVal(payload, "car_mode");
  String modeVal = getJsonVal(payload, "mode");
  if (carMode == "ap" || modeVal == "ap") {
    Serial.println("[CLOUD] Switch to Local AP requested!");
    switchToLocalAp();
    return;
  }

  // B. Check Driving Direction
  String cmdVal = getJsonVal(payload, "cmd");
  if (cmdVal.length() > 0) {
    char c = cmdVal[0];
    if ((c == 'F' || c == 'B' || c == 'L' || c == 'R' || c == 'S') && c != currentCmd) {
      currentCmd = c;
      Serial.println(c);
      Serial.flush();
    }
  }

  // C. Check Speed
  String spdVal = getJsonVal(payload, "fs");
  if (spdVal.length() > 0) {
    int spd = spdVal.toInt();
    if (spd >= 0 && spd <= 255 && spd != currentSpeed) {
      currentSpeed = spd;
      Serial.print("FSP:");
      Serial.println(spd);
      Serial.print("BSP:");
      Serial.println(spd);
      Serial.flush();
    }
  }

  // D. Check Horn
  String hornVal = getJsonVal(payload, "horn");
  if (hornVal.length() > 0) {
    bool h = (hornVal == "1" || hornVal == "H");
    static bool lastHorn = false;
    if (h != lastHorn) {
      lastHorn = h;
      Serial.println(h ? "H" : "h");
      Serial.flush();
    }
  }

  // E. Check Flash Light
  String flashVal = getJsonVal(payload, "flash");
  if (flashVal.length() > 0) {
    bool f = (flashVal == "1");
    if (f != flashState) {
      flashState = f;
      digitalWrite(FLASH_LED_PIN, flashState ? HIGH : LOW);
    }
  }
}

void handlePendingWifiConnect() {
  if (!pendingWifiConnect) return;
  if (millis() < pendingConnectStartTime) return;
  pendingWifiConnect = false;

  wifiConnectState = "connecting";
  wifiConnectError = "";
  wifiConnectStartMs = millis();

  Serial.println("\n[WIFI] Connecting to: " + pendingSsid);
  WiFi.disconnect(false, true);
  delay(50);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(pendingSsid.c_str(), pendingPass.c_str());
}

void checkWifiConnectionProgress() {
  if (wifiConnectState != "connecting") return;

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnectState = "connected";
    isApMode = false;
    connectedSsid = pendingSsid;
    String localIp = WiFi.localIP().toString();
    Serial.println("\n[WIFI] Connected! Local IP: " + localIp);
    Serial.println("Z:WIFI_OK");
    Serial.flush();

    // Auto-register IP with Azure Cloud VM
    HTTPClient http;
    http.begin("http://172.198.73.188/set.php?car_ip=" + localIp + "&esp_hb=1");
    http.setTimeout(2500);
    http.GET();
    http.end();
  } else if (millis() - wifiConnectStartMs > 16000) {
    wifiConnectState = "failed";
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECT_FAILED) {
      wifiConnectError = "Incorrect Wi-Fi password";
    } else if (st == WL_NO_SSID_AVAIL) {
      wifiConnectError = "SSID not found or out of range";
    } else {
      wifiConnectError = "Connection timed out (Check 2.4GHz Wi-Fi)";
    }
    Serial.println("\n[WIFI] Connection failed: " + wifiConnectError);
    WiFi.disconnect(false, true);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("FPV-Car", "");
    isApMode = true;
  }
}

void pollCloudServer() {
  if (isApMode) return;
  if (WiFi.status() != WL_CONNECTED) return;

  unsigned long now = millis();

  // Heartbeat every 3 seconds
  if (now - lastHeartbeat >= 3000) {
    lastHeartbeat = now;
    HTTPClient hb;
    hb.begin("http://172.198.73.188/set.php?esp_hb=1&car_ip=" + WiFi.localIP().toString());
    hb.setTimeout(1200);
    hb.GET();
    hb.end();
  }

  // Poll cloud for driving commands every 80ms
  if (now - lastCloudPoll >= 80) {
    lastCloudPoll = now;
    HTTPClient http;
    http.begin("http://172.198.73.188/get.php");
    http.setTimeout(400);
    int code = http.GET();
    if (code == HTTP_CODE_OK) {
      String payload = http.getString();
      parseAndExecuteCloudCommand(payload);
    }
    http.end();
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
  WiFi.mode(WIFI_AP_STA);

  // Fast pre-scan of nearby 2.4GHz Wi-Fi networks before starting AP
  Serial.println("[WIFI] Scanning nearby networks...");
  int n = WiFi.scanNetworks(false, false, false, 120);
  updateScanCacheFromCount(n);
  Serial.printf("[WIFI] Boot scan found %d networks\n", n);

  // Start "FPV-Car" Hotspot
  WiFi.softAP("FPV-Car", "");
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
  // Check async scan completion
  if (isScanningAsync) {
    int16_t count = WiFi.scanComplete();
    if (count >= 0) {
      updateScanCacheFromCount(count);
      isScanningAsync = false;
      Serial.printf("[WIFI] Async scan completed: %d networks\n", count);
    } else if (count == WIFI_SCAN_FAILED) {
      isScanningAsync = false;
    }
  }

  // Handle Wi-Fi connection progression
  handlePendingWifiConnect();
  checkWifiConnectionProgress();

  if (isApMode) {
    dnsServer.processNextRequest();
  } else {
    pollCloudServer();
  }

  delay(2);
}
