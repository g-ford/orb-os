#!/bin/bash
# Builds and runs tests/ip_locate_test.cpp on the host: normally, and again under
# ThreadSanitizer when the compiler has it. Needs the native env's libdeps for ArduinoJson.
set -euo pipefail
cd "$(dirname "$0")/.."
AJ=.pio/libdeps/native/ArduinoJson/src
[ -d "$AJ" ] || { echo "ArduinoJson not found at $AJ; run: pio run -e native" >&2; exit 2; }
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
SRC="tests/ip_locate_test.cpp src/core/ip_locate.cpp"
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc/core -I"$AJ" $SRC -o "$OUT/ip_locate_test" -pthread
"$OUT/ip_locate_test"
if c++ -std=c++17 -O1 -g -fsanitize=thread -Isrc/core -I"$AJ" $SRC -o "$OUT/ip_locate_tsan" -pthread 2>/dev/null; then
    echo "--- under ThreadSanitizer"
    "$OUT/ip_locate_tsan"
else
    echo "--- ThreadSanitizer not available here; skipped"
fi
