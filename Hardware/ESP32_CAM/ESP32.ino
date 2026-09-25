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
bool cameraReady = false;
bool flashState = false;
int lastForwardSpeed = 255;
int lastBackwardSpeed = 255;
char lastCmd = 'S';

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

int clampSpeed(long val) {
  if (val < 0) return 0;
  if (val > 255) return 255;
  return (int)val;
}

bool startCamera() {
  if (cameraReady) return true;

  pinMode(PWDN_GPIO_NUM, OUTPUT);
  digitalWrite(PWDN_GPIO_NUM, HIGH);
  delay(100);
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count     = 2;
    config.grab_mode    = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 14;
    config.fb_count     = 1;
    config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAMERA] Retry with 10MHz XCLK... (err 0x%x)\n", err);
    digitalWrite(PWDN_GPIO_NUM, HIGH);
    delay(100);
    digitalWrite(PWDN_GPIO_NUM, LOW);
    delay(150);
    config.xclk_freq_hz = 10000000;
    config.frame_size   = FRAMESIZE_QVGA;
    config.fb_count     = 1;
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
    Serial.printf("[CAMERA] Probe failed (0x%x)\n", err);
    return false;
  }
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
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send_P(200, "image/gif", (const char*)PROBE_GIF, sizeof(PROBE_GIF));
}

#define PART_BOUNDARY "123456789000000000000987654321"

void streamTask(void* pvParameters) {
  streamServer.begin();
  streamServer.setNoDelay(true);
  Serial.println("[STREAM] Port 81 Ready");

  while (true) {
    WiFiClient sClient = streamServer.available();
    if (sClient) {
      // 1. Consume and clear incoming browser HTTP request headers
      unsigned long startWait = millis();
      while (sClient.connected() && millis() - startWait < 400) {
        if (sClient.available()) {
          String l = sClient.readStringUntil('\n');
          if (l == "\r" || l.length() == 0) break;
        } else {
          vTaskDelay(5 / portTICK_PERIOD_MS);
        }
      }

      // 2. Send standard multipart HTTP headers
      sClient.print("HTTP/1.1 200 OK\r\n"
                    "Content-Type: multipart/x-mixed-replace; boundary=" PART_BOUNDARY "\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Cache-Control: no-cache, no-store, must-revalidate\r\n"
                    "Pragma: no-cache\r\n"
                    "Connection: close\r\n\r\n");

      // 3. Continuous frame loop
      while (sClient.connected()) {
        if (!cameraReady) {
          vTaskDelay(100 / portTICK_PERIOD_MS);
          continue;
        }

        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
          vTaskDelay(10 / portTICK_PERIOD_MS);
          continue;
        }

        sClient.print("--" PART_BOUNDARY "\r\n"
                      "Content-Type: image/jpeg\r\n"
                      "Content-Length: " + String(fb->len) + "\r\n\r\n");

        // Write in safe 1024-byte chunks to prevent TCP socket overflow
        size_t rem = fb->len;
        uint8_t* p = fb->buf;
        while (rem > 0 && sClient.connected()) {
          size_t chunk = rem > 1024 ? 1024 : rem;
          size_t w = sClient.write(p, chunk);
          if (w == 0) break;
          p += w;
          rem -= w;
        }

        sClient.print("\r\n");
        esp_camera_fb_return(fb);
        vTaskDelay(20 / portTICK_PERIOD_MS);
      }
      sClient.stop();
    }
    vTaskDelay(25 / portTICK_PERIOD_MS);
  }
}

void handleCapture() {
  handleCORS();
  if (!cameraReady) {
    server.send(503, "text/plain", "Camera Not Ready");
    return;
  }
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Capture Failed");
    return;
  }
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  WiFiClient client = server.client();
  size_t rem = fb->len;
  uint8_t* p = fb->buf;
  while (rem > 0 && client.connected()) {
    size_t chunk = rem > 1024 ? 1024 : rem;
    size_t w = client.write(p, chunk);
    if (w == 0) break;
    p += w;
    rem -= w;
  }
  esp_camera_fb_return(fb);
}

