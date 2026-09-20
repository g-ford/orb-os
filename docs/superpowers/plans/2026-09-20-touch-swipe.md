# Touch Swipes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Swipe sideways on the touch panel to move between apps and up or down to move between the screens inside an app (Weather's Now / Radar / 7-Day first), with the knob still a complete input on its own.

**Architecture:** A pure recogniser (`src/core/swipe.{h,cpp}`) turns raw touch samples into `Left/Right/Up/Down/None`. `main.cpp` (device) and `sim_main.cpp` (simulator) both feed it and hand the result to one new router entry, `input_router::onSwipe()`, which enforces every "must not happen" rule in one place and calls `app_shell::swipeApp()` (sideways) or `app_shell::pageCurrent()` (up/down). No LVGL pointer device is registered on the device.

**Tech Stack:** C++17, LVGL 8.x, PlatformIO (`~/.platformio/penv/bin/pio`), Arduino-ESP32 on ESP32-S3, desktop simulator (`native` env, SDL2), plain-`c++` host tests under `tests/`.

**Spec:** `docs/superpowers/specs/2026-09-20-touch-swipe-design.md`

## Global Constraints

- Touch is an **addition**: the knob keeps working exactly as before; nothing the knob does may change.
- Detector limits live in `config.h`: `SWIPE_MIN_PX` 60, `SWIPE_AXIS_RATIO` 2.0, `SWIPE_MAX_MS` 700, `SWIPE_POLL_MS` 20 (starting values, tuned on hardware). No magic numbers in render or router code.
- A swipe is a flick: nothing follows the finger; a recognised swipe triggers the normal switch, hidden by a short slide (about 150-250 ms). No drag-follow.
- The swipe ring skips hidden apps **and any app registered with `capture=true`** (so Settings is out of the ring). A ring with one member does nothing.
- Up/down is opt-in per app through `app_shell::setPager(slot, fn)`. **Touch stops at the ends and does not wrap**; the knob still wraps.
- Enforced in the shared router, not per app: swipes are dropped while the app has captured the knob, while the app switcher is up, and while a transition is running (dropped, not queued).
- **No LVGL pointer input device on the device.** No taps, buttons, long presses, drag-follow or page dots. No new theme keys, so no `THEME_CAPS` bump.
- The detector rotates the swipe back through `display::rotation()` before classifying, so "left" means the same thing however the Orb is mounted.
- Both targets build: `pio run -e native` and `pio run -e esp32-s3-amoled-175`. `bash tests/run_host_tests.sh` and `python3 tests/test_default_theme.py` pass.
- Stage explicit paths, never `git add -A`. Fix before its test in commit order. Every commit ends with the trailer `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>`. No release, publish or deploy step: CLAUDE.md rule 1 needs a hardware boot first.
- Work in this worktree (`/Users/geoffford/code/orb-os/.claude/worktrees/touch-swipe`, branch `feat/touch-swipe`). It has no `.pio/` cache, so the first `pio` build is cold.

## Refinements found while planning (the spec's open items, resolved)

These change no agreed behaviour; they are what reading the code showed.

1. **Router entry is named `input_router::onSwipe`**, not `swipe`, so it never reads like the `swipe::` namespace.
2. **The router also drops swipes while the "Ready" notice, the knob-help panel or the wind notice is up** (`update_ui::awaitingAck()`, `knob_help::showing()`, `wind_notice::showing()`), because those own input in `dispatch()` too.
3. **`touch_begin()` is called from `main.cpp` beside `imu_begin()`/`rtc_begin()`**, not from `display.cpp`; `imu_begin()` is what brings up the shared I2C bus. `touch_begin()` already returns true even when the chip does not answer yet (it logs "will keep polling"), so "knob-only if touch fails" is the driver's existing behaviour.
4. **The simulator's LVGL pointer device is removed** so the sim matches the device. The mouse feeds the detector instead. (The sim currently registers `sdl_mouse_read` as an LVGL pointer indev, which the device does not have; left alone, a drag would scroll the tileview in the sim.)
5. **Weather's pager is `ui_weather_page()` in `ui.cpp`**, next to `s_weatherMode`, not in `main.cpp`.
6. **The up/down transition is a black-scrim fade**, not a slide. Now/Radar/7-Day switch by an instant show/hide inside `build_weather()`, and fading the whole panel's opacity would make LVGL allocate a 466x466 layer (about 434 KB).
7. **The sideways slide is `app_shell`'s existing `ANIM_MS` (250 ms), which nothing calls today** (the knob path uses the unanimated `load(idx, false, ...)`). `load()` runs the outgoing app's `onExit` before the slide and the incoming `onEnter` (a blocking decode) after it starts, so the slide may be invisible or the outgoing screen may lose its art mid-slide. `SWIPE_SLIDE` in `config.h` (1 = slide, 0 = cut like the knob) is the fallback, decided from the simulator frames in Task 2 and re-checked on a board.
8. Settings on the device is registered with `capture=true` (`main.cpp`, near the `settingsview::` registration), as in the simulator.

## Review Focus

Failure modes the spec implies but that no happy-path test would catch, most likely first. Each has a test in the task named.

1. **A touch that wakes a dimmed screen must not also navigate.** A gesture that begins while dimmed reports `None` (Task 1: `cancel_swallows_only_the_gesture_in_progress`, wired in Task 6).
2. **A theme that hides apps leaves a ring of one or none; the roster may also not exist yet.** A swipe does nothing: no spin, no crash (Task 1: `a_ring_of_one_or_none_goes_nowhere`).
3. **A second flick lands inside a running slide.** It is dropped, not queued, so one gesture cannot fire twice (Task 2: `--swipeshot`).
4. **A swipe while something else owns input**: Settings, the switcher, the knob-help panel. It must not steal the screen (Task 2: `--swipeshot`).
5. **A rotated Orb.** The direction stays logical at 90, 180, 270, an odd angle, and past 360 (Task 1: `rotation_*`).

---

## File Structure

| File | Responsibility |
|---|---|
| `src/core/swipe.h`, `src/core/swipe.cpp` (new) | Pure gesture logic: `Detector` (samples to `Dir`) and `ringNeighbour` (which app a swipe lands on). No Arduino, no LVGL. |
| `tests/swipe_test.cpp`, `tests/run_swipe_test.sh` (new) | Host tests for the above. |
| `src/config.h` | Swipe constants, `SWIPE_SLIDE`, `SWIPE_FADE_MS`, `FW_VERSION`. |
| `src/app/shell/app_shell.{h,cpp}` | `swipeApp`, `transitioning`, `setPager`, `pageCurrent`. |
| `src/app/shell/input_router.{h,cpp}` | `onSwipe`: the one place touch behaviour and its guards live. |
| `src/app/ui/ui.{h,cpp}` | `ui_weather_page` (pager with ends) and the fade scrim. |
| `src/main.cpp` | `touch_begin`, the touch poll and wake handling, Weather pager registration. |
| `src/platform/sim/sim_main.cpp` | Mouse feeds the detector, LVGL pointer device removed, `--swipeshot` harness, Weather pager registration. |
| `platformio.ini` | Device env stops excluding the touch driver; native env compiles `swipe.cpp`. |
| Docs and comments | Everything that says "touch is off" now says what is true. |

---

## Task 1: The swipe recogniser (pure logic)

**Files:**
- Create: `src/core/swipe.h`, `src/core/swipe.cpp`, `tests/swipe_test.cpp`, `tests/run_swipe_test.sh`
- Modify: `tests/run_host_tests.sh`, `src/config.h` (add after the `IDLE_DIM_MS` line, currently line 145)

**Interfaces:**
- Produces (used by Tasks 2-6):
  - `enum class swipe::Dir { None, Left, Right, Up, Down }`
  - `struct swipe::Limits { int minPx; float axisRatio; uint32_t maxMs; }`
  - `class swipe::Detector { explicit Detector(const Limits &); Dir feed(int x, int y, bool down, uint32_t nowMs, uint16_t rotationDeg); void cancel(); }`
  - `int swipe::ringNeighbour(const bool *skip, int count, int from, int dir)` returns the index a swipe lands on, or -1 for nowhere.
  - `config.h`: `SWIPE_MIN_PX`, `SWIPE_AXIS_RATIO`, `SWIPE_MAX_MS`, `SWIPE_POLL_MS`.

- [ ] **Step 1: Baseline the worktree**

