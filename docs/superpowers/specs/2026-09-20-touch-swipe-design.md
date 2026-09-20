# Touch swipes: sideways between apps, up and down between screens

Date: 2026-09-20. Status: design approved in conversation, spec awaiting review.

## Intent

Let a finger do what the knob already does: swipe **sideways** to move between apps, swipe
**up or down** to move between the screens inside an app (Weather's Now, Radar and 7-Day is the
first user). Touch is an **addition**. The knob stays a complete input on its own, and nothing
that works on the knob stops working.

This reverses, in part, the knob-only decision of 2026-08-16 (`docs/ARCHITECTURE.md`, "Known
gap: touch is still live" and the comment in `src/app/ui/ui.cpp` that says swiping went with
touch). It reverses only the *swipe* half: touch stays unable to tap, press or drag a widget.

Agreed in conversation:

- Touch is added alongside the knob, not instead of it.
- A swipe is a flick, not a drag. Nothing follows the finger. On release a recognised swipe
  triggers the normal switch, hidden by a short slide (about 150-250 ms). Drag-follow needs two
  screens alive at once and is out of scope (it conflicts with the enter/exit PSRAM model).
- Settings is not in the swipe ring.

## Findings that shaped this

- The plumbing mostly exists. `app_shell::next()/prev()` already slide, skip hidden apps and
  run `onExit`/`onEnter`. `ui_weather_step(int delta)` and `ui_weather_screen()` already step
  Weather's three screens.
- The CST9217 driver (`src/platform/display/touch_cst9217.{h,cpp}`) is kept in the repo but
  excluded from the device build by `build_src_filter` in `platformio.ini`. `display.cpp`
  registers no LVGL pointer device, and the code that mapped touch back through
  `display::rotation()` was removed with it.
- The simulator already reads the mouse as a pointer (`sdl_mouse_read`, `sim_main.cpp`).
- `ui.cpp` builds a scrollable LVGL tileview (Flight Tracker | Weather). Registering an LVGL
  pointer device would let a drag slide it between tiles without going through `app_shell`.
  That is why the gesture recogniser lives outside LVGL (see Rejected approaches).
- `display::setRotation()` accepts any angle for enclosure mounting. A swipe has to be
  rotated back into the logical frame or "left" means the wrong thing on a rotated Orb.
