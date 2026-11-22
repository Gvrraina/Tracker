// 08a_button_finder — press each button and see which GPIO it's really on
// Skips the matrix pins (5, 18, 23), the hall sensor (34) and boot pins.

const int PINS[] = { 4, 13, 14, 16, 17, 19, 21, 22, 25, 26, 27, 32, 33 };
const int N = sizeof(PINS) / sizeof(PINS[0]);
bool last[N];

void setup() {
  Serial.begin(115200);
  delay(300);
  for (int i = 0; i < N; i++) {
    pinMode(PINS[i], INPUT_PULLUP);
    last[i] = digitalRead(PINS[i]);
  }
  Serial.println("Button finder ready — press each button");
  for (int i = 0; i < N; i++) {
    if (last[i] == LOW) {
      Serial.print("WARNING: GPIO "); Serial.print(PINS[i]);
      Serial.println(" is already LOW (button wired on the wrong legs, or pin tied to GND)");
    }
  }
}

void loop() {
  for (int i = 0; i < N; i++) {
    bool now = digitalRead(PINS[i]);
    if (now != last[i]) {
      Serial.print("GPIO "); Serial.print(PINS[i]);
      Serial.println(now == LOW ? "  PRESSED" : "  released");
      last[i] = now;
    }
  }
  delay(20);
}
