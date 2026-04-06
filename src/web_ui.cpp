#include "web_ui.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "app_config.h"
#include "app_state.h"
#include "display_ui.h"
#include "ha_client.h"
#include "mqtt_client.h"
#include "stt_client.h"
#include "audio_in.h"
#include "audio_out.h"
#include "audio_record.h"
#include <LittleFS.h>
#include "assistant_client.h"
#include "runtime_config.h"

extern AppState appState;
extern WebServer server;

static bool requireWebAuth() {
  if (strlen(WEB_UI_PASSWORD) == 0) {
    return true;
  }

  if (server.authenticate(WEB_UI_USER, WEB_UI_PASSWORD)) {
    return true;
  }

  server.requestAuthentication();
  return false;
}

static String htmlEscape(const String& input) {
  String out;
  out.reserve(input.length() + 16);

  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c; break;
    }
  }

  return out;
}

String htmlPage() {
  String html;
  html += R"rawliteral(
    <!doctype html>
    <html>
    <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title>Xiaozhi</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 16px; }
        input[type=text], select { width: 100%; max-width: 420px; padding: 8px; }
        button { padding: 8px 12px; margin-top: 8px; margin-right: 8px; }
        .box { margin-top: 14px; padding: 10px; border: 1px solid #ccc; border-radius: 8px; }
        .mono { font-family: monospace; white-space: pre-wrap; word-break: break-word; }
    </style>
    </head>
    <body>
    <h2>Xiaozhi ESP32-S3</h2>
    )rawliteral";

    html += "<p><b>IP:</b> " + htmlEscape(WiFi.localIP().toString()) + "</p>";
    html += "<p><b>SSID:</b> " + htmlEscape(WiFi.SSID()) + "</p>";
    html += "<p><b>MQTT:</b> ";
    html += mqttIsConnected() ? "verbunden" : "getrennt";
    html += "</p>";

    html += "<div class='box'>";
    html += "<p><b>Assistant:</b> " + htmlEscape(String(getAssistantModeName(appState.assistantMode))) + "</p>";
    html += "<p><b>Volume:</b> " + htmlEscape(String(appState.speakerVolume) + "%") + "</p>";
    html += "<p><b>Letzte Eingabe:</b> " + htmlEscape(appState.lastIncomingText) + "</p>";
    html += "<p><b>Antwort:</b> " + htmlEscape(appState.lastHaAnswer) + "</p>";
    html += "<p><b>TTS URL:</b> " + htmlEscape(appState.lastTtsUrl) + "</p>";
    html += "<p><b>TTS Path:</b> " + htmlEscape(appState.lastTtsPath) + "</p>";
    html += "<p><b>Mikro:</b> " + htmlEscape(appState.lastMicInfo) + "</p>";
    html += "<p><b>Speaker:</b> " + htmlEscape(appState.lastSpeakerInfo) + "</p>";
    html += "<p><b>Mic avg:</b> " + htmlEscape(String(appState.currentMicAvg)) + "</p>";
    html += "<p><b>OpenAI Proxy:</b> " + htmlEscape(runtimeConfig.openai_proxy_url) + "</p>";
    html += "<p><b>TTS Voice:</b> " + htmlEscape(runtimeConfig.tts_voice) + "</p>";
    html += "<p><b>TTS base:</b> " + htmlEscape(runtimeConfig.tts_base_url) + "</p>";
    html += "<p><b>STT base:</b> " + htmlEscape(runtimeConfig.stt_base_url) + "</p>";
    html += "</div>";
    html += R"rawliteral(
        <div class="box">
            <form id="configForm">
            <label><b>Aufnahme-Parameter</b></label><br><br>

            <label>Silence Threshold</label><br>
            <input id="cfg_threshold" name="recording_silence_threshold" type="text"><br><br>

            <label>Speech Hits Required</label><br>
            <input id="cfg_hits" name="recording_speech_hits_required" type="text"><br><br>

            <label>Silence ms</label><br>
            <input id="cfg_silence_ms" name="recording_silence_ms" type="text"><br><br>

            <label>Earliest Stop ms</label><br>
            <input id="cfg_earliest_ms" name="recording_earliest_stop_ms" type="text"><br><br>

            <label>Min Speech ms</label><br>
            <input id="cfg_min_speech_ms" name="recording_min_speech_ms" type="text"><br><br>

            <label>Max Recording ms</label><br>
            <input id="cfg_max_ms" name="recording_max_ms" type="text"><br><br>

            <label>TTS Base URL</label><br>
            <input id="cfg_tts_base_url" name="tts_base_url" type="text"><br><br>

            <label>STT Base URL</label><br>
            <input id="cfg_stt_base_url" name="stt_base_url" type="text"><br><br>

            <label>OpenAI Proxy URL</label><br>
            <input id="cfg_openai_proxy_url" name="openai_proxy_url" type="text"><br><br>

            <label>TTS Voice</label><br>
            <select id="cfg_tts_voice" name="tts_voice">
                <option value="alloy">alloy</option>
                <option value="ash">ash</option>
                <option value="ballad">ballad</option>
                <option value="coral">coral</option>
                <option value="echo">echo</option>
                <option value="fable">fable</option>
                <option value="marin">marin</option>
                <option value="nova">nova</option>
                <option value="onyx">onyx</option>
                <option value="sage">sage</option>
                <option value="shimmer">shimmer</option>
                <option value="verse">verse</option>
            </select><br><br>

            <label>TTS Preset</label><br>
            <select id="cfg_tts_preset">
                <option value="qwen3">Qwen3-TTS</option>
                <option value="legacy">Legacy TTS</option>
                <option value="custom">Benutzerdefiniert</option>
            </select>
            <button type="button" onclick="applyTtsPreset()">Preset anwenden</button><br><br>

            <button type="submit">Speichern</button>
            <button type="button" onclick="loadConfig()">Neu laden</button>
            </form>
            <div id="configResult" class="mono" style="margin-top:10px;">-</div>
        </div>
        )rawliteral";

    html += R"rawliteral(
    <div class="box">
        <form id="assistantForm">
        <label><b>Assistant Auswahl</b></label><br>
        <select id="assistantMode" name="mode">
            <option value="homeassistant">Home Assistant</option>
            <option value="openai">OpenAI</option>
            <option value="claude">Claude</option>
            <option value="alexa">Alexa</option>
        </select>
        <br>
        <button type="submit">Umschalten</button>
        </form>
        <div id="assistantResult" class="mono" style="margin-top:10px;">-</div>
    </div>

    <div class="box">
        <form id="askForm">
        <label><b>Text senden</b></label><br>
        <input id="askText" name="text" type="text" placeholder="z. B. Wie spät ist es?">
        <br>
        <button type="submit">Senden</button>
        </form>
        <div id="askResult" class="mono" style="margin-top:10px;">Bereit.</div>
    </div>

    <div class="box">
        <button onclick="postSimple('/tts-test', 'TTS URL wird erzeugt ...')">TTS Test</button>
        <button onclick="postSimple('/tts-play', 'TTS wird abgespielt ...')">TTS Play</button>
        <button onclick="postSimple('/mic-test', 'Mikro-Test läuft ...')">Mikro testen</button>
        <button onclick="postSimple('/vu-on', 'VU an ...')">VU an</button>
        <button onclick="postSimple('/vu-off', 'VU aus ...')">VU aus</button>
        <button onclick="postSimple('/speaker-test', 'Speaker-Test läuft ...')">Speaker Test</button>
        <div id="actionResult" class="mono" style="margin-top:10px;">-</div>
    </div>

    <div class="box">
        <button onclick="postSimple('/rec-start', 'Aufnahme startet ...')">Aufnahme Start</button>
        <button onclick="postSimple('/rec-stop', 'Aufnahme stoppt ...')">Aufnahme Stop</button>
        <button onclick="window.open('/rec-file', '_blank')">Aufnahme Download</button>
        <button onclick="postSimple('/stt-test', 'STT läuft ...')">STT Test</button>
        <button onclick="postSimple('/ask-recording', 'Sprache -> Assistant ...')">Aufnahme an Assistant</button>
        <button onclick="postSimple('/rec-play', 'Spiele Aufnahme ab ...', 'recordResult')">Aufnahme abspielen</button>
        <div id="recordResult" class="mono" style="margin-top:10px;">-</div>
    </div>

    <div class="box">
        <button onclick="loadStatus()">Status laden</button>
        <div id="statusResult" class="mono" style="margin-top:10px;">-</div>
        <audio id="recordAudio" controls style="width:100%; max-width:420px; margin-top:10px;"></audio>

    </div>

    <script>
    async function postSimple(url, loadingText, targetId = 'actionResult') {
    const box = document.getElementById(targetId);
    box.textContent = loadingText;
    try {
        const resp = await fetch(url, { method: 'POST' });
        const txt = await resp.text();
        box.textContent = txt;
    } catch (e) {
        box.textContent = 'Fehler: ' + e;
    }
    }

    document.getElementById('askForm').addEventListener('submit', async function(e) {
    e.preventDefault();

    const text = document.getElementById('askText').value.trim();
    const box = document.getElementById('askResult');

    if (!text) {
        box.textContent = 'Bitte Text eingeben.';
        return;
    }

    box.textContent = 'Sende ...';

    try {
        const body = new URLSearchParams();
        body.append('text', text);

        const resp = await fetch('/ask', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: body.toString()
        });

        const txt = await resp.text();
        box.textContent = txt;
    } catch (e) {
        box.textContent = 'Fehler: ' + e;
    }
    });

    async function loadStatus() {
    const box = document.getElementById('statusResult');
    box.textContent = 'Lade ...';
    try {
        const resp = await fetch('/api/status');
        const txt = await resp.text();
        box.textContent = txt;
    } catch (e) {
        box.textContent = 'Fehler: ' + e;
    }
    }

    function presetUrlFor(name) {
        if (name === 'qwen3') {
            return 'http://192.168.1.94:8001/tts?text=';
        }
        if (name === 'legacy') {
            return 'http://192.168.1.94:5001/tts?text=';
        }
        return document.getElementById('cfg_tts_base_url').value;
    }

    function detectTtsPreset(url) {
        if (url === 'http://192.168.1.94:8001/tts?text=') return 'qwen3';
        if (url === 'http://192.168.1.94:5001/tts?text=') return 'legacy';
        return 'custom';
    }

    function applyTtsPreset() {
        const preset = document.getElementById('cfg_tts_preset').value;
        if (preset === 'custom') {
            return;
        }
        document.getElementById('cfg_tts_base_url').value = presetUrlFor(preset);
    }
    )rawliteral";

    html += "document.getElementById('assistantMode').value = '" + String(getAssistantModeName(appState.assistantMode)) + "';\n";
    html += R"rawliteral(
    async function askRecording() {
    const box = document.getElementById('recordResult');
    box.textContent = 'Sprache -> Assistant ...';

    try {
        const resp = await fetch('/ask-recording', { method: 'POST' });
        const txt = await resp.text();
        box.textContent = txt;

        const audio = document.getElementById('recordAudio');
        audio.src = '/rec-file?t=' + Date.now();
        audio.load();
    } catch (e) {
        box.textContent = 'Fehler: ' + e;
    }
    }
    )rawliteral";

    html += R"rawliteral(
        async function loadConfig() {
        const box = document.getElementById('configResult');
        box.textContent = 'Lade Config ...';

        try {
            const resp = await fetch('/config-get');
            const cfg = await resp.json();

            document.getElementById('cfg_threshold').value = cfg.recording_silence_threshold;
            document.getElementById('cfg_hits').value = cfg.recording_speech_hits_required;
            document.getElementById('cfg_silence_ms').value = cfg.recording_silence_ms;
            document.getElementById('cfg_earliest_ms').value = cfg.recording_earliest_stop_ms;
            document.getElementById('cfg_min_speech_ms').value = cfg.recording_min_speech_ms;
            document.getElementById('cfg_max_ms').value = cfg.recording_max_ms;
            document.getElementById('cfg_openai_proxy_url').value = cfg.openai_proxy_url;
            document.getElementById('cfg_tts_voice').value = cfg.tts_voice;
            document.getElementById('cfg_tts_base_url').value = cfg.tts_base_url;
            document.getElementById('cfg_stt_base_url').value = cfg.stt_base_url;
            document.getElementById('cfg_tts_preset').value = detectTtsPreset(cfg.tts_base_url);

            box.textContent = 'Config geladen';
        } catch (e) {
            box.textContent = 'Fehler: ' + e;
        }
        }

        document.getElementById('configForm').addEventListener('submit', async function(e) {
        e.preventDefault();

        const box = document.getElementById('configResult');
        box.textContent = 'Speichere ...';

        try {
            const body = new URLSearchParams();
            body.append('recording_silence_threshold', document.getElementById('cfg_threshold').value);
            body.append('recording_speech_hits_required', document.getElementById('cfg_hits').value);
            body.append('recording_silence_ms', document.getElementById('cfg_silence_ms').value);
            body.append('recording_earliest_stop_ms', document.getElementById('cfg_earliest_ms').value);
            body.append('recording_min_speech_ms', document.getElementById('cfg_min_speech_ms').value);
            body.append('recording_max_ms', document.getElementById('cfg_max_ms').value);
            body.append('openai_proxy_url', document.getElementById('cfg_openai_proxy_url').value);
            body.append('tts_voice', document.getElementById('cfg_tts_voice').value);
            body.append('tts_base_url', document.getElementById('cfg_tts_base_url').value);
            body.append('stt_base_url', document.getElementById('cfg_stt_base_url').value);

            const resp = await fetch('/config-set', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: body.toString()
            });

            const txt = await resp.text();
            box.textContent = txt;
        } catch (e) {
            box.textContent = 'Fehler: ' + e;
        }
        });

        loadConfig();
        )rawliteral";
    html += R"rawliteral(
    document.getElementById('assistantForm').addEventListener('submit', async function(e) {
    e.preventDefault();

    const mode = document.getElementById('assistantMode').value;
    const box = document.getElementById('assistantResult');
    box.textContent = 'Schalte um ...';

    try {
        const body = new URLSearchParams();
        body.append('mode', mode);

        const resp = await fetch('/assistant-set', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: body.toString()
        });

        const txt = await resp.text();
        box.textContent = txt;
    } catch (e) {
        box.textContent = 'Fehler: ' + e;
    }
    });

    document.querySelector("button[onclick*='/rec-start']").onclick = function() {
    postSimple('/rec-start', 'Aufnahme startet ...', 'recordResult');
    };

    document.querySelector("button[onclick*='/rec-stop']").onclick = function() {
    postSimple('/rec-stop', 'Aufnahme stoppt ...', 'recordResult');
    };

    document.querySelector("button[onclick*='/stt-test']").onclick = function() {
    postSimple('/stt-test', 'STT läuft ...', 'recordResult');
    };

    document.querySelector("button[onclick*='/ask-recording']").onclick = function() {
    postSimple('/ask-recording', 'Sprache -> Assistant ...', 'recordResult');
    };
    </script>
    </body>
    </html>
    )rawliteral";

  return html;
}

