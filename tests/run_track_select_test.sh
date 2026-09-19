#!/bin/bash
# Builds and runs tests/track_select_test.cpp on the host. Header-only.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc/app/radar tests/track_select_test.cpp -o "$OUT/track_select_test"
"$OUT/track_select_test"
