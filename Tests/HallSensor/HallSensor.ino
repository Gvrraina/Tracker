// 02b_dock_detect — onboard LED shows "strap docked"
#define HALL_PIN 34
#define LED_PIN  2

const int DOCK_ON   = 250;   // above this = docked
const int DOCK_OFF  = 120;   // below this = removed
const unsigned long HOLD_MS = 300;

int  baseline = 0;
bool docked = false;
unsigned long changeStart = 0;

int readHall() {
  long sum = 0;
  for (int i = 0; i < 20; i++) sum += analogRead(HALL_PIN);
  return sum / 20;
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  delay(500);
  long sum = 0;                                  // keep strap AWAY at power-on
  for (int i = 0; i < 50; i++) { sum += readHall(); delay(10); }
  baseline = sum / 50;
  Serial.print("Baseline: "); Serial.println(baseline);
}

void loop() {
  int strength = abs(readHall() - baseline);     // abs = either magnet side works
  bool wantDocked = docked ? (strength > DOCK_OFF) : (strength > DOCK_ON);

  if (wantDocked != docked) {
    if (changeStart == 0) changeStart = millis();
    if (millis() - changeStart > HOLD_MS) {
      docked = wantDocked;
      changeStart = 0;
      Serial.println(docked ? ">>> STRAP DOCKED" : "<<< STRAP REMOVED");
      digitalWrite(LED_PIN, docked ? HIGH : LOW);
    }
  } else {
    changeStart = 0;
  }

  Serial.print("strength: "); Serial.println(strength);
  delay(50);
}