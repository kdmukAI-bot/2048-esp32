# 2048 ESP32-S3 — Project Instructions

## What This Project Is
A standalone 2048 game running on the Waveshare ESP32-S3 Touch LCD 3.5B. Pure C/C++ with LVGL v8 UI and touch input — no MicroPython, no Python.

### Target Hardware
- **Board:** Waveshare ESP32-S3 Touch LCD 3.5B
- **Display:** 480x320 AXS15231B QSPI (landscape via software rotation)
- **Touch:** Capacitive touch via I2C (AXS15231B controller)
- **PMIC:** AXP2101 (power management, battery charging)
- **Flash:** 16MB QIO @ 80MHz
- **PSRAM:** 8MB Octal @ 80MHz

### Architecture
```
main/main.c          — app_main(): BSP init, LVGL init, game launch
components/
  esp_bsp/           — Display, touch, I2C, PMIC drivers (copied from seedsigner-c-modules)
  esp_lv_port/       — LVGL display/input porting layer (copied from seedsigner-c-modules)
  XPowersLib/        — AXP2101 PMIC library (copied from seedsigner-c-modules)
  game_2048/         — Game logic + LVGL UI + gesture detection
```

### BSP Drivers
The `esp_bsp`, `esp_lv_port`, and `XPowersLib` components are copied from `seedsigner-c-modules`. Stripped of camera, audio, SD card, IMU, RTC, and WiFi dependencies.

### Build Commands
```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

### Dependencies
- ESP-IDF v5.x (tested with v5.5.1)
- LVGL v8 (via ESP Component Registry: lvgl/lvgl ^8)
- esp_lcd_axs15231b driver (via ESP Component Registry)
- esp_io_expander_tca9554 (via ESP Component Registry)

## Git & GitHub
- **Never** run `git commit`, `git push`, or any variant without explicit user permission.
- After making code changes, report what was done and wait for the user to request a commit or push.

## Builds & Compilation
- **Never** start a build or compilation without explicit user permission.
- Do **not** automatically iterate through build-debug-rebuild cycles unless the user explicitly requests it.
