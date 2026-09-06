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

Future schematics, PCB layouts, bills of materials, CAD, and manufacturing files belong under the `electronics/` and `mechanical/` directories and use the same hardware license.