Run:
```bash
cd /Users/geoffford/code/orb-os/.claude/worktrees/touch-swipe
~/.platformio/penv/bin/pio run -e native 2>&1 | tail -5
bash tests/run_host_tests.sh 2>&1 | tail -6
```
Expected: the native build ends `SUCCESS` (cold: it downloads libdeps first) and the host tests end `all host tests passed`. If either fails before any change, stop and report it; do not build on a red baseline.

- [ ] **Step 2: Write the failing test**

Create `tests/swipe_test.cpp`:
```cpp
// Host test for src/core/swipe.{h,cpp}.   tests/run_swipe_test.sh
#include "swipe.h"

#include <assert.h>
#include <stdio.h>

using swipe::Dir;

// The tests own their limits, so retuning config.h on the board cannot break them.
static const swipe::Limits LIM = { 60, 2.0f, 700 };

// One finger: down at (x0,y0), a few samples on the way, lifted after `ms`. The lift is fed
// position (0,0) on purpose: the detector must ignore where a lift claims to be.
static Dir drag(swipe::Detector &d, int x0, int y0, int x1, int y1, uint32_t ms, uint16_t rot = 0) {
    const int STEPS = 5;
    for (int i = 0; i <= STEPS; ++i)
        d.feed(x0 + (x1 - x0) * i / STEPS, y0 + (y1 - y0) * i / STEPS, true, ms * i / (STEPS + 1), rot);
    return d.feed(0, 0, false, ms, rot);
}

static void the_four_directions() {
    swipe::Detector d(LIM);
    assert(drag(d, 300, 233, 180, 233, 200) == Dir::Left);
    assert(drag(d, 180, 233, 300, 233, 200) == Dir::Right);
    assert(drag(d, 233, 320, 233, 200, 200) == Dir::Up);      // the finger moves UP the screen
    assert(drag(d, 233, 200, 233, 320, 200) == Dir::Down);
}

static void short_slow_and_still_are_not_swipes() {
    swipe::Detector d(LIM);
    assert(drag(d, 233, 233, 193, 233, 200) == Dir::None);    // 40 px
    assert(drag(d, 300, 233, 180, 233, 900) == Dir::None);    // 900 ms
    assert(drag(d, 233, 233, 233, 233, 80) == Dir::None);     // a tap
    assert(drag(d, 233, 233, 233, 233, 2000) == Dir::None);   // a long press
}

static void the_limits_are_inclusive() {
    swipe::Detector d(LIM);
    assert(drag(d, 200, 233, 140, 233, 200) == Dir::Left);    // exactly 60 px
    assert(drag(d, 200, 233, 141, 233, 200) == Dir::None);    // 59 px
    assert(drag(d, 100, 100, 200, 150, 200) == Dir::Right);   // exactly 2:1
    assert(drag(d, 100, 100, 200, 151, 200) == Dir::None);    // just under 2:1
    assert(drag(d, 300, 233, 180, 233, 700) == Dir::Left);    // exactly 700 ms
    assert(drag(d, 300, 233, 180, 233, 701) == Dir::None);    // one over
}

static void a_diagonal_is_ignored_not_guessed() {
    swipe::Detector d(LIM);
    assert(drag(d, 150, 150, 250, 250, 200) == Dir::None);
    assert(drag(d, 150, 150, 250, 190, 200) == Dir::Right);   // 100 x 40 is plainly sideways
}

// A display rotated clockwise by R shows logical Right as a finger moving in the direction
// R(Right). The detector turns the physical swipe back, so the logical direction comes out.
static void rotation_90() {
    swipe::Detector d(LIM);
    assert(drag(d, 233, 150, 233, 250, 200, 90) == Dir::Right);   // physical down
    assert(drag(d, 150, 233, 250, 233, 200, 90) == Dir::Up);      // physical right
    assert(drag(d, 233, 250, 233, 150, 200, 90) == Dir::Left);    // physical up
    assert(drag(d, 250, 233, 150, 233, 200, 90) == Dir::Down);    // physical left
}

static void rotation_180_and_270() {
    swipe::Detector d(LIM);
    assert(drag(d, 250, 233, 150, 233, 200, 180) == Dir::Right);  // physical left
    assert(drag(d, 233, 150, 233, 250, 200, 180) == Dir::Up);     // physical down
    assert(drag(d, 233, 250, 233, 150, 200, 270) == Dir::Right);  // physical up
    assert(drag(d, 250, 233, 150, 233, 200, 270) == Dir::Up);     // physical left
}

static void rotation_wraps_past_360() {
    swipe::Detector d(LIM);
    assert(drag(d, 233, 150, 233, 250, 200, 450) == Dir::Right);  // 450 is 90
}

static void an_odd_mounting_angle_still_reads_a_clear_swipe() {
    swipe::Detector d(LIM);
    // Logical Right at 30 degrees clockwise is about (87, 50) on the panel.
    assert(drag(d, 150, 150, 237, 200, 200, 30) == Dir::Right);
}

static void forty_five_degrees_is_ambiguous_so_it_is_nothing() {
    swipe::Detector d(LIM);
    assert(drag(d, 150, 233, 250, 233, 200, 45) == Dir::None);
}

static void cancel_swallows_only_the_gesture_in_progress() {
    swipe::Detector d(LIM);
    d.feed(300, 233, true, 0, 0);
    d.feed(240, 233, true, 100, 0);
    d.cancel();
    assert(d.feed(180, 233, false, 200, 0) == Dir::None);         // the wake touch: nothing happens
    assert(drag(d, 300, 233, 180, 233, 200) == Dir::Left);        // the next one is a fresh gesture
    d.cancel();                                                    // nothing in progress: a no-op...
    assert(drag(d, 300, 233, 180, 233, 200) == Dir::Left);        // ...that must not swallow the NEXT one
}

static void a_lift_with_no_touch_is_nothing() {
    swipe::Detector d(LIM);
    assert(d.feed(0, 0, false, 10, 0) == Dir::None);
    assert(d.feed(0, 0, false, 30, 0) == Dir::None);
}

static void the_answer_arrives_once_and_only_on_lift() {
    swipe::Detector d(LIM);
    for (int i = 0; i < 5; ++i)
        assert(d.feed(300 - 30 * i, 233, true, i * 40, 0) == Dir::None);   // still down: no answer yet
    assert(d.feed(0, 0, false, 200, 0) == Dir::Left);
    assert(d.feed(0, 0, false, 220, 0) == Dir::None);             // a second lift is not a second swipe
}

static void the_ring_skips_and_wraps() {
    // Clock, Flight, Weather, Intel, Settings (captured, so skipped)
    const bool skip[5] = { false, false, false, false, true };
    assert(swipe::ringNeighbour(skip, 5, 3, +1) == 0);   // Intel -> Clock, Settings skipped
    assert(swipe::ringNeighbour(skip, 5, 0, -1) == 3);   // Clock back -> Intel
    assert(swipe::ringNeighbour(skip, 5, 0, +1) == 1);
    assert(swipe::ringNeighbour(skip, 5, 1, -1) == 0);
    assert(swipe::ringNeighbour(skip, 5, 4, +1) == 0);   // from a skipped app: still lands somewhere sane
}

static void a_ring_of_one_or_none_goes_nowhere() {
    const bool onlyClock[3] = { false, true, true };
    assert(swipe::ringNeighbour(onlyClock, 3, 0, +1) == -1);
    assert(swipe::ringNeighbour(onlyClock, 3, 0, -1) == -1);
    const bool none[2] = { true, true };
    assert(swipe::ringNeighbour(none, 2, 0, +1) == -1);
    assert(swipe::ringNeighbour(none, 0, 0, +1) == -1);           // no roster registered yet
    const bool one[1] = { false };
    assert(swipe::ringNeighbour(one, 1, 0, +1) == -1);
    assert(swipe::ringNeighbour(onlyClock, 3, 0, 0) == -1);       // no direction, no move
}

int main() {
    the_four_directions();
    short_slow_and_still_are_not_swipes();
    the_limits_are_inclusive();
    a_diagonal_is_ignored_not_guessed();
    rotation_90();
    rotation_180_and_270();
    rotation_wraps_past_360();
    an_odd_mounting_angle_still_reads_a_clear_swipe();
    forty_five_degrees_is_ambiguous_so_it_is_nothing();
    cancel_swallows_only_the_gesture_in_progress();
    a_lift_with_no_touch_is_nothing();
    the_answer_arrives_once_and_only_on_lift();
    the_ring_skips_and_wraps();
    a_ring_of_one_or_none_goes_nowhere();
    printf("swipe: all ok\n");
    return 0;
}
```

