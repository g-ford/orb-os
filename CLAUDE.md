# The Orb OS — CLAUDE.md

> Named Capsule Radar until August 2026, and Plane Radar 2.0 before that. The rename
> is display-only: the NVS namespace is still `capsuleradar` and always will be, the
> mDNS name became `theorb.local` in 1.40, and the repo folder became `orb-os` on 2026-08-30
> (origin was already `Ziplock78/orb-firmware`; `upstream` still points at socquique/capsule-radar).
> See the note on the namespace in main.cpp before touching any of those.
>
> The inherited product documents are gone as of 1.42: `docs/LISTING.md`,
> `docs/MAKERWORLD.md` and `docs/FEATURES.md` described Quique Tortosa's flight-radar
> gadget, not this, and README's body has been rewritten. One reference remains and is
> deliberate: the community port credited at the bottom of README carries its own name,
> because it is a third party's project.

Master context for Claude Code. Read this first, then `docs/` for detail.

> **Adding or extending a screen? Read `docs/adding-a-screen.md` first.**
> That file is the checklist of the standard parts every screen has — background, glass,
> per-slot typefaces, the full set of text controls, margins, capability level — written
> after the Headlines screen shipped without five of them and had to be repaired one
> complaint at a time.

## How work is done here

Four rules. They exist because each one was learned by nearly getting it wrong.

1. **Firmware runs on a real Orb before it is called done.** A clean build, a green host
   test run and a good simulator screenshot are not evidence that the display, audio, WiFi,
   NVS and the two cores work together. Boot it on hardware.
2. **Personal permissions go in `.claude/settings.local.json`.** Never `settings.json`, which
   is not gitignored and would ship to anyone who clones this repo.
3. **A regression found in your own recent commit outranks the task in hand.** Say it plainly
   and early, the way the GPS re-centre guard was caught.
4. **When something must never happen, make the shared path enforce it.** A warning beside
   one call site protects one call site. Three faults this week were a lesson written where it
   last happened: the enum comment that said not to renumber and lost, the WiFi
   credential write whose 2026-08-15 note sat beside the old caller while Settings walked into
   the same function from the other side, and a manifest import that looked free to the second
   person exactly as it had to the first. A comment cannot fail. A guard can.

## What we're building

Firmware for **The Orb**, a round-AMOLED desk instrument on the **Waveshare
ESP32-S3-Touch-AMOLED-1.75** (466x466, knob-driven, touch for swipes only). Five screens: a
clock, a live flight tracker (adsb.lol), a weather screen (Now, Radar and 7-Day), a news
screen, and Settings. A stock ticker and a camera view are compiled out (`APPS_LAUNCH_ONE` in
`src/config.h`). README.md is the product description; keep it true.

There is NO fixed visual target. The look is the THEME's: a folder on the SD card built from a
`theme.yaml` (see `docs/theme-yaml.md`). Backgrounds, glass, typefaces, colours, opacity, glow,
layer order and layout belong to the design rather than to the code. When something on screen
looks wrong, the first question is whether the firmware drew it wrong or the theme asked for it.

## Hardware (full detail in docs/HARDWARE.md)
- MCU: ESP32-S3R8, 8 MB PSRAM, 16 MB flash, dual-core 240 MHz, WiFi + BLE5.
- Display: CO5300 AMOLED, 466x466, QSPI. Brightness via panel command (no PWM backlight pin).
- Touch: CST9217, I2C. IMU: QMI8658. RTC: PCF85063. PMIC: AXP2101. Audio: ES8311 + speaker.
- Every pin is in `src/config.h`, taken from the board definition. **Never guess a GPIO.** If one is
  missing, it comes from the Waveshare demo for this board, not from memory.

## Stack
**PlatformIO + Arduino framework**, two environments in `platformio.ini`:
`esp32-s3-amoled-175` (the device) and `native` (the SDL desktop simulator, the same LVGL UI).
Libraries are pinned under `lib_deps`: GFX Library for Arduino (the CO5300 driver), LVGL 8.4,
ArduinoJson 7, WiFiManager, XPowersLib, TJpg_Decoder, TinyGPSPlus, PNGdec.

The Orb is **flashed over USB only**. Wireless update is compiled out (`ORB_OTA_ENABLED` in
`main.cpp`) because its partition was given to theme art; flip that flag and
`partitions_16MB_themeart.csv` together, never one alone.

## Data source (full detail in docs/DATA_SOURCE.md)
**adsb.lol** (`ADSB_PRIMARY_HOST` in `src/config.h`), position + radius, readsb JSON, free and
non-commercial. There is **no fallback host**: airplanes.live answers 403 and the alternatives
need HTTPS the device cannot follow. Be polite (`POLL_INTERVAL_MS`) and keep the User-Agent
honest (`ORB_USER_AGENT`). Weather is Open-Meteo, rain radar RainViewer, news BBC/Guardian/NASA
RSS; credits are in README.md.