static void handlePlayRecording() {
  if (!requireWebAuth()) return;

  String err;
  bool ok = playRecordedWav(err);

  if (!ok) {
    server.send(500, "text/plain; charset=utf-8", err);
    return;
  }

  server.send(200, "text/plain; charset=utf-8", "Aufnahme abgespielt");
}

static void handleConfigGet() {
  if (!requireWebAuth()) return;

  DynamicJsonDocument doc(768);
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

  String out;
  serializeJsonPretty(doc, out);
  server.send(200, "application/json", out);
}

static void handleAssistantSet() {
  if (!requireWebAuth()) return;

  String mode = server.hasArg("mode") ? server.arg("mode") : "";
  if (mode.isEmpty()) {
    server.send(400, "text/plain; charset=utf-8", "Fehler: mode fehlt");
    return;
  }

  if (!setAssistantModeByString(mode)) {
    server.send(400, "text/plain; charset=utf-8", "Fehler: ungueltiger Assistant");
    return;
  }

  drawLines(
    "Assistant",
    getAssistantModeName(appState.assistantMode),
    "",
    WiFi.localIP().toString()
  );

  String msg = "Assistant gesetzt auf: " + String(getAssistantModeName(appState.assistantMode));
  server.send(200, "text/plain; charset=utf-8", msg);
}

