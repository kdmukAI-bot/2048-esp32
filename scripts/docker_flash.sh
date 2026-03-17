#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

export HOME="/tmp/home"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-$HOME/.cache}"
mkdir -p "$HOME" "$XDG_CACHE_HOME"

git config --global --add safe.directory '*' || true
git config --global --add safe.directory /opt/toolchains/esp-idf || true

source ./scripts/setup_env.sh

SERIAL_PORT="${SERIAL_PORT:-/dev/ttyACM0}"

echo "=== Flashing to $SERIAL_PORT ==="
idf.py -p "$SERIAL_PORT" flash monitor
