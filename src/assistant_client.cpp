#include "assistant_client.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "app_config.h"
#include "app_state.h"
#include "ha_client.h"
#include "runtime_config.h"

extern AppState appState;

static String processOpenAIText(const String& text, String& errorOut) {
  errorOut = "";

  HTTPClient http;
  String proxyUrl = runtimeConfig.openai_proxy_url.length() > 0
      ? runtimeConfig.openai_proxy_url
      : String(OPENAI_PROXY_URL);
  http.begin(proxyUrl);
  http.addHeader("Content-Type", "application/json");
  http.setConnectTimeout(5000);
  http.setTimeout(60000);

  DynamicJsonDocument req(512);
  req["text"] = text;

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
    errorOut = String("OpenAI Proxy HTTP ") + httpCode + ": " + resp;
    return "";
  }

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, resp);
  if (err) {
    errorOut = String("JSON Fehler: ") + err.c_str();
    return "";
  }

  bool ok = doc["ok"] | false;
  if (!ok) {
    errorOut = String((const char*)(doc["error"] | "Unbekannter Fehler"));
    return "";
  }

  const char* answer = doc["answer"] | "";
  if (strlen(answer) == 0) {
    errorOut = "Keine Antwort im Proxy";
    return "";
  }

  return String(answer);
}

const char* getAssistantModeName(int mode) {
  switch (mode) {
    case ASSISTANT_MODE_HOMEASSISTANT: return "homeassistant";
    case ASSISTANT_MODE_OPENAI: return "openai";
    case ASSISTANT_MODE_CLAUDE: return "claude";
    case ASSISTANT_MODE_ALEXA: return "alexa";
    default: return "unknown";
  }
}

bool setAssistantModeByString(const String& modeStr) {
  String s = modeStr;
  s.toLowerCase();

  if (s == "homeassistant" || s == "ha") {
    appState.assistantMode = ASSISTANT_MODE_HOMEASSISTANT;
    return true;
  }
  if (s == "openai" || s == "chatgpt") {
    appState.assistantMode = ASSISTANT_MODE_OPENAI;
    return true;
  }
  if (s == "claude") {
    appState.assistantMode = ASSISTANT_MODE_CLAUDE;
    return true;
  }
  if (s == "alexa") {
    appState.assistantMode = ASSISTANT_MODE_ALEXA;
    return true;
  }

  return false;
}

String processAssistantText(const String& text, String& errorOut) {
  switch (appState.assistantMode) {
    case ASSISTANT_MODE_HOMEASSISTANT:
      return processConversationWithHA(text, errorOut);

    case ASSISTANT_MODE_OPENAI:
      return processOpenAIText(text, errorOut);

    case ASSISTANT_MODE_CLAUDE:
      errorOut = "CLAUDE noch nicht implementiert";
      return "";

    case ASSISTANT_MODE_ALEXA:
      errorOut = "ALEXA noch nicht implementiert";
      return "";

    default:
      errorOut = "Unbekannter Assistant-Modus";
      return "";
  }
}
