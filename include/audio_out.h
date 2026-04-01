#pragma once

#include <Arduino.h>

bool runSpeakerTone(int freq = 1000, int ms = 800);
bool playWavFromUrl(const String& url, String& errorOut);
bool playRecordedWav(String& errorOut);
String buildTtsUrl(const String& text);