static void handleSttTest() {
  if (!requireWebAuth()) return;

  String textOut, errorOut;

  drawLines("STT...", "lade Aufnahme hoch", "", WiFi.localIP().toString());

  bool ok = transcribeRecordedFile(textOut, errorOut);
  if (!ok) {
    drawLines("STT Fehler", errorOut.substring(0, 20), errorOut.length() > 20 ? errorOut.substring(20, 40) : "", WiFi.localIP().toString());
    server.send(500, "text/plain; charset=utf-8", errorOut);
    return;
  }

  appState.lastIncomingText = textOut;

  drawLines("STT Text:", textOut.substring(0, 20), textOut.length() > 20 ? textOut.substring(20, 40) : "", WiFi.localIP().toString());
  server.send(200, "text/plain; charset=utf-8", textOut);
}

static void handleAskRecording() {
  if (!requireWebAuth()) return;

  String textOut, errorOut;

  drawLines("STT...", "lade Aufnahme hoch", "", WiFi.localIP().toString());

  bool ok = transcribeRecordedFile(textOut, errorOut);
  if (!ok) {
    drawLines("STT Fehler", errorOut.substring(0, 20), errorOut.length() > 20 ? errorOut.substring(20, 40) : "", WiFi.localIP().toString());
    server.send(500, "text/plain; charset=utf-8", errorOut);
    return;
  }

  appState.lastIncomingText = textOut;

  String haError;
//   String answer = processConversationWithHA(textOut, haError);
  String answer = processAssistantText(textOut, haError);
  if (answer.length() == 0) {
    appState.lastHaAnswer = haError;
    drawLines("HA Fehler", haError.substring(0, 20), haError.length() > 20 ? haError.substring(20, 40) : "", WiFi.localIP().toString());
    server.send(500, "text/plain; charset=utf-8", haError);
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
    String msg = "STT: " + textOut + "\n\nHA: " + answer + "\n\nTTS Fehler: " + playError;
    server.send(200, "text/plain; charset=utf-8", msg);
    return;
  }
#endif

  String msg = "STT: " + textOut + "\n\nHA: " + answer;
  server.send(200, "text/plain; charset=utf-8", msg);
}
static void handleConfigSet() {
  if (!requireWebAuth()) return;

  runtimeConfig.recording_silence_threshold =
      server.hasArg("recording_silence_threshold") ? server.arg("recording_silence_threshold").toInt() : runtimeConfig.recording_silence_threshold;

  runtimeConfig.recording_speech_hits_required =
      server.hasArg("recording_speech_hits_required") ? server.arg("recording_speech_hits_required").toInt() : runtimeConfig.recording_speech_hits_required;

  runtimeConfig.recording_silence_ms =
      server.hasArg("recording_silence_ms") ? server.arg("recording_silence_ms").toInt() : runtimeConfig.recording_silence_ms;

  runtimeConfig.recording_earliest_stop_ms =
      server.hasArg("recording_earliest_stop_ms") ? server.arg("recording_earliest_stop_ms").toInt() : runtimeConfig.recording_earliest_stop_ms;

  runtimeConfig.recording_min_speech_ms =
      server.hasArg("recording_min_speech_ms") ? server.arg("recording_min_speech_ms").toInt() : runtimeConfig.recording_min_speech_ms;

  runtimeConfig.recording_max_ms =
      server.hasArg("recording_max_ms") ? server.arg("recording_max_ms").toInt() : runtimeConfig.recording_max_ms;

  runtimeConfig.openai_proxy_url =
      server.hasArg("openai_proxy_url") ? server.arg("openai_proxy_url") : runtimeConfig.openai_proxy_url;

  runtimeConfig.tts_voice =
      server.hasArg("tts_voice") ? server.arg("tts_voice") : runtimeConfig.tts_voice;

  runtimeConfig.tts_base_url =
      server.hasArg("tts_base_url") ? server.arg("tts_base_url") : runtimeConfig.tts_base_url;

  runtimeConfig.stt_base_url =
      server.hasArg("stt_base_url") ? server.arg("stt_base_url") : runtimeConfig.stt_base_url;

  if (!saveRuntimeConfig()) {
    server.send(500, "text/plain; charset=utf-8", "Config konnte nicht gespeichert werden");
    return;
  }

  server.send(200, "text/plain; charset=utf-8", "Config gespeichert");
}

