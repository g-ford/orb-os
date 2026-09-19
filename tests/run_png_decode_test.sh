#!/bin/bash
# Builds and runs tests/png_decode_test.cpp on the host. Needs the native env's libdeps
# (`pio run -e native` once) for PNGdec, and python3 for the fixture PNGs.
set -euo pipefail
cd "$(dirname "$0")/.."
PNGDEC=.pio/libdeps/native/PNGdec/src
[ -d "$PNGDEC" ] || { echo "PNGdec not found at $PNGDEC — run: pio run -e native" >&2; exit 2; }
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

python3 tests/make_png_fixtures.py "$OUT"

# PNGdec bundles zlib as C sources; build them as C, the rest as C++.
for f in adler32 crc32 infback inffast inflate inftrees zutil; do
    cc -c -O1 -I"$PNGDEC" "$PNGDEC/$f.c" -o "$OUT/$f.o"
done
c++ -std=c++17 -O1 -g -DPNG_MAX_BUFFERED_PIXELS=8192 -Isrc/theme/graphics -I"$PNGDEC" \
    tests/png_decode_test.cpp src/theme/graphics/png_decode.cpp "$PNGDEC/PNGdec.cpp" "$OUT"/*.o \
    -o "$OUT/png_decode_test"
"$OUT/png_decode_test" "$OUT"
