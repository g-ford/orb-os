#!/bin/bash
# Builds and runs tests/clock_face_test.cpp on the host. Needs no libraries.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc/app/clock tests/clock_face_test.cpp -o "$OUT/clock_face_test"
"$OUT/clock_face_test"
