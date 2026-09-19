#!/bin/bash
# Builds and runs tests/aircraft_aging_test.cpp on the host. Header-only, so nothing else to link.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc -Isrc/core tests/aircraft_aging_test.cpp -o "$OUT/aircraft_aging_test"
"$OUT/aircraft_aging_test"
