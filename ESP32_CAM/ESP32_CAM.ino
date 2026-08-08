#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// Select Camera Model: AI THINKER
#define CAMERA_MODEL_AI_THINKER

//======================================================
// WiFi Credentials (Apna WiFi SSID aur Password yahan daalein)
//======================================================
const char* ssid     = "sumit";
const char* password = "12345678";

//======================================================
// ESP32-CAM AI-Thinker Pin Definitions
//======================================================
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

httpd_handle_t camera_httpd = NULL;

//======================================================
// Web Dashboard (UI & Responsive Controller)
//======================================================
static const char MAIN_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>FPV Surveillance Car Control</title>
<style>
    * { box-sizing: border-box; }
    body { margin: 0; background: #121212; color: #00ff88; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; text-align: center; overflow-x: hidden; user-select: none; }
    h2 { background: #1f1f1f; color: #fff; margin: 0; padding: 14px 0; font-size: 22px; border-bottom: 2px solid #00ff88; letter-spacing: 1px; }
    .video-container { margin: 12px auto; width: 95%; max-width: 640px; border: 2px solid #00ff88; border-radius: 10px; overflow: hidden; background: #000; box-shadow: 0 0 15px rgba(0,255,136,0.3); }
    img { width: 100%; height: auto; display: block; }
    .controls { display: flex; flex-direction: column; align-items: center; margin-top: 10px; }
    .row { display: flex; justify-content: center; margin: 4px 0; }
    .btn { background: #222; color: #fff; border: 2px solid #444; width: 75px; height: 75px; margin: 6px; font-size: 22px; font-weight: bold; border-radius: 16px; cursor: pointer; outline: none; touch-action: manipulation; transition: 0.1s ease; box-shadow: 0 4px 8px rgba(0,0,0,0.5); }
    .btn:active { background: #00ff88; color: #000; border-color: #00ff88; transform: scale(0.92); }
    .btn-stop { background: #e53935; border-color: #c62828; width: 85px; }
    .btn-stop:active { background: #ff5252; color: white; border-color: #ff5252; }
    .status { margin-top: 12px; font-size: 14px; color: #aaa; font-weight: 500; }
</style>
</head>
<body>

<h2>🏎️ FPV CAR CONTROLLER</h2>

<div class="video-container">
    <img src="/stream" id="video-stream" alt="Live Camera Stream">
</div>

<div class="controls">
    <div class="row">
        <button class="btn" onmousedown="sendCmd('F')" onmouseup="sendCmd('S')" ontouchstart="sendCmd('F')" ontouchend="sendCmd('S')">▲<br><small style="font-size:12px">W</small></button>
    </div>
    <div class="row">
        <button class="btn" onmousedown="sendCmd('L')" onmouseup="sendCmd('S')" ontouchstart="sendCmd('L')" ontouchend="sendCmd('S')">◀<br><small style="font-size:12px">A</small></button>
        <button class="btn btn-stop" onclick="sendCmd('S')">STOP</button>
        <button class="btn" onmousedown="sendCmd('R')" onmouseup="sendCmd('S')" ontouchstart="sendCmd('R')" ontouchend="sendCmd('S')">▶<br><small style="font-size:12px">D</small></button>
    </div>
    <div class="row">
        <button class="btn" onmousedown="sendCmd('B')" onmouseup="sendCmd('S')" ontouchstart="sendCmd('B')" ontouchend="sendCmd('S')">▼<br><small style="font-size:12px">S</small></button>
    </div>
</div>

<div class="status" id="status-text">Status: Connected & Ready | Keyboard: W, A, S, D, Space</div>

<script>
    function sendCmd(cmd) {
        fetch('/control?cmd=' + cmd).catch(e => console.error(e));
        document.getElementById('status-text').innerText = "Command Sent: " + cmd;
    }

    let activeKey = null;
    document.addEventListener('keydown', function(event) {
        if (event.repeat) return;
        const key = event.key.toLowerCase();
        if (['w','a','s','d',' '].includes(key)) {
            activeKey = key;
            if (key === 'w') sendCmd('F');
            else if (key === 's') sendCmd('B');
            else if (key === 'a') sendCmd('L');
            else if (key === 'd') sendCmd('R');
            else if (key === ' ') sendCmd('S');
        }
    });

    document.addEventListener('keyup', function(event) {
        const key = event.key.toLowerCase();
        if (['w','a','s','d'].includes(key)) {
            sendCmd('S');
            activeKey = null;
        }
    });
</script>
</body>
</html>
)rawliteral";

//======================================================
// HTTP Handlers
//======================================================
esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, MAIN_PAGE, strlen(MAIN_PAGE));
}

esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char part_buf[64];
    
    res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
    if (res != ESP_OK) return res;

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            res = ESP_FAIL;
            break;
        }

        res = httpd_resp_send_chunk(req, "\r\n--frame\r\n", 14);
        if (res == ESP_OK) {
            size_t hlen = snprintf(part_buf, 64, "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        }
        esp_camera_fb_return(fb);
        
        if (res != ESP_OK) break;
    }
    return res;
}

esp_err_t control_handler(httpd_req_t *req) {
    char query[100];
    char cmd[16];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "cmd", cmd, sizeof(cmd)) == ESP_OK) {
            
            // Send command byte over Serial to Arduino UNO
            Serial.write(cmd[0]); 
            
            httpd_resp_set_type(req, "text/plain");
            httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
    }
    httpd_resp_send(req, "Invalid Command", HTTPD_RESP_USE_STRLEN);
    return ESP_FAIL;
}

//======================================================
// Start Web Server
//======================================================
void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_uri_t index_uri   = { .uri = "/",        .method = HTTP_GET, .handler = index_handler,   .user_ctx = NULL };
    httpd_uri_t stream_uri  = { .uri = "/stream",  .method = HTTP_GET, .handler = stream_handler,  .user_ctx = NULL };
    httpd_uri_t control_uri = { .uri = "/control", .method = HTTP_GET, .handler = control_handler, .user_ctx = NULL };

    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &stream_uri);
        httpd_register_uri_handler(camera_httpd, &control_uri);
    }
}

//======================================================
// Setup & Loop
//======================================================
void setup() {
    // Serial Baud rate must match Arduino UNO (115200)
    Serial.begin(115200);
    Serial.setDebugOutput(false); 

    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(FLASH_LED_PIN, LOW); // LED OFF initially

    // Initialize Camera Config
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
        config.frame_size   = FRAMESIZE_VGA;  // 640x480 resolution
        config.jpeg_quality = 10;           // High quality stream
        config.fb_count     = 2;            // Double buffer for high FPS
        config.fb_location  = CAMERA_FB_IN_PSRAM;
        config.grab_mode    = CAMERA_GRAB_LATEST;
    } else {
        config.frame_size   = FRAMESIZE_QVGA;
        config.jpeg_quality = 12;
        config.fb_count     = 1;
        config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return;
    }
    
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_vflip(s, 0); // Set to 1 if camera feed is upside down
    }

    // Connect to WiFi Network
    Serial.println("\nConnecting to WiFi...");
    WiFi.setSleep(false); // Disables WiFi sleep mode for ultra-low latency
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\n==========================================");
    Serial.println(" WiFi Connected Successfully!");
    Serial.print(" Open this URL in browser: http://");
    Serial.println(WiFi.localIP());
    Serial.println("==========================================\n");

    startCameraServer();
}

void loop() {
    delay(10);
}
