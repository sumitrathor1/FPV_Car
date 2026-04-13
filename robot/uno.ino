#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "sumit";
const char* password = "12345678";

const char* url = "http://20.244.113.234/robot/get.php";

char lastCmd = 'X';  // store last command
const uint32_t CONTROL_INTERVAL_MS = 35;
uint32_t lastControlAt = 0;

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

void setup() {
  Serial.begin(9600);

  Serial.println("START");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nCONNECTED");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    delay(50);
    return;
  }

  uint32_t now = millis();
  if (now - lastControlAt < CONTROL_INTERVAL_MS) {
    delay(5);
    return;
  }

  lastControlAt = now;

  HTTPClient http;
  http.begin(url);
  http.setTimeout(120);

  int code = http.GET();
  if (code == 200) {
    String res = http.getString();
    res.trim();

    char cmd = readJsonCommand(res);
    if (cmd != lastCmd) {
      Serial.println(cmd);
      lastCmd = cmd;
    }
  }

  http.end();
  delay(5);
}