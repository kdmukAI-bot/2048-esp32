#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

# Ensure writable HOME/cache for non-root container users.
export HOME="/tmp/home"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-$HOME/.cache}"
mkdir -p "$HOME" "$XDG_CACHE_HOME"

# Avoid git ownership issues with host-mounted workspace and prebaked IDF checkout.
git config --global --add safe.directory '*' || true
git config --global --add safe.directory /opt/toolchains/esp-idf || true

# Source ESP-IDF environment
source ./scripts/setup_env.sh

# Build
./scripts/build_firmware.sh

echo "DONE: firmware built."
