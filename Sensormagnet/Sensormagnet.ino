const int HALL = 34;
const int LED  = 14;
int base = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  long sum = 0;
  for (int i = 0; i < 64; i++) { sum += analogRead(HALL); delay(5); }
  base = sum / 64;                       // keep the magnet away during startup
  Serial.printf("Baseline: %d\n", base);
}

void loop() {
  int raw  = analogRead(HALL);
  int diff = abs(raw - base);
  digitalWrite(LED, diff > 300 ? HIGH : LOW);
  Serial.printf("raw %d   diff %d\n", raw, diff);
  delay(200);
}