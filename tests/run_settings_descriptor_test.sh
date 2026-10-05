#!/bin/bash
# tests/run_settings_descriptor_test.sh
# Builds and runs tests/settings_descriptor_test.cpp on the host. Header-only.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc -Isrc/platform/storage -Itests \
    tests/settings_descriptor_test.cpp -o "$OUT/settings_descriptor_test"
"$OUT/settings_descriptor_test"
