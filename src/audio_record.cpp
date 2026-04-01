#include "audio_record.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <driver/i2s.h>

#include "app_state.h"
#include "audio_in.h"
#include "runtime_config.h"

extern AppState appState;

static File recordFile;
static bool recording = false;
static const char* RECORD_PATH = "/record.wav";
static uint32_t dataBytesWritten = 0;
static long lastRecordingLevel = 0;

static void writeWavHeader(File& f, uint32_t sampleRate, uint16_t bitsPerSample, uint16_t channels, uint32_t dataSize) {
  uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
  uint16_t blockAlign = channels * bitsPerSample / 8;
  uint32_t chunkSize = 36 + dataSize;

  f.seek(0);

  f.write((const uint8_t*)"RIFF", 4);
  f.write((uint8_t*)&chunkSize, 4);
  f.write((const uint8_t*)"WAVE", 4);

  f.write((const uint8_t*)"fmt ", 4);
  uint32_t subchunk1Size = 16;
  uint16_t audioFormat = 1;
  f.write((uint8_t*)&subchunk1Size, 4);
  f.write((uint8_t*)&audioFormat, 2);
  f.write((uint8_t*)&channels, 2);
  f.write((uint8_t*)&sampleRate, 4);
  f.write((uint8_t*)&byteRate, 4);
  f.write((uint8_t*)&blockAlign, 2);
  f.write((uint8_t*)&bitsPerSample, 2);

  f.write((const uint8_t*)"data", 4);
  f.write((uint8_t*)&dataSize, 4);
}

bool recordInit() {
  return LittleFS.begin(true);
}

bool startRecording() {
  if (recording) return true;

  if (!initMicI2S()) {
    appState.lastMicInfo = "record init fail";
    return false;
  }

  delay(120);

  if (LittleFS.exists(RECORD_PATH)) {
    LittleFS.remove(RECORD_PATH);
  }

  recordFile = LittleFS.open(RECORD_PATH, "w");
  if (!recordFile) {
    appState.lastMicInfo = "record file fail";
    return false;
  }

  uint8_t emptyHeader[44] = {0};
  recordFile.write(emptyHeader, sizeof(emptyHeader));

  dataBytesWritten = 0;
  lastRecordingLevel = 0;
  recording = true;

  appState.recordingStartedMs = millis();
  appState.recordingLastSpeechMs = millis();
  appState.recordingHadSpeech = false;
  appState.recordingSpeechHits = 0;
  appState.lastMicInfo = "recording...";

  Serial.printf(
    "REC start, autoStop=%d threshold=%d hits=%d silenceMs=%d earliest=%d minSpeech=%d max=%d\n",
    appState.recordingAutoStopEnabled ? 1 : 0,
    runtimeConfig.recording_silence_threshold,
    runtimeConfig.recording_speech_hits_required,
    runtimeConfig.recording_silence_ms,
    runtimeConfig.recording_earliest_stop_ms,
    runtimeConfig.recording_min_speech_ms,
    runtimeConfig.recording_max_ms
  );

  return true;
}

void stopRecording() {
  if (!recording) return;

  Serial.println("REC stop entered");

  writeWavHeader(recordFile, 16000, 16, 1, dataBytesWritten);
  recordFile.close();
  stopMicI2S();

  recording = false;
  Serial.printf("REC stop bytes=%u\n", dataBytesWritten);
  appState.recordingAutoStopEnabled = true;

  appState.recordingStartedMs = 0;
  appState.recordingLastSpeechMs = 0;
  appState.recordingHadSpeech = false;
  appState.recordingSpeechHits = 0;
  appState.lastMicInfo = String("recorded ") + dataBytesWritten + " bytes";
}

bool isRecording() {
  return recording;
}

void handleRecordingTask() {
  if (!recording) return;

  int32_t inSamples[256];
  int16_t outSamples[256];
  size_t bytesRead = 0;

  esp_err_t err = i2s_read(I2S_NUM_0, inSamples, sizeof(inSamples), &bytesRead, pdMS_TO_TICKS(20));
  if (err != ESP_OK || bytesRead == 0) {
    return;
  }

  size_t count = bytesRead / sizeof(int32_t);

  int64_t sumAbs = 0;
  for (size_t i = 0; i < count; i++) {
    int32_t shifted = inSamples[i] >> 14;

    // Testweise neutral, ohne zusätzliche Verstärkung
    if (shifted > 32767) shifted = 32767;
    if (shifted < -32768) shifted = -32768;

    outSamples[i] = (int16_t)shifted;
    sumAbs += llabs(shifted);
  }

  long avg = (count > 0) ? (long)(sumAbs / (int64_t)count) : 0;
  lastRecordingLevel = avg;
  appState.currentMicAvg = avg;

  if (avg >= runtimeConfig.recording_silence_threshold) {
    if (appState.recordingSpeechHits < 1000) {
      appState.recordingSpeechHits++;
    }

    if (appState.recordingSpeechHits >= runtimeConfig.recording_speech_hits_required) {
      appState.recordingLastSpeechMs = millis();
      appState.recordingHadSpeech = true;
    }
  } else {
    if (appState.recordingSpeechHits > 0) {
      appState.recordingSpeechHits--;
    }
  }

  size_t outBytes = count * sizeof(int16_t);
  recordFile.write((uint8_t*)outSamples, outBytes);
  dataBytesWritten += outBytes;

  static unsigned long lastDbg = 0;
  if (millis() - lastDbg > 500) {
    lastDbg = millis();
    // Serial.printf(
    //   "REC running bytes=%u lvl=%ld hits=%d hadSpeech=%d age=%lu\n",
    //   dataBytesWritten,
    //   lastRecordingLevel,
    //   appState.recordingSpeechHits,
    //   appState.recordingHadSpeech ? 1 : 0,
    //   getRecordingAgeMs()
    // );
  }
}

unsigned long getRecordingAgeMs() {
  if (!recording || appState.recordingStartedMs == 0) return 0;
  return millis() - appState.recordingStartedMs;
}

bool recordingShouldAutoStop() {
  if (!recording) return false;
  if (!appState.recordingAutoStopEnabled) return false;

  unsigned long ageMs = getRecordingAgeMs();
  if (ageMs == 0) return false;

  // Serial.printf(
  //   "AUTOCHK age=%lu max=%d earliest=%d minSpeech=%d silence=%d hadSpeech=%d hits=%d\n",
  //   ageMs,
  //   runtimeConfig.recording_max_ms,
  //   runtimeConfig.recording_earliest_stop_ms,
  //   runtimeConfig.recording_min_speech_ms,
  //   runtimeConfig.recording_silence_ms,
  //   appState.recordingHadSpeech ? 1 : 0,
  //   appState.recordingSpeechHits
  // );

  // Maximaldauer
  if (ageMs >= (unsigned long)runtimeConfig.recording_max_ms) {
    return true;
  }

  // Nicht zu früh stoppen
  if (ageMs < (unsigned long)runtimeConfig.recording_earliest_stop_ms) {
    return false;
  }

  // Erst stoppen, wenn wirklich Sprache erkannt wurde
  if (!appState.recordingHadSpeech) {
    return false;
  }

  if (ageMs < (unsigned long)runtimeConfig.recording_min_speech_ms) {
    return false;
  }

  // 2s oder konfigurierte Stille seit letzter erkannter Sprache
  if ((millis() - appState.recordingLastSpeechMs) >= (unsigned long)runtimeConfig.recording_silence_ms) {
    return true;
  }

  return false;
}

long getRecordingCurrentLevel() {
  return lastRecordingLevel;
}

String getRecordingFilename() {
  return String(RECORD_PATH);
}
