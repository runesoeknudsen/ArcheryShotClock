# ESP32 Archery Shot Clock

An independent, unofficial World Archery shot clock. The default build runs on an ESP32-WROOM with two 8x32 WS2812B panels and a MAX98357A speaker. A second build targets the Waveshare ESP32-S3 RGB Matrix Driver Board and HUB75 P5 panels, with one horizontal 64×32 or two to five modules stood on end (64×64, 96×64, 128×64, 160×64), each rotatable 90° for more stacked lines. It implements timing programs, match logic, display, sound signals, web UI, and trace logging for field testing.

This project is not affiliated with or endorsed by World Archery.

## Licensing

The repository uses separate licenses:

- Software: AGPL-3.0-or-later
- Hardware designs and hardware design documentation: CERN-OHL-S-2.0
- Documentation and other original creative content: CC-BY-SA-4.0

See [LICENSE](LICENSE), [NOTICE.md](NOTICE.md), and [LICENSES](LICENSES/). Third-party dependencies and World Archery material retain their own rights and licenses.

## Build and flash

Install PlatformIO and run these commands from the repository root:

```text
pio run -e esp32dev -d software/firmware
pio run -e esp32dev -d software/firmware -t upload
pio device monitor -b 921600
```

For a Waveshare ESP32-S3-RGB-Matrix board and P5 HUB75 panels:

```text
pio run -e waveshare_s3_hub75 -d software/firmware
pio run -e waveshare_s3_hub75 -d software/firmware -t upload
```

The firmware project is in [software/firmware](software/firmware). Its web page is sourced from [software/web/index.html](software/web/index.html) and embedded into the firmware during the build.

Hardware wiring, GPIO assignments, power requirements, and panel orientation are documented in [hardware/README.md](hardware/README.md).

## Tests

Run native firmware tests with PlatformIO:

```text
pio test -e native -d software/firmware
```

Run the trace conformance self-test:

```text
python software/tools/logcheck.py --selftest
```

Install browser dependencies and run the Playwright suite:

```text
npm install
npx playwright install chromium
npm test
```

## Browser demo

The standalone demo lives in [software/web/demo](software/web/demo). Build its WebAssembly module and serve it locally:

```text
./software/tools/build_wasm.sh
python3 -m http.server 4173 --directory software/web
```

Open `http://127.0.0.1:4173/demo/`.

## Documentation

- [Timing programs](docs/timing-programs.md)
- [Rulebook mapping](docs/rulebook-mapping.md)
- [Trace format](docs/trace-format.md)
- [Development plan](docs/plan.md)

## Copyright

Copyright (c) 2026 Rune Soe-Knudsen.
