# ESP32 Incubator PlatformIO Project

This project was converted from the provided Arduino `.ino` sketch into a PlatformIO structure for ESP32 Arduino.

## Structure
- `platformio.ini` - PlatformIO board/framework/dependency config
- `src/main.cpp` - converted sketch

## Notes
- The project targets `esp32-s3-devkitc-1` by default.
- `D7`, `D2`, `A9` style aliases from the original sketch were converted to numeric GPIO values for PlatformIO/ESP32 use. You may need to adjust them to match your exact board pinout.
- Built-in headers like `Wire.h`, `SPI.h`, and `math.h` do not need `lib_deps` in PlatformIO.
- The Grove water level readout was changed to a startup-stable version that samples repeatedly after boot to reduce the wet-at-startup issue.
- Some third-party library names in PlatformIO may vary by version. The current project uses GitHub/registry references that should be a good starting point, but you may need to tweak one or two if your exact hardware library differs.

## How to use
1. Open this folder in VS Code with PlatformIO installed.
2. Let PlatformIO resolve dependencies.
3. Connect your ESP32 board.
4. Build and upload.
5. Adjust board type and pin mapping if needed.
