#include "display_ui.h"
#include <U8g2lib.h>
#include <WiFi.h>
#include "app_config.h"

static U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(
  U8G2_R0,
  OLED_SCL,
  OLED_SDA,
  U8X8_PIN_NONE
);

void displayInit() {
  u8g2.begin();
}

void drawLines(const String& l1, const String& l2, const String& l3, const String& l4) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);

  if (l1.length()) u8g2.drawUTF8(0, 12, l1.c_str());
  if (l2.length()) u8g2.drawUTF8(0, 28, l2.c_str());
  if (l3.length()) u8g2.drawUTF8(0, 44, l3.c_str());
  if (l4.length()) u8g2.drawUTF8(0, 60, l4.c_str());

  u8g2.sendBuffer();
}

void drawVuMeter(long avg) {
  int bar = map((int)constrain(avg, 0L, 12000L), 0, 12000, 0, 120);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawUTF8(0, 12, "Mikro Live");

  char buf[32];
  snprintf(buf, sizeof(buf), "avg=%ld", avg);
  u8g2.drawStr(0, 28, buf);

  u8g2.drawFrame(4, 38, 120, 14);
  u8g2.drawBox(4, 38, bar, 14);

  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawUTF8(0, 62, WiFi.localIP().toString().c_str());
  u8g2.sendBuffer();
}