// 13_gestures_v2 — The Great Quantifier (hand gestures, chosen number of metrics)
//
// POWER-ON SETUP (strap AWAY from the ledge while powering on)
//   Matrix shows 4 dots: big dots = how many metrics to track this week.
//   Swipe left -> right   -> one more (1 -> 2 -> 3 -> 4 -> 1)
//   Hover 3 s (hold still) -> confirm. A line fills along the bottom while you hold;
//                             take your hand away early to cancel.
//   The mirror then SECRETLY picks which metrics and shuffles them across the 7 days.
//
// DURING THE WEEK
//   Strap docks                     -> wake line -> pause -> score (+ arrow) fades in [look 1]
//   Hover over either sensor ~2.5 s -> Again: score returns dimmer                     [look 2, 3]
//   Swipe left -> right             -> next day, looks reset
//   Swipe right -> left             -> week line graph (Day 7 only), "- -" before then
//   Swipe left -> right on Day 7    -> week ends, back to choosing the number of metrics
//
// Arrow = this metric vs the last day the SAME metric was shown (no arrow on its first show).
// Graph = one "body improving" line: average of the chosen metrics each day.
//
// Backup for the team: Again button (32) tap/hold, hidden trigger (25) = next day.
// Serial (115200): n / p = next / previous day, a = again, g = graph, 1-4 = choose count at setup.

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// ================= WEEK DATA (hard-coded for demo) =================
#define NUM_METRICS 4
const char* METRIC_NAMES[NUM_METRICS] = { "activity", "sleep", "mindfulness", "hydration" };
const int SCORES[NUM_METRICS][7] = {
  { 54, 61, 52, 66, 59, 70, 64 },   // activity
  { 70, 62, 68, 58, 66, 73, 67 },   // sleep
  { 38, 31, 44, 40, 47, 39, 52 },   // mindfulness
  { 62, 68, 57, 65, 60, 71, 63 }    // hydration
};

// ================= LOOKS (the 3 chances) =================
#define MAX_LOOKS 3
const int lookIntensity[MAX_LOOKS] = { 15, 8, 4 };   // matrix brightness 0-15
#define SHOW_MS   5000
#define FADE_MS   600

// ================= GESTURES =================
#define HAND_CM          20      // closer than this = hand. Further = person standing, ignored
#define SWIPE_MS         700     // max time between the two sensors for a swipe
#define AGAIN_HOVER_MS   2500    // hover this long = Again
#define CONFIRM_HOVER_MS 3000    // hover this long = confirm setup
#define BUTTON_HOLD_MS   3000    // backup Again button: hold = graph
#define DROPOUT_MS       150     // ignore brief missed readings while the hand is there
#define COOLDOWN_MS      700     // ignore gestures right after one is used

// ================= TIMING / LOOK =================
#define USE_WAKE_LINE    true
#define WAKE_MS          250
#define WAKE_HOLD_MS     80
#define ANTICIPATION_MS  300
#define GRAPH_SHOW_MS    6000
#define GRAPH_INTENSITY  8
#define SETUP_INTENSITY  6
#define GRAPH_DOT_MS     20
#define GRAPH_SEG_MS     60
#define GRAPH_RELATIVE   true
#define FLIP_VERTICAL    false
#define FLIP_HORIZONTAL  false

const int DOCK_ON  = 250;
const int DOCK_OFF = 120;
const unsigned long HOLD_MS = 150;

// ================= PINS =================
#define HALL_PIN     34
#define TRIG_L       26
#define ECHO_L       27          // via level board Chan 1 (TXO -> TXI)
#define TRIG_R       14
#define ECHO_R       33          // via level board Chan 2 (TXO -> TXI)
#define AGAIN_PIN    32          // backup button -> GND
#define TRIGGER_PIN  25          // backup hidden next-day button -> GND
#define CS_PIN       5           // matrix: DIN = 23, CLK = 18, VCC = VIN (5V)
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES  4

// ================= INTERNALS =================
MD_Parola   matrix = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
MD_MAX72XX* mx;

