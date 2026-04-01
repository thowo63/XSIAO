#pragma once

#include <Arduino.h>
#include "app_config.h"
#include "assistant_client.h"

struct AppState {
  String lastIncomingText = "-";
  String lastHaAnswer = "-";
  String lastConversationId = "";
  String lastMicInfo = "mic idle";
  String lastSpeakerInfo = "speaker idle";
  String lastTtsUrl = "-";
  String lastTtsPath = "-";

  unsigned long lastStatusMs = 0;
  unsigned long lastReconnectTry = 0;
  unsigned long lastVuReadMs = 0;
  unsigned long lastVolumeOverlayMs = 0;

  unsigned long recordingStartedMs = 0;
  unsigned long recordingLastSpeechMs = 0;
  unsigned long lastRecordingUiMs = 0;
  int recordingSpeechHits = 0;
  bool recordingAutoStopEnabled = true;

  bool recordingHadSpeech = false;

  long currentMicAvg = 0;
  bool vuMode = false;
  bool micI2sActive = false;

  int speakerVolume = DEFAULT_SPEAKER_VOLUME;
  int assistantMode = DEFAULT_ASSISTANT_MODE;
};

extern AppState appState;