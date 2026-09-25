# Architecture

How the Orb Firmware is actually put together today, not how it was originally
sketched. Where the current build differs from `orb-ux-requirements.md` (the top-level
target, which supersedes `orb-user-requirements.md`), the gap is
called out explicitly rather than papered over.

Board: Waveshare ESP32-S3-Touch-AMOLED-1.75 (ESP32-S3R8, 8 MB PSRAM, 16 MB flash,
466x466 AMOLED via CO5300 over QSPI). Full detail in `HARDWARE.md`.

## Two build targets, one source tree

| Env | What it is | Command |
|-----|------------|---------|
| `esp32-s3-amoled-175` | Real firmware, flashed to the board | `pio run -e esp32-s3-amoled-175 -t upload` |
| `native` | Desktop LVGL simulator (SDL2), same UI code on the Mac | `pio run -e native -t exec` |

The simulator is not a mockup. It is the same source compiled for the desktop, so
layout, logic, state machines, interaction flow, and theme rendering are genuinely
identical. What the simulator cannot tell you: frame time, PSRAM behaviour, SD read
latency, real network conditions, and AMOLED colour/gamma. Those five still need a
check on real hardware.

There is no over-the-air update: the Orb is flashed over USB. ArduinoOTA and the browser upload page are compiled out (`ORB_OTA_ENABLED` in `main.cpp`) because their partition was given to theme art; see `partitions_16MB_themeart.csv`.

## Input model

**The knob is the input surface, and touch adds swipes (below).** A KY-040 rotary encoder on the 8-pin header:
A/CLK on GPIO18, B/DT on GPIO17, push switch on GPIO16 (all `INPUT_PULLUP`, switch
active low). Driver: `src/knob.cpp`.

**`src/input_router.cpp` is the single source of truth for what the knob does.** It
takes one poll's worth of input (`int delta`, `bool pressed`) and turns it into
app-shell actions. The device (`main.cpp`) and the simulator (`sim_main.cpp`) both
call the same `input_router::dispatch()`, which is what makes "it behaved right in the
sim" mean "it behaves right on the Orb". Do not add knob behaviour anywhere else.

**`src/app_shell.cpp` is the channel changer.** It holds an ordered, wrapping list of
full-screen apps (one LVGL screen each):

- Turn: cycle apps. The first turn opens an app-switcher overlay on the current app,
  further turns move through the list, a push commits into the shown app.
- Push: run the current app's `onPress` handler.
- An app can **capture** the knob (Settings does this), so turning scrolls inside the
  app instead of switching apps. It holds the knob until it releases it.
- `onEnter` / `onExit` fire around switches, so an app can decode assets on entry and
  free them on exit.

Every app is always registered, so indices never shift. Apps a theme turns off are
marked `hidden` and skipped when cycling. Current roster, in order:

`Clock`, `Flight Tracker`, `Weather` (Now / Radar / 7-Day on the knob), `Intel`, `Surveillance`, `Settings`

## Touch: swipes only

The touch panel is an addition to the knob, not a replacement, and it does one thing: swipes.

