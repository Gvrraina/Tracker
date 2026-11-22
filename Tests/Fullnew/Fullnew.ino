// 10_two_metrics — The Great Quantifier
//
// Two hidden metrics (A and B). Each day the mirror shows ONE of them, by a fixed
// shuffled order. No hint which one it is.
// Arrow = that metric vs the LAST DAY IT WAS SHOWN. First time a metric appears
// there is no arrow — "something new" is the clue.
//
// Strap docks            -> wake line -> pause -> score (+ arrow) fades in   [look 1]
// AGAIN tap (docked)     -> score returns, dimmer each time                 [look 2, 3]
// AGAIN hold 3 s         -> week graph: one "body improving" line (A+B average), Day 7 only
// HIDDEN TRIGGER tap     -> next day (jury demo), looks reset
//
// Serial Monitor (115200): n / p = next / previous day, a = again, g = graph

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// ================= WEEK DATA (hard-coded for demo) =================
// Two holistic scores per day, 1-100. Change these to tell your week's story.
const int SCORES[2][7] = {
  { 54, 61, 58, 66, 71, 67, 78 },   // metric A
  { 30, 34, 41, 38, 45, 50, 48 }    // metric B
};

// Which metric the mirror shows each day: 0 = A, 1 = B. Shuffle as you like.
const int SHOW_METRIC[7] = { 0, 0, 1, 0, 1, 1, 0 };

// ================= LOOKS (the 3 chances) =================
#define MAX_LOOKS 3
const int   lookIntensity[MAX_LOOKS] = { 8
,   2,   0    };   // matrix 0-15
const float lookLevel[MAX_LOOKS]     = { 1.0, 1.0, 0.35 };   // extra dimming 0-1
#define SHOW_MS   5000     // how long the score stays each look
#define FADE_MS   600

// ================= TIMING / LOOK =================
#define USE_WAKE_LINE    true
#define WAKE_MS          250
#define WAKE_HOLD_MS     80      // line stays briefly
#define ANTICIPATION_MS  300     // empty pause before the number — sets the mood
#define HOLD_GRAPH_MS    3000
#define GRAPH_SHOW_MS    6000
#define GRAPH_INTENSITY  6
#define GRAPH_DOT_MS     20      // line drawing speed
#define GRAPH_SEG_MS     60      // pause between days
#define GRAPH_RELATIVE   true    // true = both lines use full height (shared scale)
#define FLIP_VERTICAL    false
#define FLIP_HORIZONTAL  false

const int DOCK_ON  = 250;
const int DOCK_OFF = 120;
const unsigned long HOLD_MS = 150;

// ================= PINS =================
#define HALL_PIN     34
#define AGAIN_PIN    32          // visible button -> GND
#define TRIGGER_PIN  25          // hidden next-day button -> GND (change to 33 if 25 is busy)
#define CS_PIN       5           // matrix: DIN = 23, CLK = 18, VCC = VIN (5V)
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES  4

// ================= INTERNALS =================
MD_Parola   matrix = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
MD_MAX72XX* mx;

uint8_t arrowUp[]   = { 5, 0x04, 0x02, 0x7F, 0x02, 0x04 };
uint8_t arrowDown[] = { 5, 0x10, 0x20, 0x7F, 0x20, 0x10 };

int  today = 0;
int  looksUsed = 0;
int  baseline = 0;
bool docked = false;
unsigned long changeStart = 0;

bool againWasDown = false, holdFired = false;
unsigned long againDownAt = 0;
bool trigWasDown = false;
unsigned long trigLastChange = 0;

char textBuf[16];

// ---------- helpers ----------
int readHall() {
  long sum = 0;
  for (int i = 0; i < 20; i++) sum += analogRead(HALL_PIN);
  return sum / 20;
}

const char* todayText() {
  int m = SHOW_METRIC[today];
  int value = SCORES[m][today];

  // Compare with the LAST DAY THIS SAME METRIC WAS SHOWN.
  // First time a metric appears -> no arrow.
  int lastDay = -1;
  for (int d = today - 1; d >= 0; d--) {
    if (SHOW_METRIC[d] == m) { lastDay = d; break; }
  }
  int diff = (lastDay < 0) ? 0 : value - SCORES[m][lastDay];
  const char* arrow = diff > 0 ? " ^" : (diff < 0 ? " ~" : "");
  snprintf(textBuf, sizeof(textBuf), "%d%s", value, arrow);
  return textBuf;
}

void printToday() {                           // for the team, in Serial Monitor
  Serial.print("Day "); Serial.print(today + 1);
  Serial.print("  metric "); Serial.print(SHOW_METRIC[today] == 0 ? "A" : "B");
  Serial.print("  showing: "); Serial.println(todayText());
}

void displayOn(bool on) {
  mx->control(MD_MAX72XX::SHUTDOWN, on ? MD_MAX72XX::OFF : MD_MAX72XX::ON);
}

