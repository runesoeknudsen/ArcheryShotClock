#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// C ABI over the Arduino-free core so the same shot clock can run in the
// browser (WASM) and later on the ESP32. No Arduino types here.

void demo_init(uint32_t now_ms);
void demo_tick(uint32_t now_ms);

// Returns 0 on success, 1 on unknown action, 2 if the engine refused it.
int demo_control(const char* action, int32_t arg);
int demo_session(const char* json);
int demo_display(const char* content);
int demo_clock_seconds(int enabled);
int demo_panel_options(const char* json);
int demo_brightness(int brightness);
int demo_sound(const char* json);
void demo_test_tone(uint32_t now_ms, uint32_t duration_ms);
int demo_score(const char* json);
int demo_trace_level(const char* level);

const char* demo_state_json(void);
const char* demo_log_json(uint32_t after_seq);

uint16_t demo_panel_columns(void);
uint16_t demo_panel_rows(void);
// 512 pixels, packed 0x00RRGGBB, logical x + y * 32 (not the LED wiring order).
const uint32_t* demo_logical_pixels(void);
uint32_t demo_pixel(uint16_t index);
int demo_sound_active(void);

#ifdef __cplusplus
}
#endif
