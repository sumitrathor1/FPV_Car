#define IN1 5   // Left motor forward  (PWM)
#define IN2 6   // Left motor backward (PWM)
#define IN3 9   // Right motor forward (PWM)
#define IN4 10  // Right motor backward (PWM)

#define FULL_SPEED 255
#define TURN_SPEED 120

char cmd = 'S';
char lastAppliedCmd = 'X';

void setup() {
  Serial.begin(9600);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  stopMotor();
}

void loop() {

  while (Serial.available()) {

    char c = Serial.read();

    if (c == '\n' || c == '\r') continue;

    if (c == 'F' || c == 'B' || c == 'L' || c == 'R' || c == 'S') {
      cmd = c;
    }
  }

  if (cmd == 'L') {
    left();
    lastAppliedCmd = cmd;
    return;
  }

  if (cmd == 'R') {
    right();
    lastAppliedCmd = cmd;
    return;
  }

  if (cmd == lastAppliedCmd) {
    return;
  }

  lastAppliedCmd = cmd;

  if (cmd == 'F') {
    forward();
  } else if (cmd == 'B') {
    backward();
  } else {
    stopMotor();
  }
}


// 🔥 MOTOR FUNCTIONS

void leftForward(uint8_t speed) {
  analogWrite(IN1, speed);
  digitalWrite(IN2, LOW);
}

void leftBackward(uint8_t speed) {
  digitalWrite(IN1, LOW);
  analogWrite(IN2, speed);
}

void rightForward(uint8_t speed) {
  analogWrite(IN3, speed);
  digitalWrite(IN4, LOW);
}

void rightBackward(uint8_t speed) {
  digitalWrite(IN3, LOW);
  analogWrite(IN4, speed);
}

void forward() {
  leftForward(FULL_SPEED);
  rightForward(FULL_SPEED);
}

void backward() {
  leftBackward(FULL_SPEED);
  rightBackward(FULL_SPEED);
}

void left() {
  // Slow left turn: left motor reverse, right motor forward.
  leftBackward(TURN_SPEED);
  rightForward(TURN_SPEED);
}

void right() {
  // Slow right turn: left motor forward, right motor reverse.
  leftForward(TURN_SPEED);
  rightBackward(TURN_SPEED);
}

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}