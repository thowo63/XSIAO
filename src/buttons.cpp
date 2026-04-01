#include "buttons.h"

#include <Arduino.h>
#include <WiFi.h>

#include "app_config.h"
#include "app_state.h"
#include "display_ui.h"
#include "audio_record.h"
#include "stt_client.h"
#include "assistant_client.h"
#include "audio_out.h"
#include "mqtt_client.h"

extern AppState appState;

static int lastUpState = HIGH;
static int lastDownState = HIGH;

static unsigned long upPressedMs = 0;
static unsigned long downPressedMs = 0;

static unsigned long lastUpRepeatMs = 0;
static unsigned long lastDownRepeatMs = 0;

static bool upLongHandled = false;
static bool downLongHandled = false;

static const unsigned long debounceMs = 35;
static const unsigned long shortPressMaxMs = 350;
static const unsigned long longPressMs = 500;
static const unsigned long repeatMs = 150;

static void showVolume() {
  appState.lastVolumeOverlayMs = millis();
  drawLines(
    "Lautstaerke",
    String(appState.speakerVolume) + "%",
    "",
    WiFi.localIP().toString()
  );
}

static void changeVolume(int delta) {
  int v = appState.speakerVolume + delta;
  if (v < 0) v = 0;
  if (v > 100) v = 100;

  if (v != appState.speakerVolume) {
    appState.speakerVolume = v;
    showVolume();
  }
}

static void handleVoiceFlowFromRecording() {
  String textOut, errorOut;

  drawLines("STT...", "lade Aufnahme hoch", "", WiFi.localIP().toString());

  bool ok = transcribeRecordedFile(textOut, errorOut);
  if (!ok) {
    drawLines(
      "STT Fehler",
      errorOut.substring(0, 20),
      errorOut.length() > 20 ? errorOut.substring(20, 40) : "",
      WiFi.localIP().toString()
    );
    return;
  }

  appState.lastIncomingText = textOut;

  String assistError;
  String answer = processAssistantText(textOut, assistError);
  if (answer.length() == 0) {
    appState.lastHaAnswer = assistError;
    drawLines(
      "Assistant Fehler",
      assistError.substring(0, 20),
      assistError.length() > 20 ? assistError.substring(20, 40) : "",
      WiFi.localIP().toString()
    );
    return;
  }

  appState.lastHaAnswer = answer;

  if (mqttIsConnected()) {
    mqttPublishTextOut(answer.c_str());
  }

#if AUTO_SPEAK_AFTER_ASK
  appState.lastTtsUrl = buildTtsUrl(answer);
  appState.lastTtsPath = "-";

  drawLines(
    "Antwort:",
    answer.substring(0, 20),
    answer.length() > 20 ? answer.substring(20, 40) : "",
    "spreche..."
  );

  String playError;
  bool played = playWavFromUrl(appState.lastTtsUrl, playError);
  if (!played) {
    drawLines(
      "TTS Fehler",
      playError.substring(0, 20),
      playError.length() > 20 ? playError.substring(20, 40) : "",
      WiFi.localIP().toString()
    );
    return;
  }
#endif

  drawLines(
    "Gesprochen:",
    answer.substring(0, 20),
    answer.length() > 20 ? answer.substring(20, 40) : "",
    WiFi.localIP().toString()
  );
}

static void toggleRecordingByButton() {
  if (!isRecording()) {
    if (startRecording()) {
      drawLines("Aufnahme", "sprich jetzt...", "", WiFi.localIP().toString());
    } else {
      drawLines("Aufnahme", "Start fehlgeschlagen", "", WiFi.localIP().toString());
    }
    return;
  }
  if (isRecording()) {
    if (getRecordingAgeMs() < 800) {
      Serial.println("STOP via BUTTON ignored (<800ms)");
      return;
    }
  }
  Serial.println("STOP via BUTTON");
  stopRecording();
  drawLines("Aufnahme", "manuell gestoppt", appState.lastMicInfo, WiFi.localIP().toString());
  delay(150);

  handleVoiceFlowFromRecording();
}

void buttonsInit() {
  pinMode(BTN_VOL_UP, INPUT_PULLUP);
  pinMode(BTN_VOL_DOWN, INPUT_PULLUP);

  lastUpState = digitalRead(BTN_VOL_UP);
  lastDownState = digitalRead(BTN_VOL_DOWN);

  upPressedMs = 0;
  downPressedMs = 0;
  lastUpRepeatMs = 0;
  lastDownRepeatMs = 0;
  upLongHandled = false;
  downLongHandled = false;
}

void handleButtons() {
  unsigned long now = millis();

  int upState = digitalRead(BTN_VOL_UP);
  int downState = digitalRead(BTN_VOL_DOWN);

  // ===== VOL+ =====
  if (upState != lastUpState) {
    delay(debounceMs);
    upState = digitalRead(BTN_VOL_UP);

    if (upState != lastUpState) {
      lastUpState = upState;

      if (upState == LOW) {
        upPressedMs = now;
        lastUpRepeatMs = now;
        upLongHandled = false;
      } else {
        if (upPressedMs > 0) {
          unsigned long dur = now - upPressedMs;
          if (!upLongHandled && dur <= shortPressMaxMs) {
            toggleRecordingByButton();
          }
        }

        upPressedMs = 0;
        upLongHandled = false;
      }
    }
  }

  if (lastUpState == LOW && upPressedMs > 0) {
    unsigned long dur = now - upPressedMs;

    if (dur >= longPressMs) {
      if (!upLongHandled) {
        upLongHandled = true;
        changeVolume(+5);
        lastUpRepeatMs = now;
      } else if (now - lastUpRepeatMs >= repeatMs) {
        changeVolume(+5);
        lastUpRepeatMs = now;
      }
    }
  }

  // ===== VOL- =====
  if (downState != lastDownState) {
    delay(debounceMs);
    downState = digitalRead(BTN_VOL_DOWN);

    if (downState != lastDownState) {
      lastDownState = downState;

      if (downState == LOW) {
        downPressedMs = now;
        lastDownRepeatMs = now;
        downLongHandled = false;
      } else {
        if (downPressedMs > 0) {
          unsigned long dur = now - downPressedMs;
          if (!downLongHandled && dur <= shortPressMaxMs) {
            toggleRecordingByButton();
          }
        }

        downPressedMs = 0;
        downLongHandled = false;
      }
    }
  }

  if (lastDownState == LOW && downPressedMs > 0) {
    unsigned long dur = now - downPressedMs;

    if (dur >= longPressMs) {
      if (!downLongHandled) {
        downLongHandled = true;
        changeVolume(-5);
        lastDownRepeatMs = now;
      } else if (now - lastDownRepeatMs >= repeatMs) {
        changeVolume(-5);
        lastDownRepeatMs = now;
      }
    }
  }
}