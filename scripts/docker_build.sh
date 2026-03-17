#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

# Container runs as root; cache volume is mounted at /root/.cache.
export HOME="/root"

# Avoid git ownership issues with host-mounted workspace and prebaked IDF checkout.
git config --global --add safe.directory '*' || true
git config --global --add safe.directory /opt/toolchains/esp-idf || true

# Source ESP-IDF environment
source ./scripts/setup_env.sh

# Build
./scripts/build_firmware.sh

echo "DONE: firmware built."