uint8_t arrowUp[]   = { 5, 0x04, 0x02, 0x7F, 0x02, 0x04 };
uint8_t arrowDown[] = { 5, 0x10, 0x20, 0x7F, 0x20, 0x10 };

int  chosenCount = 0;
int  chosen[NUM_METRICS];
int  plan[7];                 // which metric is shown each day
int  today = 0;
int  looksUsed = 0;
int  baseline = 0;
bool docked = false;
unsigned long changeStart = 0;
char textBuf[16];

enum Gesture { G_NONE, G_SWIPE_LR, G_SWIPE_RL, G_HOVER };
unsigned long hoverMs = CONFIRM_HOVER_MS;   // setup uses CONFIRM, the week uses AGAIN

// ---------- sensors ----------
int readHall() {
  long sum = 0;
  for (int i = 0; i < 20; i++) sum += analogRead(HALL_PIN);
  return sum / 20;
}

long readCm(int trig, int echo) {
  digitalWrite(trig, LOW);  delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long us = pulseIn(echo, HIGH, 4000);       // only care about ~60 cm or closer
  return us == 0 ? -1 : us / 58;
}

// ---------- gesture detection ----------
unsigned long lLastSeen = 0, rLastSeen = 0, lStart = 0, rStart = 0;
bool lNear = false, rNear = false;
bool handPresent = false, handConsumed = false;
unsigned long handStart = 0, cooldownUntil = 0;

void resetGestures() {
  lLastSeen = rLastSeen = lStart = rStart = 0;
  lNear = rNear = false;
  handPresent = false;
  handConsumed = true;                       // a hand still there after an action is ignored
  cooldownUntil = millis() + COOLDOWN_MS;
}

Gesture pollGesture() {
  long l = readCm(TRIG_L, ECHO_L); delay(8);
  long r = readCm(TRIG_R, ECHO_R); delay(8);
  unsigned long now = millis();

  if (l > 0 && l < HAND_CM) lLastSeen = now;
  if (r > 0 && r < HAND_CM) rLastSeen = now;
  bool lNow = lLastSeen && (now - lLastSeen < DROPOUT_MS);
  bool rNow = rLastSeen && (now - rLastSeen < DROPOUT_MS);
  bool lRise = lNow && !lNear, rRise = rNow && !rNear;
  if (lRise) lStart = now;
  if (rRise) rStart = now;
  lNear = lNow; rNear = rNow;

  bool hand = lNear || rNear;
  if (hand && !handPresent) { handStart = now; handConsumed = false; }

  if (now < cooldownUntil) {                 // still settling after an action
    if (hand) handConsumed = true;
    handPresent = hand;
    return G_NONE;
  }

  // Swipes: one sensor, then the other, quickly
  if (rRise && lStart && rStart > lStart && rStart - lStart <= SWIPE_MS && !handConsumed) {
    handConsumed = true; handPresent = hand; cooldownUntil = now + COOLDOWN_MS;
    return G_SWIPE_LR;                       // left sensor first, then right
  }
  if (lRise && rStart && lStart > rStart && lStart - rStart <= SWIPE_MS && !handConsumed) {
    handConsumed = true; handPresent = hand; cooldownUntil = now + COOLDOWN_MS;
    return G_SWIPE_RL;                       // right sensor first, then left
  }

  // Hover: hand held still in front of either sensor
  if (hand && !handConsumed && now - handStart >= hoverMs) {
    handConsumed = true; handPresent = hand;
    return G_HOVER;
  }

  handPresent = hand;
  return G_NONE;
}

// 0.0 - 1.0 progress of the current hover (for the setup progress line)
float hoverProgress() {
  if (!handPresent || handConsumed || millis() < cooldownUntil) return 0;
  return min(1.0f, (float)(millis() - handStart) / hoverMs);
}

// ---------- backup buttons ----------
bool trigWasDown = false;  unsigned long trigLastChange = 0;
bool againWasDown = false, againHoldFired = false;  unsigned long againDownAt = 0;

