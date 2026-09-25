#!/bin/bash
# Builds and runs tests/pixel_slot_test.cpp on the host. pixel_slot.h is pure, so no libraries are needed.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc/app/common tests/pixel_slot_test.cpp -o "$OUT/pixel_slot_test"
"$OUT/pixel_slot_test"
