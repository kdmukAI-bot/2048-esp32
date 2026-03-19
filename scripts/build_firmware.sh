#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

BOARD="${BOARD:-waveshare_s3_lcd35b}"
BUILD_DIR="$ROOT_DIR/build"

# Derive SoC target from board name
case "$BOARD" in
    waveshare_s3_*)  TARGET=esp32s3 ;;
    waveshare_p4_*)  TARGET=esp32p4 ;;
    *)
        echo "ERROR: Unknown board '$BOARD' — cannot determine SoC target"
        exit 1
        ;;
esac

echo "=== Board: $BOARD (target: $TARGET) ==="

echo "=== Setting target: $TARGET ==="
idf.py set-target "$TARGET"

echo "=== Building firmware ==="
idf.py build -DBOARD="$BOARD" -DCCACHE_ENABLE=1 2>&1 | tee "${ROOT_DIR}/build-log.txt"

echo "=== Build complete ==="
echo "Firmware: $BUILD_DIR/game_2048.bin"
ls -lh "$BUILD_DIR/game_2048.bin" 2>/dev/null || echo "(binary name may differ — check build/ dir)"
