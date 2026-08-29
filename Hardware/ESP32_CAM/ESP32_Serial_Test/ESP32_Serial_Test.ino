/*************************************************
 * ESP32-CAM to Arduino UNO Direct Serial Test Code
 * NO WiFi required! NO Cloud Server required!
 * 
 * What it does:
 * Every 3 seconds, ESP32-CAM sends a command ('F', 'B', 'L', 'R', 'S')
 * over Serial (TX Pin 1) to Arduino UNO (RX Pin 0).
 * 
 * Connections:
 * ESP32-CAM TX (GPIO 1) -> Arduino UNO RX (Pin 0)
 * ESP32-CAM GND         -> Arduino UNO GND (Common Ground)
 *************************************************/

void setup() {
  // Start Serial at 115200 baud (Must match Arduino UNO baud rate)
  Serial.begin(115200);
  delay(1000);
}

void loop() {
  // 1. FORWARD Test
  Serial.write('F');
  delay(3000);

  // 2. STOP Test
  Serial.write('S');
  delay(1000);

  // 3. BACKWARD Test
  Serial.write('B');
  delay(3000);

  // 4. STOP Test
  Serial.write('S');
  delay(1000);

  // 5. LEFT Test
  Serial.write('L');
  delay(3000);

  // 6. STOP Test
  Serial.write('S');
  delay(1000);

  // 7. RIGHT Test
  Serial.write('R');
  delay(3000);

  // 8. STOP Test
  Serial.write('S');
  delay(2000);
}