Create `tests/run_swipe_test.sh`:
```bash
#!/bin/bash
# Builds and runs tests/swipe_test.cpp on the host. swipe.cpp is pure, so no libdeps are needed.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc/core tests/swipe_test.cpp src/core/swipe.cpp -o "$OUT/swipe_test"
"$OUT/swipe_test"
```

- [ ] **Step 3: Run it to verify it fails**

Run: `bash tests/run_swipe_test.sh`
Expected: FAIL to compile with `'swipe.h' file not found`.

- [ ] **Step 4: Write the header**

Create `src/core/swipe.h`:
```cpp
#pragma once
// Touch swipes, as pure logic: no Arduino, no LVGL, no hardware, so it runs in the host tests.
//
// The device reads the CST9217 and the simulator reads the mouse, and both feed the same
// Detector, which is what makes "it behaved right in the sim" mean "it behaves right on the
// Orb". What a swipe DOES is decided in input_router::onSwipe(), never here.
#include <stdint.h>

namespace swipe {

enum class Dir { None, Left, Right, Up, Down };

// The three tests a touch has to pass to be a swipe. Passed in rather than read from config.h so
// the tests own their limits: retuning the defaults on the board must not break them.
struct Limits {
    int      minPx;       // travel along the main axis
    float    axisRatio;   // the main axis must be at least this many times the other
    uint32_t maxMs;       // finger down to finger up
};

class Detector {
public:
    explicit Detector(const Limits &limits) : lim_(limits) {}

    // Call once per poll with whether a finger is down and where. Returns a direction only on
    // the poll where the finger lifts; x and y are ignored on a lift. `rotationDeg` is
    // display::rotation(): the swipe is turned back through it so Left means the logical left
    // however the Orb is mounted. A swipe within about 26 degrees of an axis (after rotation)
    // is a swipe; near 45 degrees it is nothing rather than a guess.
    Dir  feed(int x, int y, bool down, uint32_t nowMs, uint16_t rotationDeg);

    // The gesture in progress will report None when it lifts. A no-op when no finger is down,
    // so it can never swallow the NEXT gesture. Used for the touch that wakes a dimmed screen.
    void cancel();

private:
    Limits   lim_;
    bool     active_ = false, cancelled_ = false;
    int      x0_ = 0, y0_ = 0, x1_ = 0, y1_ = 0;
    uint32_t t0_ = 0;
};

// Which app a swipe lands on. `skip[i]` is true for an app the ring must not visit (hidden, or
// one that holds the knob, like Settings). `dir` is +1 or -1; the ring wraps. Returns -1 when
// there is nowhere to go: no roster, a ring of one, or everything else skipped. `from` may
// itself be skipped.
int ringNeighbour(const bool *skip, int count, int from, int dir);

}  // namespace swipe
```

- [ ] **Step 5: Write the implementation**

Create `src/core/swipe.cpp`:
```cpp
#include "swipe.h"
#include <math.h>

namespace swipe {

namespace {

// The physical vector, turned back through the display's clockwise rotation into the logical
// frame. Screen coordinates have y pointing down, and a clockwise turn by R takes (x,y) to
// (x cos R - y sin R, x sin R + y cos R), so the way back is the transpose. Cardinal angles use
// exact values: cosf(M_PI/2) is not quite zero, and a swipe test should not depend on that.
void rotate_back(int px, int py, uint16_t deg, float &lx, float &ly) {
    deg %= 360;
    float c, s;
    switch (deg) {
        case 0:   c = 1;  s = 0;  break;
        case 90:  c = 0;  s = 1;  break;
        case 180: c = -1; s = 0;  break;
        case 270: c = 0;  s = -1; break;
        default: {
            const float r = (float)deg * 3.14159265f / 180.0f;
            c = cosf(r);
            s = sinf(r);
        }
    }
    lx = px * c + py * s;
    ly = -px * s + py * c;
}

Dir classify(float dx, float dy, const Limits &lim) {
    const float ax = fabsf(dx), ay = fabsf(dy);
    if (ax >= ay) {
        if (ax < (float)lim.minPx || ax < lim.axisRatio * ay) return Dir::None;
        return dx < 0 ? Dir::Left : Dir::Right;
    }
    if (ay < (float)lim.minPx || ay < lim.axisRatio * ax) return Dir::None;
    return dy < 0 ? Dir::Up : Dir::Down;   // y grows downward: a negative dy is a finger moving up
}

}  // namespace

Dir Detector::feed(int x, int y, bool down, uint32_t nowMs, uint16_t rotationDeg) {
    if (down) {
        if (!active_) { active_ = true; cancelled_ = false; x0_ = x; y0_ = y; t0_ = nowMs; }
        x1_ = x;
        y1_ = y;
        return Dir::None;
    }
    if (!active_) return Dir::None;          // a lift with no touch before it
    active_ = false;
    if (cancelled_) return Dir::None;
    if ((uint32_t)(nowMs - t0_) > lim_.maxMs) return Dir::None;
    float lx, ly;
    rotate_back(x1_ - x0_, y1_ - y0_, rotationDeg, lx, ly);
    return classify(lx, ly, lim_);
}

void Detector::cancel() {
    if (active_) cancelled_ = true;
}

int ringNeighbour(const bool *skip, int count, int from, int dir) {
    if (count <= 0 || dir == 0) return -1;
    const int step = dir > 0 ? 1 : -1;
    for (int n = 1; n < count; ++n) {        // count-1 candidates: never `from` itself
        const int idx = ((from + step * n) % count + count) % count;
        if (!skip[idx]) return idx;
    }
    return -1;
}

}  // namespace swipe
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `bash tests/run_swipe_test.sh`
Expected: PASS, last line `swipe: all ok`. If a rotation assertion fails, the fault is the rotation sign in `rotate_back`, not in the test: the comment above the `rotation_90` test states the convention.

- [ ] **Step 7: Add the constants**

In `src/config.h`, directly below the `#define IDLE_DIM_MS ...` line (currently line 145), add:
```c
// Touch swipes (src/core/swipe.*). A swipe is a flick: it counts only when it travels at least
// SWIPE_MIN_PX along its main axis, that axis is at least SWIPE_AXIS_RATIO times the other, and
// the finger is up again within SWIPE_MAX_MS. Anything else (a tap, a slow drag, a diagonal) is
// ignored rather than guessed at. Starting values, to be tuned on the board.
#define SWIPE_MIN_PX        60
#define SWIPE_AXIS_RATIO    2.0f
#define SWIPE_MAX_MS        700
#define SWIPE_POLL_MS       20             // how often loop() reads the touch panel
```

- [ ] **Step 8: Wire the test into the host runner**

In `tests/run_host_tests.sh`, add after the `weather` line:
```bash
run "swipe"                              bash tests/run_swipe_test.sh
```

- [ ] **Step 9: Run the host suite**

Run: `bash tests/run_host_tests.sh 2>&1 | tail -8`
Expected: `── swipe` followed by `swipe: all ok`, and the run ends `all host tests passed`.

- [ ] **Step 10: Commit**

```bash
git add src/core/swipe.h src/core/swipe.cpp tests/swipe_test.cpp tests/run_swipe_test.sh tests/run_host_tests.sh src/config.h
git commit -F - <<'EOF'
Add the swipe recogniser: touch samples to Left/Right/Up/Down, and the app ring

Pure logic with host tests: a detector that classifies one finger's path
(distance, axis ratio, duration, rotation back through display::rotation()),
and ringNeighbour, which says which app a swipe lands on when some are hidden
or hold the knob. Nothing uses it yet.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
```

---

## Task 2: Swipe between apps (shell, router, simulator harness)

**Files:**
- Modify: `src/app/shell/app_shell.h`, `src/app/shell/app_shell.cpp`, `src/app/shell/input_router.h`, `src/app/shell/input_router.cpp`, `src/config.h`, `platformio.ini` (native `build_src_filter`, line 116), `src/platform/sim/sim_main.cpp`

**Interfaces:**
- Consumes: `swipe::Dir`, `swipe::ringNeighbour` (Task 1).
- Produces (used by Tasks 3-6):
  - `bool app_shell::swipeApp(int dir)`: `dir > 0` next, `< 0` previous, in the swipe ring; returns whether it moved.
  - `bool app_shell::transitioning()`: a swipe's slide is still running.
  - `void input_router::onSwipe(swipe::Dir d)`.
  - `SWIPE_SLIDE` in `config.h`.
  - Sim harness: `--swipeshot <prefix>`; file-scope helpers `g_swPlan`, `sw_check`, `sw_swipe`, `sw_build_plan(const std::string &prefix)` in `sim_main.cpp` (Task 3 appends to `sw_build_plan`).

