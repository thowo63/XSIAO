#include "audio_out.h"

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <driver/i2s.h>
#include <math.h>

#include "app_config.h"
#include "app_state.h"
#include "audio_in.h"
#include "runtime_config.h"
#include <LittleFS.h>

extern AppState appState;

static bool initSpeakerI2S(uint32_t sampleRate) {
  const i2s_port_t port = I2S_NUM_1;
  i2s_driver_uninstall(port);

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = sampleRate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  cfg.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = SPK_BCLK;
  pins.ws_io_num = SPK_LRC;
  pins.data_out_num = SPK_DIN;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  esp_err_t err = i2s_driver_install(port, &cfg, 0, nullptr);
  if (err != ESP_OK) {
    appState.lastSpeakerInfo = "spk install fail";
    return false;
  }

  err = i2s_set_pin(port, &pins);
  if (err != ESP_OK) {
    appState.lastSpeakerInfo = "spk pin fail";
    i2s_driver_uninstall(port);
    return false;
  }

  return true;
}

static void stopSpeakerI2S() {
  i2s_zero_dma_buffer(I2S_NUM_1);
  i2s_driver_uninstall(I2S_NUM_1);
}

bool playRecordedWav(String& errorOut) {
  errorOut = "";

  if (!LittleFS.begin(true)) {
    errorOut = "LittleFS Fehler";
    appState.lastSpeakerInfo = errorOut;
    return false;
  }

  File f = LittleFS.open("/record.wav", "r");
  if (!f) {
    errorOut = "record.wav fehlt";
    appState.lastSpeakerInfo = errorOut;
    return false;
  }

  uint8_t header[44];
  if (f.read(header, sizeof(header)) != 44) {
    f.close();
    errorOut = "WAV Header Fehler";
    appState.lastSpeakerInfo = errorOut;
    return false;
  }

  if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
    f.close();
    errorOut = "Kein WAV";
    appState.lastSpeakerInfo = errorOut;
    return false;
  }

  uint16_t numChannels = header[22] | (header[23] << 8);
  uint32_t sampleRate = header[24] | (header[25] << 8) | (header[26] << 16) | (header[27] << 24);
  uint16_t bitsPerSample = header[34] | (header[35] << 8);

  if (bitsPerSample != 16 || (numChannels != 1 && numChannels != 2)) {
    f.close();
    errorOut = "Nur 16bit mono/stereo";
    appState.lastSpeakerInfo = errorOut;
    return false;
  }

  stopMicI2S();

  if (!initSpeakerI2S(sampleRate)) {
    f.close();
    errorOut = "Speaker Init Fehler";
    appState.lastSpeakerInfo = errorOut;
    return false;
  }

  const size_t INBUF = 1024;
  uint8_t inbuf[INBUF];
  int16_t outbuf[INBUF];
  float vol = appState.speakerVolume / 100.0f;

  while (f.available()) {
    int n = f.read(inbuf, INBUF);
    if (n <= 0) break;

    if (numChannels == 2) {
      size_t samples = n / sizeof(int16_t);
      if (samples > INBUF) samples = INBUF;

      int16_t* stereoIn = (int16_t*)inbuf;
      for (size_t i = 0; i < samples; i++) {
        outbuf[i] = (int16_t)(stereoIn[i] * vol);
      }

      size_t bytesWritten = 0;
      esp_err_t err = i2s_write(I2S_NUM_1, outbuf, samples * sizeof(int16_t), &bytesWritten, pdMS_TO_TICKS(500));
      if (err != ESP_OK) {
        f.close();
        stopSpeakerI2S();
        errorOut = "i2s write fail";
        appState.lastSpeakerInfo = errorOut;
        return false;
      }
    } else {
      size_t samples = n / 2;
      if (samples > (INBUF / 2)) samples = INBUF / 2;

      int16_t* mono = (int16_t*)inbuf;
      for (size_t i = 0; i < samples; i++) {
        int16_t s = (int16_t)(mono[i] * vol);
        outbuf[i * 2] = s;
        outbuf[i * 2 + 1] = s;
      }

      size_t bytesWritten = 0;
      esp_err_t err = i2s_write(I2S_NUM_1, outbuf, samples * 2 * sizeof(int16_t), &bytesWritten, pdMS_TO_TICKS(500));
      if (err != ESP_OK) {
        f.close();
        stopSpeakerI2S();
        errorOut = "i2s write fail";
        appState.lastSpeakerInfo = errorOut;
        return false;
      }
    }
  }

  f.close();
  stopSpeakerI2S();
  appState.lastSpeakerInfo = "record wav ok";
  return true;
}



