#!/bin/bash
# Runs the whole Python suite with everything it needs provisioned, so that nothing has to be skipped.
#
#     bash tests/run_python_tests.sh
#
# What the suite needs beyond Python itself, and what this does about each:
#   - PyYAML, Pillow, numpy        pip-installed from tests/requirements.txt if any is missing
#   - the native build tree        `pio run -e native` if PNGdec or liblvgl.a is not there yet; the decoder and
#                                  font-loader harnesses compile against them
#   - node/npx                     lv_font_conv, which bakes theme typefaces, is fetched on demand by npx
#   - a C++ compiler               already required by the host tests
#
# An unmet prerequisite is a FAILURE (tests/skips.py). Set ORB_ALLOW_SKIPS=1 to skip on purpose.
set -euo pipefail
cd "$(dirname "$0")/.."
PIO="${PIO:-$(command -v pio || echo "$HOME/.platformio/penv/bin/pio")}"

if ! python3 -c "import yaml, PIL, numpy" 2>/dev/null; then
    echo "== installing the Python packages the tests need"
    python3 -m pip install -r tests/requirements.txt || {
        echo "pip could not install tests/requirements.txt; install PyYAML, Pillow and numpy yourself" >&2
        exit 1
    }
fi

if ! ls .pio/build/native/lib*/liblvgl.a >/dev/null 2>&1 || [ ! -d .pio/libdeps/native/PNGdec ]; then
    echo "== building the native tree once (PNGdec and LVGL, for the decoder and font-loader harnesses)"
    "$PIO" run -e native
fi

command -v npx >/dev/null || { echo "node and npx are needed to bake theme typefaces (lv_font_conv)" >&2; exit 1; }

LOG=$(mktemp)
trap 'rm -f "$LOG"' EXIT
status=0
python3 -m unittest discover -s tests -p "test_*.py" 2>&1 | tee "$LOG" || status=$?
grep -E '^(FAIL|ERROR):' "$LOG" || true
# Belt and braces: with ORB_ALLOW_SKIPS unset a skip cannot happen, but say so loudly if one ever does.
if [ -z "${ORB_ALLOW_SKIPS:-}" ] && grep -q "skipped=" "$LOG"; then
    echo "SKIPPED TESTS PRESENT: a skip is not a pass" >&2
    exit 1
fi
exit $status
