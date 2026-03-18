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

## Desktop Simulator

Pre-built binaries for macOS, Linux, and Windows are available as CI artifacts from the [Actions tab](../../actions). Download the artifact for your platform from the latest successful build.

### Running pre-built binaries

**macOS**

The binary is unsigned, so macOS will block it by default. After downloading and unzipping:

```bash
# Remove the quarantine attribute
xattr -d com.apple.quarantine game_2048_desktop

# Run
./game_2048_desktop
```

Requires SDL2: `brew install sdl2`

**Linux**

```bash
chmod +x game_2048_desktop
./game_2048_desktop
```

Requires SDL2: `sudo apt install libsdl2-2.0-0` (Ubuntu/Debian) or equivalent for your distro.

**Windows**

The download includes `game_2048_desktop.exe` and `SDL2.dll`. Keep both files in the same directory and double-click the exe. No additional dependencies needed.

### Controls

| Input | Action |
|---|---|
| Arrow keys / WASD | Swipe direction |
| Mouse click | "New Game" button |
| Close window / Ctrl+C | Quit |

### Building from source

Requires SDL2, cmake, pkg-config (Linux/macOS), and LVGL v8.3 installed on the system.

**macOS:**

```bash
brew install sdl2 cmake pkg-config

# Install LVGL v8.3 from source
git clone --depth 1 --branch release/v8.3 https://github.com/lvgl/lvgl.git /tmp/lvgl
cp desktop/lv_conf.h /tmp/lvgl/
cd /tmp/lvgl && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DLV_CONF_PATH=/tmp/lvgl/lv_conf.h -DBUILD_SHARED_LIBS=OFF
make -j$(sysctl -n hw.ncpu)
sudo make install
cd -

# Build and run
make desktop-build
make desktop-run
```

**Linux (Ubuntu/Debian):**

```bash
sudo apt install libsdl2-dev cmake pkg-config

# Install LVGL v8.3 from source
git clone --depth 1 --branch release/v8.3 https://github.com/lvgl/lvgl.git /tmp/lvgl
cp desktop/lv_conf.h /tmp/lvgl/
cd /tmp/lvgl && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DLV_CONF_PATH=/tmp/lvgl/lv_conf.h -DBUILD_SHARED_LIBS=OFF
make -j$(nproc)
sudo make install && sudo ldconfig
cd -

# Build and run
make desktop-build
make desktop-run
```

## ESP32 Firmware

### Docker build (recommended)

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

### Native build

Requires [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/) v5.x installed on the host.

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

### CI

GitHub Actions builds firmware and desktop simulators on every push/PR. Artifacts are uploaded for download from the [Actions tab](../../actions).

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
├── desktop/
│   ├── main.c                  # SDL2 desktop simulator entry point
│   ├── CMakeLists.txt          # Desktop build (Linux/macOS/Windows)
│   └── lv_conf.h               # LVGL config for desktop builds
├── fonts/                      # Clear Sans font files (LVGL format)
├── sdkconfig.defaults          # Flash, PSRAM, LVGL font config
└── partitions.csv              # 2MB app + NVS
```

The BSP drivers (`esp_bsp`, `esp_lv_port`, `XPowersLib`) are copied from the [seedsigner-c-modules](https://github.com/SeedSigner/seedsigner-c-modules) repository, stripped of camera/audio/SD/IMU/RTC dependencies.

## Architecture

The game logic in `game_logic.c` is a standalone C module with no UI dependencies — it manages the 4×4 grid, tile merging, scoring, and win/lose detection. The UI layer in `game_ui.c` renders the board using LVGL widgets and drives the game loop in response to swipe gestures detected by `game_gesture.c`.

## Credits

- Game concept by [Gabriele Cirulli](https://github.com/gabrielecirulli/2048)
- BSP drivers from [SeedSigner](https://github.com/SeedSigner)
