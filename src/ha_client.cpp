#include "ha_client.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "app_config.h"
#include "app_state.h"

extern AppState appState;

String processConversationWithHA(const String& text, String& errorOut) {
  errorOut = "";

  HTTPClient http;
  String url = String(HA_BASE_URL) + "/api/conversation/process";

  http.begin(url);
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument req(512);
  req["text"] = text;
  req["language"] = HA_LANGUAGE;

  if (strlen(HA_AGENT_ID) > 0) {
    req["agent_id"] = HA_AGENT_ID;
  }

  if (appState.lastConversationId.length() > 0) {
    req["conversation_id"] = appState.lastConversationId;
  }

  String body;
  serializeJson(req, body);

  int httpCode = http.POST(body);
  if (httpCode <= 0) {
    errorOut = String("HTTP Fehler: ") + http.errorToString(httpCode);
    http.end();
    return "";
  }

  String resp = http.getString();
  http.end();

  if (httpCode != 200) {
    errorOut = String("HA HTTP ") + httpCode + ": " + resp;
    return "";
  }

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, resp);
  if (err) {
    errorOut = String("JSON Fehler: ") + err.c_str();
    return "";
  }

  if (doc["conversation_id"].is<const char*>()) {
    appState.lastConversationId = String((const char*)doc["conversation_id"]);
  }

  const char* speech =
    doc["response"]["speech"]["plain"]["speech"] |
    doc["response"]["speech"]["speech"] |
    "";

  if (strlen(speech) == 0) {
    errorOut = "Keine Sprachantwort in HA-Response gefunden";
    return "";
  }

  return String(speech);
}