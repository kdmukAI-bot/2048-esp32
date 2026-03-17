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

### Build System
Docker-based builds using a pre-built GHCR base image (`Dockerfile.ghcr`) with ESP-IDF v5.5.1 baked in. The local Docker flow and CI workflow use the same image.

```bash
make docker-build    # One-shot firmware build
make docker-shell    # Interactive shell in build container
make docker-flash    # Flash via USB (needs --device access)
```

Native build (requires ESP-IDF on host):
```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

### Build Scripts
- `scripts/setup_env.sh` — Sources ESP-IDF from prebaked image path
- `scripts/build_firmware.sh` — Runs `idf.py set-target` + `idf.py build`
- `scripts/docker_build.sh` — Container entry point: sets up HOME/cache, calls setup + build
- `scripts/docker_flash.sh` — Container entry point for flashing via serial

### CI/CD
- `build-base-image.yml` — Builds/pushes GHCR base image (ESP-IDF only, triggered by Dockerfile changes)
- `build-firmware.yml` — Builds firmware on push/PR using GHCR base image, uploads artifacts

### Dependencies
- ESP-IDF v5.5.1 (prebaked in GHCR base image)
- LVGL v8 (via ESP Component Registry: lvgl/lvgl ^8)
- esp_lcd_axs15231b driver (via ESP Component Registry)
- esp_io_expander_tca9554 (via ESP Component Registry)

## Git & GitHub
- **Never** run `git commit`, `git push`, or any variant without explicit user permission.
- After making code changes, report what was done and wait for the user to request a commit or push.

## Builds & Compilation
- **Never** start a build or compilation without explicit user permission.
- Do **not** automatically iterate through build-debug-rebuild cycles unless the user explicitly requests it.
