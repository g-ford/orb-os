#!/bin/bash
# tests/run_clock_settings_test.sh
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc -Isrc/platform/storage -Isrc/app/clock \
    tests/clock_settings_test.cpp src/app/clock/clock_settings.cpp -o "$OUT/clock_settings_test"
"$OUT/clock_settings_test"
