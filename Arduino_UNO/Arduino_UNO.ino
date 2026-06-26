/*************************************************
 * FPV Car - Arduino UNO
 * Receives commands from ESP32 over Serial
 *************************************************/

//==============================
// Motor Pins (L298N)
//==============================

#define IN1 5
#define IN2 6
#define IN3 9
#define IN4 10

char command = 'S';

//==============================
// Motor Functions
//==============================

void forward()
{
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void backward()
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void left()
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void right()
{
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void stopMotor()
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

//==============================
// Setup
//==============================

void setup()
{
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  stopMotor();

  Serial.println("UNO Ready");
}

//==============================
// Loop
//==============================

void loop()
{
  while (Serial.available())
  {
    command = Serial.read();

    Serial.print("Received : ");
    Serial.println(command);

    switch (command)
    {
      case 'F':
        Serial.println("Forward");
        forward();
        break;

      case 'B':
        Serial.println("Backward");
        backward();
        break;

      case 'L':
        Serial.println("Left");
        left();
        break;

      case 'R':
        Serial.println("Right");
        right();
        break;

      case 'S':
        Serial.println("Stop");
        stopMotor();
        break;

      default:
        Serial.println("Unknown Command");
        stopMotor();
        break;
    }
  }
}