#include "audio_in.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include "app_config.h"
#include "app_state.h"

extern AppState appState;

bool initMicI2S() {
  if (appState.micI2sActive) return true;

  const i2s_port_t port = I2S_NUM_0;
  i2s_driver_uninstall(port);

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = 16000;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = false;
  cfg.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = MIC_SCK;
  pins.ws_io_num = MIC_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = MIC_SD;

  esp_err_t err = i2s_driver_install(port, &cfg, 0, nullptr);
  if (err != ESP_OK) {
    appState.lastMicInfo = "i2s install fail";
    return false;
  }

  err = i2s_set_pin(port, &pins);
  if (err != ESP_OK) {
    appState.lastMicInfo = "i2s pin fail";
    i2s_driver_uninstall(port);
    return false;
  }

  err = i2s_zero_dma_buffer(port);
  if (err != ESP_OK) {
    appState.lastMicInfo = "dma fail";
    i2s_driver_uninstall(port);
    return false;
  }

  appState.micI2sActive = true;
  return true;
}

void stopMicI2S() {
  if (!appState.micI2sActive) return;
  i2s_driver_uninstall(I2S_NUM_0);
  appState.micI2sActive = false;
}

long readMicAverage() {
  const i2s_port_t port = I2S_NUM_0;
  int32_t samples[256];
  size_t bytesRead = 0;

  esp_err_t err = i2s_read(port, samples, sizeof(samples), &bytesRead, pdMS_TO_TICKS(120));
  if (err != ESP_OK || bytesRead == 0) {
    appState.lastMicInfo = "keine daten";
    appState.currentMicAvg = 0;
    return 0;
  }

  int64_t sumAbs = 0;
  size_t count = bytesRead / sizeof(int32_t);

  for (size_t i = 0; i < count; i++) {
    int32_t v = samples[i] >> 14;
    sumAbs += llabs(v);
  }

  long avg = (count > 0) ? (long)(sumAbs / (int64_t)count) : 0;
  appState.currentMicAvg = avg;
  appState.lastMicInfo = String("avg=") + avg + " bytes=" + bytesRead;
  return avg;
}