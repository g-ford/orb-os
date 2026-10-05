#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc -Isrc/platform/storage -Isrc/app/shell \
    tests/settings_registry_test.cpp src/app/shell/settings_registry.cpp -o "$OUT/settings_registry_test"
"$OUT/settings_registry_test"
