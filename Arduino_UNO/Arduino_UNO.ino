/*************************************************
 * FPV Car - Arduino UNO Code (Advanced Speed Control)
 * Receives commands ('F', 'B', 'L', 'R', 'S') and speed values ('FSP:xxx', 'BSP:xxx')
 * Drives L298N Motor Driver with PWM Speed Control
 * Includes Safety Timeout (Auto-Stop if connection lost)
 *************************************************/

#define IN1 5
#define IN2 6
#define IN3 9
#define IN4 10

char currentCommand = 'S';
int forwardSpeed = 255;
int backwardSpeed = 255;

unsigned long lastCommandTime = 0;
const unsigned long SAFETY_TIMEOUT = 1000; // 1 second auto-stop safety

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

void setup() {
  // Serial Baud rate must match ESP32-CAM (115200)
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  stopMotor();
  lastCommandTime = millis();
}

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

  // 2. Check for Single Character Direction Commands
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

void loop() {
  // Read line/character commands from ESP32-CAM over Serial
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    processCommand(input);
  }

  // Safety Feature: Auto STOP if no command received for > 1 second
  if (currentCommand != 'S' && (millis() - lastCommandTime > SAFETY_TIMEOUT)) {
    stopMotor();
    currentCommand = 'S';
  }
}