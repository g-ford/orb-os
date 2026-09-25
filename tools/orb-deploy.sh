#!/bin/bash
# One-command deploy for the Orb: every step, in order, stopping loudly at the first failure.
#
#   tools/orb-deploy.sh [host]        host defaults to theorb.local
#
# Steps: build the simulator -> simulator self-test -> build the device firmware -> find the Orb on USB
# (by Espressif VID, never by path) -> flash -> wait for the Orb's /health page -> budget check.
#
# Themes are not sent by this script: copy a built theme folder to /themes/<slug>/ on the card
# (tools/build_theme.py, see docs/theme-yaml.md). The Orb is flashed over USB only; wireless update is
# compiled out (ORB_OTA_ENABLED in src/main.cpp).
set -u
cd "$(dirname "$0")/.."
PIO="${PIO:-$HOME/.platformio/penv/bin/pio}"
ORB_HOST="${1:-theorb.local}"
step() { printf '\n== %s\n' "$1"; }
fail() { printf 'FAIL: %s\n' "$1"; exit 1; }

step "build simulator"
"$PIO" run -e native 2>&1 | tail -2 | grep -q SUCCESS || fail "simulator build"
echo "ok"

step "simulator self-test (virtual knob -> app shell -> Settings, through the real input router)"
OUT=$(SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SELFTEST=1 ./.pio/build/native/program 2>&1 | grep "selftest")
echo "$OUT" | tail -12
echo "$OUT" | grep -q "PASS" || fail "sim self-test produced no PASS lines"
echo "$OUT" | grep -q "FAIL" && fail "sim self-test"

step "build device firmware"
"$PIO" run -e esp32-s3-amoled-175 2>&1 | tail -2 | grep -q SUCCESS || fail "device build"
echo "ok"

step "find the Orb (Espressif VID 303A, never by path)"
PORT=$("$PIO" device list --json-output 2>/dev/null | python3 -c "
import json,sys
for e in json.load(sys.stdin):
    if 'VID:PID=303A' in (e.get('hwid') or '').upper(): print(e['port']); break")
[ -n "$PORT" ] && echo "$PORT" || fail "no Orb on USB"

step "flash"
"$PIO" run -e esp32-s3-amoled-175 -t upload --upload-port "$PORT" 2>&1 | tail -2 | grep -q "1 succeeded" || fail "flash"
echo "ok"

step "wait for a fresh boot on WiFi ($ORB_HOST)"
# uptime under 60 s proves this is the boot after the flash, not the one before it
H=""
for i in $(seq 1 40); do
  H=$(curl -s -m 3 "http://$ORB_HOST/health" 2>/dev/null)
  echo "$H" | python3 -c "
import json,sys
d=json.load(sys.stdin); sys.exit(0 if d.get('uptime_s',9999) < 60 else 1)" 2>/dev/null && break
  H=""; sleep 3
done
[ -n "$H" ] || fail "no fresh health report from $ORB_HOST"
echo "$H"

step "memory budget"
echo "$H" | python3 -c "
import json,sys
d=json.load(sys.stdin)
ok=True
if d.get('psram_free_kb',0)    < 2500: print('BUDGET WARNING: free PSRAM below 2.5 MB'); ok=False
if d.get('psram_largest_kb',0) < 1024: print('BUDGET WARNING: largest free block below 1 MB'); ok=False
print('budgets:', 'PASS' if ok else 'CHECK ABOVE')"

printf '\n== DEPLOY COMPLETE\n'
