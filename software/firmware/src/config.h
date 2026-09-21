#pragma once

#include <Arduino.h>

namespace Config {
#ifdef BOARD_WAVESHARE_S3
// Waveshare ESP32-S3-RGB-Matrix HUB75 connector. These GPIOs are fixed on the
// driver board; do not reassign them to the console or the speaker.
constexpr uint8_t HUB75_R1 = 4;
constexpr uint8_t HUB75_G1 = 5;
constexpr uint8_t HUB75_B1 = 6;
constexpr uint8_t HUB75_R2 = 7;
constexpr uint8_t HUB75_G2 = 15;
constexpr uint8_t HUB75_B2 = 16;
constexpr uint8_t HUB75_A = 18;
constexpr uint8_t HUB75_B = 8;
constexpr uint8_t HUB75_C = 3;
constexpr uint8_t HUB75_D = 42;
constexpr uint8_t HUB75_E = 9;
constexpr uint8_t HUB75_LAT = 40;
constexpr uint8_t HUB75_OE = 2;
constexpr uint8_t HUB75_CLK = 41;

// Remaining expansion GPIOs, kept off the HUB75, USB, and Octal PSRAM pins.
constexpr uint8_t DATA_PIN = 13;
constexpr uint8_t SOUND_PIN = 38;
constexpr uint8_t I2S_BCLK_PIN = 21;
constexpr uint8_t I2S_LRC_PIN = 47;
constexpr uint8_t I2S_DOUT_PIN = 48;
constexpr uint8_t I2S_SD_PIN = 14;
constexpr uint8_t PIN_START = 11;
constexpr uint8_t PIN_STOP = 12;
constexpr uint8_t PIN_LINE_CLEAR = 13;
constexpr uint8_t PIN_NEXT_END = 17;
constexpr uint8_t PIN_SUSPEND = 1;
constexpr uint8_t PIN_EMERGENCY = 10;
#else
constexpr uint8_t DATA_PIN = 13;
constexpr uint8_t SOUND_PIN = 27;
// MAX98357A I2S amplifier. These pins are free of the console and LED data.
constexpr uint8_t I2S_BCLK_PIN = 18;
constexpr uint8_t I2S_LRC_PIN = 19;
constexpr uint8_t I2S_DOUT_PIN = 23;
// SD high = left channel / amp on. SD low = shutdown.
constexpr uint8_t I2S_SD_PIN = 16;
constexpr uint8_t PIN_START = 32;
constexpr uint8_t PIN_STOP = 33;
constexpr uint8_t PIN_LINE_CLEAR = 14;
constexpr uint8_t PIN_NEXT_END = 26;
constexpr uint8_t PIN_SUSPEND = 25;
// Wire this one normally closed, so a broken wire triggers the stop rather
// than disabling it. See hal/buttons.cpp.
constexpr uint8_t PIN_EMERGENCY = 4;
#endif

constexpr uint8_t PANEL_COLUMNS = 32;
constexpr uint8_t PANEL_ROWS = 8;
constexpr uint8_t PANEL_COUNT = 2;
constexpr uint8_t DISPLAY_COLUMNS = 32;
constexpr uint8_t DISPLAY_ROWS = 16;
constexpr uint16_t LED_COUNT = PANEL_COLUMNS * PANEL_ROWS * PANEL_COUNT;
constexpr bool PANEL_VERTICAL_SERPENTINE = true;
// Viewed from the front: panel 1 is above panel 0 and rotated 180 degrees.
constexpr uint8_t PANEL_ORDER[PANEL_COUNT] = {1, 0};
constexpr uint32_t DEFAULT_DURATION_SECONDS = 240;
constexpr uint8_t DEFAULT_BRIGHTNESS = 16;
constexpr uint32_t MIN_DURATION_SECONDS = 1;
constexpr uint32_t MAX_DURATION_SECONDS = 5999;
constexpr uint8_t DEFAULT_AP_BRIGHTNESS = 16;
constexpr char AP_NAME[] = "Archery-Timer";
constexpr char AP_PASSWORD[] = "archery123";
// Identifies the firmware in the trace log, so a captured session says which
// build produced it.
constexpr char FIRMWARE_ID[] = "esp-archery-timer 0.3.0";
// The trace carries the full state at 1 Hz plus every change; 921600 keeps
// draining the ring buffer well clear of blocking even under a busy end.
constexpr uint32_t SERIAL_BAUD = 921600;
constexpr uint32_t TRACE_HEARTBEAT_MS = 1000;
}
