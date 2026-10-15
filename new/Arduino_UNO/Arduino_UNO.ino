/******************************************************************************
 * 🏎️ FPV Car - Arduino UNO Motor Driver & Sound Controller (Lightweight Edition)
 *
 * Receives serial commands from ESP32-CAM (Master) at 115200 baud:
 *   'F'        -> Drive Forward
 *   'B'        -> Drive Backward
 *   'L'        -> Turn Left (Spin Turn)
 *   'R'        -> Turn Right (Spin Turn)
 *   'S'        -> Stop All Motors
 *   'H' / 'h'  -> Horn Buzzer ON / OFF
 *   "FSP:xxx"  -> Set Forward Speed (0 - 255)
 *   "BSP:xxx"  -> Set Backward Speed (0 - 255)
 *   "Z:BOOT"   -> Boot Melody & Physical Motor Test
 *   "Z:AP"     -> Hotspot Ready Chime
 ******************************************************************************/

#define IN1 5
#define IN2 6
#define IN3 9
#define IN4 10
#define BUZZER_PIN 4

// Global Variables
char currentCommand = 'S';
int forwardSpeed = 255;
int backwardSpeed = 255;
unsigned long lastCommandTime = 0;
const unsigned long SAFETY_TIMEOUT_MS = 1500; // Auto-stop if signal lost for 1.5s

// ==========================================
// Buzzer & Sound Effects
// ==========================================
void buzzerTone(int freq, int durationMs = 0) {
  tone(BUZZER_PIN, freq);
  if (durationMs > 0) {
    delay(durationMs);
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void buzzerOff() {
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
}

void playBootSound() {
  buzzerTone(1200, 80);
  delay(60);
  buzzerTone(1800, 100);
  delay(60);
  buzzerTone(2400, 140);
  delay(150);

  // Quick motor kick (Forward 150ms, Backward 150ms) to test H-Bridge
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  delay(150);
  stopMotors();
  delay(80);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  delay(150);
  stopMotors();

  // Final confirmation pip
  buzzerTone(2800, 100);
}

void playApReadyChime() {
  for (int i = 0; i < 2; i++) {
    buzzerTone(2000, 90);
    delay(70);
    buzzerTone(2500, 130);
    delay(120);
  }
}

// ==========================================
// Motor Controls
// ==========================================
void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void driveForward() {
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

void driveBackward() {
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

void turnLeft() {
  analogWrite(IN1, 0);
  analogWrite(IN2, backwardSpeed);
  analogWrite(IN3, forwardSpeed);
  analogWrite(IN4, 0);
}

void turnRight() {
  analogWrite(IN1, forwardSpeed);
  analogWrite(IN2, 0);
  analogWrite(IN3, 0);
  analogWrite(IN4, backwardSpeed);
}

// ==========================================
// Setup
// ==========================================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(20);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  buzzerOff();
  stopMotors();

  lastCommandTime = millis();
}

// ==========================================
// Command Parser
// ==========================================
void parseCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd == "Z:BOOT") {
    playBootSound();
    return;
  }
  if (cmd == "Z:AP") {
    playApReadyChime();
    return;
  }
  if (cmd.startsWith("FSP:")) {
    int s = cmd.substring(4).toInt();
    forwardSpeed = constrain(s, 0, 255);
    return;
  }
  if (cmd.startsWith("BSP:")) {
    int s = cmd.substring(4).toInt();
    backwardSpeed = constrain(s, 0, 255);
    return;
  }
  if (cmd == "H" || cmd == "HORN:1") {
    tone(BUZZER_PIN, 1900);
    return;
  }
  if (cmd == "h" || cmd == "HORN:0") {
    buzzerOff();
    return;
  }

  char c = cmd.charAt(0);
  if (c == 'F' || c == 'B' || c == 'L' || c == 'R' || c == 'S') {
    currentCommand = c;
    lastCommandTime = millis();
    switch (currentCommand) {
      case 'F': driveForward();  break;
      case 'B': driveBackward(); break;
      case 'L': turnLeft();      break;
      case 'R': turnRight();     break;
      case 'S': stopMotors();    break;
    }
  }
}

// ==========================================
// Main Loop
// ==========================================
void loop() {
  // Read incoming serial commands from ESP32
  if (Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');
    parseCommand(line);
  }

  // Fail-safe auto-stop if no command received within timeout
  if (currentCommand != 'S' && (millis() - lastCommandTime > SAFETY_TIMEOUT_MS)) {
    stopMotors();
    currentCommand = 'S';
  }
}