void handleCmd() {
  handleCORS();
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

void handleStatus() {
  handleCORS();
  String json = "{";
  json += "\"mode\":\"" + String(isApMode ? "AP" : "STA") + "\",";
  json += "\"ip\":\"" + (isApMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "\",";
  json += "\"cam\":" + String(cameraReady ? 1 : 0) + ",";
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
    delay(500);
    ESP.restart();
  } else {
    server.send(400, "application/json", "{\"error\":\"SSID empty\"}");
  }
}

const char PAGE_INDEX[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>🏎️ FPV Car Direct</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;user-select:none;-webkit-user-select:none;}
body{background:#090d16;color:#e2e8f0;font-family:system-ui,-apple-system,sans-serif;text-align:center;padding:12px;overflow-x:hidden;}
.header{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;padding:8px 12px;background:rgba(255,255,255,0.05);border-radius:12px;border:1px solid rgba(255,255,255,0.1);}
.title{font-size:1.05rem;font-weight:800;letter-spacing:1px;color:#38bdf8;}
.badge{background:#10b981;color:#fff;font-size:0.7rem;padding:3px 8px;border-radius:999px;font-weight:700;}
.video-wrap{position:relative;width:100%;max-width:540px;margin:0 auto 12px;border-radius:14px;overflow:hidden;background:#000;border:2px solid #1e293b;aspect-ratio:4/3;box-shadow:0 8px 24px rgba(0,0,0,0.5);}
.video-wrap img{width:100%;height:100%;object-fit:cover;display:block;}
.hud-btn-row{display:flex;gap:8px;justify-content:center;margin-bottom:12px;flex-wrap:wrap;}
.btn{background:#1e293b;color:#f8fafc;border:1px solid rgba(255,255,255,0.15);padding:10px 18px;border-radius:10px;font-size:0.9rem;font-weight:700;cursor:pointer;touch-action:manipulation;transition:all .15s;}
.btn:active,.btn.active{transform:scale(0.95);background:#0284c7;}
.btn-horn{background:rgba(239,68,68,0.2);border-color:#ef4444;color:#fca5a5;}
.btn-horn:active,.btn-horn.active{background:#ef4444;color:#fff;}
.btn-dash{background:rgba(14,165,233,0.15);border-color:#0ea5e9;color:#38bdf8;text-decoration:none;display:inline-block;}
.controls{display:grid;grid-template-columns:repeat(3,75px);grid-template-rows:repeat(3,75px);gap:10px;justify-content:center;margin:10px auto 16px;}
.ctrl-btn{background:#1e293b;border:2px solid #334155;border-radius:16px;color:#fff;font-size:1.6rem;display:flex;align-items:center;justify-content:center;cursor:pointer;touch-action:manipulation;}
.ctrl-btn:active,.ctrl-btn.pressed{background:#0284c7;border-color:#38bdf8;box-shadow:0 0 15px rgba(56,189,248,0.5);}
.ctrl-stop{background:#dc2626;border-color:#ef4444;font-size:1rem;font-weight:900;}
.ctrl-stop:active{background:#b91c1c;}
.slider-wrap{max-width:320px;margin:0 auto 14px;background:rgba(255,255,255,0.04);padding:8px 14px;border-radius:10px;}
.slider-wrap label{display:flex;justify-content:space-between;font-size:0.8rem;margin-bottom:4px;color:#94a3b8;}
input[type=range]{width:100%;accent-color:#0284c7;}
</style>
</head>
<body>
<div class="header">
  <span class="title">🏎️ FPV CAR DIRECT</span>
  <span class="badge" id="statBadge">CONNECTED</span>
</div>
<div class="video-wrap">
  <img id="stream" src="" onerror="if(!this.src.includes('/capture'))this.src='/capture?t='+Date.now()" alt="Camera Feed" />
</div>
<div class="hud-btn-row">
  <button class="btn btn-horn" id="hornBtn" onpointerdown="sendHorn(1)" onpointerup="sendHorn(0)" onpointerleave="sendHorn(0)">📢 HORN</button>
  <button class="btn" id="lightBtn" onclick="toggleLight()">💡 LIGHT</button>
  <a class="btn btn-dash" id="dashLink" href="http://sumitrathor.rf.gd/FPV_Car/">🌐 FULL DASHBOARD</a>
</div>
<div class="slider-wrap">
  <label><span>⚡ MOTOR SPEED</span><span id="spdVal">255</span></label>
  <input type="range" min="100" max="255" value="255" oninput="setSpeed(this.value)" />
</div>
<div class="controls">
  <div></div>
  <button class="ctrl-btn" onpointerdown="sendDir('F')" onpointerup="sendDir('S')">▲</button>
  <div></div>
  <button class="ctrl-btn" onpointerdown="sendDir('L')" onpointerup="sendDir('S')">◀</button>
  <button class="ctrl-btn ctrl-stop" onclick="sendDir('S')">STOP</button>
  <button class="ctrl-btn" onpointerdown="sendDir('R')" onpointerup="sendDir('S')">▶</button>
  <div></div>
  <button class="ctrl-btn" onpointerdown="sendDir('B')" onpointerup="sendDir('S')">▼</button>
  <div></div>
</div>
<script>
let lightOn=false;
const myHost=window.location.hostname||'192.168.4.1';
const stImg=document.getElementById('stream');
stImg.src='http://'+myHost+':81/stream';
let snapTimer=null;
stImg.onerror=function(){
  if(!snapTimer){
    snapTimer=setInterval(()=>{
      const tmp=new Image();
      tmp.onload=()=>{stImg.src=tmp.src;};
      tmp.src='http://'+myHost+'/capture?t='+Date.now();
    },100);
  }
};
stImg.onload=function(){
  if(snapTimer){clearInterval(snapTimer);snapTimer=null;}
};
document.getElementById('dashLink').href='http://sumitrathor.rf.gd/FPV_Car/?car='+myHost;
function sendReq(u){fetch(u,{mode:'no-cors'}).catch(()=>{new Image().src=u;});}
function sendDir(d){sendReq('/cmd?dir='+d);}
function sendHorn(v){sendReq('/horn?val='+v);document.getElementById('hornBtn').classList.toggle('active',v===1);}
function toggleLight(){lightOn=!lightOn;sendReq('/flash?val='+(lightOn?1:0));document.getElementById('lightBtn').classList.toggle('active',lightOn);}
function setSpeed(v){document.getElementById('spdVal').innerText=v;sendReq('/speed?fs='+v+'&bs='+v);}
window.addEventListener('keydown',e=>{
  if(e.repeat)return;
  const k=e.key.toLowerCase();
  if(k==='w'||k==='arrowup')sendDir('F');
  else if(k==='s'||k==='arrowdown')sendDir('B');
  else if(k==='a'||k==='arrowleft')sendDir('L');
  else if(k==='d'||k==='arrowright')sendDir('R');
  else if(k===' '||k==='escape')sendDir('S');
  else if(k==='h')sendHorn(1);
});
window.addEventListener('keyup',e=>{
  const k=e.key.toLowerCase();
  if(['w','s','a','d','arrowup','arrowdown','arrowleft','arrowright'].includes(k))sendDir('S');
  else if(k==='h')sendHorn(0);
});
</script>
</body>
</html>)rawliteral";

void handleRoot() {
  handleCORS();
  server.send_P(200, "text/html", PAGE_INDEX);
}

void setup() {
  Serial.begin(115200);
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  delay(800);
  startCamera();
  delay(200);

  // ESP32 Master triggers the Boot Self-Test (Buzzer + Motor Kick) on Arduino!
  Serial.println();
  Serial.println("Z:BOOT");
  Serial.flush();
  flashBlink(3, 120);

  prefs.begin("fpv_wifi", true);
  saved_ssid = prefs.getString("ssid", "");
  saved_password = prefs.getString("pass", "");
  prefs.end();

  WiFi.setSleep(false);

  bool connected = false;
  if (saved_ssid.length() > 0) {
    Serial.print("[WIFI] Connecting to: ");
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
      flashBlink(2, 90);
      Serial.println("Z:WIFI_OK");
      Serial.flush();
    }
  }

  if (!connected) {
    isApMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("FPV-Car-Setup", "");
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    flashBlink(3, 150);
    Serial.println("Z:AP_MODE");
    Serial.flush();
    Serial.println("\n[WIFI] AP Hotspot Started: FPV-Car-Setup (192.168.4.1)");
  }

  if (MDNS.begin("fpvcar")) {
    MDNS.addService("http", "tcp", 80);
  }

  server.on("/ping.gif", HTTP_GET, handleProbeGif);
  server.on("/ping", HTTP_GET, []() { handleCORS(); server.send(200, "text/plain", "pong"); });
  server.on("/cmd", HTTP_GET, handleCmd);
  server.on("/set.php", HTTP_GET, handleCmd);
  server.on("/horn", HTTP_GET, handleHorn);
  server.on("/speed", HTTP_GET, handleSpeed);
  server.on("/flash", HTTP_GET, handleFlash);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/get.php", HTTP_GET, handleStatus);
  server.on("/scan", HTTP_GET, handleScanWifi);
  server.on("/save-wifi", HTTP_GET, handleSaveWifi);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/stream", HTTP_GET, []() {
    handleCORS();
    server.sendHeader("Location", "http://" + (isApMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + ":81/stream");
    server.send(302, "text/plain", "Redirecting");
  });
  server.on("/", HTTP_GET, handleRoot);
  server.onNotFound([]() {
    handleCORS();
    server.send(200, "text/plain", "FPV Car Ready");
  });
  server.begin();

  // Start Core 0 Stream Task on Port 81 (8KB stack to prevent FreeRTOS overflow)
  xTaskCreatePinnedToCore(streamTask, "streamTask", 8192, NULL, 1, NULL, 0);

  Serial.println("=========================================");
  Serial.print("READY! Direct Controller: http://");
  Serial.println(connected ? WiFi.localIP().toString() : "192.168.4.1");
  Serial.println("=========================================");
}

void loop() {
  if (isApMode) {
    dnsServer.processNextRequest();
  }
  // Pure local execution with zero blocking network calls
  server.handleClient();
  delay(2);
}

