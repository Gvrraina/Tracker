#include <Wire.h>
#include <U8g2lib.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

void setup() {
  Wire.begin(21, 22);
  oled.begin();
}

void loop() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_logisoso32_tn);
  oled.drawStr(20, 48, "7842");
  oled.sendBuffer();
  delay(1000);
}