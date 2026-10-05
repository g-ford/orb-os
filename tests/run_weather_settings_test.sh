#!/bin/bash
# tests/run_weather_settings_test.sh
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc -Isrc/platform/storage -Isrc/app/weather \
    tests/weather_settings_test.cpp src/app/weather/weather_settings.cpp -o "$OUT/weather_settings_test"
"$OUT/weather_settings_test"
