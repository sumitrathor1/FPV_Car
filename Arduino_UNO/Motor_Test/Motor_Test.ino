/*************************************************
 * FPV Car - Motor Connection & Direction Test
 * Arduino UNO Standalone Motor Test Code
 * 
 * Pin Configuration (L298N / Motor Driver):
 * IN1 -> Pin 5  (Left Motor Forward)
 * IN2 -> Pin 6  (Left Motor Backward)
 * IN3 -> Pin 9  (Right Motor Forward)
 * IN4 -> Pin 10 (Right Motor Backward)
 * 
 * Instructions:
 * 1. Upload this sketch to Arduino UNO.
 * 2. Open Serial Monitor in Arduino IDE (Baud Rate: 115200).
 * 3. Watch the automatic step-by-step test sequence.
 * 4. Or send commands manually via Serial Monitor:
 *    F = Forward | B = Backward | L = Left | R = Right | S = Stop
 *    1 = Test IN1 | 2 = Test IN2 | 3 = Test IN3 | 4 = Test IN4
 *    A = Run Auto-Test Sequence Again
 *************************************************/

#define IN1 5
#define IN2 6
#define IN3 9
#define IN4 10

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void forward() {
  stopMotors();
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void backward() {
  stopMotors();
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void left() {
  stopMotors();
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void right() {
  stopMotors();
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void runAutoTest() {
  Serial.println("\n==========================================");
  Serial.println("  STARTING AUTOMATIC MOTOR TEST SEQUENCE  ");
  Serial.println("==========================================");
  
  // 1. Test IN1 (Left Motor Forward)
  Serial.println("\n[STEP 1/8] Testing IN1 (Pin 5) - Left Motor FORWARD");
  stopMotors();
  digitalWrite(IN1, HIGH);
  delay(2000);
  stopMotors();
  delay(1000);

  // 2. Test IN2 (Left Motor Backward)
  Serial.println("[STEP 2/8] Testing IN2 (Pin 6) - Left Motor BACKWARD");
  stopMotors();
  digitalWrite(IN2, HIGH);
  delay(2000);
  stopMotors();
  delay(1000);

  // 3. Test IN3 (Right Motor Forward)
  Serial.println("[STEP 3/8] Testing IN3 (Pin 9) - Right Motor FORWARD");
  stopMotors();
  digitalWrite(IN3, HIGH);
  delay(2000);
  stopMotors();
  delay(1000);

  // 4. Test IN4 (Right Motor Backward)
  Serial.println("[STEP 4/8] Testing IN4 (Pin 10) - Right Motor BACKWARD");
  stopMotors();
  digitalWrite(IN4, HIGH);
  delay(2000);
  stopMotors();
  delay(1000);

  // 5. Test Full FORWARD
  Serial.println("[STEP 5/8] Testing BOTH MOTORS - FORWARD");
  forward();
  delay(2500);
  stopMotors();
  delay(1000);

  // 6. Test Full BACKWARD
  Serial.println("[STEP 6/8] Testing BOTH MOTORS - BACKWARD");
  backward();
  delay(2500);
  stopMotors();
  delay(1000);

  // 7. Test TURN LEFT
  Serial.println("[STEP 7/8] Testing TURN LEFT");
  left();
  delay(2500);
  stopMotors();
  delay(1000);

  // 8. Test TURN RIGHT
  Serial.println("[STEP 8/8] Testing TURN RIGHT");
  right();
  delay(2500);
  stopMotors();
  delay(1000);

  Serial.println("\n==========================================");
  Serial.println("  AUTO-TEST COMPLETED! ");
  Serial.println("  You can now type manual test commands: ");
  Serial.println("  F = Forward | B = Backward | L = Left");
  Serial.println("  R = Right   | S = Stop     | A = Re-run Auto Test");
  Serial.println("  1, 2, 3, 4 = Single Pin Test");
  Serial.println("==========================================\n");
}

void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  stopMotors();

  Serial.println("Arduino UNO Motor Tester Ready.");
  delay(1000);
  
  // Run Auto Test once on startup
  runAutoTest();
}

void loop() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();

    // Ignore newline / carriage return characters
    if (cmd == '\n' || cmd == '\r') return;

    Serial.print("Command Received: ");
    Serial.println(cmd);

    switch (cmd) {
      case 'F': case 'f':
        Serial.println(">> Running: FORWARD");
        forward();
        break;

      case 'B': case 'b':
        Serial.println(">> Running: BACKWARD");
        backward();
        break;

      case 'L': case 'l':
        Serial.println(">> Running: TURN LEFT");
        left();
        break;

      case 'R': case 'r':
        Serial.println(">> Running: TURN RIGHT");
        right();
        break;

      case 'S': case 's':
        Serial.println(">> Running: STOP");
        stopMotors();
        break;

      case '1':
        Serial.println(">> Testing Pin 5 (IN1) only for 2 seconds");
        stopMotors();
        digitalWrite(IN1, HIGH);
        delay(2000);
        stopMotors();
        break;

      case '2':
        Serial.println(">> Testing Pin 6 (IN2) only for 2 seconds");
        stopMotors();
        digitalWrite(IN2, HIGH);
        delay(2000);
        stopMotors();
        break;

      case '3':
        Serial.println(">> Testing Pin 9 (IN3) only for 2 seconds");
        stopMotors();
        digitalWrite(IN3, HIGH);
        delay(2000);
        stopMotors();
        break;

      case '4':
        Serial.println(">> Testing Pin 10 (IN4) only for 2 seconds");
        stopMotors();
        digitalWrite(IN4, HIGH);
        delay(2000);
        stopMotors();
        break;

      case 'A': case 'a':
        runAutoTest();
        break;

      default:
        Serial.println("Unknown command! Options: F, B, L, R, S, 1, 2, 3, 4, A");
        break;
    }
  }
}