bool runSpeakerTone(int freq, int ms) {
  stopMicI2S();

  if (!initSpeakerI2S(16000)) {
    return false;
  }

  const int sampleRate = 16000;
  const int samplesPerChunk = 256;
  int16_t buffer[samplesPerChunk * 2];

  float phase = 0.0f;
  const float phaseInc = 2.0f * 3.14159265f * freq / sampleRate;
  const int amplitude = 5000;

  unsigned long start = millis();
  while (millis() - start < (unsigned long)ms) {
    for (int i = 0; i < samplesPerChunk; i++) {
      float vol = appState.speakerVolume / 100.0f;
      int16_t s = (int16_t)(sinf(phase) * amplitude * vol);
      phase += phaseInc;
      if (phase > 2.0f * 3.14159265f) phase -= 2.0f * 3.14159265f;
      buffer[i * 2] = s;
      buffer[i * 2 + 1] = s;
    }

    size_t bytesWritten = 0;
    esp_err_t err = i2s_write(I2S_NUM_1, buffer, sizeof(buffer), &bytesWritten, pdMS_TO_TICKS(200));
    if (err != ESP_OK) {
      appState.lastSpeakerInfo = "spk write fail";
      stopSpeakerI2S();
      return false;
    }
  }

  stopSpeakerI2S();
  appState.lastSpeakerInfo = "tone ok";
  return true;
}