- [ ] **Step 1: Write the failing harness**

In `src/platform/sim/sim_main.cpp`:

1. Add includes near the other `#include`s at the top (after `#include "knob_help.h"`, line 39):
```cpp
#include "swipe.h"
#include <functional>
#include <string>
#include <vector>
```
2. Add above the line `int main(int argc, char **argv)` (find it with `grep -n "^int main" src/platform/sim/sim_main.cpp`):
```cpp
// --swipeshot <prefix>: what a recognised swipe DOES to a real roster, driven straight at
// input_router::onSwipe() the way --rockshot drives the knob. The detector has its own host test;
// this checks which apps a swipe visits, which screens it refuses to land on, and what stops it.
// One action per tick (450 ms apart) so a slide has finished before the next swipe lands, except
// where a step means to land inside one.
static std::vector<std::function<void()>> g_swPlan;
static bool g_swFailed = false;
static void sw_check(const char *what, bool ok) {
    printf("[swipeshot] %-58s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) g_swFailed = true;
}
static void sw_swipe(swipe::Dir d, int wantApp, const char *what) {
    g_swPlan.push_back([=]() { input_router::onSwipe(d); sw_check(what, app_shell::index() == wantApp); });
}
static void sw_build_plan(const std::string &prefix) {
    using swipe::Dir;
    g_swPlan.push_back([]() {
        if (app_shell::browsing()) app_shell::browsePress();
        app_shell::selectApp(app_shell::APP_CLOCK);
    });
    // The first swipe is also photographed 100 ms into its slide, to see what the outgoing screen
    // looks like once its onExit has run.
    g_swPlan.push_back([prefix]() {
        input_router::onSwipe(Dir::Left);
#if SWIPE_SLIDE
        lv_tick_inc(100); lv_timer_handler(); lv_refr_now(NULL);
        sim_save_frame((prefix + "-mid-slide.bmp").c_str());
#endif
        sw_check("left from Clock visits Flight Tracker", app_shell::index() == app_shell::APP_FLIGHT);
    });
#if APPS_WEATHER
    sw_swipe(Dir::Left,  app_shell::APP_WEATHER, "left again visits Weather");
#endif
    sw_swipe(Dir::Left,  app_shell::APP_INTEL,   "left again visits Intel");
    sw_swipe(Dir::Left,  app_shell::APP_CLOCK,   "left from Intel wraps to Clock, skipping Settings");
    sw_swipe(Dir::Right, app_shell::APP_INTEL,   "right from Clock wraps to Intel, skipping Settings");
    g_swPlan.push_back([prefix]() {      // a settled frame after two animated loads: is the art all there?
        lv_timer_handler(); lv_refr_now(NULL);
        sim_save_frame((prefix + "-after-ring.bmp").c_str());
    });

    // Things that must stop a swipe.
    g_swPlan.push_back([]() { app_shell::selectApp(app_shell::APP_SETTINGS); });
    g_swPlan.push_back([]() {
        input_router::onSwipe(Dir::Left);
        input_router::onSwipe(Dir::Right);
        sw_check("swipes are dropped inside Settings (it holds the knob)",
                 app_shell::index() == app_shell::APP_SETTINGS);
        app_shell::selectApp(app_shell::APP_CLOCK);
        app_shell::openSwitcher();
    });
    g_swPlan.push_back([]() {
        input_router::onSwipe(Dir::Left);
        sw_check("swipes are dropped while the app switcher is up",
                 app_shell::browsing() && app_shell::index() == app_shell::APP_CLOCK);
        app_shell::browsePress();        // commit back into the clock
        knob_help::show();
    });
    g_swPlan.push_back([]() {
        input_router::onSwipe(Dir::Left);
        sw_check("swipes are dropped while the knob-help panel is up",
                 knob_help::showing() && app_shell::index() == app_shell::APP_CLOCK);
        knob_help::dismiss();
    });
#if SWIPE_SLIDE
    g_swPlan.push_back([]() {
        input_router::onSwipe(Dir::Left);    // Clock -> Flight: a real slide between two screens
        input_router::onSwipe(Dir::Left);    // lands inside it
        sw_check("a second swipe inside a slide is dropped, not queued",
                 app_shell::index() == app_shell::APP_FLIGHT);
    });
#endif
}
```
3. Parse the flag: after the `rockShot` line (line 810) add
```cpp
    const char *swipeShot  = (argc >= 3 && strcmp(argv[1], "--swipeshot")  == 0) ? argv[2] : NULL;
```
4. Add `&& !swipeShot` to the end of the `interactive` definition (line 852), and `|| swipeShot` to the roster line (line 972): `if (interactive || wifiShot || knobShot || windShot || rockShot || swipeShot) sim_register_apps(radarScreen);`
5. In the main loop, directly before the `if (knobShot) {` block (line ~1650), add:
```cpp
        if (swipeShot) {
            static size_t swNext = 0;
            static Uint32 swAt = 0;
            if (g_swPlan.empty()) sw_build_plan(swipeShot);
            if (app_shell::count() > 0 && now - start > 3000 && now - swAt > 450) {
                swAt = now;
                if (swNext < g_swPlan.size()) g_swPlan[swNext++]();
                else { printf("[swipeshot] %s\n", g_swFailed ? "FAILED" : "all ok"); run = false; }
            }
        }
```

- [ ] **Step 2: Run the build to verify it fails**

Run: `~/.platformio/penv/bin/pio run -e native 2>&1 | grep -E "error|SUCCESS|FAILED" | head`
Expected: FAIL, with an error that `onSwipe` is not a member of `input_router`. (An undefined `SWIPE_SLIDE` is not an error: `#if` treats it as 0. A link error naming `swipe::` can also appear, because `swipe.cpp` is not in the native `build_src_filter` yet.)

- [ ] **Step 3: Compile `swipe.cpp` into the native build and add `SWIPE_SLIDE`**

In `platformio.ini`, on the native env's long `build_src_filter` line (line 116), replace `+<core/roads_sd.cpp>` with `+<core/roads_sd.cpp> +<core/swipe.cpp>`. (The device env globs `**/*.cpp`, so it needs nothing.)

In `src/config.h`, below the swipe constants from Task 1, add:
```c
// 1: a swipe between apps slides (app_shell's ANIM_MS). 0: it cuts, exactly like the knob. The
// slide runs the outgoing app's onExit before it starts and the incoming onEnter (a blocking
// decode) right after, and no other caller has ever used that path, so this is the switch to
// throw if the simulator or the board shows the outgoing screen breaking mid-slide.
#define SWIPE_SLIDE         1
```

- [ ] **Step 4: Add the shell primitives**

In `src/app/shell/app_shell.h`, add inside `namespace app_shell`, directly after the `void prev();` line:
```cpp
    // Touch. A swipe moves through the same apps the knob switcher does, except that it skips any
    // app registered with capture=true (Settings): touch never lands anywhere touch cannot leave.
    // dir > 0 is the next app, dir < 0 the previous one; the ring wraps. Returns whether it moved.
    // Whether a swipe is ALLOWED right now is input_router::onSwipe()'s decision, not this one's.
    bool swipeApp(int dir);
    // True while a swipe's slide is still running, so a second flick cannot land on top of it.
    bool transitioning();
```

In `src/app/shell/app_shell.cpp`:
1. Add `#include "swipe.h"` after `#include "diag_log.h"`.
2. In the anonymous namespace, below `bool s_captured = false;`, add:
```cpp
    uint32_t s_slideUntil = 0;   // millis() when a running slide ends; 0 = none has run yet
```
3. In `load()`, inside `if (animate) { ... }`, directly after the `lv_scr_load_anim(...)` call, add:
```cpp
                s_slideUntil = millis() + ANIM_MS;   // a swipe that lands inside this is dropped: see transitioning()
```
4. At the end of the file (after `app_shell::selectApp`), add:
```cpp
bool app_shell::swipeApp(int dir) {
    if (!s_count || dir == 0) return false;
    bool skip[MAX_APPS];
    for (int i = 0; i < s_count; ++i) skip[i] = s_apps[i].hidden || s_apps[i].capture;
    const int to = swipe::ringNeighbour(skip, s_count, s_cur, dir > 0 ? +1 : -1);
    if (to < 0) return false;
#if SWIPE_SLIDE
    load(to, true, dir > 0);
#else
    load(to, false, true);
#endif
    return true;
}

bool app_shell::transitioning() {
    return s_slideUntil != 0 && (int32_t)(s_slideUntil - millis()) > 0;
}
```

