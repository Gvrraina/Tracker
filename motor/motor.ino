const int MOTOR = 26, LED = 14;

void setup() {
  pinMode(MOTOR, OUTPUT);
  pinMode(LED, OUTPUT);
}

void loop() {
  digitalWrite(MOTOR, HIGH); digitalWrite(LED, HIGH);
  delay(150);
  digitalWrite(MOTOR, LOW);  digitalWrite(LED, LOW);
  delay(2000);
}