static void handleRecStart() {
  if (!requireWebAuth()) return;

  appState.recordingAutoStopEnabled = false;
  Serial.println("WEB rec-start: autoStop=OFF");
  if (startRecording()) {
    server.send(200, "text/plain; charset=utf-8", "Aufnahme gestartet");
  } else {
    appState.recordingAutoStopEnabled = true;
    server.send(500, "text/plain; charset=utf-8", "Aufnahme konnte nicht gestartet werden");
  }
}

static void handleRecStop() {
  if (!requireWebAuth()) return;

  Serial.println("WEB rec-stop");
  stopRecording();
  Serial.println("WEB rec-stop: autoStop=ON");
  server.send(200, "text/plain; charset=utf-8", "Aufnahme gestoppt");
}

static void handleRecFile() {
  if (!requireWebAuth()) return;

  String path = getRecordingFilename();
  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain; charset=utf-8", "Keine Aufnahme vorhanden");
    return;
  }

  File f = LittleFS.open(path, "r");
  if (!f) {
    server.send(500, "text/plain; charset=utf-8", "Datei konnte nicht geoeffnet werden");
    return;
  }

  server.streamFile(f, "audio/wav");
  f.close();
}

static void handleRoot() {
  if (!requireWebAuth()) return;

  server.send(200, "text/html; charset=utf-8", htmlPage());
}

