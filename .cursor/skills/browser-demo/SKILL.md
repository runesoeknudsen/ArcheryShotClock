---
name: browser-demo
description: Work on the WASM browser demo of the shot clock. Use when changing web/demo, src/browser, scripts/build_wasm.sh, or GitHub Pages.
---

# Browser demo

The shareable demo lives in `software/web/demo/`.
It runs the same C++ core as the ESP32, compiled to WebAssembly.

## Always

1. Change clock rules in `software/firmware/src/core/`, not in JavaScript.
2. Rebuild WASM with `./software/tools/build_wasm.sh` after a core or `software/firmware/src/browser` change, then `./software/tools/sync_pages.sh`.
3. Keep `web/index.html` free of demo-only code. The firmware embed must stay small.
4. Serve the demo over HTTP. Do not open `index.html` as a file.
5. Commit rebuilt `web/demo/core.js` and `web/demo/core.wasm` with core changes. The browser CI job does not run Emscripten.
6. Keep `tests/browser/teardown-upload-demo.js` so the existing CI zip always contains `test-results/browser-demo`. Workflow file edits need the `workflow` token scope.

## Never

1. Never put `src/browser/` into the ESP32 build. `platformio.ini` excludes it.
2. Never invent a second clock in JavaScript.
3. Never tell reviewers that a 64×32 preview is what the board draws. Firmware is 32×16.

## Commands

```text
source /path/to/emsdk_env.sh
./software/tools/build_wasm.sh
python3 -m http.server 4173 --directory software/web
npm test
```
