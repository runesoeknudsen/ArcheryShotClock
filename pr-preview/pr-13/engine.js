// Loads the WASM core and exposes the same shapes the director page posts.
import createDemoModule from './core.js';

export async function createEngine() {
  const Module = await createDemoModule();

  function stateJson() {
    return JSON.parse(Module.UTF8ToString(Module._demo_state_json()));
  }

  function now() {
    return Math.floor(performance.now());
  }

  Module._demo_init(now());

  return {
    now,
    tick() {
      Module._demo_tick(now());
    },
    state() {
      this.tick();
      return stateJson();
    },
    control(action, arg = 0) {
      this.tick();
      return Module.ccall('demo_control', 'number', ['string', 'number'], [action, arg]);
    },
    session(body) {
      this.tick();
      return Module.ccall('demo_session', 'number', ['string'], [JSON.stringify(body)]);
    },
    display(content) {
      this.tick();
      return Module.ccall('demo_display', 'number', ['string'], [content]);
    },
    clockSeconds(enabled) {
      this.tick();
      return Module._demo_clock_seconds(enabled ? 1 : 0);
    },
    panelOptions(body) {
      this.tick();
      return Module.ccall('demo_panel_options', 'number', ['string'], [JSON.stringify(body)]);
    },
    brightness(value) {
      return Module._demo_brightness(value);
    },
    sound(body) {
      return Module.ccall('demo_sound', 'number', ['string'], [JSON.stringify(body)]);
    },
    testTone(ms = 2000) {
      Module._demo_test_tone(now(), ms);
    },
    score(body) {
      this.tick();
      return Module.ccall('demo_score', 'number', ['string'], [JSON.stringify(body)]);
    },
    traceLevel(level) {
      return Module.ccall('demo_trace_level', 'number', ['string'], [level]);
    },
    log(after) {
      this.tick();
      return JSON.parse(Module.UTF8ToString(Module._demo_log_json(after)));
    },
    panelSize() {
      return { columns: Module._demo_panel_columns(), rows: Module._demo_panel_rows() };
    },
    pixels() {
      const columns = Module._demo_panel_columns();
      const rows = Module._demo_panel_rows();
      const count = columns * rows;
      if (Module.HEAPU32) {
        const ptr = Module._demo_logical_pixels();
        return Module.HEAPU32.subarray(ptr >> 2, (ptr >> 2) + count);
      }
      const out = new Uint32Array(count);
      for (let index = 0; index < count; index++) out[index] = Module._demo_pixel(index);
      return out;
    },
    soundActive() {
      return Module._demo_sound_active() === 1;
    }
  };
}
