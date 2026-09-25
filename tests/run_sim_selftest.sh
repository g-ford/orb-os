#!/bin/bash
# Runs the desktop simulator's headless self-test (virtual knob -> input router -> app shell -> Settings, and the
# checks added since) and fails on any FAIL line, on too few PASS lines, on a non-zero exit, or on a hang.
#
#     pio run -e native && bash tests/run_sim_selftest.sh
#
# Needs the native build (.pio/build/native/program) and SDL2. SDL's dummy drivers mean no window and no sound.
set -uo pipefail
cd "$(dirname "$0")/.."
BIN=.pio/build/native/program
MIN_PASS="${MIN_PASS:-9}"
LIMIT_S="${SELFTEST_TIMEOUT_S:-180}"
[ -x "$BIN" ] || { echo "no $BIN: run 'pio run -e native' first" >&2; exit 1; }

LOG=$(mktemp)
trap 'rm -f "$LOG"' EXIT
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SELFTEST=1 SIM_SETTLE_MS=500 "$BIN" > "$LOG" 2>&1 &
pid=$!
for _ in $(seq 1 "$LIMIT_S"); do kill -0 "$pid" 2>/dev/null || break; sleep 1; done
if kill -0 "$pid" 2>/dev/null; then
    kill -9 "$pid" 2>/dev/null
    grep -E "selftest" "$LOG" | tail -5
    echo "FAIL: the self-test did not finish within ${LIMIT_S}s" >&2
    exit 1
fi
wait "$pid"; rc=$?

grep -E "selftest.*(PASS|FAIL)" "$LOG"
passes=$(grep -cE "selftest.*: PASS" "$LOG")
fails=$(grep -cE "selftest.*: FAIL" "$LOG")
echo "self-test: $passes passed, $fails failed (exit $rc)"
[ "$rc" -eq 0 ] || { echo "FAIL: the simulator exited $rc" >&2; exit 1; }
[ "$fails" -eq 0 ] || exit 1
[ "$passes" -ge "$MIN_PASS" ] || { echo "FAIL: expected at least $MIN_PASS PASS lines, got $passes" >&2; exit 1; }