- [ ] **Step 5: Add the router entry**

In `src/app/shell/input_router.h`, add `#include "swipe.h"` after `#pragma once`, and inside `namespace input_router` add:
```cpp
    // A recognised touch swipe. To touch what dispatch() is to the knob: the single place its
    // behaviour and its guards live, called identically by the device loop and the simulator.
    void onSwipe(swipe::Dir d);
```

In `src/app/shell/input_router.cpp`, at the end of the file add:
```cpp
// Touch, in one place. Sideways swipes move between apps; up and down step the screens inside an
// app that has registered a pager.
//
// EVERY reason a swipe must not act is checked HERE, not left to each app, because a rule kept
// beside one call site protects one call site:
//  - the notices that own input in dispatch() above (the Ready notice, the knob-help panel, the
//    wind screen) own it against touch too;
//  - an app that has captured the knob (Settings) is not to be pulled out from under itself, and
//    touch has no way to leave it, so it never gets there either (app_shell's swipe ring skips it);
//  - while the switcher is up the knob owns it;
//  - while a slide is running a second flick is DROPPED, not queued, so one gesture cannot fire
//    twice (the same rule as s_firedAt for the rock).
void input_router::onSwipe(swipe::Dir d) {
    if (d == swipe::Dir::None) return;
    if (update_ui::awaitingAck() || knob_help::showing() || wind_notice::showing()) return;
    if (app_shell::browsing() || app_shell::captured() || app_shell::transitioning()) return;
    switch (d) {
        case swipe::Dir::Left:  app_shell::swipeApp(+1); break;
        case swipe::Dir::Right: app_shell::swipeApp(-1); break;
        default: break;   // Up and Down: pagers, added with the Weather pager
    }
}
```

- [ ] **Step 6: Build and run the harness**

Run:
```bash
~/.platformio/penv/bin/pio run -e native 2>&1 | grep -E "error|SUCCESS|FAILED" | head
OUT=$(mktemp -d); echo "$OUT"
.pio/build/native/program --swipeshot "$OUT/sw" 2>&1 | grep swipeshot
```
Expected: every `[swipeshot]` line ends `ok`, the last line is `[swipeshot] all ok`. If the first swipe reports a wrong app, read the printed `[shell] app N/M` lines to see the roster the sim actually registered before touching the expectations.

- [ ] **Step 7: Look at the two frames**

Run:
```bash
for f in mid-slide after-ring; do sips -s format png "$OUT/sw-$f.bmp" --out "$OUT/sw-$f.png" >/dev/null; done; ls "$OUT"
```
Then open `$OUT/sw-mid-slide.png` and `$OUT/sw-after-ring.png` with the Read tool.

Decide from the frames:
- `after-ring` must show Intel drawn completely (plate, text, glass). If parts are missing, an `onEnter` decode did not finish or an `onExit` freed something the screen still shows: set `SWIPE_SLIDE` to `0` in `src/config.h`, rebuild, and re-run Step 6 (the "second swipe inside a slide" check is compiled out with it). Record which happened in the commit message.
- `mid-slide` may show a plain frame (the blocking `onEnter` can consume the whole slide before the first draw). That is acceptable and is what the fallback exists for; note what you saw. It is not acceptable for it to show garbage or a crash.

- [ ] **Step 8: Commit**

```bash
git add src/app/shell/app_shell.h src/app/shell/app_shell.cpp src/app/shell/input_router.h src/app/shell/input_router.cpp src/config.h platformio.ini src/platform/sim/sim_main.cpp
git commit -F - <<'EOF'
Swipe sideways between apps: the ring, the router entry and its guards

app_shell::swipeApp walks the apps the knob switcher does, skipping hidden
ones and any that hold the knob (Settings). input_router::onSwipe is the one
place every guard lives: notices, a captured app, the switcher, and a slide
already running. --swipeshot drives it against a real roster.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
```

---

## Task 3: Up and down between screens (pager, Weather)

**Files:**
- Modify: `src/app/shell/app_shell.h`, `src/app/shell/app_shell.cpp`, `src/app/shell/input_router.cpp`, `src/app/ui/ui.h`, `src/app/ui/ui.cpp`, `src/main.cpp` (Weather `app_shell::add`, line ~2484), `src/platform/sim/sim_main.cpp` (Weather `app_shell::add`, line ~669, and `sw_build_plan`)

**Interfaces:**
- Consumes: `input_router::onSwipe`, `sw_build_plan` (Task 2); `ui_weather_step`, `ui_weather_screen`, `WX_SCREEN_*`.
- Produces:
  - `typedef bool (*app_pager_t)(int delta)`; `void app_shell::setPager(int slot, app_pager_t fn)`; `bool app_shell::pageCurrent(int delta)`.
  - `bool ui_weather_page(int delta)`: one Weather screen forward or back, stopping at the ends.

- [ ] **Step 1: Extend the harness (failing)**

In `sw_build_plan` (`sim_main.cpp`), append before its closing brace, and add the helper above `sw_build_plan`:
```cpp
static void sw_page(swipe::Dir d, int wantScreen, const char *what) {
    g_swPlan.push_back([=]() { input_router::onSwipe(d); sw_check(what, ui_weather_screen() == wantScreen); });
}
```
and inside `sw_build_plan`, last:
```cpp
#if APPS_WEATHER
    g_swPlan.push_back([]() { app_shell::selectApp(app_shell::APP_WEATHER); });
    sw_page(Dir::Down, WX_SCREEN_NOW,   "down at Now stops: touch has ends");
    sw_page(Dir::Up,   WX_SCREEN_RADAR, "up steps Now to Radar");
    sw_page(Dir::Up,   WX_SCREEN_WEEK,  "up steps Radar to 7-Day");
    sw_page(Dir::Up,   WX_SCREEN_WEEK,  "up at 7-Day stops");
    sw_page(Dir::Down, WX_SCREEN_RADAR, "down steps 7-Day to Radar");
    sw_page(Dir::Down, WX_SCREEN_NOW,   "down steps Radar to Now");
#endif
    g_swPlan.push_back([]() { app_shell::selectApp(app_shell::APP_FLIGHT); });
    g_swPlan.push_back([]() {
        input_router::onSwipe(Dir::Up);
        input_router::onSwipe(Dir::Down);
        sw_check("up and down do nothing where no pager is registered",
                 app_shell::index() == app_shell::APP_FLIGHT);
    });
```

- [ ] **Step 2: Run the build to verify it fails**

Run: `~/.platformio/penv/bin/pio run -e native 2>&1 | grep -E "error|SUCCESS" | head -3`
Expected: it still builds (the harness only uses existing symbols), so run the harness instead:
```bash
OUT=$(mktemp -d); .pio/build/native/program --swipeshot "$OUT/sw" 2>&1 | grep -E "swipeshot" | tail -9
```
Expected: FAIL lines: `up steps Now to Radar` (nothing steps Weather yet).

- [ ] **Step 3: The shell pager**

In `src/app/shell/app_shell.h`, add next to the other typedefs (after `typedef void (*app_turn_t)(int delta);`):
```cpp
// A pager steps the screens INSIDE an app for touch: +1 is the later screen, -1 the earlier one.
// It returns whether it moved. Touch stops at the ends (a north/south layout has ends); the
// knob's own turn handler still wraps.
typedef bool (*app_pager_t)(int delta);
```
and inside `namespace app_shell`, after `transitioning()`:
```cpp
    // Opt an app in to up/down swipes. `slot` is an app_shell::Slot. An app with no pager simply
    // ignores up and down.
    void setPager(int slot, app_pager_t fn);
    bool pageCurrent(int delta);   // false when the current app has no pager, or it did not move
```

In `src/app/shell/app_shell.cpp`: add `app_pager_t pager;` as the last member of `struct App`, add `s_apps[s_count].pager = nullptr;` after `s_apps[s_count].hidden = hidden;` in `app_shell::add`, and at the end of the file:
```cpp
void app_shell::setPager(int slot, app_pager_t fn) {
    if (slot >= 0 && slot < s_count) s_apps[slot].pager = fn;
}

bool app_shell::pageCurrent(int delta) {
    if (!s_count || delta == 0 || !s_apps[s_cur].pager) return false;
    return s_apps[s_cur].pager(delta);
}
```