// Software dimming (250 Hz on/off) ramping from levelFrom to levelTo over ms.
void glowFor(float levelFrom, float levelTo, unsigned long ms) {
  if (levelFrom >= 0.999 && levelTo >= 0.999) { displayOn(true); delay(ms); return; }
  const unsigned long PERIOD_US = 4000;
  unsigned long start = millis();
  while (millis() - start < ms) {
    float t = (float)(millis() - start) / ms;
    float lvl = levelFrom + (levelTo - levelFrom) * t;
    lvl = lvl * lvl;
    unsigned long onUs = lvl * PERIOD_US;
    if (onUs > 0)         { displayOn(true);  delayMicroseconds(onUs); }
    if (onUs < PERIOD_US) { displayOn(false); delayMicroseconds(PERIOD_US - onUs); }
    yield();
  }
  displayOn(levelTo > 0.001);
}

// ---------- the 3 looks ----------
void showLook() {
  if (looksUsed >= MAX_LOOKS) { Serial.println("No looks left today"); return; }
  float lvl = lookLevel[looksUsed];
  Serial.print("Look "); Serial.print(looksUsed + 1); Serial.print("/"); Serial.print(MAX_LOOKS);
  Serial.print("  "); printToday();

  displayOn(false);
  matrix.setIntensity(lookIntensity[looksUsed]);
  matrix.print(todayText());
  glowFor(0, lvl, FADE_MS);
  glowFor(lvl, lvl, SHOW_MS);
  glowFor(lvl, 0, FADE_MS);
  matrix.displayClear();
  displayOn(true);

  looksUsed++;
  if (looksUsed >= MAX_LOOKS) Serial.println("That was the last look until tomorrow");
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
// Each day's point = average of metric A and B that day (both are tracked every day,
// even though only one is shown).
int bodyScore(int d) {
  return (SCORES[0][d] + SCORES[1][d]) / 2;
}

int dayRow(int d) {                           // 0 = bottom .. 7 = top
  int lo = 1, hi = 100;
  if (GRAPH_RELATIVE) {
    lo = 100; hi = 1;
    for (int i = 0; i < 7; i++) { lo = min(lo, bodyScore(i)); hi = max(hi, bodyScore(i)); }
    if (hi == lo) return 3;
  }
  return constrain((bodyScore(d) - lo) * 7 / (hi - lo), 0, 7);
}

void plot(int x, int y) {
  int cols = mx->getColumnCount();
  int row = FLIP_VERTICAL ? y : (7 - y);
  int col = FLIP_HORIZONTAL ? x : (cols - 1 - x);
  mx->setPoint(row, col, true);
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
  displayOn(true);
  matrix.setIntensity(GRAPH_INTENSITY);

  int cols = mx->getColumnCount();
  int px = 0, py = dayRow(0);
  for (int d = 1; d < 7; d++) {               // line draws itself left to right
    int x = d * (cols - 1) / 6, y = dayRow(d);
    drawLine(px, py, x, y);
    px = x; py = y;
    delay(GRAPH_SEG_MS);
  }

  delay(GRAPH_SHOW_MS);
  glowFor(1.0, 0, FADE_MS);
  mx->clear();
  matrix.displayClear();
  displayOn(true);
}

void showLocked() {
  Serial.println("Graph locked until Day 7");
  matrix.setIntensity(GRAPH_INTENSITY);
  matrix.print("- -");
  delay(1000);
  matrix.displayClear();
}

// ---------- events ----------
void nextDay(int step) {
  today = (today + step + 7) % 7;
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

void handleAgain() {
  bool down = digitalRead(AGAIN_PIN) == LOW;
  unsigned long now = millis();
  if (down && !againWasDown) { againDownAt = now; holdFired = false; }

  if (down && !holdFired && now - againDownAt >= HOLD_GRAPH_MS) {
    holdFired = true;
    if (today == 6) showGraph(); else showLocked();
  }
  if (!down && againWasDown && !holdFired && now - againDownAt > 50) {
    if (docked) showLook();
    else Serial.println("Again pressed — dock the strap first");
  }
  againWasDown = down;
}

void handleTrigger() {
  bool down = digitalRead(TRIGGER_PIN) == LOW;
  if (down != trigWasDown && millis() - trigLastChange > 50) {
    trigLastChange = millis();
    trigWasDown = down;
    if (down) nextDay(1);
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

  pinMode(AGAIN_PIN,   INPUT_PULLUP);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);

  matrix.begin();
  mx = matrix.getGraphicObject();
  matrix.setTextAlignment(PA_CENTER);
  matrix.addChar('^', FLIP_VERTICAL ? arrowDown : arrowUp);
  matrix.addChar('~', FLIP_VERTICAL ? arrowUp   : arrowDown);
  matrix.displayClear();

  long sum = 0;                               // keep the strap AWAY at power-on
  for (int i = 0; i < 50; i++) { sum += readHall(); delay(10); }
  baseline = sum / 50;

  Serial.println("The Great Quantifier (2 metrics) — ready");
  Serial.print("Hall baseline: "); Serial.println(baseline);
  if (digitalRead(TRIGGER_PIN) == LOW) Serial.println("WARNING: trigger pin reads pressed — check wiring");
}

void loop() {
  handleAgain();
  handleTrigger();
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
  delay(20);
}