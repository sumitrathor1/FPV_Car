/*************************************************
 * FPV Car - Arduino UNO Code
 * Receives commands ('F', 'B', 'L', 'R', 'S') from ESP32 over Serial
 * Drives L298N Motor Driver
 * Includes Safety Timeout (Auto-Stop if connection lost)
 *************************************************/

// L298N Motor Driver Pins
#define IN1 5
#define IN2 6
#define IN3 9
#define IN4 10

char currentCommand = 'S';
unsigned long lastCommandTime = 0;
const unsigned long SAFETY_TIMEOUT = 1000; // 1 second auto-stop safety

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void forward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void backward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void left() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void right() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
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

void loop() {
  // Read incoming command from ESP32-CAM
  if (Serial.available() > 0) {
    char cmd = Serial.read();

    // Ignore newlines or whitespace
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

  // Safety Feature: If moving and no command received for > 1 second, auto STOP!
  if (currentCommand != 'S' && (millis() - lastCommandTime > SAFETY_TIMEOUT)) {
    stopMotor();
    currentCommand = 'S';
  }
}