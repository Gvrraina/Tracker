// 01_board_check — The Great Quantifier
#define LED_PIN 2   // onboard blue LED on most ESP32 dev boards

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Great Quantifier: board OK");
}

void loop() {
  digitalWrite(LED_PIN, HIGH); Serial.println("ON");
  delay(500);
  digitalWrite(LED_PIN, LOW);  Serial.println("OFF");
  delay(500);
}