- [ ] **Step 4: The router**

In `input_router.cpp` `onSwipe`, replace the `default:` line with:
```cpp
        case swipe::Dir::Up:    app_shell::pageCurrent(+1); break;   // a finger moving up asks for the later screen
        case swipe::Dir::Down:  app_shell::pageCurrent(-1); break;
        default: break;
```
and fix the function's comment: change "(Task 3)" to nothing.

- [ ] **Step 5: Weather's pager**

In `src/app/ui/ui.h`, after `ui_weather_reset`:
```cpp
bool ui_weather_page(int delta);             // touch: one screen forward (+1) or back (-1), STOPPING at the ends; false if it did not move
```
In `src/app/ui/ui.cpp`, directly after `ui_weather_step`:
```cpp
// Touch's version of ui_weather_step(): the same step, but it stops at the ends instead of
// wrapping. The knob turns a dial, which has no ends; a swipe moves along a column, which does.
bool ui_weather_page(int delta) {
    const int to = (int)s_weatherMode + (delta > 0 ? 1 : -1);
    if (delta == 0 || to < 0 || to >= WX_SCREEN_COUNT) return false;
    ui_weather_step(delta);
    return true;
}
```

- [ ] **Step 6: Register the pager on the device and in the simulator**

`src/main.cpp`, right after the Weather `app_shell::add(radarScreen, theme_style::names().weather, ...)` line (~2484; read the two lines around it first and put the new line inside the same `#if APPS_WEATHER` guard the `add` sits in):
```cpp
    app_shell::setPager(app_shell::APP_WEATHER, ui_weather_page);   // up/down swipes step Now / Radar / 7-Day, and stop at the ends
```
`src/platform/sim/sim_main.cpp`, right after the Weather `app_shell::add(radarScreen, theme_style::names().weather, ...)` statement in `sim_register_apps` (~line 669, inside the same `#if APPS_WEATHER`):
```cpp
    app_shell::setPager(app_shell::APP_WEATHER, ui_weather_page);
```
(`ui.h` is already included by both files: they call `ui_weather_step`.)

- [ ] **Step 7: Build and run the harness**

```bash
~/.platformio/penv/bin/pio run -e native 2>&1 | grep -E "error|SUCCESS" | head -3
OUT=$(mktemp -d); .pio/build/native/program --swipeshot "$OUT/sw" 2>&1 | grep swipeshot
```
Expected: every line `ok`, ending `[swipeshot] all ok`. Also re-run the existing Weather harness to prove the knob path is untouched: `.pio/build/native/program --wxscreens "$OUT/wx" 2>&1 | grep wxscreens` should show every check `ok` (it wraps 7-Day to Now on a turn, which the pager must not have changed).

- [ ] **Step 8: Commit**

```bash
git add src/app/shell/app_shell.h src/app/shell/app_shell.cpp src/app/shell/input_router.cpp src/app/ui/ui.h src/app/ui/ui.cpp src/main.cpp src/platform/sim/sim_main.cpp
git commit -F - <<'EOF'
Swipe up and down between the screens inside an app; Weather registers a pager

An app opts in with app_shell::setPager. Touch stops at the ends where the
knob wraps. Weather's pager is ui_weather_page, so Now, Radar and 7-Day
answer to a swipe without the knob's behaviour changing.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
```

---

## Task 4: A short fade on Weather's up/down

**Files:**
- Modify: `src/app/ui/ui.cpp`, `src/config.h`, `src/platform/sim/sim_main.cpp` (`sw_build_plan`)

**Interfaces:**
- Consumes: `ui_weather_page` (Task 3).
- Produces: `SWIPE_FADE_MS` in `config.h`; Weather's up/down (not the knob) fades the new screen in.

Why a scrim and not the panel's own opacity: Now/Radar/7-Day switch by an instant show/hide inside `build_weather()`, and an `opa` below 255 on a panel that holds children makes LVGL blend through a layer buffer the size of the panel (466x466x2, about 434 KB). A black sheet whose own `bg_opa` animates is a plain rectangle blend and needs no buffer.

- [ ] **Step 1: Constant**

In `src/config.h`, below `SWIPE_SLIDE`:
```c
#define SWIPE_FADE_MS       180            // Weather's up/down: how long the new screen takes to come out of a dimmed sheet
```

- [ ] **Step 2: The scrim**

In `src/app/ui/ui.cpp`, near the other `static lv_obj_t *` weather handles (line 80):
```cpp
static lv_obj_t *s_wxScrim = nullptr;   // black sheet over the weather panel: a swipe's fade
```
Directly after the `wx_screens::build(wp, {...});` call (~line 1470) add:
```cpp
    // A black sheet above everything on the weather panel, hidden at rest. A swipe drops it to
    // 70% and lets it clear (weather_fade), which is the whole up/down transition. Its OWN
    // opacity is animated, not the panel's: see the plan for why a panel fade would cost a
    // 434 KB layer buffer.
    s_wxScrim = lv_obj_create(wp);
    lv_obj_remove_style_all(s_wxScrim);
    lv_obj_set_size(s_wxScrim, SCREEN_W, SCREEN_H);
    lv_obj_center(s_wxScrim);
    lv_obj_set_style_bg_color(s_wxScrim, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_wxScrim, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_wxScrim, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_flag(s_wxScrim, LV_OBJ_FLAG_HIDDEN);
```
Above `ui_weather_page`, add:
```cpp
static void scrim_opa_cb(void *obj, int32_t v) { lv_obj_set_style_bg_opa((lv_obj_t *)obj, (lv_opa_t)v, 0); }
static void scrim_done_cb(lv_anim_t *a)        { lv_obj_add_flag((lv_obj_t *)a->var, LV_OBJ_FLAG_HIDDEN); }
static void weather_fade() {
    if (!s_wxScrim) return;
    lv_anim_del(s_wxScrim, scrim_opa_cb);              // a second swipe restarts it, never stacks
    lv_obj_clear_flag(s_wxScrim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(s_wxScrim, LV_OPA_70, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_wxScrim);
    lv_anim_set_exec_cb(&a, scrim_opa_cb);
    lv_anim_set_values(&a, LV_OPA_70, LV_OPA_TRANSP);
    lv_anim_set_time(&a, SWIPE_FADE_MS);
    lv_anim_set_ready_cb(&a, scrim_done_cb);
    lv_anim_start(&a);
}
```
and in `ui_weather_page`, replace `ui_weather_step(delta);` with:
```cpp
    ui_weather_step(delta);
    weather_fade();
```
Also, in `ui_weather_reset()`, add first line `if (s_wxScrim) { lv_anim_del(s_wxScrim, scrim_opa_cb); lv_obj_add_flag(s_wxScrim, LV_OBJ_FLAG_HIDDEN); }` so leaving mid-fade and coming back never finds a dimmed sheet. (Move `scrim_opa_cb` above `ui_weather_reset` if the compiler needs it declared first.)

- [ ] **Step 3: Photograph a fade**

In `sw_build_plan`, change the `"up steps Now to Radar"` step from `sw_page(...)` to:
```cpp
    g_swPlan.push_back([prefix]() {
        input_router::onSwipe(Dir::Up);
        lv_tick_inc(60); lv_timer_handler(); lv_refr_now(NULL);       // 60 ms into the fade
        sim_save_frame((prefix + "-mid-fade.bmp").c_str());
        sw_check("up steps Now to Radar", ui_weather_screen() == WX_SCREEN_RADAR);
    });
```

- [ ] **Step 4: Build, run, look**

```bash
~/.platformio/penv/bin/pio run -e native 2>&1 | grep -E "error|SUCCESS" | head -3
OUT=$(mktemp -d); .pio/build/native/program --swipeshot "$OUT/sw" 2>&1 | grep swipeshot | tail -12
sips -s format png "$OUT/sw-mid-fade.bmp" --out "$OUT/sw-mid-fade.png" >/dev/null
```
Expected: all `ok`. Open `$OUT/sw-mid-fade.png`: the Radar screen should be visibly dimmed but legible. Then run `.pio/build/native/program --wxscreens "$OUT/wx" 2>&1 | grep wxscreens` and confirm the knob path is unchanged and undimmed (its screenshots have no scrim).

- [ ] **Step 5: Commit**

```bash
git add src/app/ui/ui.cpp src/config.h src/platform/sim/sim_main.cpp
git commit -F - <<'EOF'
Fade Weather's up/down swipes in from a dimmed sheet

The screens switch by an instant show/hide, and fading the panel's own
opacity would make LVGL blend through a panel-sized layer buffer. A black
sheet animating its own opacity is a plain blend. The knob's steps stay
instant.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
```

