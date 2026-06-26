#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

#define CAMERA_MODEL_AI_THINKER

// Camera Pins

//======================================================
// AI Thinker ESP32-CAM Pin Definition
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

// WiFi

//======================================================
// WiFi Credentials
//======================================================

const char* ssid     = "sumit";
const char* password = "12345678";

// Camera Init

//======================================================
// HTTP Server Handles
//======================================================

httpd_handle_t camera_httpd = NULL;
// httpd_handle_t stream_httpd = NULL;

//======================================================
// Camera Initialization
//======================================================

bool initCamera()
{
    camera_config_t config;

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;

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

    config.pin_pwdn  = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    if (psramFound())
    {
        Serial.println("PSRAM Found");

        config.frame_size   = FRAMESIZE_VGA;   // 640x480
        config.jpeg_quality = 12;
        config.fb_count     = 2;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode    = CAMERA_GRAB_LATEST;
    }
    else
    {
        Serial.println("PSRAM Not Found");

        config.frame_size   = FRAMESIZE_QVGA;  // 320x240
        config.jpeg_quality = 12;
        config.fb_count     = 1;
        config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
    }

    esp_err_t err = esp_camera_init(&config);

    if (err != ESP_OK)
    {
        Serial.printf("Camera Init Failed! Error: 0x%x\n", err);
        return false;
    }

    sensor_t *s = esp_camera_sensor_get();
    s->set_gainceiling(s, GAINCEILING_16X);

    s->set_framesize(s, FRAMESIZE_VGA);
    s->set_quality(s, 10);

    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);

    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);

    s->set_gain_ctrl(s, 1);
    s->set_exposure_ctrl(s, 1);

    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
    // s->set_vflip(s, 0);
    
    Serial.println("Camera Initialized Successfully");

    return true;
}

// Stream Handler

//======================================================
// MJPEG Stream Handler
//======================================================

static const char* STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=frame";

static const char* STREAM_BOUNDARY = "\r\n--frame\r\n";

static const char* STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;

    char part_buf[64];

    res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);

    if (res != ESP_OK)
        return res;

    while (true)
    {
        fb = esp_camera_fb_get();

        if (!fb)
        {
            Serial.println("Camera Capture Failed");
            return ESP_FAIL;
        }

        httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));

        size_t hlen = snprintf(
            part_buf,
            sizeof(part_buf),
            STREAM_PART,
            fb->len
        );

        httpd_resp_send_chunk(req, part_buf, hlen);

        httpd_resp_send_chunk(
            req,
            (const char*)fb->buf,
            fb->len
        );

        esp_camera_fb_return(fb);

        if (res != ESP_OK)
            break;
    }

    return res;
}

// Control Handler

//======================================================
// Control Handler
//======================================================

esp_err_t control_handler(httpd_req_t *req)
{
    char query[100];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        char cmd[16];

        if (httpd_query_key_value(query, "cmd", cmd, sizeof(cmd)) == ESP_OK)
        {
            Serial.print("Command : ");
            Serial.println(cmd);

            Serial.write(cmd[0]);

            // Future
            // Serial2.println(cmd);

            httpd_resp_set_type(req, "text/plain");
            httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

            return ESP_OK;
        }
    }

    httpd_resp_send(req, "Invalid Command", HTTPD_RESP_USE_STRLEN);

    return ESP_FAIL;
}


// Root Handler

//======================================================
// Root Page Handler
//======================================================

static const char MAIN_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>

<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">

<title>FPV Car</title>

<style>

body{
    margin:0;
    background:#111;
    color:white;
    font-family:Arial;
    text-align:center;
}

h1{
    padding:15px;
    margin:0;
    background:#222;
}

img{
    width:100%;
    max-width:900px;
    height:auto;
    margin-top:10px;
    border-radius:8px;
}

</style>

</head>

<body>

<h1>FPV Car Live Stream</h1>

<img src="/stream">

</body>
</html>
)rawliteral";


esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");

    return httpd_resp_send(
        req,
        MAIN_PAGE,
        strlen(MAIN_PAGE)
    );
}

// Camera Settings

//======================================================
// Start Camera Web Server
//======================================================

void startCameraServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.server_port = 80;

    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t control_uri = {
    .uri      = "/control",
    .method   = HTTP_GET,
    .handler  = control_handler,
    .user_ctx = NULL
};
httpd_uri_t settings_uri = {
    .uri      = "/settings",
    .method   = HTTP_GET,
    .handler  = settings_handler,
    .user_ctx = NULL
};

    if (httpd_start(&camera_httpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &control_uri);
        httpd_register_uri_handler(camera_httpd, &settings_uri);
        httpd_register_uri_handler(camera_httpd, &stream_uri);

        Serial.println("--------------------------------");
        Serial.println("Camera Server Started");
        Serial.println("--------------------------------");
    }
    else
    {
        Serial.println("Failed to Start HTTP Server");
    }
}

//======================================================
// Camera Settings Handler
//======================================================

esp_err_t settings_handler(httpd_req_t *req)
{
    char query[128];

    sensor_t *s = esp_camera_sensor_get();

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        char var[32];
        char val[32];

        if (httpd_query_key_value(query, "var", var, sizeof(var)) == ESP_OK &&
            httpd_query_key_value(query, "val", val, sizeof(val)) == ESP_OK)
        {
            int value = atoi(val);

            if (!strcmp(var, "brightness"))
                s->set_brightness(s, value);

            else if (!strcmp(var, "contrast"))
                s->set_contrast(s, value);

            else if (!strcmp(var, "quality"))
                s->set_quality(s, value);

            else if (!strcmp(var, "framesize"))
                s->set_framesize(s, (framesize_t)value);

            httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

            return ESP_OK;
        }
    }

    httpd_resp_send(req, "Invalid", HTTPD_RESP_USE_STRLEN);

    return ESP_FAIL;
}

//======================================================
// Setup
//======================================================

void setup()
{
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    Serial.println();
    Serial.println("=================================");
    Serial.println("       FPV CAR STARTING");
    Serial.println("=================================");

    // Initialize Camera
    if (!initCamera())
    {
        Serial.println("Camera Initialization Failed!");

        while (true)
        {
            delay(1000);
        }
    }

    // Connect WiFi
    Serial.print("Connecting to WiFi");

    WiFi.begin(ssid, password);
    WiFi.setSleep(false);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi Connected");

    Serial.print("IP Address : ");
    Serial.println(WiFi.localIP());

    // Start Web Server
    startCameraServer();

    Serial.println("System Ready");
}

//======================================================
// Loop
//======================================================

void loop()
{
    delay(10);
}

// setup()

// loop()