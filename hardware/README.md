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

Build `waveshare_s3_hub75` for the Waveshare ESP32-S3 RGB Matrix Driver Board. The HUB75 ribbon pins are fixed on that board:

| HUB75 | GPIO | HUB75 | GPIO |
|---|---|---|---|
| R1 | 4 | R2 | 7 |
| G1 | 5 | G2 | 15 |
| B1 | 6 | B2 | 16 |
| A | 18 | B | 8 |
| C | 3 | D | 42 |
| E | 9 | CLK | 41 |
| LAT | 40 | OE | 2 |

Supported P5 64×32 layouts, set from the web UI:

- One panel, kept horizontal: 64×32, or 32×64 when the cabinet is rotated 90°
- Two to five panels stood on end, long side against long side: 64×64, 96×64, 128×64 or 160×64. Rotate the whole cabinet 90° for a tall stand (64×64, 64×96, 64×128 or 64×160) so more lines fit. Chain the HUB75 ribbon left to right; each of those modules is rotated 90° clockwise.

Lines share the available height by default, and the last line takes any leftover pixels. You can instead pick one large line and keep the others the same size.

The same sizes also work on the original addressable-LED ESP32 build. Extra 32×8 WS2812B modules follow the existing rule: the first module in the chain is at the bottom, and every other row from the top is rotated 180°.

HUB75 occupies many GPIOs, so the S3 console and MAX98357A move to the expansion header:

| Function | GPIO |
|---|---|
| Start / handoff | 11 |
| Stop | 12 |
| Line clear | 13 |
| Next end | 17 |
| Suspend / resume | 1 |
| Emergency stop | 10 |
| Active buzzer | 38 |
| I2S BCLK | 21 |
| I2S LRC | 47 |
| I2S DIN | 48 |
| I2S SD | 14 |

Power the P5 panels from a 5 V supply sized for the module count. Do not power them from the ESP32 3.3 V pin.

Future schematics, PCB layouts, bills of materials, CAD, and manufacturing files belong under the `electronics/` and `mechanical/` directories and use the same hardware license.