static void handleAsk() {
  if (!requireWebAuth()) return;

  String text = server.hasArg("text") ? server.arg("text") : server.arg("plain");
  if (text.isEmpty()) {
    server.send(400, "text/plain; charset=utf-8", "Fehler: kein Text");
    return;
  }

  appState.lastIncomingText = text;

  drawLines(
    "Sende an HA...",
    text.substring(0, 20),
    text.length() > 20 ? text.substring(20, 40) : "",
    WiFi.localIP().toString()
  );

  String error;
//   String answer = processConversationWithHA(text, error);
  String answer = processAssistantText(text, error);

  if (answer.length() == 0) {
    appState.lastHaAnswer = error;

    drawLines(
      "HA Fehler",
      error.substring(0, 20),
      error.length() > 20 ? error.substring(20, 40) : "",
      WiFi.localIP().toString()
    );

    server.send(500, "text/plain; charset=utf-8", error);
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
    "HA Antwort:",
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

    String msg = answer + "\n\nTTS Fehler: " + playError;
    server.send(200, "text/plain; charset=utf-8", msg);
    return;
  }

  drawLines(
    "Gesprochen:",
    answer.substring(0, 20),
    answer.length() > 20 ? answer.substring(20, 40) : "",
    WiFi.localIP().toString()
  );
#else
  drawLines(
    "HA Antwort:",
    answer.substring(0, 20),
    answer.length() > 20 ? answer.substring(20, 40) : "",
    WiFi.localIP().toString()
  );
#endif

  server.send(200, "text/plain; charset=utf-8", answer);
}

