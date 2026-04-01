#pragma once

#include <Arduino.h>

void displayInit();
void drawLines(const String& l1,
               const String& l2 = "",
               const String& l3 = "",
               const String& l4 = "");
void drawVuMeter(long avg);