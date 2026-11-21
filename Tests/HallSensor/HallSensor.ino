// 02_hall_test — find how the sensor reacts to our magnet
#define HALL_PIN 34

int baseline = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Keep the magnet AWAY. Measuring baseline...");
  long sum = 0;
  for (int i = 0; i < 100; i++) { sum += analogRead(HALL_PIN); delay(10); }
  baseline = sum / 100;
  Serial.print("Baseline: "); Serial.println(baseline);
  Serial.println("Now bring the magnet closer slowly.");
}

void loop() {
  long sum = 0;
  for (int i = 0; i < 20; i++) sum += analogRead(HALL_PIN);   // smooth out noise
  int raw = sum / 20;
  int diff = raw - baseline;

  Serial.print("raw: ");    Serial.print(raw);
  Serial.print("   diff: "); Serial.println(diff);
  delay(150);
}