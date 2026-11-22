// 07_holistic_score — The Great Quantifier
// Strap docks on the ledge  -> short wake line on the mirror -> today's score (+ arrow) fades in
// Strap removed             -> score fades out
// Button short press        -> next day
// Button held 3 s           -> week graph (bars only, no values) — unlocked on Day 7 only
//
// One holistic score per day (1-100). Arrows and graph are worked out from the scores.
//
// Serial Monitor (115200) commands, as backup:
//   n / p  -> next / previous day
//   g      -> show week graph (ignores the Day 7 lock, for testing)
//   b6     -> matrix brightness 0-15

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// ================= WEEK DATA (hard-coded for demo) =================
// Holistic score for each day, 1-100. Change these to tell your week's story.
const int SCORES[7] = { 54, 61, 58, 66, 71, 67, 78 };

// ================= SETTINGS (tweak these) =================
int     brightness     = 6;      // matrix brightness 0-15
#define USE_WAKE_LINE    true    // short line sweep on dock, before the score
#define WAKE_MS          600     // how long the line sweep takes
#define NUMBER_FADE_MS   1000
#define HOLD_GRAPH_MS    3000    // hold button this long for the graph
#define GRAPH_SHOW_MS    6000    // how long the graph stays
#define GRAPH_RELATIVE   false   // true = stretch bars between week's lowest and highest
                                 //        score, so small changes look bigger

// If arrows or graph bars appear upside down / reversed, flip these:
#define FLIP_VERTICAL    false
#define FLIP_HORIZONTAL  false

const int DOCK_ON  = 250;
const int DOCK_OFF = 120;
const unsigned long HOLD_MS = 300;

// ================= PINS =================
#define HALL_PIN    34
#define BUTTON_PIN  27           // button between GPIO 27 and GND, no resistor
#define CS_PIN      5            // matrix: DIN = 23, CLK = 18
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

// ================= INTERNALS =================
MD_Parola matrix = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Custom arrow characters: first byte = width, then one byte per column
uint8_t arrowUp[]   = { 5, 0x04, 0x02, 0x7F, 0x02, 0x04 };
uint8_t arrowDown[] = { 5, 0x10, 0x20, 0x7F, 0x20, 0x10 };
#define CH_UP   '^'
#define CH_DOWN '~'

int  today = 0;
int  baseline = 0;
bool docked = false;
bool numberShowing = false;
unsigned long changeStart = 0;

bool btnWasDown = false;
bool holdFired  = false;
unsigned long btnDownAt = 0;

char textBuf[16];

// ---------- helpers ----------
int readHall() {
  long sum = 0;
  for (int i = 0; i < 20; i++) sum += analogRead(HALL_PIN);
  return sum / 20;
}

int trendToday() {                           // 1 up, -1 down, 0 same / Day 1
  if (today == 0) return 0;
  int diff = SCORES[today] - SCORES[today - 1];
  return (diff > 0) - (diff < 0);
}

const char* todayText() {
  int t = trendToday();
  const char* arrow = t > 0 ? " ^" : (t < 0 ? " ~" : "");
  snprintf(textBuf, sizeof(textBuf), "%d%s", SCORES[today], arrow);
  return textBuf;
}

int barHeight(int d) {                       // score -> 1..8 rows
  int lo = 1, hi = 100;
  if (GRAPH_RELATIVE) {
    lo = 100; hi = 1;
    for (int i = 0; i < 7; i++) { lo = min(lo, SCORES[i]); hi = max(hi, SCORES[i]); }
    if (hi == lo) return 4;
  }
  return constrain(1 + (SCORES[d] - lo) * 7 / (hi - lo), 1, 8);
}

// ---------- matrix ----------
void fadeIntensity(bool in) {
  int stepDelay = NUMBER_FADE_MS / (brightness + 1);
  if (in) for (int b = 0; b <= brightness; b++) { matrix.setIntensity(b); delay(stepDelay); }
  else    for (int b = brightness; b >= 0; b--) { matrix.setIntensity(b); delay(stepDelay); }
}

void wakeLine() {                            // thin line grows out from the centre, then vanishes
  if (!USE_WAKE_LINE) return;
  MD_MAX72XX* mx = matrix.getGraphicObject();
  int cols = mx->getColumnCount();
  int mid = cols / 2;
  mx->clear();
  matrix.setIntensity(max(1, brightness / 2));
  for (int i = 0; i < mid; i++) {
    mx->setPoint(4, mid + i, true);
    mx->setPoint(4, mid - 1 - i, true);
    delay(WAKE_MS / mid);
  }
  delay(150);
  mx->clear();
  delay(150);
}

