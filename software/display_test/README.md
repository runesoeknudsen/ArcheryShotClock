# HUB75 pixel-mapping test

This is a standalone PlatformIO project for testing one 64x32, 1/8-scan YS-P5
panel on the Waveshare ESP32-S3-RGB-Matrix board. It does not use or modify the
shot-clock firmware.

The test is adapted from the upstream
[Pixel_Mapping_Test example](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/tree/master/examples/Pixel_Mapping_Test).
It starts with the `TYPE595` decoder enabled, as suggested in
[issue #955](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/issues/955).
The copied example is distributed under the upstream repository's MIT license;
see [UPSTREAM-LICENSE.txt](UPSTREAM-LICENSE.txt).

From the repository root, build, upload, and monitor with:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d software/display_test -e waveshare_s3_hub75
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d software/display_test -t upload -e waveshare_s3_hub75
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d software/display_test -t monitor -e waveshare_s3_hub75
```

The sketch scans red pixels from top-left to bottom-right, then clears the
panel. To tune the mapping after the initial test, change `pixelBase` and the
row-block mapping in `src/main.cpp` following the upstream example README.
Keep the Waveshare pin list and panel dimensions unchanged unless the hardware
or wiring changes.