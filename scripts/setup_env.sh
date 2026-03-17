#!/usr/bin/env bash
set -euo pipefail

PREBAKED_IDF_DIR="/opt/toolchains/esp-idf"

if [ ! -d "$PREBAKED_IDF_DIR" ]; then
  echo "ERROR: ESP-IDF not found at $PREBAKED_IDF_DIR"
  exit 1
fi

if [ -z "${IDF_TOOLS_PATH:-}" ]; then
  if [ -d "/opt/espressif" ]; then
    export IDF_TOOLS_PATH="/opt/espressif"
  fi
fi
mkdir -p "$IDF_TOOLS_PATH"

export IDF_PATH="$PREBAKED_IDF_DIR"
# shellcheck disable=SC1091
source "$IDF_PATH/export.sh"
idf.py --version >/dev/null

echo "Environment setup complete."
echo "IDF_PATH=$IDF_PATH"
echo "IDF_TOOLS_PATH=$IDF_TOOLS_PATH"
