/*************************************************
 * FPV Car - Arduino UNO Firmware (Slave Driver)
 * Master ESP32 controls all Motors, Buzzer, and Startup Sounds
 * Receives:
 *   'F', 'B', 'L', 'R', 'S' -> Motor Directions
 *   'H', 'h'               -> Horn ON / OFF
 *   "FSP:xxx", "BSP:xxx"   -> Speed PWM (0-255)
 *   "Z:BOOT"               -> Trigger 3-Sec Power-On Self-Test (Buzzer + Motor Kick)
 *   "Z:WIFI_OK"            -> Wi-Fi Connected Chime
 *   "Z:AP_MODE"            -> AP Hotspot Chime
 *************************************************/

#define IN1 5
#define IN2 6
#define IN3 9
#define IN4 10

#define BUZZER_PIN 4

char currentCommand = 'S';
int forwardSpeed = 255;
int backwardSpeed = 255;
bool hornActive = false;

unsigned long lastCommandTime = 0;
const unsigned long SAFETY_TIMEOUT = 2000; // 2-sec auto-stop safety

void buzzerOn(int freq = 1800) {
  tone(BUZZER_PIN, freq);
  digitalWrite(BUZZER_PIN, HIGH);
  hornActive = true;
}

void buzzerOff() {
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
  hornActive = false;
}

void buzzerBeep(int durationMs, int freq = 2000) {
  buzzerOn(freq);
  delay(durationMs);
  buzzerOff();
}

// Boot Self-Test: Ascending Melody + Physical Motor Kick
// Triggered ONLY by ESP32 via "Z:BOOT" command
void runBootSelfTest() {
  // Phase 1: Ascending Chimes
  buzzerBeep(100, 1000);
  delay(70);
  buzzerBeep(100, 1500);
  delay(70);
  buzzerBeep(150, 2200);
  delay(180);

  // Phase 2: Strong Physical Motor Kick (Full 255 Power forward & reverse)
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  delay(180);
  stopMotor();
  delay(120);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  delay(180);
  stopMotor();
  delay(180);

  // Phase 3: Final Cheerful Pip
  buzzerBeep(80, 2400);
  delay(50);
  buzzerBeep(120, 2800);
}

void playWifiOkChime() {
  buzzerBeep(100, 2000);
  delay(60);
  buzzerBeep(160, 2600);
}

void playApModeChime() {
  for (int i = 0; i < 3; i++) {
    buzzerBeep(120, 1200);
    delay(100);
  }
}

void playClientConnectChime() {
  buzzerBeep(80, 1800);
  delay(50);
  buzzerBeep(130, 2400);
}

void playClientDisconnectChime() {
  buzzerBeep(140, 1600);
  delay(70);
  buzzerBeep(200, 1000);
}

// ======================================================
// Motor Movement Functions
// ======================================================
void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void forward() {
  if (forwardSpeed >= 250) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  } else {
    analogWrite(IN1, forwardSpeed);
    digitalWrite(IN2, LOW);
    analogWrite(IN3, forwardSpeed);
    digitalWrite(IN4, LOW);
  }
}

void backward() {
  if (backwardSpeed >= 250) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  } else {
    digitalWrite(IN1, LOW);
    analogWrite(IN2, backwardSpeed);
    digitalWrite(IN3, LOW);
    analogWrite(IN4, backwardSpeed);
  }
}

void left() {
  if (backwardSpeed >= 250 && forwardSpeed >= 250) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  } else {
    digitalWrite(IN1, LOW);
    analogWrite(IN2, backwardSpeed);
    analogWrite(IN3, forwardSpeed);
    digitalWrite(IN4, LOW);
  }
}

void right() {
  if (backwardSpeed >= 250 && forwardSpeed >= 250) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  } else {
    analogWrite(IN1, forwardSpeed);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    analogWrite(IN4, backwardSpeed);
  }
}

// ======================================================
// Setup
// ======================================================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(30);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  buzzerOff();
  stopMotor();

  lastCommandTime = millis();
  // Arduino waits for ESP32 Master to issue commands!
}

// ======================================================
// Command Processor
// ======================================================
void processCommand(const String& input) {
  String text = input;
  text.trim();
  if (text.length() == 0) return;

  if (text.startsWith("FSP:")) {
    int val = text.substring(4).toInt();
    if (val >= 0 && val <= 255) forwardSpeed = val;
    return;
  }
  if (text.startsWith("BSP:")) {
    int val = text.substring(4).toInt();
    if (val >= 0 && val <= 255) backwardSpeed = val;
    return;
  }
  if (text.startsWith("H")) {
    buzzerOn(1900);
    return;
  }
  if (text.startsWith("h")) {
    buzzerOff();
    return;
  }
  if (text.startsWith("Z:BOOT")) {
    runBootSelfTest();
    return;
  }
  if (text.startsWith("Z:WIFI_OK")) {
    playWifiOkChime();
    return;
  }
  if (text.startsWith("Z:AP_MODE")) {
    playApModeChime();
    return;
  }
  if (text.startsWith("Z:CLIENT_ON")) {
    playClientConnectChime();
    return;
  }
  if (text.startsWith("Z:CLIENT_OFF")) {
    playClientDisconnectChime();
    return;
  }

  char cmd = text.charAt(0);
  if (cmd == 'F' || cmd == 'B' || cmd == 'L' || cmd == 'R' || cmd == 'S') {
    currentCommand = cmd;
    lastCommandTime = millis();
    switch (currentCommand) {
      case 'F': forward(); break;
      case 'B': backward(); break;
      case 'L': left(); break;
      case 'R': right(); break;
      case 'S': stopMotor(); break;
    }
  }
}

// ======================================================
// Main Loop
// ======================================================
void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    processCommand(input);
  }

  if (currentCommand != 'S' && (millis() - lastCommandTime > SAFETY_TIMEOUT)) {
    stopMotor();
    currentCommand = 'S';
  }
}