void fadeNumber(bool in) {
  if (in) {
    matrix.setIntensity(0);
    matrix.print(todayText());
    fadeIntensity(true);
    numberShowing = true;
  } else {
    fadeIntensity(false);
    matrix.displayClear();
    numberShowing = false;
  }
}

void showGraph() {
  Serial.println("Showing week graph");
  bool wasShowing = numberShowing;
  if (numberShowing) fadeNumber(false);

  MD_MAX72XX* mx = matrix.getGraphicObject();
  int cols = mx->getColumnCount();           // 32 for 4-in-1
  const int barW = 3, gap = 1;
  int startX = (cols - (7 * barW + 6 * gap)) / 2;

  mx->clear();
  matrix.setIntensity(brightness);

  for (int d = 0; d < 7; d++) {              // bars grow one by one, left to right
    int h = barHeight(d);
    for (int level = 1; level <= h; level++) {
      int row = FLIP_VERTICAL ? (level - 1) : (8 - level);
      for (int w = 0; w < barW; w++) {
        int x = startX + d * (barW + gap) + w;
        int col = FLIP_HORIZONTAL ? x : (cols - 1 - x);
        mx->setPoint(row, col, true);
      }
      delay(45);
    }
    delay(150);
  }

  delay(GRAPH_SHOW_MS);
  fadeIntensity(false);
  mx->clear();
  matrix.displayClear();

  if (docked || wasShowing) { delay(300); fadeNumber(true); }
}

void showLocked() {                          // graph held before Day 7
  Serial.println("Graph locked until Day 7");
  bool wasShowing = numberShowing;
  matrix.setIntensity(brightness);
  matrix.print("- -");
  delay(1000);
  matrix.displayClear();
  if (wasShowing) matrix.print(todayText());
}

// ---------- events ----------
void onDock() {
  Serial.print(">>> DOCKED — Day "); Serial.print(today + 1);
  Serial.print("  showing: ");      Serial.println(todayText());
  wakeLine();
  fadeNumber(true);
}

void onUndock() {
  Serial.println("<<< REMOVED");
  if (numberShowing) fadeNumber(false);
}

void changeDay(int step) {
  today = (today + step + 7) % 7;
  Serial.print("Now Day "); Serial.print(today + 1);
  Serial.print("  score: "); Serial.println(todayText());
  if (numberShowing) { fadeNumber(false); fadeNumber(true); }
}

void handleButton() {
  bool down = digitalRead(BUTTON_PIN) == LOW;
  unsigned long now = millis();

  if (down && !btnWasDown) { btnDownAt = now; holdFired = false; }

  if (down && !holdFired && now - btnDownAt >= HOLD_GRAPH_MS) {
    holdFired = true;
    if (today == 6) showGraph(); else showLocked();
  }

  if (!down && btnWasDown && !holdFired && now - btnDownAt > 50) {
    changeDay(1);                            // short press = next day
  }

  btnWasDown = down;
}

void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd == "n") changeDay(1);
  else if (cmd == "p") changeDay(-1);
  else if (cmd == "g") showGraph();
  else if (cmd.startsWith("b")) {
    brightness = constrain(cmd.substring(1).toInt(), 0, 15);
    if (numberShowing) matrix.setIntensity(brightness);
    Serial.print("Brightness: "); Serial.println(brightness);
  }
}

// ---------- main ----------
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  matrix.begin();
  matrix.setTextAlignment(PA_CENTER);
  matrix.addChar(CH_UP,   FLIP_VERTICAL ? arrowDown : arrowUp);
  matrix.addChar(CH_DOWN, FLIP_VERTICAL ? arrowUp   : arrowDown);
  matrix.displayClear();

  long sum = 0;                              // keep the strap AWAY at power-on
  for (int i = 0; i < 50; i++) { sum += readHall(); delay(10); }
  baseline = sum / 50;

  Serial.println("The Great Quantifier — ready");
  Serial.print("Hall baseline: "); Serial.println(baseline);
  Serial.println("Button: tap = next day, hold 3 s = week graph (Day 7)");
}

void loop() {
  handleButton();
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