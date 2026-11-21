// 03_matrix_test — show numbers, test brightness behind acrylic
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW   // if text looks wrong, see notes
#define MAX_DEVICES 4                       // 1 = single 8x8, 4 = 4-in-1 strip
#define CS_PIN 5
#define BRIGHTNESS 6 

MD_Parola matrix = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

void setup() {
  Serial.begin(115200);
  matrix.begin();
  matrix.setTextAlignment(PA_CENTER);
  matrix.displayClear();
  Serial.println("Matrix test start");
}

void loop() {
  // 1. Brightness sweep: 0 = dimmest, 15 = brightest
  for (int b = 0; b <= 15; b += 3) {
    matrix.setIntensity(b);
    matrix.print("72");
    Serial.print("Brightness: "); Serial.println(b);
    delay(1500);
  }

  // 2. Count 0-99 to check all digit shapes
  matrix.setIntensity(4);
  for (int n = 0; n < 100; n += 7) {
    matrix.print(String(n));
    delay(400);
  }

  // 3. Scroll a longer number (needed for 3+ digits on a single 8x8)
  matrix.displayClear();
  matrix.displayText("7340", PA_CENTER, 60, 1000, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  while (!matrix.displayAnimate()) { }
  delay(500);
}