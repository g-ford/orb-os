#!/bin/bash
# Builds and runs tests/weather_test.cpp on the host. Needs the native env's libdeps for ArduinoJson.
set -euo pipefail
cd "$(dirname "$0")/.."
AJ=.pio/libdeps/native/ArduinoJson/src
[ -d "$AJ" ] || { echo "ArduinoJson not found at $AJ; run: pio run -e native" >&2; exit 2; }
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
SRC="tests/weather_test.cpp src/app/weather/weather.cpp"
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc/app/weather -I"$AJ" $SRC -o "$OUT/weather_test" -pthread
"$OUT/weather_test"