## Architecture (full detail in docs/ARCHITECTURE.md)
- **Core 0, `adsb_task`**: WiFi keepalive, fetches and parses the feed every `POLL_INTERVAL_MS`,
  writes the shared aircraft table under `g_ac_mutex`.
- **Core 1, Arduino `loop()`**: LVGL tick and render, the knob, the web config page. Reads the
  aircraft snapshot under the mutex. SD card access is serialised by one mutex (`sdcard::Guard`).
- PSRAM holds the big buffers; see `docs/memory.md` before allocating anything over ~100 KB.
  Internal RAM is the scarce pool, not PSRAM.
- Settings live in NVS through `settings_store` (namespace `capsuleradar`, kept deliberately).
  First-boot WiFi is set up on the Orb's own screen, or from a phone via the **The Orb Setup** portal.
- Themes are read from the SD card and baked into flash on first boot (`theme_art_bake.cpp`).

## Repo layout
```
src/
  main.cpp         boot, tasks, WiFi/NTP, the web config page
  config.h         pins, hostname, user agent, tunables, FW_VERSION
  app/             one folder per screen, plus the shell
    shell/         app list, the picker overlay, the input router
    clock/ radar/ weather/ intel/ settings/ ticker/ spycam/ photo/ route/
    common/        shared drawing helpers, wheel_look (a theme made into a wheel)
    boot/ ui/      hello screen, the main UI, the update screen
  core/            feed client, geometry, aircraft model, GPS, IMU, swipe recogniser
  platform/        display, input (knob), audio, rtc, storage, the wheel, the SDL simulator
  theme/           core (style model, THEME_CAPS, palette, fonts, baking), graphics, custom, fonts
  theme_assets/    the shipped themes
tools/             theme builder, generators, screenshot tools (themeshots.sh, shot_diff.py)
tests/             host tests (bash) and the Python suite
docs/              architecture, memory, hardware, theme format, specs and plans (docs/superpowers)
```

## Build, test, simulate
`pio` is often not on `PATH`: use `~/.platformio/penv/bin/pio`.
```
pio run -e esp32-s3-amoled-175                  # build the firmware
pio run -e esp32-s3-amoled-175 -t upload        # flash over USB-C
pio run -e native -t exec                       # the desktop simulator
bash tests/run_host_tests.sh                    # pure-logic host tests, no board
bash tests/run_python_tests.sh                  # theme builder and firmware-facing checks (~2 min)
SIM_SELFTEST=1 .pio/build/native/program        # headless knob and navigation checks
bash tools/themeshots.sh <outdir> [slug ...]    # photograph every app of one or more themes
```
Run the whole Python directory, not a hand-picked list. `tests/run_python_tests.sh` provisions what the suite
needs (Python packages, the native build tree, npx) and runs it. An unmet prerequisite is a FAILURE, not a skip
(`tests/skips.py`): a skipped test exits 0, which once let a firmware compile error look green. Set
`ORB_ALLOW_SKIPS=1` only when you mean to skip.

**A firmware change is not finished when it compiles.** Boot it on an Orb (rule 1) and bump
`FW_VERSION` in `src/config.h` when a build goes out that a device could be behind; it is shown on the
web config page and in the User-Agent. Until an Orb is to hand, record what is unverified in
`docs/HARDWARE_PENDING.md`.

## Conventions and guardrails
- C++17. Keep the render path non-blocking: no network or `delay()` in the LVGL loop.
- Touch the shared aircraft table only under `xSemaphoreTake(g_ac_mutex, ...)`.
- All tunables live in `config.h`. No magic numbers in render code.
- Every SD card call goes through `sdcard::Guard` (`tools/check_sd_guard.py` fails on one that does not).
  Every saved setting is declared once in `settings_store`; a guard test refuses a raw NVS key.
- No compiled art or bitmap fonts in `src/app` or `src/theme`: art ships in the theme
  (`tests/test_no_compiled_art.py`).
- Lists that the knob drives use the one wheel (`src/platform/wheel`, dressed by `wheel_look`); never
  draw a selection background or pick per-screen selected colours.
- A capability added to a theme needs a `THEME_CAPS` bump (`src/theme/core/theme_style.h`) and a ledger entry in
  `docs/theme-caps.md`; a test fails when they disagree. `docs/adding-a-screen.md` is the checklist for a screen.
- HTTPS uses `setInsecure()` on this hobby device; that is a documented choice, not an oversight
  (`ADSB_HTTPS_INSECURE` and the notes in `config.h`).
- The native build compiles everything under `src/` except a named exclusion list in
  `platformio.ini`; a new file that needs the board goes on that list.
