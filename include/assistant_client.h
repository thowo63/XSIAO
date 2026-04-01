#pragma once

#include <Arduino.h>

String processAssistantText(const String& text, String& errorOut);
const char* getAssistantModeName(int mode);
bool setAssistantModeByString(const String& modeStr);