static uint16_t rd16(const uint8_t* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd32(const uint8_t* p) {
  return (uint32_t)p[0] |
         ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

bool playWavFromUrl(const String& inputUrl, String& errorOut) {
  errorOut = "";
  stopMicI2S();

  String url = inputUrl;
  if (url.startsWith("/")) {
    url = String(HA_BASE_URL) + url;
  }

  HTTPClient http;
  http.begin(url);

  http.setConnectTimeout(5000);
  http.setTimeout(30000);

  int httpCode = http.GET();
  if (httpCode <= 0) {
    errorOut = String("HTTP Fehler: ") + http.errorToString(httpCode);
    http.end();
    appState.lastSpeakerInfo = errorOut;
    return false;
  }

  if (httpCode != 200) {
    errorOut = String("HTTP ") + httpCode;
    http.end();
    appState.lastSpeakerInfo = errorOut;
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  stream->setTimeout(30000);

  uint8_t header[44];
  int got = stream->readBytes((char*)header, sizeof(header));
  if (got != 44) {
    errorOut = "WAV Header unvollständig";
    http.end();
    appState.lastSpeakerInfo = errorOut;
    return false;
  }

  if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
    errorOut = "Kein WAV/RIFF";
    http.end();
    appState.lastSpeakerInfo = errorOut;
    return false;
  }

  if (memcmp(header + 12, "fmt ", 4) != 0) {
    errorOut = "fmt chunk fehlt";
    http.end();
    appState.lastSpeakerInfo = errorOut;
    return false;
  }

  uint16_t audioFormat   = rd16(header + 20);
  uint16_t numChannels   = rd16(header + 22);
  uint32_t sampleRate    = rd32(header + 24);
  uint16_t bitsPerSample = rd16(header + 34);

  if (audioFormat != 1) {
    errorOut = "Nur PCM WAV";
    http.end();
    appState.lastSpeakerInfo = errorOut;
    return false;
  }

  if (bitsPerSample != 16) {
    errorOut = "Nur 16-bit WAV";
    http.end();
    appState.lastSpeakerInfo = errorOut;
    return false;
  }

  // einfache Annahme: data-Chunk beginnt direkt nach 44 Bytes
  if (memcmp(header + 36, "data", 4) != 0) {
    errorOut = "data chunk nicht an Position 44";
    http.end();
    appState.lastSpeakerInfo = errorOut;
    return false;
  }

  if (numChannels != 1 && numChannels != 2) {
    errorOut = "Nur mono/stereo";
    http.end();
    appState.lastSpeakerInfo = errorOut;
    return false;
  }

  if (!initSpeakerI2S(sampleRate)) {
    http.end();
    errorOut = "Speaker I2S init fail";
    appState.lastSpeakerInfo = errorOut;
    return false;
  }

  static const size_t INBUF = 1024;
  uint8_t inbuf[INBUF];

  // Stereo-Ausgabe für I2S
  int16_t outbuf[INBUF];
  size_t bytesReadTotal = 0;

  while (http.connected()) {
    size_t avail = stream->available();
    if (!avail) {
      delay(1);
      continue;
    }

    size_t toRead = avail > INBUF ? INBUF : avail;
    int n = stream->readBytes((char*)inbuf, toRead);
    if (n <= 0) break;

    bytesReadTotal += (size_t)n;

    float vol = appState.speakerVolume / 100.0f;

    if (numChannels == 2) {
        size_t samples = n / sizeof(int16_t);
        if (samples > INBUF) samples = INBUF;

        int16_t* stereoIn = (int16_t*)inbuf;
        for (size_t i = 0; i < samples; i++) {
            outbuf[i] = (int16_t)(stereoIn[i] * vol);
        }

        size_t bytesWritten = 0;
        esp_err_t err = i2s_write(
            I2S_NUM_1,
            outbuf,
            samples * sizeof(int16_t),
            &bytesWritten,
            pdMS_TO_TICKS(500)
        );

        if (err != ESP_OK) {
            errorOut = "i2s write fail";
            stopSpeakerI2S();
            http.end();
            appState.lastSpeakerInfo = errorOut;
            return false;
        }
            } else {
            size_t samples = n / 2;
            if (samples > (INBUF / 2)) samples = INBUF / 2;

            int16_t* mono = (int16_t*)inbuf;
            for (size_t i = 0; i < samples; i++) {
                int16_t s = (int16_t)(mono[i] * vol);
                outbuf[i * 2]     = s;
                outbuf[i * 2 + 1] = s;
            }

            size_t bytesWritten = 0;
            esp_err_t err = i2s_write(
                I2S_NUM_1,
                outbuf,
                samples * 2 * sizeof(int16_t),
                &bytesWritten,
                pdMS_TO_TICKS(500)
            );

            if (err != ESP_OK) {
                errorOut = "i2s write fail";
                stopSpeakerI2S();
                http.end();
                appState.lastSpeakerInfo = errorOut;
                return false;
            }
            }
        }

    stopSpeakerI2S();
    http.end();

    appState.lastSpeakerInfo = String("wav ok @") + sampleRate + "Hz";
    return true;
    }
static String urlEncode(const String& s) {
  String out;
  char hex[4];

  for (size_t i = 0; i < s.length(); i++) {
    unsigned char c = (unsigned char)s[i];

    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else if (c == ' ') {
      out += "%20";
    } else {
      snprintf(hex, sizeof(hex), "%%%02X", c);
      out += hex;
    }
  }

  return out;
}

String buildTtsUrl(const String& text) {
  String base = runtimeConfig.tts_base_url.length() > 0 ? runtimeConfig.tts_base_url : String(TTS_BASE_URL);
  String url = base + urlEncode(text);

  if (runtimeConfig.tts_voice.length() > 0) {
    url += "&voice=" + urlEncode(runtimeConfig.tts_voice);
  }

  return url;
}