bool triggerPressed() {
  bool down = digitalRead(TRIGGER_PIN) == LOW;
  if (down != trigWasDown && millis() - trigLastChange > 50) {
    trigLastChange = millis();
    trigWasDown = down;
    return down;
  }
  return false;
}

// Backup button: tap = Again (G_HOVER), hold 3 s = graph (G_SWIPE_RL)
Gesture againButton() {
  bool down = digitalRead(AGAIN_PIN) == LOW;
  unsigned long now = millis();
  Gesture g = G_NONE;
  if (down && !againWasDown) { againDownAt = now; againHoldFired = false; }
  if (down && !againHoldFired && now - againDownAt >= BUTTON_HOLD_MS) { againHoldFired = true; g = G_SWIPE_RL; }
  if (!down && againWasDown && !againHoldFired && now - againDownAt > 50) g = G_HOVER;
  againWasDown = down;
  return g;
}

// ---------- matrix helpers ----------
void displayOn(bool on) {
  mx->control(MD_MAX72XX::SHUTDOWN, on ? MD_MAX72XX::OFF : MD_MAX72XX::ON);
}

void fadeIntensity(int from, int to) {
  int steps = abs(to - from);
  if (steps == 0) { matrix.setIntensity(to); delay(FADE_MS); return; }
  int dir = to > from ? 1 : -1;
  for (int b = from; b != to + dir; b += dir) { matrix.setIntensity(b); delay(FADE_MS / (steps + 1)); }
}

void plotState(int x, int y, bool on) {      // x: 0 = left, y: 0 = bottom
  int cols = mx->getColumnCount();
  int row = FLIP_VERTICAL ? y : (7 - y);
  int col = FLIP_HORIZONTAL ? x : (cols - 1 - x);
  mx->setPoint(row, col, on);
}
void plot(int x, int y) { plotState(x, y, true); }

int lastBar = -1;
void drawProgress(float p) {                 // line along the bottom row
  int cols = mx->getColumnCount();
  int len = p * cols;
  if (len == lastBar) return;
  for (int x = 0; x < cols; x++) plotState(x, 0, x < len);
  lastBar = len;
}

// ---------- setup: choose how many metrics ----------
void drawDots(int count) {
  mx->clear();
  matrix.setIntensity(SETUP_INTENSITY);
  for (int i = 0; i < NUM_METRICS; i++) {
    int x = 3 + i * 8;                       // dots at x = 3, 11, 19, 27
    if (i < count) { plot(x, 3); plot(x + 1, 3); plot(x, 4); plot(x + 1, 4); }  // big dot
    else           { plot(x, 3); }                                             // small dot
  }
}

void pickMetrics(int count) {
  int pool[NUM_METRICS] = { 0, 1, 2, 3 };
  for (int i = NUM_METRICS - 1; i > 0; i--) { int j = esp_random() % (i + 1); int t = pool[i]; pool[i] = pool[j]; pool[j] = t; }
  for (int i = 0; i < count; i++) chosen[i] = pool[i];

  for (int d = 0; d < 7; d++) plan[d] = (d < count) ? chosen[d] : chosen[esp_random() % count];
  for (int i = 6; i > 0; i--) { int j = esp_random() % (i + 1); int t = plan[i]; plan[i] = plan[j]; plan[j] = t; }

  Serial.println("---- This week's secret plan (team only) ----");
  for (int d = 0; d < 7; d++) {
    Serial.print("Day "); Serial.print(d + 1); Serial.print(": ");
    Serial.print(METRIC_NAMES[plan[d]]); Serial.print("  "); Serial.println(SCORES[plan[d]][d]);
  }
}

