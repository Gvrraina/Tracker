// 09_line_graph — The Great Quantifier
//
// Strap docks            -> wake line -> score (+ arrow) fades in, stays, fades away   [look 1]
// AGAIN tap (docked)     -> score returns, dimmer each time (whole number stays readable) [look 2, 3]
// After 3 looks          -> nothing until tomorrow
// AGAIN hold 3 s         -> week LINE graph — Day 7 only, doesn't use a look
// HIDDEN TRIGGER tap     -> next day (jury demo), looks reset
//
// Serial Monitor (115200) backup commands:
//   n / p -> next / previous day     a -> again     g -> graph (ignores Day 7 lock)

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// ================= WEEK DATA (hard-coded for demo) =================
const int SCORES[7] = { 54, 61, 58, 66, 71, 67, 78 };   // holistic score 1-100

// ================= LOOKS (the 3 chances) =================
// Each look = matrix intensity (0-15) + extra software dimming (1.0 = full, 0.3 = 30%).
// Software dimming lets look 3 go dimmer than the matrix's lowest setting.
#define MAX_LOOKS 3
const int   lookIntensity[MAX_LOOKS] = { 10,   5,   3    };
const float lookLevel[MAX_LOOKS]     = { 1.0, 1.0, 0.35 };
#define SHOW_MS   5000     // how long the score stays each look
#define FADE_MS   1000

// ================= OTHER SETTINGS =================
#define USE_WAKE_LINE    true    // thin line on dock before the score
#define WAKE_MS          600
#define HOLD_GRAPH_MS    3000
#define GRAPH_SHOW_MS    6000
#define GRAPH_INTENSITY  6
#define GRAPH_RELATIVE   true    // true = line uses full height between week's min & max
                                 // false = 1-100 mapped to full height (flatter line)
#define FLIP_VERTICAL    false   // flip if arrows / graph are upside down
#define FLIP_HORIZONTAL  false   // flip if graph runs right-to-left

const int DOCK_ON  = 250;
const int DOCK_OFF = 120;
const unsigned long HOLD_MS = 300;

// ================= PINS =================
#define HALL_PIN     34
#define AGAIN_PIN    32          // visible button -> GND (diagonal legs, no resistor)
#define TRIGGER_PIN  25          // hidden next-day button -> GND
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
  int diff = (today == 0) ? 0 : SCORES[today] - SCORES[today - 1];
  const char* arrow = diff > 0 ? " ^" : (diff < 0 ? " ~" : "");
  snprintf(textBuf, sizeof(textBuf), "%d%s", SCORES[today], arrow);
  return textBuf;
}

void displayOn(bool on) {
  mx->control(MD_MAX72XX::SHUTDOWN, on ? MD_MAX72XX::OFF : MD_MAX72XX::ON);
}

// Software dimming: flick the display on/off very fast (250 Hz) so it looks dimmer.
// Ramps from levelFrom to levelTo over ms. Level 1.0 = fully on.
void glowFor(float levelFrom, float levelTo, unsigned long ms) {
  if (levelFrom >= 0.999 && levelTo >= 0.999) { displayOn(true); delay(ms); return; }
  const unsigned long PERIOD_US = 4000;
  unsigned long start = millis();
  while (millis() - start < ms) {
    float t = (float)(millis() - start) / ms;
    float lvl = levelFrom + (levelTo - levelFrom) * t;
    lvl = lvl * lvl;                                   // looks more even to the eye
    unsigned long onUs = lvl * PERIOD_US;
    if (onUs > 0)          { displayOn(true);  delayMicroseconds(onUs); }
    if (onUs < PERIOD_US)  { displayOn(false); delayMicroseconds(PERIOD_US - onUs); }
    yield();
  }
  displayOn(levelTo > 0.001);
}

// ---------- the 3 looks ----------
void showLook() {
  if (looksUsed >= MAX_LOOKS) { Serial.println("No looks left today"); return; }

  float lvl = lookLevel[looksUsed];
  Serial.print("Look "); Serial.print(looksUsed + 1); Serial.print("/"); Serial.print(MAX_LOOKS);
  Serial.print("  showing: "); Serial.println(todayText());

  displayOn(false);
  matrix.setIntensity(lookIntensity[looksUsed]);
  matrix.print(todayText());
  glowFor(0, lvl, FADE_MS);      // fade in
  glowFor(lvl, lvl, SHOW_MS);    // hold
  glowFor(lvl, 0, FADE_MS);      // fade out
  matrix.displayClear();
  displayOn(true);

  looksUsed++;
  if (looksUsed >= MAX_LOOKS) Serial.println("That was the last look until tomorrow");
}

// ---------- wake line ----------
void wakeLine() {
  if (!USE_WAKE_LINE) return;
  int cols = mx->getColumnCount(), mid = cols / 2;
  mx->clear();
  matrix.setIntensity(2);
  for (int i = 0; i < mid; i++) {
    mx->setPoint(4, mid + i, true);
    mx->setPoint(4, mid - 1 - i, true);
    delay(WAKE_MS / mid);
  }
  delay(150);
  mx->clear();
  delay(250);
}

// ---------- line graph ----------
int dayRow(int d) {                          // score -> row 0 (bottom) .. 7 (top)
  int lo = 1, hi = 100;
  if (GRAPH_RELATIVE) {
    lo = 100; hi = 1;
    for (int i = 0; i < 7; i++) { lo = min(lo, SCORES[i]); hi = max(hi, SCORES[i]); }
    if (hi == lo) return 3;
  }
  return constrain((SCORES[d] - lo) * 7 / (hi - lo), 0, 7);
}

void plot(int x, int y) {                    // x: 0 = left, y: 0 = bottom
  int cols = mx->getColumnCount();
  int row = FLIP_VERTICAL ? y : (7 - y);
  int col = FLIP_HORIZONTAL ? x : (cols - 1 - x);
  mx->setPoint(row, col, true);
}

void drawLine(int x0, int y0, int x1, int y1, int stepDelay) {   // Bresenham
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    plot(x0, y0);
    delay(stepDelay);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void showGraph() {
  Serial.println("Showing week line graph");
  int cols = mx->getColumnCount();           // 32
  mx->clear();
  displayOn(true);
  matrix.setIntensity(GRAPH_INTENSITY);

  int px = 0, py = dayRow(0);
  plot(px, py);
  for (int d = 1; d < 7; d++) {              // line draws itself left to right
    int x = d * (cols - 1) / 6;
    int y = dayRow(d);
    drawLine(px, py, x, y, 40);
    px = x; py = y;
    delay(120);
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
  Serial.print("Now Day "); Serial.print(today + 1);
  Serial.print("  score: "); Serial.println(todayText());
  if (docked) showLook();
}

void onDock() {
  Serial.print(">>> DOCKED — Day "); Serial.println(today + 1);
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

  if (down && !againWasDown) {
    againDownAt = now; holdFired = false;
    Serial.println("[Again button down]");
  }

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
    if (down) { Serial.println("[Trigger pressed]"); nextDay(1); }
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

  long sum = 0;                              // keep the strap AWAY at power-on
  for (int i = 0; i < 50; i++) { sum += readHall(); delay(10); }
  baseline = sum / 50;

  Serial.println("The Great Quantifier — ready");
  Serial.print("Hall baseline: "); Serial.println(baseline);
  if (digitalRead(AGAIN_PIN) == LOW)   Serial.println("WARNING: Again pin reads pressed at start — check button legs");
  if (digitalRead(TRIGGER_PIN) == LOW) Serial.println("WARNING: Trigger pin reads pressed at start — check button legs");
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