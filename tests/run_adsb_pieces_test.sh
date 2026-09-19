#!/bin/bash
# Builds and runs tests/adsb_pieces_test.cpp on the host.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc -Isrc/core tests/adsb_pieces_test.cpp src/core/sim_traffic.cpp -o "$OUT/adsb_pieces_test"
"$OUT/adsb_pieces_test"
