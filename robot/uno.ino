#define IN1 8
#define IN2 9
#define IN3 10
#define IN4 11

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

  if (cmd == lastAppliedCmd) {
    return;
  }

  lastAppliedCmd = cmd;

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
  // Left turn: left motor reverse, right motor forward.
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void right() {
  // Right turn: left motor forward, right motor reverse.
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}