#!/bin/bash
# Every host-side test and check for the firmware's portable logic, in one command.
#
#     bash tests/run_host_tests.sh
#
# None of these needs a board. They cover the parts of the firmware that are pure enough to run
# on the desktop; what they cannot cover (display, audio, WiFi, NVS, the two cores together) is
# still checked by booting an Orb, which CLAUDE.md requires before anything is deployed.
#
# Needs the native environment's libdeps once (`pio run -e native`) for PNGdec and ArduinoJson.
# The older tests in tests/test_*.py that build themes need PyYAML and are run separately.
set -uo pipefail
cd "$(dirname "$0")/.."

failed=()
run() {
    local name="$1"; shift
    echo "── $name"
    if "$@" > /tmp/host_test.$$.log 2>&1; then
        tail -1 /tmp/host_test.$$.log | sed 's/^/   /'
    else
        cat /tmp/host_test.$$.log | sed 's/^/   /'
        failed+=("$name")
    fi
}

run "SD calls all under sdcard::Guard"   python3 tools/check_sd_guard.py
run "SD guard checker"                   python3 -m unittest tests/test_sd_guard.py
run "boot wiring (bake is called)"       python3 -m unittest tests/test_boot_wiring.py
run "settings store (guards)"            python3 -m unittest tests/test_settings_store.py
run "settings store"                     bash tests/run_settings_store_test.sh
run "png decode"                         bash tests/run_png_decode_test.sh
run "aircraft aging"                     bash tests/run_aircraft_aging_test.sh
run "tracked-set selection"              bash tests/run_track_select_test.sh
run "traffic simulator + feed backoff"   bash tests/run_adsb_pieces_test.sh
run "ip locate"                          bash tests/run_ip_locate_test.sh
run "weather"                            bash tests/run_weather_test.sh
run "swipe"                              bash tests/run_swipe_test.sh
run "theme font resolution"              bash tests/run_theme_font_resolve_test.sh
run "theme bake policy"                  bash tests/run_theme_bake_policy_test.sh
run "theme roles"                        bash tests/run_theme_roles_test.sh
run "theme slug policy"                  bash tests/run_theme_slug_policy_test.sh
rm -f /tmp/host_test.$$.log

echo
if [ ${#failed[@]} -eq 0 ]; then
    echo "all host tests passed"
else
    echo "FAILED: ${failed[*]}"
    exit 1
fi
