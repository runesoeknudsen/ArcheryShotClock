# Hardware

This directory contains hardware design documentation for the Archery Shot Clock. Original hardware designs and hardware design documentation are licensed under CERN-OHL-S-2.0; see [LICENSE](LICENSE).

## Current wiring

The current prototype uses two 8x32 WS2812B panels arranged vertically. Panel 1 is rotated 180 degrees and receives data from Panel 0. Connect ESP32 GPIO 13 to Panel 0 DIN, Panel 0 DOUT to Panel 1 DIN, and connect all grounds together.

Use a regulated 5 V supply sized for 512 LEDs. Do not power the panels from the ESP32 3.3 V pin. A 330-470 ohm data resistor, a large supply capacitor, and a 3.3 V to 5 V level shifter are recommended.

## Console wiring

| Control | GPIO |
|---|---|
| Start / handoff | 32 |
| Stop | 33 |
| Line clear | 14 |
| Next end | 26 |
| Suspend / resume | 25 |
| Emergency stop | 4 |
| Active buzzer | 27 |

Buttons are wired to ground and use the ESP32 internal pull-ups. The emergency stop input is normally closed so a pressed or broken wire is detected.

## MAX98357A

| MAX98357A | ESP32 |
|---|---|
| BCLK | GPIO 18 |
| LRC / WS | GPIO 19 |
| DIN | GPIO 23 |
| SD | GPIO 16 |
| VIN | 5 V speaker supply |
| GND | Common ground |

Do not power the speaker amplifier from the ESP32 3.3 V pin. Pin and panel defaults are defined in `software/firmware/src/config.h`.

## Waveshare ESP32-S3 HUB75

Build `waveshare_s3_hub75` for the Waveshare ESP32-S3-RGB-Matrix board (ESP32-S3-N32R16: 32 MB OPI flash, 16 MB OPI PSRAM). The HUB75 ribbon pins are fixed on that board:

| HUB75 | GPIO | HUB75 | GPIO |
|---|---|---|---|
| R1 | 4 | R2 | 7 |
| G1 | 5 | G2 | 15 |
| B1 | 6 | B2 | 16 |
| A | 18 | B | 8 |
| C | 3 | D | unused (1/8 scan) |
| E | unused (1/8 scan) | CLK | 41 |
| LAT | 40 | OE | 2 |

The YS-P5-320X160 modules are 64×32, 1/8 scan, FM6124HJ shift registers and a TC7559 3-to-8 line decoder. Address lines D and E stay unmapped. The DMA engine drives each module as 128×16; the library's `ScanTypeMapping<FOUR_SCAN_32PX_HIGH>` maps the 64×32 cabinet onto that buffer, with pixel base set to 1.

Supported P5 64×32 layouts, set from the web UI:

- One panel, kept horizontal: 64×32, or 32×64 when the cabinet is rotated 90°
- Two panels stood on end: 64×64
- Three panels stood on end: 96×64
- Four panels stood on end: 128×64
- Five panels stood on end: 160×64

The Waveshare build defaults to three panels (96×64) with one clock line. Rotate the whole cabinet 90° for a tall stand (64×64, 64×96, 64×128 or 64×160) so more lines fit. Chain the HUB75 ribbon left to right; each module is rotated 90° clockwise. Larger cabinets stamp generated sans-serif outlines at the panel resolution with soft edges. Lines share the available height when added, and the last line takes any leftover pixels. Add another typeface with `software/tools/font_to_glyphs.py`, or choose one large line and keep the others the same size.

The same sizes also work on the original addressable-LED ESP32 build. Extra 32×8 WS2812B modules follow the existing rule: the first module in the chain is at the bottom, and every other row from the top is rotated 180°.

HUB75 occupies many GPIOs, so the S3 controls and active buzzer use the expansion header. The
board's onboard ES8311 codec drives a speaker connected to its speaker header; no external
MAX98357A is needed.

| Function | GPIO |
|---|---|
| Start / handoff | 42 |
| Stop | 9 |
| Line clear | 13 |
| Next end | 17 |
| Suspend / resume | 1 |
| Emergency stop | 10 |
| Active buzzer | 14 |
| ES8311 I2C SDA | 47 |
| ES8311 I2C SCL | 48 |
| ES8311 I2S MCLK | 12 |
| ES8311 I2S BCLK | 43 |
| ES8311 I2S LRC | 38 |
| ES8311 I2S DOUT | 21 |
| Speaker amplifier enable | 11 |

Move the Start and Stop button wires to GPIO 42 and GPIO 9 respectively for this
build. GPIO 42 and GPIO 9 are not connected to the HUB75 driver because its D
and E address lines are unused for the supported 1/8-scan panels.

Power the P5 panels from a 5 V supply sized for the module count. Do not power them from the ESP32 3.3 V pin.

Future schematics, PCB layouts, bills of materials, CAD, and manufacturing files belong under the `electronics/` and `mechanical/` directories and use the same hardware license.
