#!/bin/bash
# Builds and runs tests/wheel_ring_test.cpp on the host. wheel_ring.h is pure, so no libraries are needed.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc/platform/wheel tests/wheel_ring_test.cpp -o "$OUT/wheel_ring_test"
"$OUT/wheel_ring_test"
