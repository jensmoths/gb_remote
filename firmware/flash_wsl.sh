#!/bin/bash
# Flash an already-built firmware image from WSL without rebuilding.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_DIR="${BUILD_DIR:-build_dual_throttle}"
PORT="${1:-}"

if [ -z "$PORT" ]; then
  for candidate in /dev/ttyACM0 /dev/ttyACM1 /dev/ttyUSB0 /dev/ttyUSB1; do
    if [ -c "$candidate" ]; then
      PORT="$candidate"
      break
    fi
  done
fi

if [ -z "$PORT" ]; then
  echo "ERROR: No serial port specified and none found." >&2
  echo "Usage: $0 /dev/ttyACM0" >&2
  exit 1
fi

if [ ! -d "$BUILD_DIR" ]; then
  echo "ERROR: build directory not found: $BUILD_DIR" >&2
  echo "Build once first, e.g. ./dual_throttle_build.sh" >&2
  exit 1
fi

if [ ! -f "$BUILD_DIR/flash_args" ]; then
  echo "ERROR: $BUILD_DIR/flash_args not found." >&2
  echo "Build once first, e.g. ./dual_throttle_build.sh" >&2
  exit 1
fi

if [ ! -w "$PORT" ]; then
  echo "WARNING: $PORT is not writable by $(whoami). Trying chmod through WSL root..." >&2
  if command -v wsl.exe >/dev/null 2>&1; then
    wsl.exe -u root -- chmod a+rw "$PORT" 2>/dev/null || true
  elif [ -x /mnt/c/Windows/System32/wsl.exe ]; then
    /mnt/c/Windows/System32/wsl.exe -u root -- chmod a+rw "$PORT" 2>/dev/null || true
  fi
fi

if command -v fuser >/dev/null 2>&1 && fuser "$PORT" >/dev/null 2>&1; then
  echo "ERROR: $PORT is busy:" >&2
  fuser -v "$PORT" >&2 || true
  exit 1
fi

if ! command -v python >/dev/null 2>&1 || ! python -c "import esptool" >/dev/null 2>&1; then
  if [ -f /home/jens/esp/esp-idf/export.sh ]; then
    # shellcheck disable=SC1091
    . /home/jens/esp/esp-idf/export.sh >/dev/null 2>&1
  fi
fi

if ! command -v python >/dev/null 2>&1 || ! python -c "import esptool" >/dev/null 2>&1; then
  echo "ERROR: python/esptool not found. Run: . /home/jens/esp/esp-idf/export.sh" >&2
  exit 1
fi

cd "$BUILD_DIR"
echo "Flashing existing build from $(pwd) to $PORT ..."
python -m esptool --chip esp32s3 -p "$PORT" -b 460800 \
  --before default_reset --after hard_reset write_flash @flash_args
