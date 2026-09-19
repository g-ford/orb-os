#!/bin/bash
# Builds and runs tests/settings_store_test.cpp on the host. Header-only.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc -Isrc/platform/storage tests/settings_store_test.cpp -o "$OUT/settings_store_test"
"$OUT/settings_store_test"
