#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

# Create writable HOME (container has no /etc/passwd entry for this UID).
mkdir -p "$HOME"

# Point ESP component manager + ccache at the persistent /cache volume.
export CCACHE_DIR="/cache/ccache"
mkdir -p "$CCACHE_DIR"
mkdir -p "/cache/Espressif"
export XDG_CACHE_HOME="/cache"

# Avoid git ownership issues with host-mounted workspace and prebaked IDF checkout.
export GIT_CONFIG_GLOBAL="$HOME/.gitconfig"
git config --global --add safe.directory '*'
git config --global --add safe.directory /opt/toolchains/esp-idf

# Source ESP-IDF environment
source ./scripts/setup_env.sh

# Build
./scripts/build_firmware.sh

echo "DONE: firmware built."
