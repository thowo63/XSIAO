#pragma once

#include <Arduino.h>

struct RuntimeConfig {
  int recording_silence_threshold;
  int recording_speech_hits_required;
  int recording_silence_ms;
  int recording_earliest_stop_ms;
  int recording_min_speech_ms;
  int recording_max_ms;
  String tts_base_url;
};

extern RuntimeConfig runtimeConfig;

void setRuntimeConfigDefaults();
bool loadRuntimeConfig();
bool saveRuntimeConfig();
