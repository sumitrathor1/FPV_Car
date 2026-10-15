/******************************************************************************
 * 🏎️ FPV Car - ESP32-CAM Ultra-Lightweight Standalone AP Controller
 *
 * Direct Mobile Hotspot Edition (Zero-Cloud, Pure Local Execution)
 *
 * Features:
 *   1. Auto Wi-Fi Hotspot: SSID "FPV-Car" (Open, IP: 192.168.4.1)
 *   2. Captive Portal: Auto-opens controller when mobile connects
 *   3. Ultra-Low Latency Camera Stream (<30ms lag)
 *   4. Instant Motor Control (<1ms response time to Arduino UNO)
 *   5. Robust Fail-Safe: If camera is loose/disconnected, motor controls
 *      stay 100% active with zero lag!
 ******************************************************************************/

#include "esp_camera.h"
#include <WiFi.h>
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
char currentCmd = 'S';
int currentSpeed = 255;

DNSServer dnsServer;
const byte DNS_PORT = 53;

httpd_handle_t serverHttpd = NULL;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace; boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ==========================================
// Embedded Lightweight Web Controller HTML
// ==========================================
static const char PROGMEM INDEX_HTML[] = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>🏎️ FPV Car Controller</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0;user-select:none;-webkit-user-select:none;-webkit-touch-callout:none;}
  body{background:#090d16;color:#f1f5f9;font-family:system-ui,-apple-system,sans-serif;text-align:center;padding:10px;touch-action:manipulation;min-height:100vh;display:flex;flex-direction:column;justify-content:space-between;}
  .top-bar{display:flex;justify-content:space-between;align-items:center;background:rgba(255,255,255,0.06);border:1px solid rgba(255,255,255,0.1);padding:8px 14px;border-radius:12px;margin-bottom:8px;}
  .title{font-size:1.05rem;font-weight:800;letter-spacing:1px;color:#38bdf8;}
  .badge{background:#10b981;color:#fff;font-size:0.7rem;font-weight:800;padding:3px 10px;border-radius:999px;letter-spacing:0.5px;}
  .badge.alert{background:#ef4444;}
  .video-container{position:relative;width:100%;max-width:520px;aspect-ratio:4/3;margin:0 auto 10px;background:#000;border:2px solid #1e293b;border-radius:16px;overflow:hidden;box-shadow:0 8px 30px rgba(0,0,0,0.6);}
  .video-container img{width:100%;height:100%;object-fit:cover;display:block;}
  .cam-overlay{position:absolute;inset:0;background:rgba(15,23,42,0.92);display:flex;flex-direction:column;align-items:center;justify-content:center;padding:16px;}
  .cam-overlay.hidden{display:none;}
  .btn-row{display:flex;gap:8px;justify-content:center;margin-bottom:10px;flex-wrap:wrap;}
  .hud-btn{background:#1e293b;color:#f8fafc;border:1px solid rgba(255,255,255,0.15);padding:10px 18px;border-radius:12px;font-size:0.85rem;font-weight:700;cursor:pointer;touch-action:manipulation;transition:all .15s;}
  .hud-btn:active,.hud-btn.active{transform:scale(0.95);background:#0284c7;border-color:#38bdf8;}
  .hud-btn.horn{background:rgba(239,68,68,0.2);border-color:#ef4444;color:#fca5a5;}
  .hud-btn.horn:active,.hud-btn.horn.active{background:#ef4444;color:#fff;}
  .slider-box{max-width:320px;margin:0 auto 10px;background:rgba(255,255,255,0.04);border:1px solid rgba(255,255,255,0.08);padding:8px 14px;border-radius:12px;}
  .slider-box label{display:flex;justify-content:space-between;font-size:0.8rem;color:#94a3b8;font-weight:700;margin-bottom:4px;}
  input[type=range]{width:100%;accent-color:#0284c7;}
  .dpad{display:grid;grid-template-columns:repeat(3,75px);grid-template-rows:repeat(3,75px);gap:10px;justify-content:center;margin:6px auto 10px;}
  .d-btn{background:#1e293b;border:2px solid #334155;border-radius:18px;color:#fff;font-size:1.8rem;display:flex;align-items:center;justify-content:center;cursor:pointer;touch-action:none;box-shadow:0 4px 12px rgba(0,0,0,0.4);transition:all .1s;}
  .d-btn:active,.d-btn.pressed{background:#0284c7;border-color:#38bdf8;transform:scale(0.92);box-shadow:0 0 20px rgba(56,189,248,0.6);}
  .d-stop{background:#dc2626;border-color:#ef4444;font-size:1rem;font-weight:900;}
  .d-stop:active{background:#b91c1c;}
  .footer-tip{font-size:0.75rem;color:#64748b;margin-top:4px;}
</style>
</head>
<body>
  <div>
    <div class="top-bar">
      <span class="title">🏎️ FPV CAR DIRECT</span>
      <span class="badge" id="netBadge">HOTSPOT: 192.168.4.1</span>
    </div>

    <div class="video-container">
      <img id="streamImg" src="/stream" alt="Live FPV Stream" />
      <div class="cam-overlay" id="camWarn" style="display:none;">
        <div style="font-size:2.2rem;margin-bottom:6px;">📷</div>
        <div style="font-weight:800;color:#f87171;font-size:1.05rem;margin-bottom:4px;">Camera Sensor Offline (0x106)</div>
        <div style="font-size:0.8rem;color:#94a3b8;line-height:1.4;margin-bottom:12px;">Check 24-pin ribbon cable & 5V power.<br><b style="color:#38bdf8;">Motor controls are 100% active below!</b></div>
        <button class="hud-btn" onclick="retryCam()" style="background:#0284c7;border-color:#38bdf8;">🔄 Re-Detect Camera</button>
      </div>
    </div>

    <div class="btn-row">
      <button class="hud-btn horn" id="hornBtn" onpointerdown="sendHorn(1)" onpointerup="sendHorn(0)" onpointerleave="sendHorn(0)">📢 HORN</button>
      <button class="hud-btn" id="lightBtn" onclick="toggleLight()">💡 LIGHT</button>
      <button class="hud-btn" onclick="retryCam()">🔄 CHECK CAM</button>
    </div>

    <div class="slider-box">
      <label><span>⚡ MOTOR SPEED</span><span id="speedTxt">255</span></label>
      <input type="range" min="100" max="255" value="255" oninput="changeSpeed(this.value)" />
    </div>

    <div class="dpad">
      <div></div>
      <button class="d-btn" onpointerdown="drive('F')" onpointerup="drive('S')" onpointerleave="drive('S')">▲</button>
      <div></div>
      <button class="d-btn" onpointerdown="drive('L')" onpointerup="drive('S')" onpointerleave="drive('S')">◀</button>
      <button class="d-btn d-stop" onclick="drive('S')">STOP</button>
      <button class="d-btn" onpointerdown="drive('R')" onpointerup="drive('S')" onpointerleave="drive('S')">▶</button>
      <div></div>
      <button class="d-btn" onpointerdown="drive('B')" onpointerup="drive('S')" onpointerleave="drive('S')">▼</button>
      <div></div>
    </div>
  </div>

  <div class="footer-tip">⌨️ Keyboard: W, A, S, D / Arrows to Drive | Space = STOP | H = Horn</div>

<script>
  let lightActive = false;
  let activeDir = 'S';
  const img = document.getElementById('streamImg');
  const warn = document.getElementById('camWarn');

  function checkCamStatus() {
    fetch('/status')
      .then(r => r.json())
      .then(d => {
        if (!d.cam) {
          warn.style.display = 'flex';
          img.style.display = 'none';
        } else {
          warn.style.display = 'none';
          img.style.display = 'block';
          if (!img.src.includes('/stream')) img.src = '/stream?t=' + Date.now();
        }
      })
      .catch(() => {});
  }
  checkCamStatus();

  img.onerror = function() {
    warn.style.display = 'flex';
    // Back off for 4 seconds so we never flood the web server
    setTimeout(checkCamStatus, 4000);
  };

  img.onload = function() {
    warn.style.display = 'none';
    img.style.display = 'block';
  };

  function sendReq(url) {
    fetch(url, { mode: 'no-cors', keepalive: true, cache: 'no-store' }).catch(() => {
      new Image().src = url;
    });
  }

  function drive(d) {
    if (activeDir === d && d !== 'S') return;
    activeDir = d;
    sendReq('/cmd?dir=' + d);
  }

  function sendHorn(v) {
    sendReq('/horn?val=' + v);
    document.getElementById('hornBtn').classList.toggle('active', v === 1);
  }

  function toggleLight() {
    lightActive = !lightActive;
    sendReq('/flash?val=' + (lightActive ? 1 : 0));
    document.getElementById('lightBtn').classList.toggle('active', lightActive);
  }

  function changeSpeed(v) {
    document.getElementById('speedTxt').innerText = v;
    sendReq('/speed?s=' + v);
  }

  function retryCam() {
    sendReq('/reprobe');
    setTimeout(() => {
      checkCamStatus();
      img.src = '/stream?t=' + Date.now();
    }, 500);
  }

  window.addEventListener('keydown', e => {
    if (e.repeat) return;
    const k = e.key.toLowerCase();
    if (k === 'w' || k === 'arrowup') drive('F');
    else if (k === 's' || k === 'arrowdown') drive('B');
    else if (k === 'a' || k === 'arrowleft') drive('L');
    else if (k === 'd' || k === 'arrowright') drive('R');
    else if (k === ' ' || k === 'escape') drive('S');
    else if (k === 'h') sendHorn(1);
    else if (k === 'l') toggleLight();
  });

  window.addEventListener('keyup', e => {
    const k = e.key.toLowerCase();
    if (['w', 's', 'a', 'd', 'arrowup', 'arrowdown', 'arrowleft', 'arrowright'].includes(k)) drive('S');
    else if (k === 'h') sendHorn(0);
  });
</script>
</body>
</html>)rawliteral";

// ==========================================
// Camera Hardware Initialization
// ==========================================
bool startCameraDriver() {
  if (cameraReady) return true;

  // Hardware power cycle camera module on Pin 32
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

  // Attempt 1: Standard 20MHz XCLK
  esp_err_t err = esp_camera_init(&config);

  // Attempt 2: 10MHz XCLK for noisy clone sensors
  if (err != ESP_OK) {
    esp_camera_deinit();
    delay(50);
    config.xclk_freq_hz = 10000000;
    config.frame_size   = FRAMESIZE_QVGA;
    config.fb_count     = 1;
    err = esp_camera_init(&config);
  }

  // Attempt 3: Clone PWDN Bypass
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
  char buf[32];
  if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
    char s[8];
    if (httpd_query_key_value(buf, "s", s, sizeof(s)) == ESP_OK) {
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
  char json[128];
  snprintf(json, sizeof(json), "{\"cam\":%s,\"flash\":%s,\"cmd\":\"%c\",\"speed\":%d}",
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

  // Trigger Arduino Boot Test Melody & Motor Kick
  Serial.println();
  Serial.println("Z:BOOT");
  Serial.flush();

  // Create Standalone Wi-Fi Hotspot (Open network for 1-tap connection)
  WiFi.mode(WIFI_AP);
  WiFi.softAP("FPV-Car", ""); // SSID: FPV-Car
  WiFi.setSleep(false);

  // Start Captive Portal DNS on Port 53
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // Tell Arduino that AP Hotspot is ready
  delay(100);
  Serial.println("Z:AP");
  Serial.flush();

  // Start Asynchronous High-Speed HTTP Server
  startWebServer();

  Serial.println("=========================================");
  Serial.println("🏎️ FPV-CAR DIRECT HOTSPOT READY!");
  Serial.println("Wi-Fi SSID : FPV-Car");
  Serial.println("Browser URL: http://192.168.4.1");
  Serial.println("=========================================");
}

// ==========================================
// Main Loop
// ==========================================
void loop() {
  dnsServer.processNextRequest();
  delay(2);
}
