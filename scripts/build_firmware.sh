#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

BOARD="${BOARD:-waveshare_s3_lcd35b}"
# Optional board variant. ESP32-P4 boards use it to pick the silicon family: the default
# build targets chip revision v3.x, and BOARD_VARIANT=PRE_REV3 targets the older
# v0.x/v1.x chips. The two families cannot run the same image.
BOARD_VARIANT="${BOARD_VARIANT:-}"
BUILD_NAME="$BOARD${BOARD_VARIANT:+-$BOARD_VARIANT}"
BUILD_DIR="$ROOT_DIR/build"

# Derive SoC target from board name
case "$BOARD" in
    waveshare_s3_*)  TARGET=esp32s3 ;;
    waveshare_p4_*)  TARGET=esp32p4 ;;
    *)
        echo "ERROR: Unknown board '$BOARD': cannot determine SoC target"
        exit 1
        ;;
esac

case "$BOARD_VARIANT" in
    "") ;;
    PRE_REV3)
        if [ "$TARGET" != "esp32p4" ]; then
            echo "ERROR: BOARD_VARIANT=PRE_REV3 only applies to ESP32-P4 boards, not $BOARD"
            exit 1
        fi
        ;;
    *)
        echo "ERROR: Unknown BOARD_VARIANT '$BOARD_VARIANT' (supported: PRE_REV3)"
        exit 1
        ;;
esac

# ESP32-P4 chip revision v3.x needs ESP-IDF v5.5.3 or later. An older IDF does not know
# the v3 Kconfig symbols, drops them without a word, and produces an image capped at
# revision v1.99 that the bootloader refuses to run on a v3 chip. Catch that here,
# before the long compile; the resolved-sdkconfig check after the build is the
# authoritative one.
idf_version_part() {
    sed -n "s/^set(IDF_VERSION_$1 \([0-9]*\))/\1/p" "$IDF_PATH/tools/cmake/version.cmake"
}
IDF_VERSION_STR="$(idf_version_part MAJOR).$(idf_version_part MINOR).$(idf_version_part PATCH)"
IDF_VERSION_NUM=$(( $(idf_version_part MAJOR) * 10000 + $(idf_version_part MINOR) * 100 + $(idf_version_part PATCH) ))
if [ "$TARGET" = "esp32p4" ] && [ "$BOARD_VARIANT" != "PRE_REV3" ] \
        && [ "$IDF_VERSION_NUM" -lt 50503 ]; then
    echo "ERROR: ESP-IDF v$IDF_VERSION_STR cannot target ESP32-P4 chip revision v3.x (needs v5.5.3+)."
    echo "  Use the idf-v5.5.5 base image (see Dockerfile.ghcr), or build for the older"
    echo "  silicon with BOARD_VARIANT=PRE_REV3."
    exit 1
fi

echo "=== Board: $BUILD_NAME (target: $TARGET, ESP-IDF v$IDF_VERSION_STR) ==="

# set-target regenerates sdkconfig from the defaults files every time, so switching
# BOARD or BOARD_VARIANT never builds against a stale configuration.
echo "=== Setting target: $TARGET ==="
idf.py -DBOARD="$BOARD" -DBOARD_VARIANT="$BOARD_VARIANT" set-target "$TARGET"

echo "=== Building firmware ==="
rm -f "$BUILD_DIR/chip_revisions.txt"
idf.py build -DCCACHE_ENABLE=1 2>&1 | tee "${ROOT_DIR}/build-log.txt"

# Confirm the image targets the intended ESP32-P4 silicon family. Read the RESOLVED
# sdkconfig, not the fragments: an IDF that does not know a revision symbol drops it and
# falls back to its own default, which still builds cleanly.
if [ "$TARGET" = "esp32p4" ]; then
    rev_min="$(sed -n 's/^CONFIG_ESP_REV_MIN_FULL=//p' "$ROOT_DIR/sdkconfig")"; rev_min="${rev_min:-0}"
    rev_max="$(sed -n 's/^CONFIG_ESP_REV_MAX_FULL=//p' "$ROOT_DIR/sdkconfig")"; rev_max="${rev_max:-0}"
    CHIP_REV_RANGE="v$((rev_min / 100)).$((rev_min % 100)) to v$((rev_max / 100)).$((rev_max % 100))"
    if [ "$BOARD_VARIANT" = "PRE_REV3" ]; then
        rev_ok=$(( rev_max == 199 ))
    else
        rev_ok=$(( rev_min == 300 && rev_max == 399 ))
    fi
    if [ "$rev_ok" != "1" ]; then
        echo "ERROR: $BUILD_NAME resolved to ESP32-P4 chip revisions $CHIP_REV_RANGE (ESP-IDF v$IDF_VERSION_STR)."
        echo "  Expected v3.0 to v3.99 for the default build, or a v1.99 ceiling for PRE_REV3."
        exit 1
    fi
    echo "Targets ESP32-P4 chip revisions $CHIP_REV_RANGE"
    echo "chip_revisions=$CHIP_REV_RANGE" > "$BUILD_DIR/chip_revisions.txt"
fi

echo "=== Build complete ==="
echo "Firmware: $BUILD_DIR/game_2048.bin"
ls -lh "$BUILD_DIR/game_2048.bin" 2>/dev/null || echo "(binary name may differ: check build/ dir)"