static void handleTtsTest() {
  if (!requireWebAuth()) return;

  String text = server.hasArg("text") ? server.arg("text") : appState.lastHaAnswer;
  if (text.isEmpty() || text == "-") {
    text = "Hallo, dies ist ein Test mit dem eigenen TTS Endpunkt.";
  }

  String ttsUrl = buildTtsUrl(text);

  appState.lastTtsUrl = ttsUrl;
  appState.lastTtsPath = "-";

  drawLines("TTS URL OK", "eigener Endpoint", "", WiFi.localIP().toString());

  String msg = "URL: " + ttsUrl;
  server.send(200, "text/plain; charset=utf-8", msg);
}

static void handleTtsPlay() {
  if (!requireWebAuth()) return;

  String text = server.hasArg("text") ? server.arg("text") : appState.lastHaAnswer;
  if (text.isEmpty() || text == "-") {
    text = "Hallo, dies ist ein Test mit dem eigenen TTS Endpunkt.";
  }

  String ttsUrl = buildTtsUrl(text);

  appState.lastTtsUrl = ttsUrl;
  appState.lastTtsPath = "-";

  drawLines("TTS Play...", "lade WAV", "", WiFi.localIP().toString());

  String playError;
  bool played = playWavFromUrl(ttsUrl, playError);

  if (!played) {
    drawLines(
      "Play Fehler",
      playError.substring(0, 20),
      playError.length() > 20 ? playError.substring(20, 40) : "",
      WiFi.localIP().toString()
    );
    server.send(500, "text/plain; charset=utf-8", playError);
    return;
  }

  drawLines("TTS gespielt", "eigener Endpoint", appState.lastSpeakerInfo, WiFi.localIP().toString());
  server.send(200, "text/plain; charset=utf-8", "TTS abgespielt");
}

