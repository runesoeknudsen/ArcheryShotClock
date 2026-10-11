# HUB75 VirtualMatrixPanel test

Standalone PlatformIO project for testing one 64x32, four-scan YS-P5 panel on
the Waveshare ESP32-S3-RGB-Matrix board. It adapts upstream `VirtualMatrixPanel`
Example 3 to this board's pinout and the panel's FM6124 driver and TYPE595 line
decoder. The library version is pinned to 3.0.14, matching the firmware.

The sketch draws colored panel edges, a diagonal, and a label, then displays
the library's panel connectivity pattern. The upstream example is distributed
under the MIT license; see [UPSTREAM-LICENSE.txt](UPSTREAM-LICENSE.txt).

From the repository root, build, upload, and monitor with:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d software/virtual_matrix_test -e waveshare_s3_hub75
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d software/virtual_matrix_test -t upload -e waveshare_s3_hub75
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d software/virtual_matrix_test -t monitor -e waveshare_s3_hub75
```