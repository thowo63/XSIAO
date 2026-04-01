#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include "runtime_config.h"

#include "app_config.h"
#include "app_state.h"
#include "display_ui.h"
#include "wifi_setup.h"
#include "web_ui.h"
#include "mqtt_client.h"
#include "app_logic.h"
#include "buttons.h"
#include "audio_record.h"

AppState appState;

WiFiClient espClient;
WebServer server(HTTP_PORT);

void setup() {
  delay(1500);
  Serial.begin(115200);
  delay(1000);

  displayInit();
  drawLines("Boot...", "Xiaozhi HA+MQTT");

  setupWifi();

  Serial.println("WLAN verbunden");
  Serial.println(WiFi.localIP());

  drawLines("WLAN verbunden", WiFi.SSID(), WiFi.localIP().toString(), "HTTP + HA");

  buttonsInit();
  recordInit();
  loadRuntimeConfig();
  setupMqtt();
  setupHttp();
}

void loop() {
  handleHttp();
  handleButtons();
  handleRecordingTask();
  handleAppLogic();
}