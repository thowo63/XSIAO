#pragma once

#include <Arduino.h>

bool recordInit();
bool startRecording();
void stopRecording();
bool isRecording();
void handleRecordingTask();

bool recordingShouldAutoStop();
long getRecordingCurrentLevel();
unsigned long getRecordingAgeMs();

String getRecordingFilename();