- Settings registers with `capture=true` (`sim_main.cpp`; the device registration in
  `main.cpp` is confirmed in the plan's first step).

## Design

### 1. Reading touch and recognising a swipe

**New pure module `src/core/swipe.{h,cpp}`.** No Arduino, LVGL or hardware dependency, so it
runs in the host tests.

```cpp
namespace swipe {
    enum class Dir { None, Left, Right, Up, Down };
    class Detector {
    public:
        // Call once per poll. Returns a Dir only on the poll where the finger lifts.
        Dir  feed(int x, int y, bool down, uint32_t nowMs, uint16_t rotationDeg);
        void cancel();   // the gesture in progress will report None when it lifts
    };
}
```

- The direction is what the finger did. Finger moving left is `Left`, which matches
  `app_shell::next()` sliding left. Finger moving up is `Up` ("later").
- A swipe counts only when all three hold, else the result is `None`:
  - travel of at least `SWIPE_MIN_PX` (default 60 of the 466 px);
  - the main axis at least `SWIPE_AXIS_RATIO` (default 2.0) times the other;
  - touch down to lift within `SWIPE_MAX_MS` (default 700).
  Diagonals, slow drags, taps and long presses are ignored, not guessed at.
- The three constants live in `config.h`, not in the detector (no magic numbers). The defaults
  are starting points, tuned on hardware.
- Rotation is handled inside the detector: the swipe vector is rotated back by `rotationDeg`
  (the display is rotated clockwise, so the vector turns anticlockwise) before it is
  classified. Near 45 degrees of mounting the axis test fails and the result is `None`, which
  is honest rather than wrong.

**Device reading.**

- Remove `touch_cst9217.cpp` from the `build_src_filter` exclusions of the device env and
  update the note beside it. Call `touch_begin()` at boot.
- If `touch_begin()` fails: log it and run knob-only. Touch must never block boot.
- `loop()` polls `touch_read()` every `SWIPE_POLL_MS` (default 20) and feeds the detector, the
  same place the knob is polled. LVGL never sees the touch and no pointer device is registered.
- Touch shares an I2C bus with the IMU, RTC and PMIC. Contention is measured on the board.

**Wake.** Any touch counts as activity (`display::noteActivity()`). A gesture that begins while
the screen is dimmed calls `Detector::cancel()`, so the touch that wakes the Orb does not also
navigate.

**Simulator.** A left-button mouse drag feeds the same detector through the same call.

### 2. What a swipe does

**One entry point: `input_router::swipe(swipe::Dir)`**, called by the device loop and the
simulator. This is the only place touch behaviour lives, as `dispatch()` is for the knob.

| Swipe | Action |
|---|---|
| `Left` | `app_shell::next()` in the swipe ring |
| `Right` | `app_shell::prev()` in the swipe ring |
| `Up` | the current app's pager with +1 (later screen) |
| `Down` | the current app's pager with -1 (earlier screen) |

**The swipe ring** skips hidden apps and any app registered with `capture=true`. Today that is
Clock, Flight Tracker, Weather and Intel, wrapping Intel to Clock. Settings stays reachable
from the knob switcher exactly as now. A ring with one member does nothing, so a theme that
hides apps cannot strand a swipe. The rule is derived from the existing `capture` flag rather
than a per-app list, so a new app that captures the knob is excluded without anyone remembering
to say so.

**Pagers.** An app opts in with `app_shell::setPager(slot, fn)` where
`bool fn(int delta)` returns whether it moved.

- Touch **does not wrap**: a north/south layout has ends, so at an end the pager returns false
  and nothing happens. The knob still wraps, as today.
- Weather's pager: if the target screen is out of range return false; otherwise call
  `ui_weather_step(delta)` and return true.
- Flight Tracker, Clock, Intel and Settings register no pager, so up and down do nothing there.
  Adding one later is one function.
- The vertical transition belongs to the Weather tile. Now, Radar and 7-Day are widgets on one
  screen, so a slide should cost no extra PSRAM. The plan's first step reads the tile's mode
  switching to confirm; if it is not cheap, the fallback is a short fade.

**Enforced in the shared router, not in each app** (CLAUDE.md rule 4). A warning beside one
call site protects one call site; these are checked in `input_router::swipe`:

- While the current app has captured the knob, all swipes are dropped.
- While the app-switcher overlay is up (`app_shell::browsing()`), swipes are dropped; the knob
  owns it.
- While a transition is running, swipes are dropped, not queued, so one flick cannot fire
  twice. If `app_shell` has no "transition in flight" query today, adding one is part of this
  work.

**Consequence, accepted:** because captured apps are not in the ring and swipes are dropped
inside them, touch cannot leave Settings. The knob's rock gesture still can.

### 3. What is deliberately not in this

- No taps, buttons, long presses or drag-follow.
- No page dots on a resting screen (`docs/adding-a-screen.md`: nothing new is drawn on a
  resting screen that a person did not ask for).
- No new theme keys, so no `THEME_CAPS` bump and no ledger entry.
- No LVGL pointer input device.

## Files touched

- New: `src/core/swipe.h`, `src/core/swipe.cpp`, `tests/test_swipe.cpp` (wired into
  `tests/run_host_tests.sh`).
- `src/config.h`: `SWIPE_MIN_PX`, `SWIPE_AXIS_RATIO`, `SWIPE_MAX_MS`, `SWIPE_POLL_MS`; bump
  `FW_VERSION`.
- `src/app/shell/app_shell.{h,cpp}`: swipe-ring cycling, `setPager`, a transition-in-flight
  query if absent.
- `src/app/shell/input_router.{h,cpp}`: `swipe()`.
- `src/main.cpp`: touch poll in `loop()`, wake handling, Weather's pager.
- `src/platform/sim/sim_main.cpp`: mouse drag feeds the detector; a capture mode for swipe
  screenshots.
- `platformio.ini`, `src/platform/display/display.cpp`: drop the touch exclusion and call
  `touch_begin()`.
- **Every place that says touch is off must change with it**, or it becomes a comment that lies:
  `docs/ARCHITECTURE.md` (Input model and "Known gap: touch is still live"), the note in
  `display.cpp`, the "Nothing here any more" note in `ui.cpp`, the `platformio.ini` note, and a
  short paragraph in `docs/adding-a-screen.md` on registering a pager.

## Testing

- **Host (`tests/test_swipe.cpp`):** each of the four directions; too short; too slow; diagonal;
  a tap; a long press; rotation 0/90/180/270 and one odd angle; `cancel()` reports `None`;
  a second gesture after the first is independent.
- **Simulator screenshots**, driven by injected samples in the pattern of `--knobshot`:
  the ring skips Settings; Weather stops at both ends; swipes are dropped inside Settings and
  during a transition; up/down do nothing in Flight Tracker.
- Both `pio run -e native` and `pio run -e esp32-s3-amoled-175` build (`pio` is at
  `~/.platformio/penv/bin/pio`). `bash tests/run_host_tests.sh` and `tests/test_default_theme.py`
  pass.
- **Not verifiable here, and said so:** touch reads on the real panel, I2C contention with the
  IMU/RTC/PMIC, physical swipe orientation, whether the threshold defaults feel right, wake
  behaviour, and the slide's frame time. These are the first checks on a board (CLAUDE.md
  rule 1).

## Open items the plan resolves first

1. Confirm the device registration of Settings (`main.cpp`, near `app_shell::add` for Settings)
   passes `capture=true`.
2. Read the Weather tile's mode switching and decide slide versus fade for up/down.
3. Read `app_shell`'s slide duration and whether a transition-in-flight query exists.
4. Check whether `touch_begin()`'s `Wire.begin()` collides with another driver's bus setup.

## Rejected approaches

- **LVGL pointer input plus `LV_EVENT_GESTURE`.** Less code, but it enables touch on every
  clickable and scrollable widget, including the tileview that would then slide between Flight
  Tracker and Weather outside `app_shell`.
- **Gestures inside Weather only.** Does not deliver sideways app switching.

## Branch and landing

Worktree branch `feat/touch-swipe`; explicit staging (never `git add -A`); fix before its test in
commit order; merge `--no-ff`. No release or publish step before a hardware boot.
