# 2048 for ESP32-S3

A touchscreen implementation of the classic [2048 game](https://github.com/gabrielecirulli/2048) running on the Waveshare ESP32-S3 Touch LCD 3.5B. Pure C/C++ with LVGL v8 — no MicroPython, no Python.

## Hardware

- **Board:** [Waveshare ESP32-S3 Touch LCD 3.5B](https://www.waveshare.com/esp32-s3-touch-lcd-3.5b.htm)
- **Display:** 480×320 AXS15231B QSPI (landscape orientation)
- **Touch:** Capacitive via I2C
- **PMIC:** AXP2101 (battery + power management)
- **Memory:** 16MB Flash, 8MB PSRAM

## Features

- Classic 2048 gameplay with swipe gestures
- Tile animations (pop-in for new tiles, pulse on merge)
- Score tracking with best score persisted to NVS flash
- Win/lose overlays with "Keep Playing" and "Try Again" options
- Full 2048 color palette

## Build

### Docker (recommended)

No host toolchain installation required. Uses a pre-built GHCR base image with ESP-IDF v5.5.1 baked in.

```bash
# Interactive shell inside the build container
make docker-shell

# One-shot build
make docker-build

# Flash (requires USB device access)
make docker-flash
```

The base image is built from `Dockerfile.ghcr` and published to GHCR by the `build-base-image.yml` CI workflow. Local builds pull it automatically.

### Native

Requires [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/) v5.x installed on the host.

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

### CI

GitHub Actions builds firmware on every push/PR using the same GHCR base image. Firmware artifacts are uploaded for download.

## Project Structure

```
├── main/
│   └── main.c                  # BSP init sequence + game launch
├── components/
│   ├── esp_bsp/                # Display, touch, I2C, PMIC drivers
│   ├── esp_lv_port/            # LVGL display/input porting layer
│   ├── XPowersLib/             # AXP2101 PMIC library
│   └── game_2048/              # Game logic, LVGL UI, gesture input
│       ├── game_logic.c        # Pure C game engine (no LVGL dependency)
│       ├── game_ui.c           # LVGL rendering, animations, overlays
│       └── game_gesture.c      # Touch swipe → direction mapping
├── sdkconfig.defaults          # Flash, PSRAM, LVGL font config
└── partitions.csv              # 2MB app + NVS
```

The BSP drivers (`esp_bsp`, `esp_lv_port`, `XPowersLib`) are copied from the [seedsigner-c-modules](https://github.com/SeedSigner/seedsigner-c-modules) repository, stripped of camera/audio/SD/IMU/RTC dependencies.

## Architecture

The game logic in `game_logic.c` is a standalone C module with no UI dependencies — it manages the 4×4 grid, tile merging, scoring, and win/lose detection. The UI layer in `game_ui.c` renders the board using LVGL widgets and drives the game loop in response to swipe gestures detected by `game_gesture.c`.

## Credits

- Game concept by [Gabriele Cirulli](https://github.com/gabrielecirulli/2048)
- BSP drivers from [SeedSigner](https://github.com/SeedSigner)
