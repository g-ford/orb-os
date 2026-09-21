#!/bin/bash
# Builds and runs tests/theme_bake_policy_test.cpp on the host. Needs no libraries.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc/theme/core tests/theme_bake_policy_test.cpp -o "$OUT/theme_bake_policy_test"
"$OUT/theme_bake_policy_test"
