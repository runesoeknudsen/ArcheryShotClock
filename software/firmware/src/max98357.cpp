#include "max98357.h"

#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "core/volume.h"

#if __has_include("ESP_I2S.h")
#include "ESP_I2S.h"
#define MAX98357_USE_ESP_I2S 1
#else
#include "driver/i2s.h"
#include "esp_idf_version.h"
#define MAX98357_USE_ESP_I2S 0
#endif

namespace {
constexpr uint32_t SAMPLE_RATE = 44100;
constexpr float TONE_FREQUENCY = 880.0f;
constexpr float TAU = 6.28318530717958647692f;
constexpr int16_t TONE_AMPLITUDE = 18000;
constexpr uint16_t TONE_TABLE_SIZE = 256;
constexpr uint16_t TONE_TABLE_MASK = TONE_TABLE_SIZE - 1;
constexpr uint16_t GAIN_UNITY = 256;
constexpr size_t FRAMES_PER_WRITE = 128;

int16_t toneTable[TONE_TABLE_SIZE] = {};
uint32_t phaseStep = 0;

#if MAX98357_USE_ESP_I2S
I2SClass i2sPort;
#endif

void buildToneTable() {
  for (uint16_t index = 0; index < TONE_TABLE_SIZE; index++) {
    toneTable[index] = static_cast<int16_t>(sinf(TAU * index / TONE_TABLE_SIZE) * TONE_AMPLITUDE);
  }
  phaseStep = static_cast<uint32_t>((TONE_FREQUENCY * TONE_TABLE_SIZE / SAMPLE_RATE) * 65536.0f);
}

bool startI2s() {
#if MAX98357_USE_ESP_I2S
  i2sPort.setPins(Config::I2S_BCLK_PIN, Config::I2S_LRC_PIN, Config::I2S_DOUT_PIN);
  return i2sPort.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
#else
  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = 0;
  config.dma_buf_count = 8;
  config.dma_buf_len = 64;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;

  i2s_pin_config_t pins = {};
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
#endif
  pins.bck_io_num = Config::I2S_BCLK_PIN;
  pins.ws_io_num = Config::I2S_LRC_PIN;
  pins.data_out_num = Config::I2S_DOUT_PIN;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  if (i2s_driver_install(I2S_NUM_0, &config, 0, nullptr) != ESP_OK) return false;
  return i2s_set_pin(I2S_NUM_0, &pins) == ESP_OK;
#endif
}

void writeI2s(const int16_t* stereo, size_t frames) {
#if MAX98357_USE_ESP_I2S
  i2sPort.write(reinterpret_cast<const uint8_t*>(stereo), frames * 4);
#else
  size_t written = 0;
  i2s_write(I2S_NUM_0, stereo, frames * 4, &written, portMAX_DELAY);
#endif
}
}  // namespace

Max98357Output::Max98357Output()
    : volume_(Core::DEFAULT_VOLUME), active_(false), ready_(false), toneUntil_(0) {}

void Max98357Output::begin() {
  buildToneTable();

  pinMode(Config::I2S_SD_PIN, OUTPUT);
  applyShutdown();

  if (!startI2s()) {
    Serial.println("MAX98357A I2S begin failed");
    ready_ = false;
    return;
  }

  ready_ = true;
  // Core 1 keeps the I2S clock fed without sharing the Wi-Fi/RMT core.
  xTaskCreatePinnedToCore(audioTaskThunk, "max98357", 4096, this, 5, nullptr, 1);
  Serial.printf("MAX98357A ready on BCLK=%u LRC=%u DIN=%u SD=%u volume=%u\n", Config::I2S_BCLK_PIN,
                Config::I2S_LRC_PIN, Config::I2S_DOUT_PIN, Config::I2S_SD_PIN, volume_);
}

void Max98357Output::update(uint32_t now) {
  if (toneUntil_ && now >= toneUntil_) {
    toneUntil_ = 0;
    active_ = false;
  }
}

void Max98357Output::setActive(bool active) {
  if (active) toneUntil_ = 0;
  active_ = active;
}

void Max98357Output::setVolume(uint8_t volume) {
  volume_ = Core::clampVolume(volume);
  applyShutdown();
}

void Max98357Output::playTestTone(uint32_t now, uint32_t durationMs) {
  toneUntil_ = now + durationMs;
  active_ = true;
  Serial.printf("MAX98357A test tone for %u ms (volume=%u ready=%d)\n", durationMs, volume_, ready_);
}

void Max98357Output::applyShutdown() {
  // SD high selects the left channel and powers the amp. Volume 0 pulls it
  // low so the speaker is electrically silent rather than playing a quiet hiss.
  digitalWrite(Config::I2S_SD_PIN, volume_ > 0 ? HIGH : LOW);
}

void Max98357Output::audioTaskThunk(void* context) { static_cast<Max98357Output*>(context)->audioTask(); }

void Max98357Output::audioTask() {
  int16_t stereo[FRAMES_PER_WRITE * 2] = {};
  uint32_t phase = 0;
  uint16_t gain = 0;

  while (true) {
    const uint16_t target = active_ ? GAIN_UNITY : 0;
    const uint8_t volume = volume_;

    for (size_t frame = 0; frame < FRAMES_PER_WRITE; frame++) {
      if (gain < target) gain++;
      else if (gain > target) gain--;

      const int32_t raw = toneTable[(phase >> 16) & TONE_TABLE_MASK];
      phase += phaseStep;
      const int16_t gated = static_cast<int16_t>((raw * gain) >> 8);
      const int16_t sample = Core::scaleSample(gated, volume);
      stereo[frame * 2] = sample;
      stereo[frame * 2 + 1] = sample;
    }

    writeI2s(stereo, FRAMES_PER_WRITE);
  }
}
