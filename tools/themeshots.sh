#!/bin/bash
# Photograph every app of one or more themes, rendered by the real firmware code in the desktop simulator.
#
#     bash tools/themeshots.sh <outdir> [slug ...]        (default: default elegant fallout portal)
#
# This is the spec's regression net: run it before and after a change, then `python3 tools/shot_diff.py before after`.
# Needs `pio run -e native` first. `default` is the built-in look and needs no folder; the others are built from
# src/theme_assets/ into sim/sdcard/themes (gitignored scratch). The simulator's saved slug is a global state file
# in /tmp, so its old value is put back when this finishes.
set -uo pipefail
cd "$(dirname "$0")/.."

out="$1"; shift
slugs=("$@")
[ ${#slugs[@]} -eq 0 ] && slugs=(default elegant fallout portal)

state=/tmp/orb_sim_theme_slug
old=$(cat "$state" 2>/dev/null || true)
mkdir -p "$out" sim/sdcard/themes

for s in "${slugs[@]}"; do
    [ "$s" = default ] && continue
    python3 tools/build_theme.py "src/theme_assets/$s" --out sim/sdcard/themes > "$out/$s.build.log" 2>&1 || { echo "build of $s failed, see $out/$s.build.log"; exit 1; }
done

for s in "${slugs[@]}"; do
    echo "$s" > "$state"
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SETTLE_MS=1500 .pio/build/native/program --themeshot "$out/$s" > "$out/$s.log" 2>&1 &
    pid=$!
    for _ in $(seq 1 90); do kill -0 "$pid" 2>/dev/null || break; sleep 1; done
    kill -9 "$pid" 2>/dev/null || true   # the simulator ignores SIGTERM
    wait "$pid" 2>/dev/null || true
    echo "$s: $(ls "$out" | grep -c "^$s.*\.bmp$") screenshots"
done

if [ -n "$old" ]; then echo "$old" > "$state"; else rm -f "$state"; fi
