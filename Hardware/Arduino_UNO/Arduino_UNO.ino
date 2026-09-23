/*************************************************
 * FPV Car - Arduino UNO Code (Advanced Speed Control & Audio Feedback)
 * Receives commands ('F', 'B', 'L', 'R', 'S', 'H', 'h') and speed values ('FSP:xxx', 'BSP:xxx')
 * Controls L298N Motor Driver with PWM Speed Control
 * Buzzer on Pin 4: Horn + Smart Audio Feedback + 3-Sec Power-On Self-Test
 * Includes Safety Timeout (Auto-Stop if connection lost)
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
const unsigned long SAFETY_TIMEOUT = 2000; // 2 second auto-stop safety

// ======================================================
// Buzzer & Sound Helpers (Works with Active & Passive Buzzers)
// ======================================================
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

// 3-Second Power-On Self-Test (Buzzer + Motor Vibration)
void runBootSelfTest() {
  // Phase 1: Ascending Chimes
  buzzerBeep(120, 1000);
  delay(80);
  buzzerBeep(120, 1500);
  delay(80);
  buzzerBeep(180, 2200);
  delay(200);

  // Phase 2: Gentle Motor Micro-Pulse (Tests L298N and Battery Power)
  analogWrite(IN1, 130);
  analogWrite(IN3, 130);
  delay(70);
  stopMotor();
  delay(120);

  analogWrite(IN2, 130);
  analogWrite(IN4, 130);
  delay(70);
  stopMotor();
  delay(200);

  // Phase 3: Final Cheerful Double-Pip
  buzzerBeep(90, 2400);
  delay(60);
  buzzerBeep(140, 2800);
}

// Status Chimes
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
  analogWrite(IN1, forwardSpeed);
  digitalWrite(IN2, LOW);
  analogWrite(IN3, forwardSpeed);
  digitalWrite(IN4, LOW);
}

void backward() {
  digitalWrite(IN1, LOW);
  analogWrite(IN2, backwardSpeed);
  digitalWrite(IN3, LOW);
  analogWrite(IN4, backwardSpeed);
}

void left() {
  digitalWrite(IN1, LOW);
  analogWrite(IN2, backwardSpeed);
  analogWrite(IN3, forwardSpeed);
  digitalWrite(IN4, LOW);
}

void right() {
  analogWrite(IN1, forwardSpeed);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  analogWrite(IN4, backwardSpeed);
}

// ======================================================
// Setup
// ======================================================
void setup() {
  // Serial Baud rate must match ESP32-CAM (115200)
  Serial.begin(115200);
  Serial.setTimeout(50); // Fast non-blocking serial read

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  buzzerOff();

  stopMotor();
  lastCommandTime = millis();

  // Run hardware power-on self-test (approx 2.5-3.0 sec)
  runBootSelfTest();
}

// ======================================================
// Command Processor
// ======================================================
void processCommand(const String& input) {
  String text = input;
  text.trim();
  if (text.length() == 0) return;

  // 1. Check for Speed Configuration Updates (FSP:xxx or BSP:xxx)
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

  // 2. Check for Horn Commands ('H' = ON, 'h' = OFF)
  if (text.startsWith("H")) {
    buzzerOn(1900);
    return;
  }
  if (text.startsWith("h")) {
    buzzerOff();
    return;
  }

  // 3. Check for Status Feedback Commands
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

  // 4. Check for Single Character Direction Commands
  char cmd = text.charAt(0);
  if (cmd == 'F' || cmd == 'B' || cmd == 'L' || cmd == 'R' || cmd == 'S') {
    currentCommand = cmd;
    lastCommandTime = millis(); // Reset safety timer

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
  // Read line/character commands from ESP32-CAM over Serial
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    processCommand(input);
  }

  // Safety Feature: Auto STOP if no command received for > 2 seconds
  if (currentCommand != 'S' && (millis() - lastCommandTime > SAFETY_TIMEOUT)) {
    stopMotor();
    currentCommand = 'S';
  }
}