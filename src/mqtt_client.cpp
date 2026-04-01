#include "mqtt_client.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "app_config.h"
#include "app_state.h"
#include "display_ui.h"
#include "audio_in.h"
#include "audio_out.h"

extern AppState appState;
extern WiFiClient espClient;

static PubSubClient mqtt(espClient);

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.printf("MQTT [%s]: %s\n", topic, msg.c_str());

  if (String(topic) == TOPIC_TEXT_OUT) {
    appState.lastIncomingText = msg;
    if (!appState.vuMode) {
      drawLines(
        "Antwort:",
        msg.substring(0, 20),
        msg.length() > 20 ? msg.substring(20, 40) : "",
        WiFi.localIP().toString()
      );
    }
  }

  if (String(topic) == TOPIC_CMD) {
    if (msg == "reboot") {
      drawLines("Neustart...", "per MQTT");
      delay(1000);
      ESP.restart();
    } else if (msg == "vu_on") {
      if (initMicI2S()) {
        appState.vuMode = true;
      }
    } else if (msg == "vu_off") {
      appState.vuMode = false;
      stopMicI2S();
      drawLines("MQTT verbunden", WiFi.SSID(), WiFi.localIP().toString(), "warte auf text");
    } else if (msg == "speaker_test") {
      bool ok = runSpeakerTone();
      drawLines(
        "Speaker-Test",
        ok ? "Ton gesendet" : "Fehler",
        appState.lastSpeakerInfo,
        WiFi.localIP().toString()
      );
    }
  }
}

bool mqttIsConnected() {
  return mqtt.connected();
}

void mqttPublishTextOut(const char* text) {
  if (!mqtt.connected()) return;
  mqtt.publish(TOPIC_TEXT_OUT, text);
}

void publishMicLevel() {
  if (!mqtt.connected()) return;

  DynamicJsonDocument doc(256);
  doc["device"] = DEVICE_NAME;
  doc["avg"] = appState.currentMicAvg;
  doc["info"] = appState.lastMicInfo;

  String out;
  serializeJson(doc, out);
  mqtt.publish(TOPIC_MIC, out.c_str(), false);
}

void publishStatus() {
  if (!mqtt.connected()) return;

  DynamicJsonDocument doc(512);
  doc["device"] = DEVICE_NAME;
  doc["ip"] = WiFi.localIP().toString();
  doc["ssid"] = WiFi.SSID();
  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  doc["mic"] = appState.lastMicInfo;
  doc["mic_avg"] = appState.currentMicAvg;
  doc["speaker"] = appState.lastSpeakerInfo;
  doc["ha_answer"] = appState.lastHaAnswer;

  String payload;
  serializeJson(doc, payload);
  mqtt.publish(TOPIC_STATUS, payload.c_str(), true);
}

static bool mqttConnect() {
  String clientId = String(DEVICE_NAME) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  bool ok;
  if (String(MQTT_USER).length() > 0) {
    ok = mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD, TOPIC_STATUS, 1, true, "offline");
  } else {
    ok = mqtt.connect(clientId.c_str());
  }

  if (ok) {
    Serial.println("MQTT verbunden");
    mqtt.subscribe(TOPIC_TEXT_OUT);
    mqtt.subscribe(TOPIC_CMD);
    mqtt.publish(TOPIC_STATUS, "online", true);
    publishStatus();

    if (!appState.vuMode) {
      drawLines("MQTT verbunden", WiFi.SSID(), WiFi.localIP().toString(), "warte auf text");
    }
    return true;
  }

  Serial.printf("MQTT Fehler, rc=%d\n", mqtt.state());
  return false;
}

void setupMqtt() {
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
}

void handleMqtt() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!mqtt.connected()) {
    unsigned long now = millis();
    if (now - appState.lastReconnectTry > 3000) {
      appState.lastReconnectTry = now;
      if (!appState.vuMode) {
        drawLines("MQTT verbindet...", MQTT_SERVER, WiFi.localIP().toString());
      }
      mqttConnect();
    }
  } else {
    mqtt.loop();
  }

  unsigned long now = millis();
  if (mqtt.connected() && now - appState.lastStatusMs > 30000) {
    appState.lastStatusMs = now;
    publishStatus();
  }
}