#include "app_logic.h"

#include <Arduino.h>
#include <WiFi.h>

#include "app_state.h"
#include "audio_in.h"
#include "audio_record.h"
#include "stt_client.h"
#include "assistant_client.h"
#include "audio_out.h"
#include "mqtt_client.h"
#include "display_ui.h"

extern AppState appState;

static void handleVoiceFlowFromRecording() {
  String textOut, errorOut;

  bool ok = transcribeRecordedFile(textOut, errorOut);
  if (!ok) {
    drawLines("STT Fehler", errorOut.substring(0, 20),
              errorOut.length() > 20 ? errorOut.substring(20, 40) : "",
              WiFi.localIP().toString());
    return;
  }

  appState.lastIncomingText = textOut;

  String assistError;
  String answer = processAssistantText(textOut, assistError);
  if (answer.length() == 0) {
    appState.lastHaAnswer = assistError;
    drawLines("Assistant Fehler", assistError.substring(0, 20),
              assistError.length() > 20 ? assistError.substring(20, 40) : "",
              WiFi.localIP().toString());
    return;
  }

  appState.lastHaAnswer = answer;

  if (mqttIsConnected()) {
    mqttPublishTextOut(answer.c_str());
  }

#if AUTO_SPEAK_AFTER_ASK
  appState.lastTtsUrl = buildTtsUrl(answer);
  appState.lastTtsPath = "-";

  String playError;
  bool played = playWavFromUrl(appState.lastTtsUrl, playError);
  if (!played) {
    drawLines("TTS Fehler", playError.substring(0, 20),
              playError.length() > 20 ? playError.substring(20, 40) : "",
              WiFi.localIP().toString());
    return;
  }
#endif

  drawLines("Gesprochen:", answer.substring(0, 20),
            answer.length() > 20 ? answer.substring(20, 40) : "",
            WiFi.localIP().toString());
}

void handleAppLogic() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  handleMqtt();

  unsigned long now = millis();

  if (appState.vuMode && now - appState.lastVuReadMs > 120) {
    appState.lastVuReadMs = now;
    long avg = readMicAverage();
    drawVuMeter(avg);
    publishMicLevel();
  }

  if (isRecording()) {
    if (recordingShouldAutoStop()) {
      Serial.println("STOP via AUTO");
      stopRecording();
      delay(120);
      handleVoiceFlowFromRecording();
    }
  }
}