static void handleMicTest() {
  if (!requireWebAuth()) return;

  if (!initMicI2S()) {
    server.send(500, "text/plain; charset=utf-8", "Fehler: i2s init fail");
    return;
  }

  long avg = readMicAverage();
  bool ok = avg > 10;
  stopMicI2S();

  drawLines("Mikro-Test", ok ? "Signal erkannt" : "kein Signal", appState.lastMicInfo, WiFi.localIP().toString());

  String msg = String(ok ? "Signal erkannt" : "kein Signal") + "\n" + appState.lastMicInfo;
  server.send(200, "text/plain; charset=utf-8", msg);
}

static void handleVuOn() {
  if (!requireWebAuth()) return;

  if (!initMicI2S()) {
    server.send(500, "text/plain; charset=utf-8", "Fehler: i2s init fail");
    return;
  }

  appState.vuMode = true;
  server.send(200, "text/plain; charset=utf-8", "VU an");
}

static void handleVuOff() {
  if (!requireWebAuth()) return;

  appState.vuMode = false;
  stopMicI2S();
  drawLines("VU aus", WiFi.SSID(), WiFi.localIP().toString(), "warte auf text");
  server.send(200, "text/plain; charset=utf-8", "VU aus");
}

static void handleSpeakerTest() {
  if (!requireWebAuth()) return;

  bool ok = runSpeakerTone(1000, 800);
  drawLines("Speaker-Test", ok ? "Ton gesendet" : "Fehler", appState.lastSpeakerInfo, WiFi.localIP().toString());

  String msg = String(ok ? "Ton gesendet" : "Fehler") + "\n" + appState.lastSpeakerInfo;
  server.send(200, "text/plain; charset=utf-8", msg);
}

static void handleApiStatus() {
  if (!requireWebAuth()) return;

  DynamicJsonDocument doc(768);
  doc["ip"] = WiFi.localIP().toString();
  doc["ssid"] = WiFi.SSID();
  doc["mqtt"] = mqttIsConnected();
  doc["text"] = appState.lastIncomingText;
  doc["ha_answer"] = appState.lastHaAnswer;
  doc["conversation_id"] = appState.lastConversationId;
  doc["tts_url"] = appState.lastTtsUrl;
  doc["tts_path"] = appState.lastTtsPath;
  doc["tts_base_url"] = runtimeConfig.tts_base_url;
  doc["stt_base_url"] = runtimeConfig.stt_base_url;
  doc["openai_proxy_url"] = runtimeConfig.openai_proxy_url;
  doc["tts_voice"] = runtimeConfig.tts_voice;
  doc["mic"] = appState.lastMicInfo;
  doc["mic_avg"] = appState.currentMicAvg;
  doc["speaker"] = appState.lastSpeakerInfo;
  doc["vu"] = appState.vuMode;
  doc["volume"] = appState.speakerVolume;
  doc["assistant"] = getAssistantModeName(appState.assistantMode);

  String out;
  serializeJsonPretty(doc, out);
  server.send(200, "application/json", out);
}

void setupHttp() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/ask", HTTP_POST, handleAsk);
  server.on("/tts-test", HTTP_POST, handleTtsTest);
  server.on("/tts-play", HTTP_POST, handleTtsPlay);
  server.on("/stt-test", HTTP_POST, handleSttTest);
  server.on("/ask-recording", HTTP_POST, handleAskRecording);
  server.on("/assistant-set", HTTP_POST, handleAssistantSet);
  server.on("/mic-test", HTTP_POST, handleMicTest);
  server.on("/vu-on", HTTP_POST, handleVuOn);
  server.on("/vu-off", HTTP_POST, handleVuOff);
  server.on("/speaker-test", HTTP_POST, handleSpeakerTest);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/rec-start", HTTP_POST, handleRecStart);
  server.on("/rec-stop", HTTP_POST, handleRecStop);
  server.on("/rec-file", HTTP_GET, handleRecFile);
  server.on("/config-get", HTTP_GET, handleConfigGet);
  server.on("/config-set", HTTP_POST, handleConfigSet);
  server.on("/rec-play", HTTP_POST, handlePlayRecording);
  server.begin();
}

void handleHttp() {
  server.handleClient();
}
