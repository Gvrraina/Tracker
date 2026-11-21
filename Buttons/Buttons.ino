const int AGAIN = 32, HISTORY = 33, TRIGGER = 25, LED = 14;

void setup() {
  Serial.begin(115200);
  pinMode(AGAIN, INPUT_PULLUP);
  pinMode(HISTORY, INPUT_PULLUP);
  pinMode(TRIGGER, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
}

void loop() {
  bool a = digitalRead(AGAIN)   == LOW;
  bool h = digitalRead(HISTORY) == LOW;
  bool t = digitalRead(TRIGGER) == LOW;
  digitalWrite(LED, (a || h || t) ? HIGH : LOW);
  if (a) Serial.println("AGAIN");
  if (h) Serial.println("HISTORY");
  if (t) Serial.println("TRIGGER");
  delay(150);
}