#!/bin/bash
# Builds and runs tests/sweep_trail_falloff_test.cpp on the host. Needs no libraries.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc/app/radar tests/sweep_trail_falloff_test.cpp -o "$OUT/sweep_trail_falloff_test"
"$OUT/sweep_trail_falloff_test"
