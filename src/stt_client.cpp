#include "stt_client.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <LittleFS.h>

#include "app_config.h"
#include "audio_record.h"
#include <ArduinoJson.h>

static bool parseHttpUrl(const String& url, String& host, uint16_t& port, String& path) {
  if (!url.startsWith("http://")) return false;

  String rest = url.substring(7);
  int slash = rest.indexOf('/');
  String hostPort = slash >= 0 ? rest.substring(0, slash) : rest;
  path = slash >= 0 ? rest.substring(slash) : "/";

  int colon = hostPort.indexOf(':');
  if (colon >= 0) {
    host = hostPort.substring(0, colon);
    port = (uint16_t)hostPort.substring(colon + 1).toInt();
  } else {
    host = hostPort;
    port = 80;
  }

  return host.length() > 0;
}

static String extractTextFromJson(const String& json, String& errorOut) {
  errorOut = "";

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    errorOut = String("JSON Fehler: ") + err.c_str();
    return "";
  }

  bool ok = doc["ok"] | false;
  if (!ok) {
    errorOut = String((const char*)(doc["error"] | "STT Antwort ohne ok=true"));
    return "";
  }

  const char* text = doc["text"] | "";
  return String(text);
}

bool transcribeRecordedFile(String& textOut, String& errorOut) {
  textOut = "";
  errorOut = "";

  String filePath = getRecordingFilename();
  if (!LittleFS.exists(filePath)) {
    errorOut = "Keine Aufnahme vorhanden";
    return false;
  }

  File f = LittleFS.open(filePath, "r");
  if (!f) {
    errorOut = "Aufnahme konnte nicht geoeffnet werden";
    return false;
  }

  String host, path;
  uint16_t port;
  if (!parseHttpUrl(STT_BASE_URL, host, port, path)) {
    f.close();
    errorOut = "STT URL ungueltig";
    return false;
  }

  WiFiClient client;
  client.setTimeout(60000);

  if (!client.connect(host.c_str(), port)) {
    f.close();
    errorOut = "STT Verbindung fehlgeschlagen";
    return false;
  }

  String boundary = "----xiaozhiBoundary7MA4YWxk";

  String part1 =
      "--" + boundary + "\r\n"
      "Content-Disposition: form-data; name=\"audio\"; filename=\"record.wav\"\r\n"
      "Content-Type: audio/wav\r\n\r\n";

  String part2 =
      "\r\n--" + boundary + "\r\n"
      "Content-Disposition: form-data; name=\"language\"\r\n\r\n"
      "de\r\n";

  String endPart =
      "--" + boundary + "--\r\n";

  size_t contentLength = part1.length() + f.size() + part2.length() + endPart.length();

  client.print(String("POST ") + path + " HTTP/1.1\r\n");
  client.print(String("Host: ") + host + "\r\n");
  client.print("Connection: close\r\n");
  client.print(String("Content-Type: multipart/form-data; boundary=") + boundary + "\r\n");
  client.print(String("Content-Length: ") + contentLength + "\r\n");
  client.print("\r\n");

  client.print(part1);

  uint8_t buf[1024];
  while (f.available()) {
    size_t n = f.read(buf, sizeof(buf));
    if (n > 0) client.write(buf, n);
  }
  f.close();

  client.print(part2);
  client.print(endPart);

  String response;
  unsigned long start = millis();
  while (client.connected() || client.available()) {
    while (client.available()) {
      response += (char)client.read();
    }
    if (millis() - start > 60000) {
      errorOut = "STT Antwort Timeout";
      client.stop();
      return false;
    }
    delay(1);
  }
  client.stop();

  int bodyPos = response.indexOf("\r\n\r\n");
  if (bodyPos < 0) {
    errorOut = "Ungueltige HTTP Antwort";
    return false;
  }

  int statusLineEnd = response.indexOf("\r\n");
  String statusLine = statusLineEnd > 0 ? response.substring(0, statusLineEnd) : "";
  if (statusLine.indexOf("200") < 0) {
    errorOut = "STT HTTP Fehler: " + statusLine;
    return false;
  }

  String body = response.substring(bodyPos + 4);

  String parseErr;
  String text = extractTextFromJson(body, parseErr);

  if (text.length() == 0) {
  if (parseErr.length() > 0) {
      errorOut = parseErr + "\nBody: " + body;
  } else {
      errorOut = "Keine Sprache erkannt";
  }
  return false;
}

textOut = text;
return true;
}