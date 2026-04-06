#include "runtime_config.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "app_config.h"

RuntimeConfig runtimeConfig;

static const char* CONFIG_PATH = "/config.json";

void setRuntimeConfigDefaults() {
  runtimeConfig.recording_silence_threshold = 80;
  runtimeConfig.recording_speech_hits_required = 2;
  runtimeConfig.recording_silence_ms = 2000;
  runtimeConfig.recording_earliest_stop_ms = 2000;
  runtimeConfig.recording_min_speech_ms = 500;
  runtimeConfig.recording_max_ms = 15000;
  runtimeConfig.openai_proxy_url = OPENAI_PROXY_URL;
  runtimeConfig.tts_voice = "marin";
  runtimeConfig.tts_base_url = TTS_BASE_URL;
  runtimeConfig.stt_base_url = STT_BASE_URL;
}

bool loadRuntimeConfig() {
  setRuntimeConfigDefaults();

  if (!LittleFS.begin(true)) {
    return false;
  }

  if (!LittleFS.exists(CONFIG_PATH)) {
    saveRuntimeConfig();
    return true;
  }

  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) {
    return false;
  }

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    return false;
  }

  runtimeConfig.recording_silence_threshold =
      doc["recording_silence_threshold"] | runtimeConfig.recording_silence_threshold;

  runtimeConfig.recording_speech_hits_required =
      doc["recording_speech_hits_required"] | runtimeConfig.recording_speech_hits_required;

  runtimeConfig.recording_silence_ms =
      doc["recording_silence_ms"] | runtimeConfig.recording_silence_ms;

  runtimeConfig.recording_earliest_stop_ms =
      doc["recording_earliest_stop_ms"] | runtimeConfig.recording_earliest_stop_ms;

  runtimeConfig.recording_min_speech_ms =
      doc["recording_min_speech_ms"] | runtimeConfig.recording_min_speech_ms;

  runtimeConfig.recording_max_ms =
      doc["recording_max_ms"] | runtimeConfig.recording_max_ms;

  runtimeConfig.openai_proxy_url =
      doc["openai_proxy_url"] | runtimeConfig.openai_proxy_url;

  runtimeConfig.tts_voice =
      doc["tts_voice"] | runtimeConfig.tts_voice;

  runtimeConfig.tts_base_url =
      doc["tts_base_url"] | runtimeConfig.tts_base_url;

  runtimeConfig.stt_base_url =
      doc["stt_base_url"] | runtimeConfig.stt_base_url;

  return true;
}

bool saveRuntimeConfig() {
  if (!LittleFS.begin(true)) {
    return false;
  }

  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) {
    return false;
  }

  DynamicJsonDocument doc(2048);
  doc["recording_silence_threshold"] = runtimeConfig.recording_silence_threshold;
  doc["recording_speech_hits_required"] = runtimeConfig.recording_speech_hits_required;
  doc["recording_silence_ms"] = runtimeConfig.recording_silence_ms;
  doc["recording_earliest_stop_ms"] = runtimeConfig.recording_earliest_stop_ms;
  doc["recording_min_speech_ms"] = runtimeConfig.recording_min_speech_ms;
  doc["recording_max_ms"] = runtimeConfig.recording_max_ms;
  doc["openai_proxy_url"] = runtimeConfig.openai_proxy_url;
  doc["tts_voice"] = runtimeConfig.tts_voice;
  doc["tts_base_url"] = runtimeConfig.tts_base_url;
  doc["stt_base_url"] = runtimeConfig.stt_base_url;

  bool ok = serializeJsonPretty(doc, f) > 0;
  f.close();
  return ok;
}
