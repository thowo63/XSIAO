#pragma once

#define ASSISTANT_MODE_HOMEASSISTANT 1
#define ASSISTANT_MODE_OPENAI        2
#define ASSISTANT_MODE_CLAUDE        3
#define ASSISTANT_MODE_ALEXA         4

// #define ASSISTANT_MODE ASSISTANT_MODE_HOMEASSISTANT
#define ASSISTANT_MODE_OPENAI 2
#define ASSISTANT_MODE ASSISTANT_MODE_OPENAI
#define DEFAULT_ASSISTANT_MODE ASSISTANT_MODE_HOMEASSISTANT

#define OPENAI_PROXY_URL "http://192.168.1.94:5003/openai"

#define CLAUDE_PROXY_URL  "http://192.168.1.94:5004/claude"
#define ALEXA_PROXY_URL   "http://192.168.1.94:5005/alexa"

#define DEVICE_NAME "xiaozhi-oled"
#define MQTT_SERVER "192.168.1.94"
#define MQTT_PORT 1883
#define MQTT_USER "homeassistant"
#define MQTT_PASSWORD "oajochoh3ohxohm3wahsheevahveeTairudeetoh2piesh6ahShah7vietiengee"

#define TOPIC_STATUS   "xiaozhi/status"
#define TOPIC_TEXT_IN  "xiaozhi/text/in"
#define TOPIC_TEXT_OUT "xiaozhi/text/out"
#define TOPIC_CMD      "xiaozhi/cmd"
#define TOPIC_MIC      "xiaozhi/mic/level"

#define HTTP_PORT 80

#define OLED_SDA 41
#define OLED_SCL 42

#define MIC_WS   4
#define MIC_SCK  5
#define MIC_SD   6

#define SPK_DIN  7
#define SPK_BCLK 15
#define SPK_LRC  16

#define BTN_VOL_DOWN 39
#define BTN_VOL_UP   40
#define BTN_WAKE     0

#define HA_BASE_URL "http://192.168.1.94:8123"
#define HA_TOKEN    "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiI1ZmUxNjIwOTAwYWY0NGEwOGZhOTdjZGMwMjAwMDUzYSIsImlhdCI6MTc3MzI2ODIwMywiZXhwIjoyMDg4NjI4MjAzfQ.ymJZsynLbHHtD2pob90M_5_ijhzBQ4x28w2XHvwJ9hk"
#define HA_LANGUAGE "de"
#define HA_AGENT_ID ""

// #define HA_TTS_ENGINE_ID "tts.piper"
#define TTS_BASE_URL "http://192.168.1.94:5001/tts?text="
#define STT_BASE_URL "http://192.168.1.94:5002/stt"
#define AUTO_SPEAK_AFTER_ASK 1
#define DEFAULT_SPEAKER_VOLUME 70

#define RECORDING_MAX_MS             15000
#define RECORDING_SILENCE_MS          2000
#define RECORDING_SILENCE_THRESHOLD    180
#define RECORDING_MIN_SPEECH_MS        500
#define RECORDING_EARLIEST_STOP_MS    2500
#define RECORDING_SPEECH_HITS_REQUIRED   1