---

## Task 5: Simulator: the mouse becomes the finger

**Files:**
- Modify: `src/platform/sim/sim_main.cpp` (`sdl_mouse_read`, line 333; indev registration, lines 917-921; the `interactive` block, line ~1347)

**Interfaces:**
- Consumes: `swipe::Detector`, `input_router::onSwipe`, `SWIPE_*` constants.
- Produces: a drag with the left mouse button in the interactive simulator behaves like a finger on the device.

The simulator currently registers the mouse as an **LVGL pointer device**. The device has none, so a mouse drag would scroll widgets in the sim that never scroll on the Orb.

- [ ] **Step 1: Turn the pointer callback into a plain reader**

Replace `sdl_mouse_read` (lines 331-351, the comment above it too) with:
```cpp
// The mouse is the finger. Returns whether the left button is down AND the pointer is on the
// screen, with x/y mapped into the 466 px display. In composite mode the screen is the round lens,
// so window coords are mapped into the circle and a press outside it is not a touch. Not an LVGL
// pointer device: the Orb registers none, so nothing here may tap, drag or scroll a widget.
static bool sim_pointer(int *ox, int *oy) {
    int x, y;
    const Uint32 btn = SDL_GetMouseState(&x, &y);
    if (!(btn & SDL_BUTTON(SDL_BUTTON_LEFT))) return false;
    if (g_composite) {
        const float dx = x - g_scx, dy = y - g_scy;
        if (g_sr <= 0 || sqrtf(dx * dx + dy * dy) > g_sr) return false;
        *ox = (int)lroundf(233.0f + (dx / g_sr) * 233.0f);
        *oy = (int)lroundf(233.0f + (dy / g_sr) * 233.0f);
        return true;
    }
    *ox = x;
    *oy = y;
    return true;
}
```
Delete the four lines that register the indev (`static lv_indev_drv_t indev_drv;` through `lv_indev_drv_register(&indev_drv);`, lines 917-921).

- [ ] **Step 2: Feed the detector**

In the `if (interactive) {` block of the main loop, directly after `poll_updating_overlay(now);`, add:
```cpp
            {   // Touch: the mouse feeds the same detector the device does. No rotation in the sim.
                static swipe::Detector det(swipe::Limits{ SWIPE_MIN_PX, SWIPE_AXIS_RATIO, SWIPE_MAX_MS });
                int mx = 0, my = 0;
                const bool down = sim_pointer(&mx, &my);
                input_router::onSwipe(det.feed(mx, my, down, now, 0));
            }
```

- [ ] **Step 3: Build and check nothing else depended on the pointer device**

```bash
~/.platformio/penv/bin/pio run -e native 2>&1 | grep -E "error|warning: unused|SUCCESS" | head
OUT=$(mktemp -d)
for m in swipeshot rockshot knobshot wxscreens; do echo "== $m"; .pio/build/native/program --$m "$OUT/$m" 2>&1 | grep -iE "FAIL|PASS|ok|all" | head -12; done
```
Expected: it builds (`sdl_mouse_read` has no other caller; if the compiler says otherwise, follow it); every harness still passes with no `FAIL`.

- [ ] **Step 4: Manual check (needs a person with a mouse)**

State in the commit message that this step was not run by the agent. Run `~/.platformio/penv/bin/pio run -e native -t exec`, then with the left button: drag left across the lens changes app; drag up in Weather steps to Radar; a short click, a slow drag and a diagonal do nothing; a drag started on Settings does nothing. If the owner is not at hand, hand this list over in the final report as the simulator's remaining check.

- [ ] **Step 5: Commit**

```bash
git add src/platform/sim/sim_main.cpp
git commit -F - <<'EOF'
Simulator: the mouse feeds the swipe detector, not an LVGL pointer device

The device registers no pointer device, so a drag in the sim used to scroll
widgets that never scroll on the Orb. The mouse now goes through the same
Detector and input_router::onSwipe as the touch panel. The drag-by-mouse
behaviour itself was not run by the agent (needs a person with a mouse).

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
```

---

## Task 6: Device: read the touch panel

**Files:**
- Modify: `platformio.ini` (device env `build_src_filter`, line 28, and the note above it, lines 24-27), `src/main.cpp`, `src/config.h` (`FW_VERSION`)

**Interfaces:**
- Consumes: `touch_begin()`, `touch_read(uint16_t*, uint16_t*)` (`src/platform/display/touch_cst9217.h`), `swipe::Detector`, `input_router::onSwipe`, `display::rotation()`, `display::noteActivity()`, `g_idle`, `g_asleep`, `applyBrightness()`.
- Produces: swipes on the real panel.

- [ ] **Step 1: Stop excluding the driver**

In `platformio.ini`, replace the note (lines 24-27) and the `build_src_filter` line (line 28) with:
```ini
; touch_cst9217.cpp is built in: touch is back for SWIPES ONLY (src/core/swipe.*, and the "Touch"
; section of docs/ARCHITECTURE.md). No LVGL pointer device is registered, so nothing can be tapped,
; dragged or scrolled; main.cpp polls the chip and hands finished swipes to input_router::onSwipe.
build_src_filter = +<**/*.c> +<**/*.cpp> -<**/sim_main.cpp> -<**/sim_knob.cpp> -<**/native_http.cpp>
```
Leave the `native` env's own `-<platform/display/touch_cst9217.cpp>` alone: the sim reads the mouse, not this driver.

- [ ] **Step 2: Includes and state**

In `src/main.cpp`, next to `#include "rtc_pcf85063.h"` (line 48) add:
```cpp
#include "touch_cst9217.h"          // CST9217 touch (swipes only)
#include "swipe.h"
```
Directly below `applyBrightness()` (after its closing brace, line ~837) add:
```cpp
// Touch is for swipes only. The detector is fed from loop() on core 1, the core the IMU and RTC
// are read from, so the shared I2C bus keeps one user at a time.
static swipe::Detector g_swipe(swipe::Limits{ SWIPE_MIN_PX, SWIPE_AXIS_RATIO, SWIPE_MAX_MS });
```

- [ ] **Step 3: Start the driver**

In `setup()`, directly after `rtc_begin();` (line ~2582) add:
```cpp
    touch_begin();     // swipes only. Answers "not responding" in the log rather than failing, so a missing panel leaves the knob as the only input and never blocks boot
```

- [ ] **Step 4: Poll it**

In `loop()`, directly after the closing brace of the knob block (the block that ends with `input_router::dispatch((int)kd, pressed);` and its `}`, line ~3000) and before `input_router::tick();`, add:
```cpp
    // Touch: swipes only. A finger counts as activity, like the knob, and wakes a dimmed screen;
    // the touch that WAKES it is cancelled, so it does not also navigate. Not read while the Orb
    // is face-down asleep (flipping it back is what ends that).
    {
        static uint32_t touchAt = 0;
        static bool     wasDown = false;
        if (!g_asleep && (uint32_t)(millis() - touchAt) >= SWIPE_POLL_MS) {
            touchAt = millis();
            uint16_t tx = 0, ty = 0;
            const bool down   = touch_read(&tx, &ty);
            const bool dimmed = g_idle;
            const swipe::Dir d = g_swipe.feed(tx, ty, down, millis(), display::rotation());
            if (down) {
                display::noteActivity();
                if (g_idle) { g_idle = false; applyBrightness(); }
                if (!wasDown && dimmed) g_swipe.cancel();   // fed first, so there IS a gesture to cancel
            }
            wasDown = down;
            if (d != swipe::Dir::None) {
                Serial.printf("[touch] swipe %d (app %s)\n", (int)d, app_shell::name());
                diag::log("swipe %d (app %s)", (int)d, app_shell::name());
            }
            input_router::onSwipe(d);
        }
    }
```
`swipe::Dir` numbering for the log: 0 None, 1 Left, 2 Right, 3 Up, 4 Down.

- [ ] **Step 5: Bump the version**

In `src/config.h`, change `#define FW_VERSION "2.17.0"` to `"2.18.0"` (keep the trailing comment).

- [ ] **Step 6: Build the device firmware**

Run: `~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 2>&1 | grep -E "error|warning: unused|RAM:|Flash:|SUCCESS|FAILED" | head`
Expected: `SUCCESS`. Record the `RAM:` and `Flash:` lines in the commit message; the touch driver and detector should add only a few KB of flash. If the build fails on `touch_cst9217.cpp`, read the error: the file has not been compiled into the firmware since it was excluded.

