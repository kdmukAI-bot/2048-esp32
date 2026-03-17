#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

BOARD="esp32s3"
BUILD_DIR="$ROOT_DIR/build"

echo "=== Setting target: $BOARD ==="
idf.py set-target "$BOARD"

echo "=== Building firmware ==="
idf.py build 2>&1 | tee "${ROOT_DIR}/build-log.txt"

echo "=== Build complete ==="
echo "Firmware: $BUILD_DIR/game_2048.bin"
ls -lh "$BUILD_DIR/game_2048.bin" 2>/dev/null || echo "(binary name may differ — check build/ dir)"
