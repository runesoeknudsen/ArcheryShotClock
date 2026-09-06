#pragma once

#include <Arduino.h>

namespace Config {
constexpr uint8_t DATA_PIN = 13;
constexpr uint8_t SOUND_PIN = 27;
// MAX98357A I2S amplifier. These pins are free of the console and LED data.
constexpr uint8_t I2S_BCLK_PIN = 18;
constexpr uint8_t I2S_LRC_PIN = 19;
constexpr uint8_t I2S_DOUT_PIN = 23;
// SD high = left channel / amp on. SD low = shutdown.
constexpr uint8_t I2S_SD_PIN = 16;
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
constexpr char FIRMWARE_ID[] = "esp-archery-timer 0.2.0";
// The trace carries the full state at 1 Hz plus every change; 921600 keeps
// draining the ring buffer well clear of blocking even under a busy end.
constexpr uint32_t SERIAL_BAUD = 921600;
constexpr uint32_t TRACE_HEARTBEAT_MS = 1000;

// Physical console. All are wired to ground through a switch and read with the
// internal pull-up, so a closed switch reads low. Avoid GPIO 34-39 here: they
// have no internal pull-up. Avoid the strapping pins 0, 2, 12 and 15.
constexpr uint8_t PIN_START = 32;
constexpr uint8_t PIN_STOP = 33;
constexpr uint8_t PIN_LINE_CLEAR = 14;
constexpr uint8_t PIN_NEXT_END = 26;
constexpr uint8_t PIN_SUSPEND = 25;
// Wire this one normally closed, so a broken wire triggers the stop rather
// than disabling it. See hal/buttons.cpp.
constexpr uint8_t PIN_EMERGENCY = 4;
}