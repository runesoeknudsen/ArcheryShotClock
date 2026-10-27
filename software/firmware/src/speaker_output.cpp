#include "speaker_output.h"

#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "core/volume.h"

#ifdef BOARD_WAVESHARE_S3
#include <Wire.h>
#endif

#if __has_include("ESP_I2S.h")
#include "ESP_I2S.h"
#define SPEAKER_USE_ESP_I2S 1
#else
#include "driver/i2s.h"
#include "esp_idf_version.h"
#define SPEAKER_USE_ESP_I2S 0
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

#if SPEAKER_USE_ESP_I2S
I2SClass i2sPort;
#endif

#ifdef BOARD_WAVESHARE_S3
constexpr uint8_t ES8311_ADDRESS = 0x18;

bool writeCodecRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ES8311_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readCodecRegister(uint8_t reg, uint8_t& value) {
  Wire.beginTransmission(ES8311_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 ||
      Wire.requestFrom(static_cast<uint16_t>(ES8311_ADDRESS), static_cast<uint8_t>(1), true) != 1) {
    return false;
  }
  if (Wire.available() == 0) return false;
  value = static_cast<uint8_t>(Wire.read());
  return true;
}

bool initializeWaveshareCodec() {
  if (!Wire.begin(Config::I2C_SDA_PIN, Config::I2C_SCL_PIN, 400000)) return false;
  Wire.beginTransmission(ES8311_ADDRESS);
  if (Wire.endTransmission() != 0) return false;

  if (!writeCodecRegister(0x00, 0x1F)) return false;
  delay(20);
  if (!writeCodecRegister(0x00, 0x00) || !writeCodecRegister(0x00, 0x80) ||
      !writeCodecRegister(0x01, 0x3F)) {
    return false;
  }

  uint8_t reg = 0;
  if (!readCodecRegister(0x06, reg) || !writeCodecRegister(0x06, reg & ~0x20U)) return false;

  // 44.1 kHz, 16-bit stereo with the codec clocked as an I2S slave.
  if (!writeCodecRegister(0x02, 0x00) || !writeCodecRegister(0x03, 0x10) ||
      !writeCodecRegister(0x04, 0x10) || !writeCodecRegister(0x05, 0x00)) {
    return false;
  }
  if (!readCodecRegister(0x06, reg) || !writeCodecRegister(0x06, (reg & 0xE0U) | 0x03U)) return false;
  if (!readCodecRegister(0x07, reg) || !writeCodecRegister(0x07, reg & 0xC0U) ||
      !writeCodecRegister(0x08, 0xFF)) {
    return false;
  }

  return writeCodecRegister(0x09, 0x0C) && writeCodecRegister(0x0A, 0x0C) &&
         writeCodecRegister(0x0D, 0x01) && writeCodecRegister(0x0E, 0x02) &&
         writeCodecRegister(0x12, 0x00) && writeCodecRegister(0x13, 0x10) &&
         writeCodecRegister(0x1C, 0x6A) && writeCodecRegister(0x32, 0xFF) &&
         writeCodecRegister(0x37, 0x08);
}
#endif

void buildToneTable() {
  for (uint16_t index = 0; index < TONE_TABLE_SIZE; index++) {
    toneTable[index] = static_cast<int16_t>(sinf(TAU * index / TONE_TABLE_SIZE) * TONE_AMPLITUDE);
  }
  phaseStep = static_cast<uint32_t>((TONE_FREQUENCY * TONE_TABLE_SIZE / SAMPLE_RATE) * 65536.0f);
}

bool startI2s() {
#if SPEAKER_USE_ESP_I2S
#ifdef BOARD_WAVESHARE_S3
  i2sPort.setPins(Config::I2S_BCLK_PIN, Config::I2S_LRC_PIN, Config::I2S_DOUT_PIN, -1,
                  Config::I2S_MCLK_PIN);
#else
  i2sPort.setPins(Config::I2S_BCLK_PIN, Config::I2S_LRC_PIN, Config::I2S_DOUT_PIN);
#endif
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
#ifdef BOARD_WAVESHARE_S3
  pins.mck_io_num = Config::I2S_MCLK_PIN;
#else
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
#endif
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
#if SPEAKER_USE_ESP_I2S
  i2sPort.write(reinterpret_cast<const uint8_t*>(stereo), frames * 4);
#else
  size_t written = 0;
  i2s_write(I2S_NUM_0, stereo, frames * 4, &written, portMAX_DELAY);
#endif
}
}  // namespace

SpeakerOutput::SpeakerOutput()
    : volume_(Core::DEFAULT_VOLUME), active_(false), ready_(false), toneUntil_(0) {}

void SpeakerOutput::begin() {
  buildToneTable();

  pinMode(Config::SPEAKER_ENABLE_PIN, OUTPUT);
#ifdef BOARD_WAVESHARE_S3
  digitalWrite(Config::SPEAKER_ENABLE_PIN, LOW);
  if (!initializeWaveshareCodec()) {
    Serial.println("Waveshare ES8311 codec initialization failed");
    ready_ = false;
    return;
  }
#endif

  if (!startI2s()) {
    Serial.println("Speaker I2S begin failed");
    ready_ = false;
    return;
  }

  ready_ = true;
  // Core 1 keeps the I2S clock fed without sharing the Wi-Fi/RMT core.
  xTaskCreatePinnedToCore(audioTaskThunk, "speaker", 4096, this, 5, nullptr, 1);
  applyShutdown();
#ifdef BOARD_WAVESHARE_S3
  Serial.printf("Waveshare speaker ready on MCLK=%u BCLK=%u LRC=%u DOUT=%u EN=%u volume=%u\n",
                Config::I2S_MCLK_PIN, Config::I2S_BCLK_PIN, Config::I2S_LRC_PIN, Config::I2S_DOUT_PIN,
                Config::SPEAKER_ENABLE_PIN, volume_);
#else
  Serial.printf("MAX98357A ready on BCLK=%u LRC=%u DIN=%u SD=%u volume=%u\n", Config::I2S_BCLK_PIN,
                Config::I2S_LRC_PIN, Config::I2S_DOUT_PIN, Config::SPEAKER_ENABLE_PIN, volume_);
#endif
}

void SpeakerOutput::update(uint32_t now) {
  if (toneUntil_ && now >= toneUntil_) {
    toneUntil_ = 0;
    active_ = false;
  }
}

void SpeakerOutput::setActive(bool active) {
  if (active) toneUntil_ = 0;
  active_ = active;
}

void SpeakerOutput::setVolume(uint8_t volume) {
  volume_ = Core::clampVolume(volume);
  if (ready_) applyShutdown();
}

void SpeakerOutput::playTestTone(uint32_t now, uint32_t durationMs) {
  toneUntil_ = now + durationMs;
  active_ = true;
  Serial.printf("Speaker test tone for %u ms (volume=%u ready=%d)\n", durationMs, volume_, ready_);
}

void SpeakerOutput::applyShutdown() {
  // Both the MAX98357A SD pin and Waveshare's speaker-enable pin are active high.
  digitalWrite(Config::SPEAKER_ENABLE_PIN, volume_ > 0 ? HIGH : LOW);
}

void SpeakerOutput::audioTaskThunk(void* context) { static_cast<SpeakerOutput*>(context)->audioTask(); }

void SpeakerOutput::audioTask() {
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
