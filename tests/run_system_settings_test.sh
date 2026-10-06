#!/bin/bash
# tests/run_system_settings_test.sh
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc -Isrc/platform/storage -Isrc/app/shell \
    tests/system_settings_test.cpp src/app/shell/system_settings.cpp -o "$OUT/system_settings_test"
"$OUT/system_settings_test"
