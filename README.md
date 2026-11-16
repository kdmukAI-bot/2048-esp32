# 2048 for ESP32

A touchscreen implementation of the classic [2048 game](https://github.com/gabrielecirulli/2048) running on Waveshare ESP32 touch display boards. Pure C/C++ with LVGL v9; no MicroPython, no Python.

## Supported Boards

| Board | SoC | Display | Touch | Resolution |
|---|---|---|---|---|
| [Waveshare ESP32-S3 Touch LCD 3.5B](https://www.waveshare.com/esp32-s3-touch-lcd-3.5b.htm) | ESP32-S3 | AXS15231B QSPI | AXS15231B | 320x480 |
| [Waveshare ESP32-S3 Touch LCD 3.5](https://www.waveshare.com/esp32-s3-touch-lcd-3.5.htm) | ESP32-S3 | ST7796 SPI | FT6336 | 320x480 |
| [Waveshare ESP32-S3 Touch LCD 2](https://www.waveshare.com/esp32-s3-touch-lcd-2.htm) | ESP32-S3 | ST7789 SPI | CST816D | 240x320 |
| [Waveshare ESP32-P4 WiFi6 Touch LCD 3.5](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-3.5.htm) | ESP32-P4 | ST7796 SPI | FT6336 | 320x480 |
| [Waveshare ESP32-P4 WiFi6 Touch LCD 4.3](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4.3.htm) | ESP32-P4 | ST7701 MIPI-DSI | GT911 | 480x800 |

ESP32-S3 boards: 16MB Flash, 8MB PSRAM. ESP32-P4 boards: 32MB PSRAM, with 16MB Flash on the 3.5" and 32MB on the 4.3". The 3.5" boards include an AXP2101 PMIC; the 2" board does not.

Board selection is compile-time via the `BOARD` variable (default: `waveshare_s3_lcd35b`). ESP32-P4 boards build for chip revision v3.x by default; see [ESP32-P4 chip revision](#esp32-p4-chip-revision) for boards with the older v1.x chips. Hardware abstraction is provided by the [esp-board-common](https://codeberg.org/kdmukAI-bot/esp-board-common) submodule.

## ESP32 Firmware

### Clone

```bash
git clone --recurse-submodules <repo-url>
cd 2048-esp32
```

If you already cloned without `--recurse-submodules`:
```bash
git submodule update --init
```

### Docker build (recommended)

No host toolchain required. Uses a pre-built GHCR base image with ESP-IDF v5.5.5.

```bash
# Build for the default board (3.5B)
make docker-build

# Build for a specific board
make docker-build BOARD=waveshare_p4_lcd35

# Build + collect flashable artifacts to dist/
make dist BOARD=waveshare_s3_lcd35b

# Interactive shell inside the build container
make docker-shell

# Flash via USB (requires device access)
make docker-flash
```

The `dist` target collects the firmware binary, bootloader, partition table, and flash arguments into `dist/<BOARD>/`:

```bash
cd dist/waveshare_s3_lcd35b
esptool --chip esp32s3 -b 460800 write-flash @flash_args
```

ESP32-P4 v3.x chips need a recent esptool: 5.3 or later (or 4.12 or later on the v4 line)
recognizes every v3 revision so far, up to v3.2.

### ESP32-P4 chip revision

ESP32-P4 chips come in two families that cannot share a firmware image: the original v0.x/v1.x
silicon and the v3.x silicon that P4 boards ship with now (Waveshare switched the 4.3" board in
2026). P4 builds target **v3.x by default**. For a board with an older chip, build the `PRE_REV3`
variant:

```bash
make dist BOARD=waveshare_p4_lcd43 BOARD_VARIANT=PRE_REV3
```

The variant packages into `dist/<BOARD>-PRE_REV3/`, and the build fails if the resolved
configuration does not target the intended silicon (v3.0 to v3.99 by default, at most v1.99 for
`PRE_REV3`). Each P4 `dist/` folder includes a `chip_revisions.txt` that records the range.

To check a board, run `esptool -p <port> chip-id`; it prints a line such as `Chip type: ESP32-P4
(revision v3.2)`. Flashing the wrong family fails: esptool refuses to write a bootloader built for
the other family, and if forced, the bootloader stops with `chip revision check failed`.

v3 support needs ESP-IDF v5.5.3 or later. Two v3-specific runtime fixes also apply: the MIPI-DSI
PHY clock is chosen per revision (in esp-board-common), and `app_main` disables the core bus
timeout that otherwise faults slow PSRAM accesses on v3. Background:
[ESP32-P4 chip revision v3](https://github.com/kdmukAI-bot/seedsigner-micropython-builder/blob/main/docs/knowledge/esp32-p4-chip-revision-v3.md)
in the SeedSigner MicroPython builder, which uses the same scheme.

### Native build

Requires [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/) v5.5.x; ESP32-P4 v3.x builds need v5.5.3 or later.

```bash
# ESP32-S3 board
idf.py -DBOARD=waveshare_s3_lcd35b set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor

# ESP32-P4 board (chip revision v3.x)
idf.py -DBOARD=waveshare_p4_lcd35 set-target esp32p4
idf.py build
idf.py -p /dev/ttyACM0 flash monitor

# ESP32-P4 board with an older v0.x/v1.x chip
idf.py -DBOARD=waveshare_p4_lcd43 -DBOARD_VARIANT=PRE_REV3 set-target esp32p4
idf.py build
```

Run `set-target` (or `idf.py fullclean`) whenever you change `BOARD` or `BOARD_VARIANT`: ESP-IDF
only applies the defaults files when it generates a fresh `sdkconfig`. The native flow skips the
chip revision check that `scripts/build_firmware.sh` runs for the Docker and CI builds.

### CI

GitHub Actions builds firmware and desktop simulators on every push/PR. Artifacts are uploaded for download from the [Actions tab](../../actions). Firmware builds use the same `scripts/build_firmware.sh` as the Docker build. The ESP32-P4 firmware targets chip revision v3.x; the Waveshare 4.3" also gets a `firmware-2048-waveshare_p4_lcd43-PRE_REV3` build for boards with the older chips.

The firmware jobs run inside a prebuilt container image, `ghcr.io/kdmukai-bot/2048-esp32-base:idf-<version>`, with ESP-IDF and its toolchains already installed, so a job doesn't reinstall them on every run. `make docker-build` uses the same image, so local and CI builds match. `Dockerfile.ghcr` defines the image, and the **Build Base Builder Image** workflow publishes it on a push to `main` that changes the Dockerfile, or on demand. The firmware workflow and the Makefile pin the `idf-<version>` tag, so after changing `ESP_IDF_REF`, publish the new tag before firmware CI can pass:

```bash
gh workflow run build-base-image.yml --ref <branch>
```

Then update the tag in `.github/workflows/build-firmware.yml` and the `Makefile`.

## Desktop Simulator

Pre-built binaries for macOS, Linux, and Windows are available from the [Releases page](../../releases). Download the archive for your platform from the latest release.

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

Requires SDL2, cmake, pkg-config (Linux/macOS), and LVGL v9.x installed on the system.

**macOS:**

```bash
brew install sdl2 cmake pkg-config

# Install LVGL v9 from source
git clone --depth 1 --branch release/v9.5 https://github.com/lvgl/lvgl.git /tmp/lvgl
cp boards/desktop/lv_conf.h /tmp/lvgl/
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

# Install LVGL v9 from source
git clone --depth 1 --branch release/v9.5 https://github.com/lvgl/lvgl.git /tmp/lvgl
cp boards/desktop/lv_conf.h /tmp/lvgl/
cd /tmp/lvgl && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DLV_CONF_PATH=/tmp/lvgl/lv_conf.h -DBUILD_SHARED_LIBS=OFF
make -j$(nproc)
sudo make install && sudo ldconfig
cd -

# Build and run
make desktop-build
make desktop-run
```

**Windows (requires Visual Studio with C compiler, cmake, and Git Bash or similar):**

```bash
# Download and set up SDL2
curl -L -o sdl2.zip https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-VC.zip
unzip sdl2.zip -d C:/SDL2
cd C:/SDL2/SDL2-2.32.10 && mkdir include/SDL2 && mv include/*.h include/SDL2/

# Install LVGL v9 from source
git clone --depth 1 --branch release/v9.5 https://github.com/lvgl/lvgl.git C:/tmp/lvgl
cp boards/desktop/lv_conf.h C:/tmp/lvgl/
cd C:/tmp/lvgl && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=C:/lvgl-install -DLV_CONF_PATH=C:/tmp/lvgl/lv_conf.h -DBUILD_SHARED_LIBS=OFF
cmake --build . --config Release
cmake --install . --config Release
cd -

# Build
cmake -S boards/desktop -B boards/desktop/build -DCMAKE_PREFIX_PATH="C:/SDL2/SDL2-2.32.10;C:/lvgl-install"
cmake --build boards/desktop/build --config Release

# Copy SDL2.dll next to the exe and run
cp C:/SDL2/SDL2-2.32.10/lib/x64/SDL2.dll boards/desktop/build/Release/
boards/desktop/build/Release/game_2048_desktop.exe
```

## Project Structure

```
├── main/
│   └── game_main.c              # App entry point (app_main) + game launch
├── components/
│   ├── esp-board-common/        # Hardware abstraction (git submodule)
│   │   ├── src/                 # Display, touch, I2C, PMIC, backlight drivers
│   │   └── boards/              # Board configs (pins, peripherals, sdkconfig)
│   └── game_2048/               # Game logic, LVGL UI, gesture input
│       ├── game_logic.c         # Pure C game engine (no LVGL dependency)
│       ├── game_ui.c            # LVGL rendering, animations, overlays
│       ├── game_gesture.c       # Touch swipe detection
│       └── game_screensaver.c   # Bouncing tile screensaver
├── boards/
│   └── desktop/                 # SDL2 desktop simulator
├── fonts/                       # Clear Sans font files (LVGL format)
├── docs/                        # Development notes
├── sdkconfig.defaults           # Shared ESP-IDF settings (LVGL, partitions)
├── sdkconfig.defaults.esp32p4   # ESP32-P4: target chip revision v3.x
├── sdkconfig.p4_pre_rev3        # ESP32-P4: BOARD_VARIANT=PRE_REV3 (v0.x/v1.x chips)
└── partitions.csv               # 2MB app + NVS
```

The [esp-board-common](https://codeberg.org/kdmukAI-bot/esp-board-common) submodule provides all ESP32 hardware drivers and board configurations. This project disables peripherals it doesn't need (SD card, audio, RTC, IMU) via compile definitions in `CMakeLists.txt`. Camera support is a separate esp-board-common component (`board_camera`) that this project never adds to its build, so no camera drivers are fetched or compiled. Board configs (pin assignments, display/touch controller selection, sdkconfig defaults) live in the submodule.

## Architecture

The game logic in `game_logic.c` is a standalone C module with no UI dependencies: it manages the 4x4 grid, tile merging, scoring, and win/lose detection. The UI layer in `game_ui.c` renders the board using LVGL widgets and drives the game loop in response to swipe gestures detected by `game_gesture.c`.

## Credits

- Game concept by [Gabriele Cirulli](https://github.com/gabrielecirulli/2048)
- Hardware abstraction by [esp-board-common](https://codeberg.org/kdmukAI-bot/esp-board-common)
