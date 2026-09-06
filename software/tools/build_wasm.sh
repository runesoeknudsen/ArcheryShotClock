#!/bin/sh
# Compiles the Arduino-free core plus the browser host to WebAssembly.
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
firmware="$root/firmware"
out="$root/web/demo"
mkdir -p "$out"

if ! command -v emcc >/dev/null 2>&1; then
  echo "emcc not found. Install Emscripten and source emsdk_env.sh" >&2
  exit 1
fi

sources=""
for file in "$firmware"/src/core/*.cpp "$firmware"/src/browser/demo_api.cpp; do
  sources="$sources $file"
done

em++ $sources \
  -I "$firmware/src" \
  -I "$firmware/src/core" \
  -I "$firmware/src/browser" \
  -std=c++17 \
  -O2 \
  -fno-exceptions \
  -s DEFAULT_TO_CXX=1 \
  -s WASM=1 \
  -s MODULARIZE=1 \
  -s EXPORT_ES6=1 \
  -s EXPORT_NAME=createDemoModule \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s ENVIRONMENT=web \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString"]' \
  -s EXPORTED_FUNCTIONS='["_demo_init","_demo_tick","_demo_control","_demo_session","_demo_display","_demo_clock_seconds","_demo_panel_options","_demo_brightness","_demo_sound","_demo_test_tone","_demo_score","_demo_trace_level","_demo_state_json","_demo_log_json","_demo_panel_columns","_demo_panel_rows","_demo_logical_pixels","_demo_pixel","_demo_sound_active"]' \
  -o "$out/core.js"

echo "wrote $out/core.js and $out/core.wasm"