void runSetup() {
  int count = 1;
  hoverMs = CONFIRM_HOVER_MS;
  drawDots(count);
  lastBar = -1;
  Serial.println("SETUP: swipe left->right = more metrics, hover 3 s = confirm (or type 1-4)");

  while (true) {
    Gesture g = pollGesture();
    Gesture b = againButton();
    if (triggerPressed()) g = G_SWIPE_LR;                // backup: trigger = +1
    if (b != G_NONE) g = G_HOVER;                        // backup: Again = confirm

    if (Serial.available()) {
      int c = Serial.read() - '0';
      if (c >= 1 && c <= NUM_METRICS) { count = c; drawDots(count); g = G_HOVER; }
    }

    if (g == G_SWIPE_LR) {
      count = count % NUM_METRICS + 1;
      drawDots(count);
      lastBar = -1;
      Serial.print("Metrics: "); Serial.println(count);
    }
    if (g == G_HOVER) { drawProgress(1.0); break; }

    drawProgress(hoverProgress());                       // fills while you hold
  }

  chosenCount = count;
  hoverMs = AGAIN_HOVER_MS;                              // from now on, hover = Again
  for (int i = 0; i < 2; i++) { displayOn(false); delay(150); displayOn(true); delay(150); }  // confirm blink
  delay(300);
  mx->clear();
  Serial.print("Confirmed: "); Serial.print(chosenCount); Serial.println(" metric(s)");
  pickMetrics(chosenCount);
  resetGestures();
}

// ---------- score text ----------
const char* todayText() {
  int m = plan[today];
  int value = SCORES[m][today];
  int lastDay = -1;
  for (int d = today - 1; d >= 0; d--) if (plan[d] == m) { lastDay = d; break; }
  int diff = (lastDay < 0) ? 0 : value - SCORES[m][lastDay];
  const char* arrow = diff > 0 ? " ^" : (diff < 0 ? " ~" : "");
  snprintf(textBuf, sizeof(textBuf), "%d%s", value, arrow);
  return textBuf;
}

void printToday() {
  Serial.print("Day "); Serial.print(today + 1);
  Serial.print(" ("); Serial.print(METRIC_NAMES[plan[today]]);
  Serial.print(")  showing: "); Serial.println(todayText());
}

// ---------- the 3 looks ----------
void showLook() {
  if (looksUsed >= MAX_LOOKS) { Serial.println("No looks left today"); return; }
  int b = lookIntensity[looksUsed];
  Serial.print("Look "); Serial.print(looksUsed + 1); Serial.print("/"); Serial.print(MAX_LOOKS);
  Serial.print("  "); printToday();

  displayOn(false);
  matrix.setIntensity(0);
  matrix.print(todayText());
  displayOn(true);
  fadeIntensity(0, b);
  delay(SHOW_MS);
  fadeIntensity(b, 0);
  displayOn(false);
  matrix.displayClear();
  displayOn(true);

  looksUsed++;
  if (looksUsed >= MAX_LOOKS) Serial.println("That was the last look until tomorrow");
  resetGestures();
}

void wakeLine() {
  if (!USE_WAKE_LINE) { delay(ANTICIPATION_MS); return; }
  int cols = mx->getColumnCount(), mid = cols / 2;
  mx->clear();
  matrix.setIntensity(2);
  for (int i = 0; i < mid; i++) {
    mx->setPoint(4, mid + i, true);
    mx->setPoint(4, mid - 1 - i, true);
    delay(WAKE_MS / mid);
  }
  delay(WAKE_HOLD_MS);
  mx->clear();
  delay(ANTICIPATION_MS);
}

// ---------- graph: one "body improving" line ----------
int bodyScore(int d) {
  int sum = 0;
  for (int i = 0; i < chosenCount; i++) sum += SCORES[chosen[i]][d];
  return sum / chosenCount;
}

int dayRow(int d) {
  int lo = 1, hi = 100;
  if (GRAPH_RELATIVE) {
    lo = 100; hi = 1;
    for (int i = 0; i < 7; i++) { lo = min(lo, bodyScore(i)); hi = max(hi, bodyScore(i)); }
    if (hi == lo) return 3;
  }
  return constrain((bodyScore(d) - lo) * 7 / (hi - lo), 0, 7);
}

