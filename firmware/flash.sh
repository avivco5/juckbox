#!/usr/bin/env bash
# Build + flash Jukebox CODE (esp32-s3-devkitc-1, env=esp32s3).
# This project also defines rgb_finder / diagtest / pinball / airhockey
# environments -- this script only builds/flashes the main "esp32s3" firmware.
# For the others: pio run -e <name> -t upload --upload-port COMx
# Usage: ./flash.sh [COM_PORT]
set -e
cd "$(dirname "$0")"

export PYTHONIOENCODING=utf-8
PIO="/c/Users/avivc/.platformio/penv/Scripts/pio.exe"
if [ ! -f "$PIO" ]; then
    PIO="pio"
fi

PORT="${1:-COM26}"

if [ -z "$PORT" ]; then
    echo "[ERROR] No COM port specified and no default configured for this project."
    echo "Usage: ./flash.sh COMx"
    echo
    echo "Available ports:"
    "$PIO" device list
    exit 1
fi

echo "================================================"
echo " Building Jukebox CODE (esp32-s3-devkitc-1, env=esp32s3)"
echo "================================================"
"$PIO" run -e esp32s3

echo
echo "================================================"
echo " Flashing to $PORT"
echo "================================================"
"$PIO" run -e esp32s3 -t upload --upload-port "$PORT"

echo
echo "================================================"
echo " Done. Flashed successfully to $PORT."
echo "================================================"
