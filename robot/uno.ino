#define IN1 5   // Left motor forward  (PWM)
#define IN2 6   // Left motor backward (PWM)
#define IN3 9   // Right motor forward (PWM)
#define IN4 10  // Right motor backward (PWM)

#define FULL_SPEED 255
#define TURN_SPEED 120
#define DEFAULT_FORWARD_SPEED 200
#define DEFAULT_BACKWARD_SPEED 200

char cmd = 'S';
char lastAppliedCmd = 'X';
int forwardSpeed = DEFAULT_FORWARD_SPEED;
int backwardSpeed = DEFAULT_BACKWARD_SPEED;
bool speedDirty = false;
char rxLine[32];
uint8_t rxLen = 0;

int clampSpeed(int value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return value;
}

void applySerialLine(char* line) {
  if (line[0] == '\0') {
    return;
  }

  if (line[1] == '\0') {
    char c = line[0];
    if (c == 'F' || c == 'B' || c == 'L' || c == 'R' || c == 'S') {
      cmd = c;
    }
    return;
  }

  if (strncmp(line, "FSP:", 4) == 0) {
    int value = clampSpeed(atoi(line + 4));
    if (value != forwardSpeed) {
      forwardSpeed = value;
      speedDirty = true;
    }
    return;
  }

  if (strncmp(line, "BSP:", 4) == 0) {
    int value = clampSpeed(atoi(line + 4));
    if (value != backwardSpeed) {
      backwardSpeed = value;
      speedDirty = true;
    }
    return;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  stopMotor();
}

void loop() {

  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (rxLen > 0) {
        rxLine[rxLen] = '\0';
        applySerialLine(rxLine);
        rxLen = 0;
      }
      continue;
    }

    if (rxLen < sizeof(rxLine) - 1) {
      rxLine[rxLen++] = c;
    } else {
      rxLen = 0;
    }
  }

  if (cmd == lastAppliedCmd && !speedDirty) {
    return;
  }

  lastAppliedCmd = cmd;
  speedDirty = false;

  if (cmd == 'F') {
    forward();
  } else if (cmd == 'B') {
    backward();
  } else if (cmd == 'L') {
    left();
  } else if (cmd == 'R') {
    right();
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
  leftForward(forwardSpeed);
  rightForward(forwardSpeed);
}

void backward() {
  leftBackward(backwardSpeed);
  rightBackward(backwardSpeed);
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