void drawLine(int x0, int y0, int x1, int y1) {   // Bresenham
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    plot(x0, y0);
    delay(GRAPH_DOT_MS);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void showGraph() {
  Serial.println("Showing week graph (body improving line)");
  mx->clear();
  matrix.setIntensity(GRAPH_INTENSITY);
  int cols = mx->getColumnCount();
  int px = 0, py = dayRow(0);
  for (int d = 1; d < 7; d++) {
    int x = d * (cols - 1) / 6, y = dayRow(d);
    drawLine(px, py, x, y);
    px = x; py = y;
    delay(GRAPH_SEG_MS);
  }
  delay(GRAPH_SHOW_MS);
  fadeIntensity(GRAPH_INTENSITY, 0);
  displayOn(false);
  mx->clear();
  matrix.displayClear();
  displayOn(true);
  resetGestures();
}

void showLocked() {
  Serial.println("Graph locked until Day 7");
  matrix.setIntensity(SETUP_INTENSITY);
  matrix.print("- -");
  delay(1000);
  matrix.displayClear();
  resetGestures();
}

// ---------- events ----------
void nextDay(int step) {
  if (step > 0 && today == 6) {              // week over -> choose metrics again
    Serial.println("==== Week complete — new week, choose metrics ====");
    matrix.displayClear();
    today = 0;
    looksUsed = 0;
    runSetup();
    Serial.print("Now "); printToday();
    if (docked) { wakeLine(); showLook(); }
    return;
  }
  today = constrain(today + step, 0, 6);
  looksUsed = 0;
  Serial.print("Now "); printToday();
  if (docked) showLook();
}

void onDock() {
  Serial.print(">>> DOCKED — "); printToday();
  if (looksUsed >= MAX_LOOKS) { Serial.println("No looks left today"); return; }
  wakeLine();
  showLook();
}

void onUndock() {
  Serial.println("<<< REMOVED");
  matrix.displayClear();
}

void handleAction(Gesture g) {
  switch (g) {
    case G_SWIPE_LR:
      Serial.println("[swipe left -> right] next day");
      nextDay(1);
      break;
    case G_SWIPE_RL:
      Serial.println("[swipe right -> left] graph");
      if (today == 6) showGraph(); else showLocked();
      break;
    case G_HOVER:
      Serial.println("[hover] again");
      if (docked) showLook(); else Serial.println("Dock the strap first");
      break;
    default: break;
  }
}

void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if      (cmd == "n") nextDay(1);
  else if (cmd == "p") nextDay(-1);
  else if (cmd == "a") showLook();
  else if (cmd == "g") showGraph();
}

// ---------- main ----------
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(TRIG_L, OUTPUT); pinMode(ECHO_L, INPUT);
  pinMode(TRIG_R, OUTPUT); pinMode(ECHO_R, INPUT);
  pinMode(AGAIN_PIN,   INPUT_PULLUP);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);

  matrix.begin();
  mx = matrix.getGraphicObject();
  matrix.setTextAlignment(PA_CENTER);
  matrix.addChar('^', FLIP_VERTICAL ? arrowDown : arrowUp);
  matrix.addChar('~', FLIP_VERTICAL ? arrowUp   : arrowDown);
  matrix.displayClear();

  long sum = 0;                              // strap AWAY at power-on
  for (int i = 0; i < 50; i++) { sum += readHall(); delay(10); }
  baseline = sum / 50;

  Serial.println("The Great Quantifier — gestures");
  Serial.print("Hall baseline: "); Serial.println(baseline);

  runSetup();
  Serial.println("Ready — dock the strap");
}

void loop() {
  handleAction(pollGesture());
  handleAction(againButton());
  if (triggerPressed()) { Serial.println("[trigger button]"); nextDay(1); }
  handleSerial();

  int strength = abs(readHall() - baseline);
  bool wantDocked = docked ? (strength > DOCK_OFF) : (strength > DOCK_ON);
  if (wantDocked != docked) {
    if (changeStart == 0) changeStart = millis();
    if (millis() - changeStart > HOLD_MS) {
      docked = wantDocked;
      changeStart = 0;
      if (docked) onDock(); else onUndock();
    }
  } else {
    changeStart = 0;
  }
}