#!/bin/bash
# Builds and runs tests/swipe_test.cpp on the host. swipe.cpp is pure, so no libdeps are needed.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc/core tests/swipe_test.cpp src/core/swipe.cpp -o "$OUT/swipe_test"
"$OUT/swipe_test"
