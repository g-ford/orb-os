#!/bin/bash
# tests/run_radar_settings_test.sh
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc -Isrc/platform/storage -Isrc/app/radar \
    tests/radar_settings_test.cpp src/app/radar/radar_settings.cpp -o "$OUT/radar_settings_test"
"$OUT/radar_settings_test"