- [ ] **Step 7: Both targets and the host suite**

```bash
~/.platformio/penv/bin/pio run -e native 2>&1 | grep -E "error|SUCCESS" | head -3
bash tests/run_host_tests.sh 2>&1 | tail -3
```
Expected: `SUCCESS` and `all host tests passed`.

- [ ] **Step 8: Commit**

```bash
git add platformio.ini src/main.cpp src/config.h
git commit -F - <<'EOF'
Read the touch panel for swipes (2.18.0)

The CST9217 driver is built in again and polled from loop() on core 1, with
the IMU and RTC that share its bus. It feeds the swipe detector, and no LVGL
pointer device is registered. A touch counts as activity and wakes a dimmed
screen; the touch that wakes it is cancelled so it does not also navigate.

Builds on both targets. NOT booted on a board: touch reads, I2C contention,
swipe orientation and threshold feel are unchecked (CLAUDE.md rule 1).

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
```

---

## Task 7: Say what is true, and verify everything

**Files:**
- Modify: `docs/ARCHITECTURE.md`, `src/platform/display/display.cpp` (lines ~250-256 and ~330-331), `src/app/ui/ui.cpp` (lines ~340-350 and ~1165-1167), `docs/adding-a-screen.md`, `docs/superpowers/specs/2026-09-20-touch-swipe-design.md`

A comment that says touch is off is now a lie, and a lie in a comment is the fault CLAUDE.md rule 4 describes.

- [ ] **Step 1: Find every stale claim**

Run:
```bash
grep -rniE "knob-only|knob only|touch is (deliberately )?not wired|touch is disabled|no touch indev|nothing swipes|touch input device" docs src README.md CLAUDE.md platformio.ini | grep -v "docs/superpowers"
```
Fix every hit that says touch is off. The known ones are the steps below; if the grep shows others, fix those too and list them in the commit message.

- [ ] **Step 2: `docs/ARCHITECTURE.md`**

Change the first sentence under `## Input model` to: `**The knob is the input surface, and touch adds swipes (below).** A KY-040 rotary encoder ...` (rest unchanged).

Replace the whole section `## Known gap: touch is still live` (through the paragraph ending "...this section is the accurate description of the build.") with:
```markdown
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
```
In the `## File map` table add: `| `swipe.h` / `swipe.cpp` | Touch swipe recogniser and the swipe ring (pure, host-tested) |`

- [ ] **Step 3: The comments in code**

`src/platform/display/display.cpp`: replace the comment block starting `// Touch is deliberately not wired up.` (lines ~250-256) with:
```cpp
// Touch is read for SWIPES ONLY, and not here: main.cpp polls the CST9217 (touch_cst9217.cpp) and
// feeds src/core/swipe.*, and input_router::onSwipe() decides what a swipe does. No LVGL pointer
// device is registered on purpose, so nothing can be tapped, dragged or scrolled. See "Touch: swipes
// only" in docs/ARCHITECTURE.md.
```
and replace the two-line comment `// No touch indev is registered. Knob only. See the note above touch_read_cb's` / `// former home, further up this file.` (~line 330) with `// No touch indev is registered, on purpose: see the note above.`

`src/app/ui/ui.cpp`: change the "input" block comment (~lines 340-350) so its first sentences read `// Touch is back, for swipes only, and not here: swipes go through src/core/swipe.* and input_router::onSwipe(). What used to live here (tap to select, the zoom button, long-press to cycle the skin) is still gone.` and keep the "Where each of those lives now" list. Change the comment at ~1165-1167 to: `// Nothing scrolls this tileview: no LVGL pointer device is registered, so it never sees a finger. The two tiles are switched programmatically by ui_show_view() from each app's onEnter; swipes move between APPS through app_shell.`

- [ ] **Step 4: `docs/adding-a-screen.md`**

Add a row to the "Every screen" table: `| Swipes | if the screen has several screens the knob steps between, register a pager (app_shell::setPager) so up/down works; see the Touch section of ARCHITECTURE.md |`. Add under "Screens fed by live data" nothing; one row is enough.

- [ ] **Step 5: Update the spec's open items**

In `docs/superpowers/specs/2026-09-20-touch-swipe-design.md`, replace the whole `## Open items the plan resolves first` section with:
```markdown
## Resolved while planning

1. Settings registers with `capture=true` on the device (`main.cpp`) as in the simulator.
2. Now/Radar/7-Day switch by an instant show/hide inside `build_weather()`. Fading the panel's own
   opacity would make LVGL blend through a 466x466 layer (about 434 KB), so up/down uses a black
   scrim that animates its own opacity (`SWIPE_FADE_MS`).
3. `app_shell`'s slide is `ANIM_MS` 250 ms, but nothing calls the animated path (the knob uses the
   unanimated one). `load()` runs `onExit` before the slide and a blocking `onEnter` after it starts,
   so `SWIPE_SLIDE` (1 slide, 0 cut) is the fallback. `transitioning()` did not exist and is added.
4. IMU, RTC and touch are all polled from `loop()` on core 1, and `imu_begin()` brings up the bus, so
   there is no new cross-core use. `touch_begin()` is called from `main.cpp` beside `rtc_begin()`, and
   already returns true when the chip does not answer yet.

Also decided: the router entry is `input_router::onSwipe`; it also drops swipes while the Ready
notice, the knob-help panel or the wind notice is up; the simulator's LVGL pointer device is removed
so it matches the device; Weather's pager is `ui_weather_page` in `ui.cpp`.
```

- [ ] **Step 6: Final verification (evidence before claims)**

```bash
~/.platformio/penv/bin/pio run -e native 2>&1 | grep -E "error|SUCCESS|FAILED" | head -3
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 2>&1 | grep -E "error|RAM:|Flash:|SUCCESS|FAILED" | head -5
bash tests/run_host_tests.sh 2>&1 | tail -4
python3 -m unittest tests/test_default_theme.py 2>&1 | tail -3   # if it reports 0 tests, run the file directly with python3
OUT=$(mktemp -d)
for m in swipeshot rockshot knobshot wxscreens; do echo "== $m"; .pio/build/native/program --$m "$OUT/$m" 2>&1 | grep -iE "FAIL|all ok|PASS" | head -8; done
git status --short
```
Expected: both builds `SUCCESS`, host tests end `all host tests passed`, the default-theme test passes (it needs PyYAML; if it cannot import it, say so), the harnesses show no `FAIL` and `--swipeshot` ends `all ok`, and `git status` shows only the files you edited in this task. Report each result with its actual output. If something fails, fix the cause, do not adjust the check.

- [ ] **Step 7: Commit**

```bash
git add docs/ARCHITECTURE.md docs/adding-a-screen.md docs/superpowers/specs/2026-09-20-touch-swipe-design.md src/platform/display/display.cpp src/app/ui/ui.cpp
git commit -F - <<'EOF'
Docs and comments: touch is back, for swipes only

Every place that said the Orb is knob-only or that no touch device is
registered now says what is true, including why no LVGL pointer device is
registered. The spec's open items are recorded as resolved.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
```

---

## Hardware checklist (for the owner; not verifiable in this session)

CLAUDE.md rule 1: none of this is done until an Orb has booted the build.

- [ ] Boot log shows `[touch] CST9217 responding (ACK ok)`, and no I2C "already started" warning from the second `Wire.begin`.
- [ ] Left/right swipes move between Clock, Flight Tracker, Weather and Intel; Settings is never reached by swipe.
- [ ] Up/down in Weather steps Now, Radar, 7-Day and stops at the ends; the knob still wraps.
- [ ] Physical orientation is right (a finger moving left is `Left`). Repeat with the display rotated (Settings > rotation), including a non-cardinal angle.
- [ ] Threshold feel: `SWIPE_MIN_PX`, `SWIPE_AXIS_RATIO`, `SWIPE_MAX_MS` are starting values. Retune in `config.h`.
- [ ] A touch on a dimmed screen wakes it and does not navigate.
- [ ] The sideways slide: watch a swipe out of an app with big art (the flash-baked plate path differs from the SD path the sim uses). If the outgoing screen loses its art mid-slide or the slide drops frames, set `SWIPE_SLIDE` to 0.
- [ ] I2C contention: IMU face-down sleep, RTC and audio still behave with touch polling every 20 ms. Check `/health` heap and PSRAM numbers are unchanged.
- [ ] Frame time on Weather's fade.