- Swipe sideways to move between apps. Swipe up or down to move between the screens inside an app
  that has several (Weather's Now / Radar / 7-Day). The knob can do all of it too.
- `src/core/swipe.{h,cpp}` is the whole recogniser: pure logic that turns raw touch samples into
  Left / Right / Up / Down or nothing (a tap, a slow drag and a diagonal are nothing). It knows
  nothing about LVGL. `main.cpp` polls the CST9217 on the loop's core, the simulator feeds it from
  the mouse, and both hand the result to `input_router::onSwipe()`, which is to touch what
  `dispatch()` is to the knob.
- **No LVGL pointer device is registered**, so no widget can be tapped, dragged or scrolled. That is
  deliberate: `ui.cpp`'s tileview would slide between Flight Tracker and Weather outside
  `app_shell` if it could see a finger.
- The router drops a swipe while an app holds the knob (Settings), while the switcher or a notice is
  up, and while a slide is running. Apps that hold the knob are also outside the swipe ring, so touch
  never lands on a screen it cannot leave; the knob's rock does that.
- Up/down is opt-in: `app_shell::setPager(slot, fn)`. Touch stops at the ends; the knob wraps.
- A swipe between apps slides (`SWIPE_SLIDE` in `config.h`, 0 to cut like the knob). Weather's
  up/down fades in from a dimmed sheet (`SWIPE_FADE_MS`).
- Rotation: the detector turns the swipe back through `display::rotation()`, so Left is the logical
  left however the Orb is mounted.
- The limits (`SWIPE_MIN_PX`, `SWIPE_AXIS_RATIO`, `SWIPE_MAX_MS`) are starting values, to be tuned on
  a board.

## IMU

`src/imu_qmi8658.cpp`. Only the accelerometer's Z axis is used, only to detect
face-down (`imu_facedown()`, read in the main loop). That drives face-down sleep.

This is motion sensing, not touchscreen input, so it is unaffected by the touch
decision above. It also answers open question 1 in the requirements doc: the IMU is
retained, for exactly this one purpose.

## Concurrency

- **`adsb_task`, pinned to core 0** (16 KB stack, priority 1). Maintains WiFi, fetches
  and parses the aircraft feed, swaps the result into a shared `std::vector<Aircraft>`
  guarded by `g_ac_mutex`. Handles feed failover and expires stale entries by
  `seen_pos`.
- **Render loop, core 1** (Arduino `loop()`). Drives `lv_timer_handler()`, copies the
  aircraft snapshot under the mutex with a short timeout, renders the active view, and
  polls the knob. No blocking calls here.

```
[feed] --HTTPS--> adsb_task (core 0) --mutex--> g_aircraft[] --mutex--> render (core 1)
                                                                              |
                            knob (GPIO16/17/18) -> input_router -> app_shell  |
                                                                              v
                                                       LVGL -> Arduino_GFX -> AMOLED
```

## Memory

- Full RGB565 framebuffer is 466 x 466 x 2, about 434 KB, allocated in PSRAM.
- **Prefer not decoding at all.** Baked assets live in flash and are drawn through the
  memory map, costing no time and no PSRAM. See *Pre-baked art* below. The two rules
  under this one apply to whatever is left on the SD path.
- **Decode once, never per frame.** Every SD read is a one-time cost on entry or theme
  switch. No per-draw filesystem access, ever.
- **Free what you decode.** Each screen's `*_sprite_release()` runs on `onExit`. This is
  what keeps 8 MB of PSRAM from filling over a long session. Release paths ask
  `theme_art::owns()` first: a flash-resident pointer was never allocated and must not be
  freed.

Internal RAM is a separate, much smaller pool than PSRAM, and it is the one that runs
out. mbedTLS takes its TLS handshake buffers from it, so anything opening repeated
outbound connections fragments it. This is why the aircraft feed polls only while Flight
Tracker is on screen (`g_radarViewActive` in `main.cpp`): polling it in the background
drove the largest free internal block down to 14 KB and caused allocation failures on
completely unrelated screens. **When something fails to allocate, check
`psram_largest_kb` and `heap_largest_kb` from `/health`, not the free totals.**
Fragmentation, not exhaustion, has been the cause every time so far.

Known constraint: outbound TLS is memory-tight on this board. A previous attempt to
move mbedTLS buffers into PSRAM only relocated the `-32512` failure rather than fixing
it. See the notes in `platformio.ini`. The intended fix is a leaner weather app, not a
bigger memory budget.

## Themes

Themes are data on the SD card, not code. Art and style both travel per theme.

A note on the name **Launch Kit**, which some source comments still use: it was the theme editor and push
tool of the Orb Studio, outside this repository, which this fork no longer uses (2026-09-20). This repo's own
theme flow is a `theme.yaml` built by `tools/build_theme.py` and copied to the card. What still exists from
the Launch Kit era is its output format: the `custom_*.h` headers in `src/theme/custom/` (compile-time
defaults an external push regenerates), and the `/sdput` and `/health` endpoints in `main.cpp` that such a push
talks to. Where a comment says "Launch Kit", it is describing that external tool, not something in this tree.

```
/themes/<slug>/
    splash.png  clock_plate.png  clock_overlay.png  radar_plate.png  ...
    clock_style.json  radar_style.json  weather_style.json  ticker_style.json  splash_style.json  intel_style.json
```

- `src/theme_sd.cpp` reads whole files into PSRAM (`theme_sd::read_whole`), portable
  across device and simulator.
- Every screen's art loads through one loader, `plate_sprite` (`src/theme/graphics/`):
  the flash-baked copy first (free), then the SD card, then nothing, which is a plainer
  screen. `plate_sprite::get` returns an LVGL image; `plate_sprite::load_pixels` returns
  raw pixels for the clock's hands and the wind screen, whose slots (`pixel_slot.h`) all
  reload after a release. A missing card never crashes.
- `src/theme_style.cpp` reads the per-theme JSON and overrides compiled defaults field
  by field.
- `src/theme_select.cpp` holds the active slug, persisted to NVS, listing whatever is
  installed under `/themes/`. Settings has a **Design** page to pick one.
- **Applying a theme is a reboot, never a live repaint.** Deliberate.

**Still compile-time, not yet per theme** (the remaining gap before themes are fully
data): fonts, clock hand pivot/blend/order, radar blip pivot and layer order, and the
`CUSTOM_HAS_*` gates that decide whether an element exists at all. The header comment
in `src/theme_style.h` carries the current, honest list.

### Pre-baked art (`src/theme_art.*`)

A PNG is a compressed picture; the panel wants raw RGB565. Measured on a 466x466 plate,
the SD path costs **226 ms to read the file and 205 ms to unpack it, every single time
the screen is shown**, plus 424 KB of PSRAM held while it is up. Storing raw pixels on
the card does not fix it: the raw file is *bigger* (424 KB vs 342 KB) and the 20 MHz SPI
card is the slow half, so it would still cost ~283 ms.

Flash does fix it. The ESP32-S3 memory-maps flash through its cache, so a baked asset is
just a pointer: no read, no decode, no PSRAM, nothing per show.

- `partitions_16MB_themeart.csv` is the stock `default_16MB.csv` with its **SPIFFS
  partition replaced by `themeart`**. Nothing ever used SPIFFS (Surveillance reads its
  frames off the SD card), so 3.375 MB had been dead since the first build. Later
  (2026-08-16) the OTA slot, `app1` and `otadata`, was reclaimed for theme art too, so
  there is no wireless update. `app0` and `nvs` keep their offset and size, so stored
  WiFi credentials survive the switch.
- `theme_art_bake.cpp` converts the active theme once, on the first boot after a push,
  inside the reboot the user is already waiting through. Its `ASSETS[]` table is in
  priority order, most-frequently-shown first, because a rich theme does not fit whole.
- **Sector alignment is not optional.** Flash erases 4 KB at a time. Packing blobs
  tightly meant each asset's erase clipped the tail of the one before it, and erased
  flash reads as `0xFF`, i.e. opaque white. It showed up as white bands along the bottom
  of most artwork, and small assets sharing a sector went entirely white.
- **Every blob is read back and compared after writing.** A mismatch rejects the asset to
  the SD path instead of displaying garbage. Corruption here is invisible in code and
  only surfaces as wrong pixels on a screen nobody may look at for days.
- **Bump `VERSION` whenever the baked layout changes.** Any other value makes the cache
  read as empty, so the next boot re-bakes. That is the only safe way to retire bad data.

Flash is a cache in front of the SD path, never a precondition for it. A miss, a format
mismatch, an asset that does not fit, or a failed verify all fall through to
`plate_sprite`'s SD path unchanged.

The simulator has no flash partition, so `theme_art` is stubbed out there and the sim
always takes the SD path. **Flash-path changes cannot be verified in the simulator** and
have to be checked on the device.

## Storage and persistence

- **NVS (`Preferences`)**: WiFi credentials, home lat/lon, range, units, mute, active
  theme slug. Survives power loss and firmware updates.
- **microSD**: plain SPI (not SD_MMC), 20 MHz. MOSI 1, SCK 2, MISO 3, CS 41. No
  card-detect line is wired. Mounted early in `setup()`, before the display starts, so
  SD-hosted splash art is not a boot-ordering problem. Holds theme folders,
  Surveillance clips, and road tile data. **Measured throughput is ~1.5 MB/s**, which is
  the whole reason pre-baked art lives in flash rather than as raw files on the card.
- **`themeart` flash partition**: 3.375 MB of raw, pre-converted theme pixels, written at
  install and read through the memory map. Rebuilt from the card whenever the active
  theme changes or the bake `VERSION` moves.

## Versioning

`FW_VERSION` in `src/config.h` is the user-visible version, shown on the Stats screen.
Bump it on release and tag the commit.

## File map

| File | Role |
|------|------|
| `input_router.cpp` | Single source of truth for knob behaviour and for what a swipe does (device + sim) |
| `swipe.h` / `swipe.cpp` | Touch swipe recogniser and the swipe ring (pure, host-tested) |
| `app_shell.cpp` | App list, switching, knob capture, enter/exit lifecycle, and the app picker overlay |
| `platform/wheel/` | The one knob-driven list renderer: fixed geometry (`wheel_layout.h`, pure and host-tested), one PSRAM canvas, blurred glow. Theme-free: it takes a `wheel::Look` |
| `wheel_look.cpp` | The one place a theme (palette `primary`/`muted`, fonts `wheel_sel`/`wheel_item`) becomes a `wheel::Look`; used by the picker and Settings |
| `knob.cpp` | KY-040 encoder decode and debounce |
| `display.cpp` | Panel init, LVGL setup, touch indev registration |
| `theme_sd.cpp` / `theme_select.cpp` / `theme_style.cpp` | SD theme load, selection, style override |
| `sdcard.cpp` | SD mount over SPI |
| `adsb_client.cpp` | Aircraft feed fetch and parse |
| `geo.h` / `aircraft.h` | Pure math and types, no hardware |
| `sim_main.cpp` / `sim_knob.cpp` | Desktop simulator entry point and virtual knob |
| `main.cpp` | Config, task creation, app registration, main loop |
