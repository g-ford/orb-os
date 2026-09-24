# One Wheel for the App Picker and Settings — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the app picker's three-line renderer and Settings' separate wheel with one theme-free platform component that both use, delete the `settings:` / `menu:` theme blocks, and remove every selection pill.

**Architecture:** `src/platform/wheel/` holds the wheel's geometry (pure, host-tested), its one canvas, and its glyph and glow drawing. It takes a plain `Look` from the caller. `src/app/common/wheel_look` is the one place that turns the theme (palette roles `primary`/`muted`, font slots `wheel_sel`/`wheel_item`) into a `Look`. The picker (`src/app/shell`) and Settings (`src/app/settings`) are separate consumers. The existing `plate_sprite` loader gains an alpha flag and replaces the two duplicated art loaders.

**Tech Stack:** C++17, LVGL v8, PlatformIO (Arduino + native SDL2 simulator), Python 3 (`unittest`, `tools/build_theme.py`), bash host tests.

**Spec:** [docs/superpowers/specs/2026-09-25-unified-wheel-design.md](../specs/2026-09-25-unified-wheel-design.md)

## Global Constraints

- Wheel geometry is constants in `src/platform/wheel/wheel_layout.h`: radius **170 px**, horizontal bow **18 px**, step **22 degrees**, centre offset **0**, fade **2.0**, selected glow **8 px**. No theme option controls any of them.
- A theme controls only: palette role `primary` (selected row and its glow), palette role `muted` (other rows), font slots `wheel_sel` and `wheel_item`, and the plate/glass PNGs (`menu_plate.png`, `menu_overlay.png`, `settings_plate.png`, `settings_overlay.png`).
- `src/platform/wheel/` must not include any `theme_*`, `custom_*` or `app_*` header. All theme knowledge lives in `src/app/common/wheel_look.*`.
- The `settings:` and `menu:` theme blocks, the font slots `menu_current`, `menu_prev`, `menu_next`, `settings`, `settings_sel`, and the option `defaultSel` are **removed, not accepted and ignored**. `tools/build_theme.py` refuses them with a message naming the replacement.
- `THEME_CAPS` goes from 53 to **54**, with a ledger entry.
- No selection pill anywhere in Settings or the picker, including first-boot, network list and password pages. Every list marks its selected row the same way: the `Look`'s selected colour and font against its item colour and font. Recovery pages use `wheel_look::system()` (stock, unthemed).
- Settings opens on its first row.
- The picker becomes a wheel of app names, cyclic (it wraps like the existing browse), single-line, clipped with `...`.
- Migrate all five in-repo themes: `default`, `elegant`, `fallout`, `portal`, `vaultec`. Leave the untracked `Untitled theme.orb` and the main checkout's untracked `src/theme_assets/vaultec/` alone.
- `FW_VERSION` becomes **2.22.0** (`src/config.h`). Nothing here is verified on an Orb: add an entry to `docs/HARDWARE_PENDING.md` and never describe the work as verified on hardware.
- Stage explicit paths; never `git add -A`. End commits with `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>`.
- Tools: `pio` is `~/.platformio/penv/bin/pio`. Run the simulator with `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`; it ignores SIGTERM, stop it with `kill -9`.

## Review Focus

Failure modes the spec implies but the task tests would otherwise miss, most likely first. Each has a test in the task named after it.

1. **A very long row** (a 32-character WiFi SSID, a long theme name) must be cut to fit and end in `...`, never become `...` alone, never wrap. Task 1 (`ellipsis_keep` cases), Task 5 (simulator shot).
2. **Canvas allocation fails** (PSRAM pressure). Settings must stay readable and navigable on plain labels, and the picker must show the app name. Task 2 adds `SIM_WHEEL_NO_CANVAS=1` to force it in the simulator; Tasks 5 and 6 photograph that path.
3. **An old theme.** A theme source still containing `settings:`, `menu:` or an old font slot must fail to build with the replacement named (Task 7). A card already holding an old build (a `fonts.map` naming retired slots, a leftover `settings_style.json`) must load without error: retired slot names in the map are never looked up (Task 7 adds the resolve test).
4. **Wrap-around in the picker.** With 5 apps the selected one sits in the middle and both neighbours exist on both sides at every position, including the first and last app. Task 6 (simulator shot at first and last app, plus a pure test of the ring order).
5. **Recovery pages without a pill.** The network list and first-boot pages must still show which row is selected, and a size step must not make rows overlap. Task 5 (simulator shot, including a 32-character SSID).

## Branching and preflight

The integration branch is `worktree-unified-wheel` (this worktree). Each task below is its own branch off it, tested, then merged back with `--no-ff`:

```bash
git switch worktree-unified-wheel
git switch -c feat/wheel-<task-number>-<slug>
# ... do the task, run its verification ...
git switch worktree-unified-wheel
git merge --no-ff feat/wheel-<task-number>-<slug> -m "Merge feat/wheel-<n>-<slug>: <what>"
```

Preflight, once, before Task 1. Record the results; later tasks must not make any of them worse.

- [ ] **P1: Baseline host tests**

Run: `bash tests/run_host_tests.sh 2>&1 | tail -15`
Expected: `all host tests passed`. If anything fails here, stop and report it: it is not this plan's regression.

- [ ] **P2: Baseline Python tests**

Run (about two minutes, so in the background): `python3 -m unittest discover -s tests -p "test_*.py" > /tmp/wheel_py_baseline.log 2>&1; grep -E '^(FAIL|ERROR):|^Ran |^OK|^FAILED|skipped' /tmp/wheel_py_baseline.log`
Expected: `OK` possibly with `skipped=N`. Write N down. A dumper-backed test that fails to compile is reported as a skip, so a rising N later means something broke.

- [ ] **P3: Baseline builds and screenshots**

```bash
~/.platformio/penv/bin/pio run -e native 2>&1 | tail -3
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 2>&1 | tail -3
bash tools/themeshots.sh /tmp/wheel_before default elegant fallout portal vaultec
```
Expected: both builds `SUCCESS`; `/tmp/wheel_before/<slug>-*.bmp` exists for the five themes. These are the "before" set for `tools/shot_diff.py`. Also capture the current Settings wheel and picker for reference:

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SETTLE_MS=1500 .pio/build/native/program --settingsshot /tmp/wheel_before/settings & sleep 30; kill -9 $! 2>/dev/null
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SETTLE_MS=1500 .pio/build/native/program --rockshot /tmp/wheel_before/rock & sleep 30; kill -9 $! 2>/dev/null
ls /tmp/wheel_before | head -40
```
Expected: `settings-*.bmp` files and `rock-rocked.bmp` (the picker overlay).

- [ ] **P4: Fix one line of the spec**

The spec says the simulator has no picker capture. It does: `--rockshot` writes `<prefix>-rocked.bmp` with the picker overlay open, and `--themeshot` writes the knob menu. In `docs/superpowers/specs/2026-09-25-unified-wheel-design.md`, replace the sentence beginning "Simulator: `--settingsshot` and `--themeshot`" through "only if neither does." with:

```
- Simulator: `--settingsshot` and `--themeshot` for each of the five themes, plus `--rockshot`, whose
  `-rocked.bmp` is the picker overlay. No new capture mode is needed. Screenshots are read by eye; they are not
  a pass criterion.
```
Then: `git add docs/superpowers/specs/2026-09-25-unified-wheel-design.md && git commit -m "Spec: the sim already photographs the picker

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"`

## File Structure

| File | Action | Responsibility |
|---|---|---|
| `src/platform/wheel/wheel_layout.h` | create | Pure geometry: row position, fade, width limit, ellipsis cut. No LVGL. |
| `src/platform/wheel/wheel.h` | create | `wheel::Look`, `acquire/release/available/clear/draw/fit`. |
| `src/platform/wheel/wheel.cpp` | create (from `menu_text.cpp`) | The one canvas, glyph blit, blurred glow, dirty-rect invalidate. |
| `tests/wheel_layout_test.cpp`, `tests/run_wheel_layout_test.sh` | create | Host test of `wheel_layout.h`. |
| `src/app/common/wheel_look.h/.cpp` | create | `themed()` and `system()`: the only place a theme becomes a `Look`. |
| `src/theme/graphics/plate_sprite.h/.cpp` | modify | Gains `alpha` for glass overlays. |
| `src/theme/core/theme_font.h/.cpp` | modify | Slots `wheel_sel`, `wheel_item` replace five. |
| `src/app/settings/settings_view.cpp` | modify | Onto the wheel; pills, `chrome()`, `defaultSel` removed. |
| `src/app/shell/app_shell.cpp` | modify | Picker onto the wheel. |
| `src/app/settings/{menu_text,menu_sprite,settings_text,settings_sprite}.*` | delete | Replaced. |
| `src/theme/core/theme_style.*`, `theme_palette.*`, `custom_menu.h`, `custom_settings.h` | modify/delete | Schema removal. |
| `tools/build_theme.py`, `tools/dump_theme_defaults.cpp` | modify | Retired keys refused; dumper loses the blocks. |
| `src/theme_assets/*/theme.yaml`, `tests/golden/*` | modify | Migration. |
| `platformio.ini` | modify | Include path for `src/platform/wheel`. |
| `docs/*`, `src/config.h` | modify | Docs, `FW_VERSION`. |

---

### Task 1: The wheel's geometry, pure and tested

**Files:**
- Create: `src/platform/wheel/wheel_layout.h`
- Create: `tests/wheel_layout_test.cpp`
- Create: `tests/run_wheel_layout_test.sh`
- Modify: `tests/run_host_tests.sh` (add one `run` line)

**Interfaces:**
- Produces (`namespace wheel_layout`): constants `RADIUS, BOW, STEP_DEG, CENTRE_DY, FADE, PANEL_C, ROW_MARGIN, MIN_ROW_W` (all `float`); `struct Row { float sx, sy; unsigned char opa; float maxW; bool offDial; }`; `Row row(int index, int sel)`; `size_t ellipsis_keep(const float *adv, const char *text, size_t n, float dot, float maxW)`.
- `row()` reproduces the arithmetic of the current `wheel_layout()` in `settings_view.cpp` (lines 517-595) with the stock values. `sx`, `sy` are offsets from the panel centre `(PANEL_C, PANEL_C)`, unrounded.
- `ellipsis_keep` returns `n` when the text fits (or `maxW <= 0`); otherwise the number of leading characters to keep so that those plus three dots fit, never ending on a space and never fewer than 1 (unless `n == 0`).

- [ ] **Step 1: Branch**

```bash
git switch worktree-unified-wheel && git switch -c feat/wheel-1-layout
```

- [ ] **Step 2: Write the failing test**

Create `tests/wheel_layout_test.cpp`:

```cpp
// Host test for src/platform/wheel/wheel_layout.h.   tests/run_wheel_layout_test.sh
//
// The expected numbers were computed from the arithmetic in the pre-refactor wheel_layout() in
// settings_view.cpp with its stock values (radius 170, bow 18, step 22 degrees, fade 2.0), before this file
// existed. If a number here changes, the wheel moved, and that needs a reason.
#include "wheel_layout.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

using namespace wheel_layout;

static bool near(float a, float b, float tol = 0.05f) { return fabsf(a - b) <= tol; }

static void the_selected_row_sits_dead_centre_at_full_strength() {
    const Row r = row(3, 3);
    assert(near(r.sx, 0.0f) && near(r.sy, 0.0f));
    assert(r.opa == 255);
    assert(!r.offDial);
    assert(near(r.maxW, 414.0f));
}

static void rows_step_away_along_the_dial() {
    static const struct { int d; float sy, sx; int opa; float maxW; } W[] = {
        { 1,  63.683f,  1.311f, 188, 393.64f },
        { 2, 118.092f,  5.052f,  68, 339.61f },
        { 3, 155.303f, 10.679f,   7, 274.03f },
        { 4, 169.896f, 17.372f,   0, 232.16f },
    };
    for (const auto &w : W) {
        const Row below = row(5 + w.d, 5);
        assert(near(below.sy,  w.sy) && near(below.sx, w.sx));
        assert(below.opa == w.opa && near(below.maxW, w.maxW));
        assert(!below.offDial);
        const Row above = row(5 - w.d, 5);          // the same distance above: mirrored in y, same lean
        assert(near(above.sy, -w.sy) && near(above.sx, w.sx));
        assert(above.opa == w.opa && near(above.maxW, w.maxW));
    }
}

static void a_quarter_turn_or_more_away_is_off_the_dial() {
    assert(!row(4, 0).offDial);                     // 88 degrees
    assert(row(5, 0).offDial && row(5, 0).opa == 0);// 110 degrees
    assert(row(0, 5).offDial);
    const Row far = row(1000, 0);                   // the clamp: no NaN however far
    assert(far.offDial && far.opa == 0);
    assert(!isnan(far.sx) && !isnan(far.sy) && near(far.sy, 170.0f));
}

static void text_that_fits_is_left_alone() {
    const float adv[] = { 10, 10, 10, 10, 10 };
    assert(ellipsis_keep(adv, "ABCDE", 5, 4.0f, 50.0f) == 5);    // fits exactly
    assert(ellipsis_keep(adv, "ABCDE", 5, 4.0f, 500.0f) == 5);
    assert(ellipsis_keep(adv, "ABCDE", 5, 4.0f, 0.0f) == 5);     // no limit given
    assert(ellipsis_keep(adv, "", 0, 4.0f, 10.0f) == 0);         // nothing to cut
}

static void a_cut_leaves_room_for_three_dots() {
    float adv[10];
    for (float &a : adv) a = 10.0f;                              // 100 wide, dots are 4 each
    assert(ellipsis_keep(adv, "ABCDEFGHIJ", 10, 4.0f, 99.0f) == 8);   // 80 + 12 = 92 <= 99
    assert(ellipsis_keep(adv, "ABCDEFGHIJ", 10, 4.0f, 92.0f) == 8);   // exactly fits with the dots
    assert(ellipsis_keep(adv, "ABCDEFGHIJ", 10, 4.0f, 91.0f) == 7);   // one pixel less loses a letter
}

static void a_cut_never_ends_on_a_space() {
    const float adv[] = { 10, 10, 10, 10, 10 };                  // "AB CD"
    assert(ellipsis_keep(adv, "AB CD", 5, 4.0f, 42.0f) == 2);    // would keep "AB ", drops the space
}

static void one_glyph_is_always_kept() {
    const float adv[] = { 200, 200 };
    assert(ellipsis_keep(adv, "ab", 2, 4.0f, 50.0f) == 1);       // never "..." on its own
}

int main() {
    the_selected_row_sits_dead_centre_at_full_strength();
    rows_step_away_along_the_dial();
    a_quarter_turn_or_more_away_is_off_the_dial();
    text_that_fits_is_left_alone();
    a_cut_leaves_room_for_three_dots();
    a_cut_never_ends_on_a_space();
    one_glyph_is_always_kept();
    printf("wheel_layout: all tests passed\n");
    return 0;
}
```

Create `tests/run_wheel_layout_test.sh`:

```bash
#!/bin/bash
# Builds and runs tests/wheel_layout_test.cpp on the host. wheel_layout.h is pure, so no libraries are needed.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc/platform/wheel tests/wheel_layout_test.cpp -o "$OUT/wheel_layout_test"
"$OUT/wheel_layout_test"
```

- [ ] **Step 3: Run it and see it fail**

Run: `bash tests/run_wheel_layout_test.sh`
Expected: FAIL to compile, `fatal error: 'wheel_layout.h' file not found`.

- [ ] **Step 4: Write the header**

Create `src/platform/wheel/wheel_layout.h`:

```cpp
#pragma once
// The wheel's geometry and nothing else: no LVGL, no theme, no fonts. That is what lets
// tests/wheel_layout_test.cpp hold the shape still on the desktop, and what stops a theme from reaching
// it: these are the values Settings' wheel had stock, now the same for every wheel and no theme's to change.
#include <math.h>
#include <stddef.h>

namespace wheel_layout {

constexpr float RADIUS     = 170.0f;   // px from the centre to a row a quarter turn away
constexpr float BOW        = 18.0f;    // px a row leans sideways at a quarter turn
constexpr float STEP_DEG   = 22.0f;    // degrees of dial between neighbouring rows
constexpr float CENTRE_DY  = 0.0f;     // where the selected row sits, relative to the panel centre
constexpr float FADE       = 2.0f;     // row opacity is cos(angle) raised to 2 * FADE
constexpr float PANEL_C    = 233.0f;   // centre of the 466 px panel, and the radius of its circle
constexpr float ROW_MARGIN = 26.0f;    // room kept clear at each end of a row for the bezel
constexpr float MIN_ROW_W  = 80.0f;    // a row is never given less than this

struct Row {
    float         sx, sy;    // offset from the panel centre, unrounded
    unsigned char opa;       // 0..255 distance fade
    float         maxW;      // the widest this row may draw on the round panel
    bool          offDial;   // a quarter turn or more away: draw nothing
};

// Row `index` when row `sel` is selected.
//
// Past a quarter turn there is no dial left to put anything on, so the angle is clamped for the position
// and the row is flagged offDial. The falloff is floored at zero: cosf(pi/2) is a tiny NEGATIVE number in
// floating point, and powf(negative, fractional) is NaN, which used to become an opacity nobody chose.
inline Row row(int index, int sel) {
    const int   d        = index - sel;
    const float angleDeg = fabsf((float)d) * STEP_DEG;
    const bool  off      = angleDeg >= 90.0f;
    const float rad      = fminf(angleDeg, 90.0f) * 3.14159265f / 180.0f;
    Row r;
    r.sy  = CENTRE_DY + (d < 0 ? -1.0f : 1.0f) * RADIUS * sinf(rad);
    r.sx  = BOW * (1.0f - cosf(rad));
    const float fall = fmaxf(0.0f, cosf(rad));
    r.opa = off ? 0 : (unsigned char)lroundf(255.0f * powf(fall, 2.0f * FADE));
    // The panel is round, so a row's room depends on its height: the chord of the circle at sy, less the lean
    // and a margin at each end.
    const float chord = 2.0f * sqrtf(fmaxf(0.0f, PANEL_C * PANEL_C - r.sy * r.sy));
    r.maxW    = fmaxf(MIN_ROW_W, chord - 2.0f * fabsf(r.sx) - 2.0f * ROW_MARGIN);
    r.offDial = off;
    return r;
}

// How many leading characters of `text` to keep so that they plus "..." fit in maxW. `adv[i]` is the advance
// of text[i], `dot` the advance of '.'. Returns n when the whole text fits (or maxW <= 0). Drops letters from
// the end, never ends on a space, and keeps at least one character, so a row is never just dots.
inline size_t ellipsis_keep(const float *adv, const char *text, size_t n, float dot, float maxW) {
    float total = 0.0f;
    for (size_t i = 0; i < n; ++i) total += adv[i];
    if (maxW <= 0.0f || total <= maxW) return n;
    size_t keep = n;
    float  kept = total;
    while (keep > 1 && kept + 3.0f * dot > maxW) { --keep; kept -= adv[keep]; }
    while (keep > 1 && text[keep - 1] == ' ')    { --keep; kept -= adv[keep]; }
    return keep;
}

} // namespace wheel_layout
```

- [ ] **Step 5: Run and see it pass**

Run: `bash tests/run_wheel_layout_test.sh`
Expected: `wheel_layout: all tests passed`.

- [ ] **Step 6: Add to the host test runner**

In `tests/run_host_tests.sh`, after the line `run "clock face geometry"                bash tests/run_clock_face_test.sh` add:

```bash
run "wheel layout"                       bash tests/run_wheel_layout_test.sh
```
Run: `bash tests/run_host_tests.sh 2>&1 | tail -6`
Expected: `all host tests passed`.

- [ ] **Step 7: Commit and merge**

```bash
git add src/platform/wheel/wheel_layout.h tests/wheel_layout_test.cpp tests/run_wheel_layout_test.sh tests/run_host_tests.sh
git commit -m "Wheel geometry as a pure header, with a host test

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
git switch worktree-unified-wheel && git merge --no-ff feat/wheel-1-layout -m "Merge feat/wheel-1-layout: pure wheel geometry and its test"
```

---

### Task 2: The wheel component (canvas, drawing, glow)

**Files:**
- Create: `src/platform/wheel/wheel.h`
- Create: `src/platform/wheel/wheel.cpp` (starts as a copy of `src/app/settings/menu_text.cpp`)
- Modify: `platformio.ini` (two include-path lines)

**Interfaces:**
- Consumes: `wheel_layout::row`, `wheel_layout::ellipsis_keep`, `wheel_layout::PANEL_C` from Task 1.
- Produces (`namespace wheel`, in `wheel.h`):

```cpp
constexpr int SELECTED_GLOW = 8;
struct Look { lv_color_t selColor, itemColor, glowColor; int selGlow; const lv_font_t *selFont, *itemFont; };
void acquire(lv_obj_t *parent);   // allocate the canvas as the topmost child of parent; no-op if already held
void release();                   // free it
bool available();                 // false if it could not be allocated
void clear();                     // blank it without drawing
void draw(const char *const *rows, int count, int sel, const Look &look);
bool fit(const lv_font_t *font, const char *text, float maxW, char *out, size_t cap);   // true if cut
```

Nothing calls the component yet, so this task has no behaviour to test; it must compile into both the firmware and native builds.

- [ ] **Step 1: Branch**

```bash
git switch worktree-unified-wheel && git switch -c feat/wheel-2-component
```

- [ ] **Step 2: Add the include path**

In `platformio.ini`, in **both** environments, add the line `    -I"${PROJECT_DIR}/src/platform/wheel"` directly after the `    -I"${PROJECT_DIR}/src/platform/storage"` line (two places, near lines 85 and 148).
Verify: `grep -c 'src/platform/wheel' platformio.ini` prints `2`.

- [ ] **Step 3: Write the header**

Create `src/platform/wheel/wheel.h`:

```cpp
#pragma once
// The wheel: the one knob-driven list renderer, used by the app picker and by every list in Settings.
//
// It knows nothing about themes. Everything a theme decides arrives in a Look from the caller
// (src/app/common/wheel_look does that), so the platform layer takes on no theme dependency and the shape of
// the wheel is the same wherever it appears. Geometry is in wheel_layout.h and is not configurable.
//
// LVGL labels have no glow, so the rows are drawn onto one transparent canvas the size of the panel (~868 KB of
// PSRAM). It is allocated when a wheel screen appears and given back when it goes, because held permanently it
// competed with decoded theme art and lost.
#include <lvgl.h>
#include <stddef.h>

namespace wheel {

constexpr int SELECTED_GLOW = 8;   // px of halo behind the selected row, when a Look asks for it

struct Look {
    lv_color_t       selColor;
    lv_color_t       itemColor;
    lv_color_t       glowColor;
    int              selGlow;     // 0 = none
    const lv_font_t *selFont;
    const lv_font_t *itemFont;
};

// Create the canvas as the topmost child of `parent`. Safe to call repeatedly. On failure (PSRAM pressure)
// available() stays false and the caller must keep a plain-label fallback visible: a list you cannot read is a
// list you cannot leave.
void acquire(lv_obj_t *parent);
void release();
bool available();

// Blank the canvas without drawing a wheel, for a page that has none.
void clear();

// Draw `count` rows with row `sel` selected: layout, ellipsis, glow, and invalidate only the rectangle that
// changed (this frame's plus the previous frame's, so the old text is erased). No-op without a canvas.
void draw(const char *const *rows, int count, int sel, const Look &look);

// Cut `text` to fit `maxW` in `font`, ending in "...", into `out`. Returns true if it was cut. The same rule
// draw() applies, exposed for callers that lay out plain labels when there is no canvas.
bool fit(const lv_font_t *font, const char *text, float maxW, char *out, size_t cap);

} // namespace wheel
```

- [ ] **Step 4: Start `wheel.cpp` from `menu_text.cpp`**

```bash
cp src/app/settings/menu_text.cpp src/platform/wheel/wheel.cpp
```
(A copy, not a move: the picker still uses `menu_text` until Task 6.) Then edit `src/platform/wheel/wheel.cpp`:

(a) Replace everything above the line `namespace {` (the first 15 lines: the includes and the `#ifdef ARDUINO` block) with:

```cpp
#include "wheel.h"
#include "wheel_layout.h"
#include "config.h"          // SCREEN_W / SCREEN_H
#include <math.h>
#include <string.h>
#include <stdio.h>
#ifdef ARDUINO
#include <Arduino.h>
#include <esp_heap_caps.h>
#else
#include <stdlib.h>
#endif

```

(b) Delete the function `format_name` and the comment above it (it starts with the comment `// Substitute every "{name}" in fmt with name` and ends at the closing `}` just before the comment `// --- Straight-line glyph blit + glow`). Nothing in the wheel uses it.

(c) Keep everything else in the anonymous namespace unchanged: the dirty-rectangle variables and `mark_px`, `glyph_alpha4`, `blit_glyph`, `GLOW_MAX_ALPHA`, `Stencil`, `stencil_alloc/free/glyph/blur/composite`, and `draw_straight`.

(d) Delete everything from the line `} // namespace` that closes the anonymous namespace (just before `namespace menu_text {`) to the end of the file, and replace it with:

```cpp
} // namespace

namespace wheel {

void acquire(lv_obj_t *parent) {
    if (s_canvas || !parent) return;
#ifndef ARDUINO
    // Desktop only. SIM_WHEEL_NO_CANVAS=1 behaves as a failed PSRAM allocation, so the plain-label fallback of
    // every wheel screen can be photographed.
    if (getenv("SIM_WHEEL_NO_CANVAS")) return;
#endif
    const size_t sz = LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(SCREEN_W, SCREEN_H);
#if defined(ESP_PLATFORM)
    s_buf = (lv_color_t *)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
#else
    s_buf = (lv_color_t *)malloc(sz);
#endif
    if (!s_buf) {
#ifdef ARDUINO
        Serial.printf("[wheel] canvas alloc FAILED (%u bytes) - callers fall back to plain labels\n", (unsigned)sz);
#else
        printf("[wheel] canvas alloc FAILED (%u bytes) - callers fall back to plain labels\n", (unsigned)sz);
#endif
        return;
    }
    memset(s_buf, 0, sz);   // all-zero bytes are transparent black in this format
    s_canvas = lv_canvas_create(parent);
    lv_obj_clear_flag(s_canvas, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_canvas_set_buffer(s_canvas, s_buf, SCREEN_W, SCREEN_H, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_obj_center(s_canvas);
    s_dx0 = s_dy0 = s_pdx0 = s_pdy0 = 0;
    s_dx1 = s_dy1 = s_pdx1 = s_pdy1 = -1;
}

void release() {
    if (s_canvas) { lv_obj_del(s_canvas); s_canvas = nullptr; }
    if (s_buf) {
#if defined(ESP_PLATFORM)
        heap_caps_free(s_buf);
#else
        free(s_buf);
#endif
        s_buf = nullptr;
    }
}

bool available() { return s_canvas != nullptr; }

void clear() {
    if (!s_canvas) return;
    memset(s_buf, 0, LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(SCREEN_W, SCREEN_H));
    s_dx0 = s_dy0 = s_pdx0 = s_pdy0 = 0;
    s_dx1 = s_dy1 = s_pdx1 = s_pdy1 = -1;
    lv_obj_invalidate(s_canvas);
}

bool fit(const lv_font_t *font, const char *text, float maxW, char *out, size_t cap) {
    if (!out || cap == 0) return false;
    out[0] = '\0';
    if (!font || !text) return false;
    constexpr size_t MAXC = 80;
    const size_t full = strlen(text);
    const size_t n = full < MAXC ? full : MAXC;
    float adv[MAXC];
    for (size_t i = 0; i < n; ++i) {
        lv_font_glyph_dsc_t g;
        adv[i] = lv_font_get_glyph_dsc(font, &g, (uint32_t)(uint8_t)text[i], 0) ? (float)g.adv_w : 0.0f;
    }
    lv_font_glyph_dsc_t gd;
    const float dot = lv_font_get_glyph_dsc(font, &gd, (uint32_t)'.', 0) ? (float)gd.adv_w : 4.0f;
    const size_t keep = wheel_layout::ellipsis_keep(adv, text, n, dot, maxW);
    const bool cut = keep < full;
    size_t w = keep < cap - 1 ? keep : cap - 1;
    memcpy(out, text, w);
    if (cut) for (int k = 0; k < 3 && w + 1 < cap; ++k) out[w++] = '.';
    out[w] = '\0';
    return cut;
}

void draw(const char *const *rows, int count, int sel, const Look &look) {
    if (!s_canvas || !rows) return;
    // A straight wipe at memory speed: the buffer is 3 bytes per pixel and transparent black is all zeros.
    // lv_canvas_fill_bg did the same through LVGL's per-pixel API and measured 143 ms.
    memset(s_buf, 0, LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(SCREEN_W, SCREEN_H));
    // Where the text now being erased was, so it is repainted too.
    s_pdx0 = s_dx0; s_pdy0 = s_dy0; s_pdx1 = s_dx1; s_pdy1 = s_dy1;
    s_dx0 = 0; s_dy0 = 0; s_dx1 = -1; s_dy1 = -1;   // empty; the draws below fill it

    for (int i = 0; i < count; ++i) {
        const wheel_layout::Row r = wheel_layout::row(i, sel);
        if (r.offDial || r.opa == 0 || !rows[i] || !rows[i][0]) continue;
        const bool isSel = (i == sel);
        const lv_font_t *font = isSel ? look.selFont : look.itemFont;
        char text[88];
        fit(font, rows[i], r.maxW, text, sizeof(text));
        draw_straight(font, text, wheel_layout::PANEL_C + r.sx, wheel_layout::PANEL_C + r.sy,
                      isSel ? look.selColor : look.itemColor,
                      isSel ? look.selGlow : 0, look.glowColor, 1 /* centred */, (lv_opa_t)r.opa);
    }

    // Only what changed: what was just drawn plus what was just erased, padded by one pixel because the canvas
    // is composited with alpha and LVGL's rounder can widen a flush area.
    int x0 = s_dx0, y0 = s_dy0, x1 = s_dx1, y1 = s_dy1;
    if (s_pdx1 >= s_pdx0) {
        if (x1 < x0) { x0 = s_pdx0; y0 = s_pdy0; x1 = s_pdx1; y1 = s_pdy1; }
        else {
            if (s_pdx0 < x0) x0 = s_pdx0;
            if (s_pdy0 < y0) y0 = s_pdy0;
            if (s_pdx1 > x1) x1 = s_pdx1;
            if (s_pdy1 > y1) y1 = s_pdy1;
        }
    }
    if (x1 < x0) {
        lv_obj_invalidate(s_canvas);              // nothing tracked: fall back to all of it
    } else {
        lv_area_t a;
        a.x1 = (lv_coord_t)(x0 > 0 ? x0 - 1 : 0);
        a.y1 = (lv_coord_t)(y0 > 0 ? y0 - 1 : 0);
        a.x2 = (lv_coord_t)(x1 < SCREEN_W - 1 ? x1 + 1 : SCREEN_W - 1);
        a.y2 = (lv_coord_t)(y1 < SCREEN_H - 1 ? y1 + 1 : SCREEN_H - 1);
        lv_obj_invalidate_area(s_canvas, &a);
    }
}

} // namespace wheel
```

- [ ] **Step 5: Build both targets**

```bash
~/.platformio/penv/bin/pio run -e native 2>&1 | tail -5
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 2>&1 | tail -5
```
Expected: both `SUCCESS`, no warnings from `wheel.cpp` or `wheel.h`. If `draw_straight` reports an unused-parameter or signature error, its parameter order is `(font, str, bx, by, col, glow, glowCol, align, opa)`; the call above matches it.
Also confirm the layering rule: `grep -n '#include "theme\|#include "custom_\|#include "app' src/platform/wheel/*` prints nothing.

- [ ] **Step 6: Commit and merge**

```bash
git add platformio.ini src/platform/wheel/wheel.h src/platform/wheel/wheel.cpp
git commit -m "Wheel component: one canvas, blurred glow, dirty-rect invalidate, theme-free Look

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
git switch worktree-unified-wheel && git merge --no-ff feat/wheel-2-component -m "Merge feat/wheel-2-component: the platform wheel, not yet used"
```

---

### Task 3: Font slots `wheel_sel` and `wheel_item`, and the theme migration of `fonts.slots`

**Files:**
- Modify: `src/theme/core/theme_font.h`, `src/theme/core/theme_font.cpp`
- Modify: `src/theme_assets/{elegant,fallout,portal,vaultec}/theme.yaml` (the `fonts.slots` block only; `default` has no fonts)
- Modify: `tests/golden/fonts_elegant.txt`, `tests/golden/fonts_fallout.txt`
- Modify: `tests/test_theme_font_coverage.py`, `tests/test_no_compiled_art.py`, `tests/theme_font_resolve_test.cpp`

**Interfaces:**
- Produces: `const lv_font_t *theme_font::wheel_sel()` and `wheel_item()`. Compiled fallbacks: `&lv_font_montserrat_44` and `&lv_font_montserrat_20`.
- Slot names in a theme's `fonts.slots` become `wheel_sel` and `wheel_item`. `menu_current`, `menu_prev`, `menu_next`, `settings`, `settings_sel` are no longer slots.
- Transitional: `menu_current() menu_prev() menu_next() settings_item() settings_sel()` remain as thin wrappers over the two new accessors so the old consumers still build. They are deleted in Tasks 5 and 6 with their last callers.

- [ ] **Step 1: Branch**

```bash
git switch worktree-unified-wheel && git switch -c feat/wheel-3-fonts
```

- [ ] **Step 2: Update the failing tests first**

In `tests/test_theme_font_coverage.py`, change the constant and its comment:

```python
# The only three text slots whose compiled fallback used to be a bitmap face baked into the firmware
# (the wheel's selected row, radar 1, radar 2). Everything else falls back to LVGL's own Montserrat, which
# stays. A theme with no face for one of these three would change lettering the day the compiled fonts are deleted.
COMPILED_BITMAP_SLOTS = ('wheel_sel', 'radar1', 'radar2')
```
and in `test_the_check_can_fail` replace `'menu_current'` with `'wheel_sel'`.

In `tests/test_no_compiled_art.py`, replace the body of `test_the_three_slots_that_had_a_bitmap_face_fall_back_to_montserrat` with:

```python
    def test_the_three_slots_that_had_a_bitmap_face_fall_back_to_montserrat(self):
        fonts = strip_comments((SRC / 'theme' / 'core' / 'theme_font.cpp').read_text(encoding='utf-8'))
        radar = strip_comments((SRC / 'theme' / 'custom' / 'custom_radar.h').read_text(encoding='utf-8'))
        self.assertIn('case S_WHEEL_SEL:  return &lv_font_montserrat_44;', fonts)
        self.assertIn('#define CUSTOM_RTEXT1_FONT (&lv_font_montserrat_26)', radar)
        self.assertIn('#define CUSTOM_RTEXT2_FONT (&lv_font_montserrat_20)', radar)
```

In `tests/theme_font_resolve_test.cpp`, change the `SLOTS` array's names `font_menu_current.bin` to `font_wheel_sel.bin` and `font_settings.bin` to `font_wheel_item.bin`, and in `slots_mapped_to_one_face_share_one_group` change the two `strcmp(r.file[0], "font_menu_current.bin")` / `strcmp(r.file[4], "font_settings.bin")` expectations and the comment to `font_wheel_sel.bin` / `font_wheel_item.bin`. In `fallout_shaped_22_slots_become_10_faces` change `{"menu_current", 46}, {"settings", 27}` to `{"wheel_sel", 46}, {"wheel_item", 27}`.

Run: `python3 -m unittest tests.test_no_compiled_art -v 2>&1 | tail -8`
Expected: the renamed test FAILS (`S_WHEEL_SEL` not in `theme_font.cpp`).

- [ ] **Step 3: Change the slots in `theme_font.cpp`**

(a) In the `enum Slot` line, replace `S_MENU_CUR, S_MENU_PREV, S_MENU_NEXT, S_SETTINGS, S_SETTINGS_SEL,` with `S_WHEEL_SEL, S_WHEEL_ITEM,`.

(b) In `SLOT_FILE`, replace the two lines
```cpp
    "font_menu_current.bin", "font_menu_prev.bin", "font_menu_next.bin",
    "font_settings.bin", "font_settings_sel.bin",
```
with
```cpp
    "font_wheel_sel.bin", "font_wheel_item.bin",
```

(c) In `compiled()`, replace the five `case S_MENU_CUR ... case S_SETTINGS_SEL` arms (and the comment above `S_SETTINGS_SEL`) with:
```cpp
        case S_WHEEL_SEL:  return &lv_font_montserrat_44;
        case S_WHEEL_ITEM: return &lv_font_montserrat_20;
```

(d) Remove the two includes `#include "custom_menu.h"` and `#include "custom_settings.h"` at the top.

(e) Replace the five accessor definitions `menu_current() ... settings_item()` and the whole `settings_sel()` definition with its comment block (the one beginning `// Falls back to the LIST's face`) by:
```cpp
const lv_font_t *wheel_sel()  { return get(S_WHEEL_SEL); }
const lv_font_t *wheel_item() { return get(S_WHEEL_ITEM); }

// TRANSITIONAL. The old names, so consumers not yet moved to the wheel still build. Each is deleted together
// with its last caller (Settings in one task, the picker in the next).
const lv_font_t *menu_current()  { return wheel_sel(); }
const lv_font_t *menu_prev()     { return wheel_item(); }
const lv_font_t *menu_next()     { return wheel_item(); }
const lv_font_t *settings_item() { return wheel_item(); }
const lv_font_t *settings_sel()  { return wheel_sel(); }
```

In `theme_font.h` add, above the old `menu_current` declaration:
```cpp
// The wheel's two faces, shared by the app picker and every list in Settings. The selected row draws in
// wheel_sel(), every other row in wheel_item().
const lv_font_t *wheel_sel();
const lv_font_t *wheel_item();
```
(keep the old declarations for now).

- [ ] **Step 4: Find any other user of the removed enum names**

Run: `grep -rn "S_MENU_\|S_SETTINGS\|slot_loaded" src tools | grep -v "^src/theme/core/theme_font.cpp"`
Expected: nothing, or only `slot_loaded` callers that pass indices they compute themselves. If a caller uses a removed enum value, replace it with `S_WHEEL_SEL` / `S_WHEEL_ITEM` as appropriate.

- [ ] **Step 5: Migrate `fonts.slots` in the four themed `theme.yaml` files**

Run this once from the repo root. It renames `menu_current` to `wheel_sel` and `settings` to `wheel_item`, and deletes `menu_prev`, `menu_next` and `settings_sel`, only on the four-space-indented lines of the `slots:` block whose value is a face name, and it asserts the expected change count per theme:

```bash
python3 - <<'EOF'
import re
from pathlib import Path
EXPECT = {'elegant': (2, 2), 'fallout': (2, 0), 'portal': (2, 2), 'vaultec': (2, 3)}   # (renamed, deleted)
for slug, (want_ren, want_del) in EXPECT.items():
    p = Path(f'src/theme_assets/{slug}/theme.yaml')
    out, ren, dele = [], 0, 0
    for line in p.read_text(encoding='utf-8').splitlines(keepends=True):
        m = re.match(r'^    (menu_current|settings|menu_prev|menu_next|settings_sel):\s+([a-z][a-z0-9_]*)\s*$', line)
        if m:
            key, face = m.groups()
            if key in ('menu_prev', 'menu_next', 'settings_sel'):
                dele += 1
                continue
            out.append(f'    {"wheel_sel" if key == "menu_current" else "wheel_item"}: {face}\n')
            ren += 1
            continue
        out.append(line)
    assert (ren, dele) == (want_ren, want_del), (slug, ren, dele)
    p.write_text(''.join(out), encoding='utf-8')
    print(slug, 'renamed', ren, 'deleted', dele)
EOF
git diff --stat src/theme_assets
```
Expected: four files changed, `renamed 2` for each, deletions 2/0/2/3 for elegant/fallout/portal/vaultec. (Elegant and portal delete `menu_prev` and `menu_next`; vaultec also deletes `settings_sel`; fallout has neither.)

Then bake each to see whether a face lost its last user:
```bash
for s in elegant fallout portal vaultec; do python3 tools/build_theme.py src/theme_assets/$s --out /tmp/wheel_built 2>&1 | grep -i "no slot uses" ; done
```
For every face reported (`fonts.faces.<name>: no slot uses it`), delete that face's line from the theme's `fonts.faces:` block. Re-run until the loop prints nothing. (If `lv_font_conv` is unavailable the bake fails with a message naming it; in that case skip the bake and remove faces by hand: a face is unused when its name appears nowhere in the file outside its own `faces:` line.)

- [ ] **Step 6: Update the font goldens**

The wheel slots carry the same faces the old slots did, so their hashes equal the old lines' hashes. Rewrite the two golden files deterministically:

```bash
python3 - <<'EOF'
from pathlib import Path
for slug in ('elegant', 'fallout'):
    p = Path(f'tests/golden/fonts_{slug}.txt')
    rows = dict(l.split(' ', 1) for l in p.read_text(encoding='utf-8').splitlines() if l.strip())
    out = {}
    for k, v in rows.items():
        if k == 'menu_current': out['wheel_sel'] = v
        elif k == 'settings': out['wheel_item'] = v
        elif k in ('menu_prev', 'menu_next', 'settings_sel'): continue
        else: out[k] = v
    p.write_text(''.join(f'{k} {v}\n' for k, v in sorted(out.items())), encoding='utf-8')
    print(slug, len(rows), '->', len(out))
EOF
git diff --stat tests/golden
```
Expected: `elegant` and `fallout` each lose the retired rows and gain the two wheel rows.

- [ ] **Step 7: Run the tests**

```bash
python3 -m unittest tests.test_no_compiled_art tests.test_theme_font_coverage tests.test_theme_fonts_golden 2>&1 | tail -8
bash tests/run_theme_font_resolve_test.sh
~/.platformio/penv/bin/pio run -e native 2>&1 | tail -3
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 2>&1 | tail -3
```
Expected: Python `OK` (a skip is only acceptable for `lv_font_conv` being absent, and must not be new compared with preflight P2), the resolve test prints its pass line, both builds `SUCCESS`.

- [ ] **Step 8: Commit and merge**

```bash
git add src/theme/core/theme_font.h src/theme/core/theme_font.cpp src/theme_assets/elegant/theme.yaml src/theme_assets/fallout/theme.yaml src/theme_assets/portal/theme.yaml src/theme_assets/vaultec/theme.yaml tests/golden/fonts_elegant.txt tests/golden/fonts_fallout.txt tests/test_theme_font_coverage.py tests/test_no_compiled_art.py tests/theme_font_resolve_test.cpp
git commit -m "Font slots wheel_sel and wheel_item replace the five menu and settings slots

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
git switch worktree-unified-wheel && git merge --no-ff feat/wheel-3-fonts -m "Merge feat/wheel-3-fonts: two wheel font slots, themes migrated"
```

---

### Task 4: `wheel_look` and alpha in `plate_sprite`

**Files:**
- Create: `src/app/common/wheel_look.h`, `src/app/common/wheel_look.cpp`
- Modify: `src/theme/graphics/plate_sprite.h`, `src/theme/graphics/plate_sprite.cpp`

**Interfaces:**
- Consumes: `wheel::Look` (Task 2), `theme_font::wheel_sel/wheel_item` (Task 3), `theme_style::palette()` returning `theme_roles::Palette` with `v[theme_roles::R_primary]`, `v[theme_roles::R_muted]`.
- Produces: `wheel::Look wheel_look::themed()`, `wheel::Look wheel_look::system()`; `plate_sprite::Plate` gains a trailing `bool alpha = false`.

There is no host test here (both files need LVGL and the SD stack). The check is that both targets build and, from Task 5 on, the simulator draws with them.

- [ ] **Step 1: Branch**

```bash
git switch worktree-unified-wheel && git switch -c feat/wheel-4-look
```

- [ ] **Step 2: Add alpha to `plate_sprite`**

In `src/theme/graphics/plate_sprite.h`, add a last member to `struct Plate` and update the comment above `get`:

```cpp
struct Plate {
    const char   *asset;      // e.g. "weather_plate.png"
    const char   *tag;        // what its log lines say
    uint8_t      *buf = nullptr;
    lv_img_dsc_t  dsc {};
    bool          tried = false;
    bool          alpha = false;   // a glass or CRT overlay: RGB565 plus an alpha byte, drawn over the screen
};
```

In `src/theme/graphics/plate_sprite.cpp`:

(a) Replace `decode` and `load_asset` signatures and bodies:

```cpp
bool decode(const uint8_t *png, uint32_t len, bool alpha, uint8_t *&out, int &w, int &h, const char *tag) {
    out = png_decode::to_buffer(png, len, alpha ? png_decode::FMT_RGB565_ALPHA : png_decode::FMT_RGB565,
                                w, h, tag, "png");
    return out != nullptr;
}
```
and
```cpp
bool load_asset(const char *assetName, bool alpha, uint8_t *&out, int &w, int &h, const char *tag) {
    const theme_art::Format want = alpha ? theme_art::FMT_RGB565_ALPHA : theme_art::FMT_RGB565;
    const uint8_t *p = nullptr;
    theme_art::Format fmt = want;
    if (theme_art::lookup(theme_select::activeSlug(), assetName, p, w, h, fmt)) {
        if (fmt == want) {
            out = (uint8_t *)p;
            Serial.printf("[%s] flash-resident %dx%d (0 ms, 0 KB PSRAM)\n", tag, w, h);
            return true;
        }
    }
```
(keep the remainder of `load_asset` as it is, except that the call becomes `decode(sdBuf, (uint32_t)sdLen, alpha, out, w, h, tag)`).

(b) In `plate_sprite::get`, replace the descriptor block:

```cpp
        if (load_asset(p.asset, p.alpha, p.buf, w, h, p.tag)) {
            p.dsc.header.always_zero = 0;
            p.dsc.header.w  = w;
            p.dsc.header.h  = h;
            p.dsc.header.cf = p.alpha ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR;
            p.dsc.data_size = (uint32_t)w * h * (p.alpha ? 3 : 2);
            p.dsc.data      = p.buf;
        }
```

- [ ] **Step 3: Write `wheel_look`**

Create `src/app/common/wheel_look.h`:

```cpp
#pragma once
// The one place a theme becomes a wheel::Look. The app picker and Settings both call this, so the two cannot be
// dressed differently. It lives here, not in the wheel, because it is the part that knows about themes.
#include "wheel.h"

namespace wheel_look {

// What a theme decides: palette `primary` for the selected row and its glow, `muted` for every other row, and
// the theme's two wheel faces (compiled Montserrat when it ships none).
wheel::Look themed();

// Stock and unthemed, for the recovery pages (first boot, network list, password): a theme must not be able to
// make the screen somebody fixes their WiFi on illegible.
wheel::Look system();

} // namespace wheel_look
```

Create `src/app/common/wheel_look.cpp`:

```cpp
#include "wheel_look.h"
#include "theme_style.h"
#include "theme_font.h"
#include "theme_roles.h"

namespace wheel_look {

wheel::Look themed() {
    const theme_roles::Palette &p = theme_style::palette();
    const lv_color_t primary = lv_color_hex(p.v[theme_roles::R_primary]);
    return { primary, lv_color_hex(p.v[theme_roles::R_muted]), primary, wheel::SELECTED_GLOW,
             theme_font::wheel_sel(), theme_font::wheel_item() };
}

wheel::Look system() {
    // White against the grey the setup pages have always used, no glow. The selected row steps up a size so the
    // selection reads without a background behind it; tuned by eye on the network list, where rows are closest.
    return { lv_color_hex(0xFFFFFF), lv_color_hex(0x6A7078), lv_color_hex(0xFFFFFF), 0,
             &lv_font_montserrat_28, &lv_font_montserrat_24 };
}

} // namespace wheel_look
```

- [ ] **Step 4: Build both targets**

```bash
~/.platformio/penv/bin/pio run -e native 2>&1 | tail -4
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 2>&1 | tail -4
```
Expected: both `SUCCESS`. If `theme_style::palette()` is not found, its declaration is at `src/theme/core/theme_style.h` line ~1443 (`const theme_roles::Palette &palette();`), in `namespace theme_style`.

- [ ] **Step 5: Commit and merge**

```bash
git add src/app/common/wheel_look.h src/app/common/wheel_look.cpp src/theme/graphics/plate_sprite.h src/theme/graphics/plate_sprite.cpp
git commit -m "wheel_look turns a theme into a Look; plate_sprite loads alpha overlays

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
git switch worktree-unified-wheel && git merge --no-ff feat/wheel-4-look -m "Merge feat/wheel-4-look: wheel_look and alpha plates"
```

---

### Task 5: Settings on the wheel; every pill gone

**Files:**
- Modify: `src/app/settings/settings_view.cpp`
- Modify: `src/theme/core/theme_font.h`, `theme_font.cpp` (delete `settings_item`, `settings_sel`)

**Interfaces:**
- Consumes: `wheel::acquire/release/available/clear/draw/fit`, `wheel::Look` (Task 2); `wheel_look::themed()/system()` (Task 4); `wheel_layout::row` (Task 1); `plate_sprite::Plate/get/release` (Task 4).
- Produces: a Settings screen with no dependency on `theme_style::settings()`, `settings_text`, `settings_sprite`, `custom_settings.h`, `Chrome`, `style_highlight`, `C_HL`, `DEFAULT_SEL`.

All edits are in `src/app/settings/settings_view.cpp` unless stated. Work top to bottom.

- [ ] **Step 1: Branch, and capture the pre-change navigation self-test**

```bash
git switch worktree-unified-wheel && git switch -c feat/wheel-5-settings
~/.platformio/penv/bin/pio run -e native 2>&1 | tail -2
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SELFTEST=1 SIM_SETTLE_MS=500 .pio/build/native/program > /tmp/wheel_selftest_before.log 2>&1 & sleep 60; kill -9 $! 2>/dev/null
grep -E "selftest.*(PASS|FAIL)" /tmp/wheel_selftest_before.log
```
Expected: a list of `PASS` lines including `Settings>Range: PASS` and `Settings>Theme: PASS`. Keep this list; the same lines must pass after this task.

- [ ] **Step 2: Includes**

Replace these three include lines near the top:
```cpp
#include "custom_settings.h"    // ...
#include "settings_sprite.h"    // ...
#include "settings_text.h"      // ...
```
with:
```cpp
#include "wheel.h"              // the one wheel canvas and its drawing
#include "wheel_layout.h"       // row geometry, for the plain-label fallback
#include "wheel_look.h"         // what a theme (or the setup path) makes of it
#include "plate_sprite.h"       // the settings plate and glass
```
and delete the line that includes `theme_style.h` for `theme_style::settings()` only if nothing else in the file uses `theme_style::` (check with `grep -n "theme_style::" src/app/settings/settings_view.cpp`; `theme_style::names()` is still used, so keep the include).

- [ ] **Step 3: Delete `DEFAULT_SEL`; Settings opens on the first row**

Delete the comment and the `#define DEFAULT_SEL ...` (around lines 76-87). Change `int  s_sel   = DEFAULT_SEL;` to `int  s_sel   = 0;`, and in `settingsview::onEnter()` change `s_sel = DEFAULT_SEL;` to `s_sel = 0;`.

- [ ] **Step 4: Delete `Chrome` and its macros; add `look()`**

Delete: the `struct Chrome { ... };` (with its comment), `bool s_systemChromeFwd();`, the `chrome()` function with its `SYSTEM` table, and the five `#define WHEEL_R / WHEEL_RX / WHEEL_STEP_DEG / WHEEL_CY / WHEEL_FADE` lines. Delete `bool s_systemChromeFwd() { return s_systemChrome; }`. Keep `bool s_systemChrome = false;` and `mode_is_system_chrome()` and `mode_paints_its_own_screen()` unchanged: they still decide which page is a setup page.

Directly after `mode_paints_its_own_screen()` add:

```cpp
    // What this page is drawn with. Setup pages take the stock look and every other page the theme's, so one
    // function decides for the wheel lists, the two-row first-boot page, the network list and the password strip
    // alike. A page that marks its selected row calls this and nothing else.
    wheel::Look look() { return s_systemChrome ? wheel_look::system() : wheel_look::themed(); }
```

- [ ] **Step 5: Delete the pill machinery**

Delete `lv_color_t C_HL = ...;` and the whole `style_highlight()` function with its comment. Then run this once; it deletes the nine declarations and nine two-line creations and the network list's six-line creation, and asserts the counts:

```bash
python3 - <<'EOF'
import re
from pathlib import Path
p = Path('src/app/settings/settings_view.cpp')
s = p.read_text(encoding='utf-8')
names = 's_hl s_lmHl s_dspHl s_sndHl s_unitsHl s_rangeHl s_chimeSelHl s_designHl s_fbHl s_wifiHl'.split()
decl = 0
for n in names:
    s, k = re.subn(r'^[ \t]*lv_obj_t \*%s\b[^\n]*\n' % n, '', s, flags=re.M); decl += k
create = 0
for n in names[:-1]:
    s, k = re.subn(r'^[ \t]*%s = lv_obj_create\([^\n]*\n[ \t]*style_highlight\(%s\);\n' % (n, n), '', s, flags=re.M); create += k
s, wifi = re.subn(r'^[ \t]*s_wifiHl = lv_obj_create\(s_wifiListPage\);\n(?:[ \t]*lv_obj_[^\n]*s_wifiHl[^\n]*\n){4}', '', s, flags=re.M)
assert (decl, create, wifi) == (10, 9, 1), (decl, create, wifi)
p.write_text(s, encoding='utf-8')
print('declarations', decl, 'creations', create, 'wifi', wifi)
EOF
grep -n "Hl\b\|s_hl\|style_highlight\|C_HL" src/app/settings/settings_view.cpp
```
Expected: `declarations 10 creations 9 wifi 1`, and the `grep` still lists a few lines: the uses in `wheel_layout(...)` call sites, `refresh_firstboot()` (the `s_fbHl` size/align block), `refresh_wifi_list()` (`s_wifiHl` hide/show/align), and the comment mentions. Remove them in the next steps.

- [ ] **Step 6: Replace `fit_label`'s cut loop and rewrite `wheel_layout()`**

In `fit_label`, replace everything from `lv_point_t sz;` to the end of the function with:

```cpp
        char buf[64];
        wheel::fit(font, full, maxW, buf, sizeof(buf));   // the same cut the canvas applies; full text if it fits
        if (strcmp(shown, buf) != 0) lv_label_set_text(lbl, buf);
    }
```
(the `if (!full ...) { free(full); full = strdup(shown); ... }` block above it stays.)

Replace the whole `void wheel_layout(lv_obj_t **items, int count, int sel, lv_obj_t *hl) { ... }` function (from its opening line to its closing brace just before `void refresh_menu()`) with:

```cpp
    // The most rows any wheel list has. Every label array feeding wheel_layout() must fit, and the
    // static_assert is what fails the build when a list outgrows it.
    constexpr int MAX_WHEEL_ROWS = 32;
    static_assert(ITEM_COUNT <= MAX_WHEEL_ROWS && theme_select::MAX_THEMES + 1 <= MAX_WHEEL_ROWS
                  && CHIME_UI_MAX + 1 <= MAX_WHEEL_ROWS, "raise MAX_WHEEL_ROWS: a wheel list would be cut short");

    // Shared by every fixed-item list in Settings, so they all roll the same way. The selected row sits at the
    // panel centre and the rest fall away along the dial (wheel_layout.h). The rows are drawn on the wheel's
    // canvas; the labels only hold the text. When the canvas could not be allocated the labels are laid out and
    // shown instead, in the same colours and faces, because Settings must stay readable and navigable.
    void wheel_layout(lv_obj_t **items, int count, int sel) {
        const wheel::Look lk = look();
        if (count > MAX_WHEEL_ROWS) count = MAX_WHEEL_ROWS;
        if (wheel::available()) {
            const char *texts[MAX_WHEEL_ROWS];
            for (int i = 0; i < count; ++i) {
                texts[i] = lv_label_get_text(items[i]);
                lv_obj_set_style_text_opa(items[i], LV_OPA_TRANSP, 0);   // the canvas draws the real glyphs
            }
            wheel::draw(texts, count, sel, lk);
            return;
        }
        for (int i = 0; i < count; ++i) {
            const wheel_layout::Row r = wheel_layout::row(i, sel);
            const bool isSel = (i == sel);
            const lv_font_t *f = isSel ? lk.selFont : lk.itemFont;
            lv_obj_set_style_text_font(items[i], f, 0);
            lv_obj_set_size(items[i], (lv_coord_t)lroundf(r.maxW), lv_font_get_line_height(f));
            lv_obj_set_style_text_align(items[i], LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_long_mode(items[i], LV_LABEL_LONG_CLIP);
            fit_label(items[i], f, r.maxW);
            lv_obj_align(items[i], LV_ALIGN_CENTER, (lv_coord_t)lroundf(r.sx), (lv_coord_t)lroundf(r.sy));
            lv_obj_set_style_text_color(items[i], isSel ? lk.selColor : lk.itemColor, 0);
            lv_obj_set_style_text_opa(items[i], r.offDial ? LV_OPA_TRANSP : (lv_opa_t)r.opa, 0);
        }
    }
```

Then drop the fourth argument at every call site (eight of them plus the design re-layout):

```bash
sed -i -E 's/wheel_layout\(([^;]*), (s_hl|s_dspHl|s_sndHl|s_chimeSelHl|s_designHl|s_unitsHl|s_rangeHl|s_lmHl)\);/wheel_layout(\1);/' src/app/settings/settings_view.cpp
grep -n "wheel_layout(" src/app/settings/settings_view.cpp
```
Expected: every `wheel_layout(` call now has three arguments.

- [ ] **Step 7: One selection rule on the label pages**

`C_WHITE` / `C_GREY` are also used for plain static text (hints, titles), which stays. Only the four places that mark a *selected* row change.

(a) In `refresh_firstboot()`: replace the loop body's font/colour lines and delete the pill block below it.

```cpp
        const wheel::Look lk = look();
        for (int i = 0; i < FB_COUNT; ++i) {
            const bool sel = (i == s_fbSel);
            lv_obj_align(s_fbItems[i], LV_ALIGN_CENTER, 0, i == 0 ? FB_ROW1_Y : FB_ROW2_Y);
            lv_obj_set_style_text_font(s_fbItems[i], sel ? lk.selFont : lk.itemFont, 0);
            lv_obj_set_style_text_color(s_fbItems[i], sel ? lk.selColor : lk.itemColor, 0);
            lv_obj_set_style_text_opa(s_fbItems[i], LV_OPA_COVER, 0);
        }
    }
```
Delete from the comment `// The pill is sized to the WIDEST row` through `lv_obj_align(s_fbHl, ...)` inclusive (that closing `}` of the function is the one shown above).

(b) In `refresh_wifi_list()`: delete the line `lv_obj_add_flag(s_wifiHl, LV_OBJ_FLAG_HIDDEN);` in the scanning branch; delete the three lines `const int hlRow = ...;`, `lv_obj_clear_flag(s_wifiHl, ...);`, `lv_obj_align(s_wifiHl, ...);` after the row loop. In the row loop replace `lv_obj_set_style_text_color(s_wifiRows[r], isSel ? C_WHITE : C_GREY, 0);` with:
```cpp
                const wheel::Look lk = look();
                lv_obj_set_style_text_font(s_wifiRows[r], isSel ? lk.selFont : lk.itemFont, 0);
                lv_obj_set_style_text_color(s_wifiRows[r], isSel ? lk.selColor : lk.itemColor, 0);
```

(c) The three character strips (`s_strip[k]` at the Location search page, `s_wkStrip[k]` at the WiFi password page): change `hot ? C_WHITE : C_GREY` to `hot ? look().selColor : look().itemColor` on both lines (currently near lines 843 and 1063). Colour only: a size change would move the strip.

- [ ] **Step 8: Wheel canvas and art lifecycle**

Delete `settings_text::init(s_screen);` (near the end of `settingsview::init()`), and replace the two `settings_text::begin_frame();` calls in `show_page` with `wheel::clear();` (there is one in `show_page`; the other was inside the deleted `wheel_layout`).

Replace `settings_art_acquire` / `settings_art_release` and `onExit` / `onEnter` with:

```cpp
namespace {
    // The plate and the glass are the settings art the theme ships. plate_sprite tries flash, then the card,
    // then nothing, and a design with no picture simply has a flat background.
    plate_sprite::Plate s_plate { "settings_plate.png",   "settings_plate" };
    plate_sprite::Plate s_glass { "settings_overlay.png", "settings_overlay", nullptr, {}, false, true };

    void settings_art_acquire() {
        if (!s_plateImg) {
            if (const lv_img_dsc_t *plate = plate_sprite::get(s_plate)) {
                s_plateImg = lv_img_create(s_screen);
                lv_img_set_src(s_plateImg, plate);
                lv_obj_center(s_plateImg);
                lv_obj_move_background(s_plateImg);   // behind every page
            }
        }
        if (!s_ovImg) {
            if (const lv_img_dsc_t *ov = plate_sprite::get(s_glass)) {
                s_ovImg = lv_img_create(s_screen);
                lv_img_set_src(s_ovImg, ov);
                lv_obj_center(s_ovImg);
            }
        }
        if (s_ovImg) lv_obj_move_foreground(s_ovImg);   // CRT/glass over everything
    }

    void settings_art_release() {
        if (s_plateImg) { lv_obj_del(s_plateImg); s_plateImg = nullptr; }
        if (s_ovImg)    { lv_obj_del(s_ovImg);    s_ovImg    = nullptr; }
        plate_sprite::release(s_plate);   // hand the decoded PSRAM back too
        plate_sprite::release(s_glass);
    }
}

// Called by app_shell when the shell switches away from Settings. Gives back the wheel canvas and the
// background art so they are not held while another app needs the PSRAM.
void settingsview::onExit() {
    wheel::release();
    settings_art_release();
}

void settingsview::onEnter() {
    settings_art_acquire();
    wheel::acquire(s_screen);
    s_sel = 0;
    show_page(MODE_MENU);
}
```
Check the order matches what was there: art first, then the canvas (so the canvas is created last, above the plate; the glass is moved to the foreground by `settings_art_acquire` before the canvas exists). If the glass ends up under the wheel text in the simulator shot, call `if (s_ovImg) lv_obj_move_foreground(s_ovImg);` again after `wheel::acquire(s_screen);` in `onEnter`.

- [ ] **Step 9: Remove the last stragglers**

```bash
grep -n "chrome()\|\.themed\|Chrome\|settings_text\|settings_custom_\|settings_sprite\|theme_style::settings\|hlShow\|s_systemChromeFwd\|Hl\b" src/app/settings/settings_view.cpp
```
Expected: no matches except comments. Fix each code hit:
- The block near `show_page` that logs `THEMED ART ON A SETUP SCREEN` used `chrome().themed`; replace `chrome().themed` with `!s_systemChrome` there (that log fires if a setup page is showing themed art, which must never happen).
- The page-title and hint loop `if (chrome().themed) lv_obj_add_flag(s_hints[i], HIDDEN); else clear` becomes `if (!s_systemChrome) ... else ...`.
Then delete `settings_item()` and `settings_sel()` from `theme_font.h/.cpp` (their last caller was `settings_view.cpp` and `settings_text.cpp`; `settings_text.cpp` is deleted in Task 7 and is the only remaining user. If `grep -rn "settings_item\|settings_sel" src` still shows `settings_text.cpp`, leave the two wrappers until Task 7).

- [ ] **Step 10: Build and run the navigation self-test**

```bash
~/.platformio/penv/bin/pio run -e native 2>&1 | grep -E "error|warning: unused|SUCCESS" | head
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SELFTEST=1 SIM_SETTLE_MS=500 .pio/build/native/program > /tmp/wheel_selftest_after.log 2>&1 & sleep 60; kill -9 $! 2>/dev/null
diff <(grep -E "selftest.*(PASS|FAIL)" /tmp/wheel_selftest_before.log) <(grep -E "selftest.*(PASS|FAIL)" /tmp/wheel_selftest_after.log) && echo "self-test lines identical"
```
Expected: `SUCCESS`, and `self-test lines identical`. (`Settings > default row` lines that mention "whichever theme's default selection" still pass: the test clamps to the ends first.)

- [ ] **Step 11: Photograph it**

```bash
SHOTS=/tmp/wheel_after5; mkdir -p $SHOTS
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SETTLE_MS=1500 .pio/build/native/program --settingsshot $SHOTS/settings & sleep 30; kill -9 $! 2>/dev/null
SIM_WHEEL_NO_CANVAS=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SETTLE_MS=1500 .pio/build/native/program --settingsshot $SHOTS/nocanvas & sleep 30; kill -9 $! 2>/dev/null
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SETTLE_MS=1500 .pio/build/native/program --wifishot $SHOTS/wifi & sleep 30; kill -9 $! 2>/dev/null
ls $SHOTS
```
Open the BMPs (Read tool) and confirm, against `/tmp/wheel_before`:
- No filled bar behind the selected row on any page.
- The selected row is larger and in the primary colour with a glow; other rows are muted and fade away from it.
- `nocanvas-*`: the same lists are readable on plain labels, selected row still distinct. (This is Review Focus 2.)
- `wifi-*`: the first-boot and network-list pages show a distinct selected row with no pill, and rows do not overlap at the 28/24 size step. If they overlap, lower `system()` in `wheel_look.cpp` to selected 26 / item 24 and re-shoot. (Review Focus 5.)
- Long-name check (Review Focus 1): the wifi sim seeds SSIDs; if none is 32 characters long, temporarily rename one to `ABCDEFGHIJKLMNOPQRSTUVWXYZ012345` in the sim's scan fixture, confirm it ends in `...`, and put it back.

- [ ] **Step 12: Host tests, then commit and merge**

```bash
bash tests/run_host_tests.sh 2>&1 | tail -4
git add src/app/settings/settings_view.cpp src/theme/core/theme_font.h src/theme/core/theme_font.cpp
git commit -m "Settings draws through the wheel; no selection pill on any page

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
git switch worktree-unified-wheel && git merge --no-ff feat/wheel-5-settings -m "Merge feat/wheel-5-settings: Settings on the shared wheel, pills removed"
```
Expected: `all host tests passed`.

---

### Task 6: The app picker on the wheel

**Files:**
- Modify: `src/app/shell/app_shell.cpp`
- Create: `tests/wheel_ring_test.cpp`, `tests/run_wheel_ring_test.sh`; add `wheel_ring.h` beside `wheel_layout.h`
- Modify: `tests/run_host_tests.sh`
- Delete: `src/app/settings/menu_text.h`, `menu_text.cpp`, `menu_sprite.h`, `menu_sprite.cpp`
- Modify: `src/theme/core/theme_font.h`, `theme_font.cpp` (delete `menu_current`, `menu_prev`, `menu_next`, and `settings_item`, `settings_sel` if still present)

**Interfaces:**
- Consumes: `wheel::*` (Task 2), `wheel_look::themed()` (Task 4), `plate_sprite` (Task 4).
- Produces (`wheel_ring.h`, `namespace wheel_layout`): `int ring_rows(int visible, int position, int *outIndex)`, which fills `outIndex[]` with the positions (0..visible-1) to show, in order, so that each visible app appears once, the current one is in the middle, and it wraps. Returns the number of rows; the selected row is at `visible / 2`.

The picker cycles and wraps, so its rows must too: the selected app in the middle, neighbours on both sides at every position.

- [ ] **Step 1: Branch**

```bash
git switch worktree-unified-wheel && git switch -c feat/wheel-6-picker
```

- [ ] **Step 2: Write the failing ring test**

Create `tests/wheel_ring_test.cpp`:

```cpp
// Host test for src/platform/wheel/wheel_ring.h.   tests/run_wheel_ring_test.sh
#include "wheel_ring.h"

#include <assert.h>
#include <stdio.h>

using namespace wheel_layout;

static void five_apps_put_the_current_one_in_the_middle_at_every_position() {
    for (int pos = 0; pos < 5; ++pos) {
        int idx[8];
        const int n = ring_rows(5, pos, idx);
        assert(n == 5);
        assert(idx[2] == pos);                       // selected sits at visible / 2
        assert(idx[1] == (pos + 4) % 5 && idx[3] == (pos + 1) % 5);
        assert(idx[0] == (pos + 3) % 5 && idx[4] == (pos + 2) % 5);
        bool seen[5] = { false, false, false, false, false };
        for (int i = 0; i < n; ++i) { assert(!seen[idx[i]]); seen[idx[i]] = true; }   // each app once
    }
}

static void four_apps_still_show_each_once_with_the_current_one_at_visible_over_two() {
    int idx[8];
    const int n = ring_rows(4, 3, idx);
    assert(n == 4 && idx[2] == 3);
    bool seen[4] = { false, false, false, false };
    for (int i = 0; i < n; ++i) seen[idx[i]] = true;
    assert(seen[0] && seen[1] && seen[2] && seen[3]);
}

static void one_and_two_apps_and_none() {
    int idx[8];
    assert(ring_rows(1, 0, idx) == 1 && idx[0] == 0);
    assert(ring_rows(2, 1, idx) == 2 && idx[1] == 1);
    assert(ring_rows(0, 0, idx) == 0);
}

int main() {
    five_apps_put_the_current_one_in_the_middle_at_every_position();
    four_apps_still_show_each_once_with_the_current_one_at_visible_over_two();
    one_and_two_apps_and_none();
    printf("wheel_ring: all tests passed\n");
    return 0;
}
```

Create `tests/run_wheel_ring_test.sh` (same shape as `run_wheel_layout_test.sh`, compiling `tests/wheel_ring_test.cpp` with `-Isrc/platform/wheel`, binary `wheel_ring_test`). Add `run "wheel ring"                         bash tests/run_wheel_ring_test.sh` after the `wheel layout` line of `tests/run_host_tests.sh`.

Run: `bash tests/run_wheel_ring_test.sh`
Expected: FAIL to compile (`wheel_ring.h` not found).

- [ ] **Step 3: Write `wheel_ring.h`**

Create `src/platform/wheel/wheel_ring.h`:

```cpp
#pragma once
// A cyclic list on a wheel: the app picker wraps, so its rows are a ring with the current entry in the middle.
namespace wheel_layout {

// Fill `outIndex[0..n)` with the entries (0..visible-1) to draw, in order, current one included, so that every
// entry appears exactly once and the current one is at row visible / 2. `outIndex` needs room for `visible`
// entries. Returns n (= visible).
inline int ring_rows(int visible, int position, int *outIndex) {
    if (visible <= 0) return 0;
    const int above = visible / 2;
    for (int i = 0; i < visible; ++i)
        outIndex[i] = ((position + (i - above)) % visible + visible) % visible;
    return visible;
}

} // namespace wheel_layout
```
Run: `bash tests/run_wheel_ring_test.sh`
Expected: `wheel_ring: all tests passed`.

- [ ] **Step 4: Move the picker to the wheel in `app_shell.cpp`**

(a) Replace the includes `#include "menu_sprite.h"` and `#include "menu_text.h"` with:
```cpp
#include "wheel.h"
#include "wheel_ring.h"
#include "wheel_look.h"
#include "plate_sprite.h"
```

(b) Replace the `menu_custom_plate()` / `menu_custom_overlay()` / `menu_sprite_release()` uses in `overlay_art_acquire()` / `overlay_art_release()`. Add above `overlay_art_acquire` (inside the same anonymous namespace):
```cpp
    plate_sprite::Plate s_menuPlate { "menu_plate.png",   "menu_plate" };
    plate_sprite::Plate s_menuGlass { "menu_overlay.png", "menu_overlay", nullptr, {}, false, true };
```
and change `menu_custom_plate()` to `plate_sprite::get(s_menuPlate)`, `menu_custom_overlay()` to `plate_sprite::get(s_menuGlass)`, `menu_text::available()` to `wheel::available()`, and in `overlay_art_release()` replace `menu_sprite_release();` with:
```cpp
        plate_sprite::release(s_menuPlate);
        plate_sprite::release(s_menuGlass);
```

(c) Replace the whole `show_overlay` function with:

```cpp
    // Every visible app as a ring with the current one in the middle, drawn by the wheel. The overlay's plain
    // label is the fallback when the wheel's canvas could not be allocated: an overlay with no text on it is
    // worse than a plain one, because there is then no way to see which app you are on.
    void show_overlay(const char *name) {
        if (!s_overlay) return;
        wheel::acquire(s_overlay);   // ~868 KB PSRAM, held only while the overlay is up
        overlay_art_acquire();
        const wheel::Look lk = wheel_look::themed();
        if (wheel::available()) {
            int vis[MAX_APPS], visN = 0, pos = 0;
            for (int i = 0; i < s_count; ++i) {
                if (s_apps[i].hidden && i != s_browseIdx) continue;   // the current app shows even if it is hidden
                if (i == s_browseIdx) pos = visN;
                vis[visN++] = i;
            }
            int ring[MAX_APPS];
            const int n = wheel_layout::ring_rows(visN, pos, ring);
            const char *rows[MAX_APPS];
            for (int i = 0; i < n; ++i) rows[i] = s_apps[vis[ring[i]]].name;
            wheel::draw(rows, n, n / 2, lk);
            if (s_overlayLabel && !lv_obj_has_flag(s_overlayLabel, LV_OBJ_FLAG_HIDDEN))
                lv_obj_add_flag(s_overlayLabel, LV_OBJ_FLAG_HIDDEN);
        } else if (s_overlayLabel) {
            lv_obj_set_style_text_color(s_overlayLabel, lk.selColor, 0);
            lv_obj_set_style_text_font(s_overlayLabel, lk.selFont, 0);
            lv_obj_set_style_text_align(s_overlayLabel, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_text(s_overlayLabel, name);
            lv_obj_align(s_overlayLabel, LV_ALIGN_CENTER, 0, 0);
            lv_obj_clear_flag(s_overlayLabel, LV_OBJ_FLAG_HIDDEN);
        }
        // Only when it is actually hidden: this runs on every detent on a full-screen container, and a
        // redundant clear is a full-frame repaint if LVGL treats it as a change.
        if (lv_obj_has_flag(s_overlay, LV_OBJ_FLAG_HIDDEN))
            lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
        s_browseTouch = millis();          // any turn/open restarts the settle countdown
    }
```
`MAX_APPS` is the constant `s_apps[MAX_APPS]` already uses.

(d) In `hide_overlay()` replace `menu_text::release();` and the `#if CUSTOM_HAS_MENU` guards with:
```cpp
        wheel::release();         // give the canvas back the moment it is off screen
        overlay_art_release();    // and the background/glass art with it
```

(e) In `app_shell::init` (around lines 392-410): delete the `s_overlayHint` creation (the "push to open" label) and its member declaration, delete the `#if CUSTOM_HAS_MENU ... menu_text::init(s_overlay); #endif` block, and instead of hiding the label there, leave `s_overlayLabel` created and hidden:
```cpp
    s_overlayLabel = lv_label_create(s_overlay);
    lv_obj_align(s_overlayLabel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_overlayLabel, LV_OBJ_FLAG_HIDDEN);   // shown only if the wheel's canvas cannot be allocated
```
(replacing the existing three-line font/colour/align setup for it).

(f) Check nothing else uses the removed pieces: `grep -n "CUSTOM_HAS_MENU\|menu_text\|menu_custom\|menu_sprite\|theme_style::menu\|s_overlayHint\|wrap_text" src/app/shell/app_shell.cpp` must print nothing.

- [ ] **Step 5: Delete the old picker files and the transitional font wrappers**

```bash
git rm src/app/settings/menu_text.h src/app/settings/menu_text.cpp src/app/settings/menu_sprite.h src/app/settings/menu_sprite.cpp
grep -rn "menu_current\|menu_prev\|menu_next\|menu_text\|menu_sprite" src | grep -v "^src/theme_assets"
```
Expected after the `git rm`: the grep lists only `theme_font.h/.cpp` (the wrappers) and possibly comments. Delete the wrappers `menu_current`, `menu_prev`, `menu_next` from `theme_font.h/.cpp`; if `settings_text.cpp` is the only remaining user of `settings_item()` / `settings_sel()`, leave those two for Task 7.

- [ ] **Step 6: Build, self-test, photograph**

```bash
~/.platformio/penv/bin/pio run -e native 2>&1 | grep -E "error|SUCCESS" | head
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 2>&1 | grep -E "error|SUCCESS" | head
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SELFTEST=1 SIM_SETTLE_MS=500 .pio/build/native/program > /tmp/wheel_selftest_6.log 2>&1 & sleep 60; kill -9 $! 2>/dev/null
diff <(grep -E "selftest.*(PASS|FAIL)" /tmp/wheel_selftest_before.log) <(grep -E "selftest.*(PASS|FAIL)" /tmp/wheel_selftest_6.log) && echo "self-test lines identical"
SHOTS=/tmp/wheel_after6; mkdir -p $SHOTS
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SETTLE_MS=1500 .pio/build/native/program --rockshot $SHOTS/rock & sleep 30; kill -9 $! 2>/dev/null
SIM_WHEEL_NO_CANVAS=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SETTLE_MS=1500 .pio/build/native/program --rockshot $SHOTS/rocknocanvas & sleep 30; kill -9 $! 2>/dev/null
bash tools/themeshots.sh /tmp/wheel_after6 default elegant fallout portal vaultec
```
Expected: both builds `SUCCESS`; `self-test lines identical`. Read `rock-rocked.bmp`: a wheel of app names, the current one large and glowing in the middle, its neighbours above and below, no pill. Read `rocknocanvas-rocked.bmp`: the current app's name on a plain label. From `themeshots` open the knob-menu BMP for each of the five themes and confirm a legible wheel in each. To check wrap-around (Review Focus 4), the `--rockshot` starts on Settings, the last app: confirm the first app (Clock) appears below the selected row.

- [ ] **Step 7: Commit and merge**

```bash
bash tests/run_host_tests.sh 2>&1 | tail -4
git add src/app/shell/app_shell.cpp src/platform/wheel/wheel_ring.h tests/wheel_ring_test.cpp tests/run_wheel_ring_test.sh tests/run_host_tests.sh src/theme/core/theme_font.h src/theme/core/theme_font.cpp
git commit -m "The app picker draws through the wheel, as a cyclic ring of app names

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
git switch worktree-unified-wheel && git merge --no-ff feat/wheel-6-picker -m "Merge feat/wheel-6-picker: the picker on the shared wheel"
```
(`git rm` already staged the deletions.)

---

### Task 7: Remove the `settings:` and `menu:` schema

**Files:**
- Delete: `src/app/settings/settings_text.h`, `settings_text.cpp`, `settings_sprite.h`, `settings_sprite.cpp`; `src/theme/custom/custom_menu.h`, `custom_settings.h`, `custom_menu_plate.h`, `custom_menu_overlay.h`, `custom_settings_plate.h`, `custom_settings_overlay.h`
- Modify: `src/theme/core/theme_style.h`, `theme_style.cpp`, `theme_palette.h`, `theme_palette.cpp`, `theme_font.h/.cpp`
- Modify: `tools/build_theme.py`, `tools/dump_theme_defaults.cpp`
- Modify: `src/theme_assets/{elegant,fallout,portal,vaultec}/theme.yaml` (remove the `settings:` and `menu:` blocks)
- Modify: `tests/test_theme_style_isolation.py`, `tests/test_theme_palette.py`, `tests/test_build_theme.py`, `tests/golden/resolved_{elegant,fallout,portal}.json`, `tests/theme_font_resolve_test.cpp`

**Interfaces:**
- Produces: `THEME_CAPS = 54`; `theme_palette::apply_role_defaults(const Palette &, Clock &, Radar &, Weather &, Ticker &, Splash &, Intel &)` (the `Menu` and `Settings` parameters removed); a builder that raises `BuildError` for a top-level `settings` / `menu` key and for a retired font slot, naming the replacement.

- [ ] **Step 1: Branch, and write the failing builder tests**

```bash
git switch worktree-unified-wheel && git switch -c feat/wheel-7-schema
```
Read `tests/test_build_theme.py` for the helper that builds a throwaway theme from a YAML string (a function like `build(yaml_text)` or `run_builder`), and add in the same style:

```python
    def test_a_retired_settings_block_is_refused_with_the_replacement_named(self):
        with self.assertRaises(build_theme.BuildError) as ctx:
            self.build_source('slug: t\nname: T\nsettings:\n  selColor: 0xFFFFFF\n')
        self.assertIn('wheel', str(ctx.exception))
        self.assertIn('palette', str(ctx.exception))

    def test_a_retired_menu_block_is_refused_with_the_replacement_named(self):
        with self.assertRaises(build_theme.BuildError) as ctx:
            self.build_source('slug: t\nname: T\nmenu:\n  current: {color: 0xFFFFFF}\n')
        self.assertIn('wheel', str(ctx.exception))

    def test_a_retired_font_slot_is_refused_with_the_replacement_named(self):
        for old, new in (('menu_current', 'wheel_sel'), ('settings', 'wheel_item'), ('settings_sel', 'wheel_sel'),
                         ('menu_prev', 'wheel_item'), ('menu_next', 'wheel_item')):
            with self.subTest(slot=old), self.assertRaises(build_theme.BuildError) as ctx:
                self.build_source(f'slug: t\nname: T\nfonts:\n  faces:\n    a: {{src: x.ttf, size: 20}}\n  slots:\n    {old}: a\n')
            self.assertIn(new, str(ctx.exception))
```
If the file has no `build_source` helper, define one at the top of the test class that writes the YAML to a temp folder and calls `build_theme.build(...)` the way the neighbouring tests do (copy their exact call).

Run: `python3 -m unittest tests.test_build_theme -v 2>&1 | tail -12`
Expected: the three new tests FAIL (the builder still accepts the keys or gives the generic message).

- [ ] **Step 2: Builder: retire the keys**

In `tools/build_theme.py`:

(a) `SECTIONS = ('clock', 'radar', 'weather', 'ticker', 'splash', 'intel')` (drop `'settings'` and `'menu'`), and update the docstring table near line 15 to remove `settings: -> settings_style.json     menu:     -> menu_style.json`.

(b) Add beside `SECTIONS`:
```python
# Removed in THEME_CAPS 54, when the app picker and Settings became one fixed wheel. Refused rather than ignored,
# so a theme that still carries one hears why instead of silently looking different.
RETIRED_SECTIONS = {
    'settings': 'the wheel is fixed and takes its colours from the palette: primary for the selected row, muted for '
                'the rest. Delete the block.',
    'menu': 'the app picker is now the same wheel as Settings and takes its colours from the palette. Delete the block.',
}
RETIRED_SLOTS = {
    'menu_current': 'wheel_sel', 'settings_sel': 'wheel_sel',
    'settings': 'wheel_item', 'menu_prev': 'wheel_item', 'menu_next': 'wheel_item',
}
```

(c) Where the builder checks top-level keys (around line 428, `if key not in SECTIONS and key not in MANIFEST_KEYS:`), insert before it:
```python
        if key in RETIRED_SECTIONS:
            raise BuildError(f'top-level key {key!r} was removed in THEME_CAPS 54: {RETIRED_SECTIONS[key]}')
```

(d) In the font slot check (around line 327, `if slot not in facts['slots']:`), insert before it:
```python
        if slot in RETIRED_SLOTS:
            raise BuildError(f'fonts.slots.{slot}: removed in THEME_CAPS 54; use {RETIRED_SLOTS[slot]}')
```

Run: `python3 -m unittest tests.test_build_theme -v 2>&1 | tail -12`
Expected: PASS.

- [ ] **Step 3: Remove the two blocks from the four theme sources**

```bash
python3 - <<'EOF'
import re
from pathlib import Path
for slug in ('elegant', 'fallout', 'portal', 'vaultec'):
    p = Path(f'src/theme_assets/{slug}/theme.yaml')
    lines = p.read_text(encoding='utf-8').splitlines(keepends=True)
    out, i, removed = [], 0, []
    while i < len(lines):
        m = re.match(r'^(settings|menu):\s*$', lines[i])
        if not m:
            out.append(lines[i]); i += 1; continue
        # the block's own header comment sits directly above it: drop those comment lines too
        while out and out[-1].lstrip().startswith('#'):
            out.pop()
        removed.append(m.group(1))
        i += 1
        while i < len(lines) and (lines[i].startswith((' ', '\t')) or not lines[i].strip()):
            i += 1
    assert sorted(removed) == ['menu', 'settings'], (slug, removed)
    p.write_text(''.join(out), encoding='utf-8')
    print(slug, removed)
EOF
git diff --stat src/theme_assets
git diff src/theme_assets | grep '^[-+]' | grep -v '^[-+][-+]' | head -80
```
Read the diff: only the `settings:` and `menu:` blocks and their own header comments may disappear. If a comment that described something else went with them, restore it by hand. Then `default/theme.yaml` needs nothing.

Confirm each theme still builds: `for s in elegant fallout portal vaultec; do python3 tools/build_theme.py src/theme_assets/$s --out /tmp/wheel_built2 2>&1 | tail -2; done`
Expected: no `BuildError`. (A `lv_font_conv` missing message is an environment gap, not a failure of this change.)

- [ ] **Step 4: Firmware: delete the blocks**

In `src/theme/core/theme_style.h`: delete `struct MenuText`, `struct Menu`, `struct Settings`, and the declarations `const Menu &menu();` and `const Settings &settings();`. Change `constexpr int THEME_CAPS = 53;` to `54` and add above it, after the entry for 53:

```
//  54  the app picker and Settings are one wheel, fixed in the firmware. The theme's `settings:` and `menu:`
//      blocks are gone, along with the font slots menu_current, menu_prev, menu_next, settings and settings_sel
//      (replaced by wheel_sel and wheel_item), the highlight pill, and Settings' default selection. The selected
//      row takes palette `primary` and its glow, every other row `muted`. An Orb below this level draws its own
//      picker and Settings from the blocks it still reads; a theme built for 54 ships neither.
```

In `src/theme/core/theme_style.cpp`: delete `#include "custom_settings.h"` and `#include "custom_menu.h"`; delete `Menu s_menu;` and `Settings s_settings;`; delete the reset block that starts `s_settings = Settings{};` through the last `s_menu.next.align = CUSTOM_MENU_NEXT_ALIGN;` and its closing `#endif`; delete `merge_menu_text`; delete the two `read_style_json` blocks, the one for `"settings_style.json"` (the braces block starting `{ JsonDocument doc; if (read_style_json(slug, "settings_style.json", doc)) {` and ending with the `}` after `defaultSel`) and the one for `"menu_style.json"` (the same shape, around line 1108). Remove `s_menu, s_settings` from both `theme_palette::apply_role_defaults(...)` calls.

In `src/theme/core/theme_palette.h/.cpp`: remove the `theme_style::Menu &menu, theme_style::Settings &settings` parameters and delete the `// ---- App switcher` and `// ---- Settings` blocks (the lines assigning `menu.*` and `settings.*`).

Delete `settings_item()` and `settings_sel()` from `theme_font.h/.cpp` if still present.

```bash
git rm src/app/settings/settings_text.h src/app/settings/settings_text.cpp src/app/settings/settings_sprite.h src/app/settings/settings_sprite.cpp \
       src/theme/custom/custom_menu.h src/theme/custom/custom_settings.h src/theme/custom/custom_menu_plate.h \
       src/theme/custom/custom_menu_overlay.h src/theme/custom/custom_settings_plate.h src/theme/custom/custom_settings_overlay.h
grep -rn "custom_menu\|custom_settings\|settings_text\|settings_sprite\|theme_style::menu\|theme_style::settings\|MenuText\|CUSTOM_HAS_MENU\|CUSTOM_HAS_SETTINGS\|settings_style\|menu_style" src tools tests --include='*.cpp' --include='*.h' --include='*.py' --include='*.sh' --include='*.ini'
```
Expected: matches only in comments, in `tests/test_theme_style_isolation.py` and `tests/test_theme_palette.py` (fixed in Step 6), and the new refusal tests. Fix any code hit.

- [ ] **Step 5: Dumper**

In `tools/dump_theme_defaults.cpp` delete `menu_text()` (line ~80), `dump_settings()` (~212), `dump_menu()` (~220) and the two calls `dump_settings(root["settings"]...)` and `dump_menu(root["menu"]...)` (~281-282). In the same file, the list `S(clock); S(flight); ... S(settings);` (~275) names screens for a different dump (app roster booleans), so leave `S(settings)` alone: `settings` there is the app name, not the block.

- [ ] **Step 6: Update the Python tests and goldens**

- `tests/test_theme_style_isolation.py`: remove `'settings_style.json': ('s_settings',)` and `'menu_style.json': ('s_menu',)` from the file-to-state map; in the `call = 'theme_palette::apply_role_defaults(...)'` string remove `s_menu, s_settings, `; in the assertion that lists the state names remove `'s_menu'` and `'s_settings'`; the test that writes `s_settings.selOpa = 10;` should use `s_intel` (or another remaining state) so its intent, a stray write into another file's state, is unchanged.
- `tests/test_theme_palette.py`: remove `'settings', 'menu'` from `KEEP`; delete the assertions at lines ~156-164 and ~225 that read `s['settings'][...]` and `s['menu'][...]`; delete the test that sets `menu:\n  current: {fmt: ...}` (~line 92) since the block is gone.
- Regenerate the three resolved goldens with the dumper (they lose the `settings` and `menu` keys). Run `python3 tools/resolved_golden.py --help` for the exact write flag, then rewrite `tests/golden/resolved_elegant.json`, `resolved_fallout.json`, `resolved_portal.json`. Review each diff: the only change must be the removal of those two top-level keys. Any other changed value is a regression to investigate, not to bless.
- `tests/theme_font_resolve_test.cpp`: add this case and call it from `main`:

```cpp
static void a_map_naming_retired_slots_is_ignored() {
    // A card holding a theme built before the wheel still carries these entries. They name slots the firmware
    // no longer has, so nothing ever looks them up, and the slots that do exist resolve as usual.
    FontMap m;
    must_add(m, "menu_current", "font_old46.bin");
    must_add(m, "settings_sel", "font_old27.bin");
    must_add(m, "radar1", "font_body.bin");
    Resolved r;
    resolve(SLOTS, N, map_lookup, &m, r);
    assert(strcmp(r.file[1], "font_body.bin") == 0);           // radar1 is mapped
    assert(strcmp(r.file[0], "font_wheel_sel.bin") == 0);      // no entry for it: its own file
}
```

- [ ] **Step 7: Run everything**

```bash
bash tests/run_host_tests.sh 2>&1 | tail -6
~/.platformio/penv/bin/pio run -e native 2>&1 | grep -E "error|SUCCESS" | head
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 2>&1 | grep -E "error|SUCCESS" | head
python3 -m unittest discover -s tests -p "test_*.py" > /tmp/wheel_py_7.log 2>&1; grep -E '^(FAIL|ERROR):|^Ran |^OK|^FAILED|skipped' /tmp/wheel_py_7.log
```
Expected: `all host tests passed`, both builds `SUCCESS`, Python `OK` with a skipped count no higher than preflight P2. If the count is higher, a dumper-backed test stopped compiling: run the failing dumper build by hand (`python3 -c "import sys; sys.path.insert(0,'tools'); import gen_elegant_theme as g; g.build_dumper('/tmp/dump')"`) and fix the compile error.

- [ ] **Step 8: Commit and merge**

```bash
git add -u
git add src/theme_assets tests/golden tools/build_theme.py tools/dump_theme_defaults.cpp
git status --short | head -30
git commit -m "Remove the settings and menu theme blocks, the old font slots and the compiled fallbacks

The picker and Settings are one fixed wheel. THEME_CAPS 54.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
git switch worktree-unified-wheel && git merge --no-ff feat/wheel-7-schema -m "Merge feat/wheel-7-schema: retire settings: and menu:, THEME_CAPS 54"
```
Before committing check `git status --short`: it must show only files named in this task. Do not stage `Untitled theme.orb` or anything untracked outside `src/theme_assets/<slug>`.

---

### Task 8: Docs, version, hardware list, and the final checks

**Files:**
- Modify: `docs/theme-yaml.md`, `docs/adding-a-screen.md`, `docs/ARCHITECTURE.md`, `docs/HARDWARE_PENDING.md`, `src/config.h`, `src/theme_assets/README.md` (only if it names the removed blocks)

- [ ] **Step 1: Branch and find every doc mention**

```bash
git switch worktree-unified-wheel && git switch -c feat/wheel-8-docs
grep -n "settings:\|menu:\|menu_current\|menu_prev\|menu_next\|settings_sel\|font_settings\|hlColor\|hlShow\|defaultSel\|settings_style\|menu_style\|highlight pill\|menu_text\|settings_text" docs/theme-yaml.md docs/adding-a-screen.md docs/ARCHITECTURE.md src/theme_assets/README.md README.md
```

- [ ] **Step 2: Edit them**

- `docs/theme-yaml.md`: remove `settings:` and `menu:` from the section list and the `settings_style.json` / `menu_style.json` table row; in the font-slot list replace `menu_current`, `menu_prev`, `menu_next`, `settings`, `settings_sel` with `wheel_sel`, `wheel_item`; add a short section:

```
## The wheel

The app picker and every list in Settings are one wheel. Its shape is fixed. A theme dresses it with:
the palette (`primary` is the selected row and its glow, `muted` is every other row), the font slots `wheel_sel`
and `wheel_item`, and the artwork `menu_plate.png` / `menu_overlay.png` (the picker) and `settings_plate.png` /
`settings_overlay.png` (Settings). There is no `settings:` or `menu:` block, no highlight bar, and no default
row: THEME_CAPS 54 removed them, and the builder refuses a theme that still has one, naming the replacement.
```
- `docs/adding-a-screen.md`: where it lists the standard parts of a screen (glass, typefaces, text controls), note that a knob-driven list must use `src/platform/wheel` through `wheel_look` and must not draw its own selection background.
- `docs/ARCHITECTURE.md`: in the source layout list add `platform/wheel` (the wheel), `app/common/wheel_look`, and remove `menu_text`, `settings_text`, `menu_sprite`, `settings_sprite` if listed.

- [ ] **Step 3: Version and hardware list**

In `src/config.h` change `#define FW_VERSION "2.21.1"` to `"2.22.0"`.

Append to `docs/HARDWARE_PENDING.md`, in that file's existing entry style (read its last two entries first and copy their heading and bullet format):

```
### Unified wheel (2.22.0), 2026-09-25

Not booted on an Orb. Simulator-verified only.

- The picker and every Settings list now draw through one canvas (`src/platform/wheel`), about 868 KB of PSRAM
  taken on entering either screen and given back on leaving. Check the PSRAM figure the `[wheel]` log lines print
  and that Flight Tracker still allocates its own overlays after visiting Settings and the picker.
- Detent latency in the picker and in Settings (the picker's dirty-rectangle repaint was kept; confirm a turn
  does not repaint the whole panel).
- The recovery pages (first-boot, network list, password) no longer have a selection pill. Confirm on the glass
  that the selected row is unmistakable, especially the network list at the 28 px / 24 px sizes.
- Settings' selected row is about 46 px on themes that ship a `wheel_sel` face (was 27). Confirm it fits and reads
  well on all five themes. (Set by you to validate.)
- A card written by an older build, with `settings_style.json` and an old `fonts.map`, must still boot into a
  working Settings and picker.
```

- [ ] **Step 4: The final gate**

```bash
bash tests/run_host_tests.sh 2>&1 | tail -6
python3 -m unittest discover -s tests -p "test_*.py" > /tmp/wheel_py_final.log 2>&1; grep -E '^(FAIL|ERROR):|^Ran |^OK|^FAILED|skipped' /tmp/wheel_py_final.log
~/.platformio/penv/bin/pio run -e native 2>&1 | grep -E "error|SUCCESS"
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 2>&1 | grep -E "error|SUCCESS"
bash tools/themeshots.sh /tmp/wheel_final default elegant fallout portal vaultec
python3 tools/shot_diff.py /tmp/wheel_before /tmp/wheel_final 2>&1 | tail -30
grep -rn "settings_style\|menu_style\|menu_text\|settings_text\|style_highlight\|defaultSel" src tools tests docs/theme-yaml.md docs/adding-a-screen.md docs/ARCHITECTURE.md | grep -v "test_build_theme\|superpowers/\|THEME_CAPS"
```
Expected: host tests pass; Python `OK` with skipped no higher than P2; both builds `SUCCESS`; `shot_diff` reports the knob-menu and Settings frames changed and every other app's frames (clock, radar, weather, news) unchanged, since nothing outside the wheel screens was touched (if a non-wheel app frame changed, that is a regression: find out why before continuing); the final `grep` prints nothing.

- [ ] **Step 5: Commit and merge**

```bash
git add docs/theme-yaml.md docs/adding-a-screen.md docs/ARCHITECTURE.md docs/HARDWARE_PENDING.md src/config.h
git commit -m "Docs for the wheel, FW_VERSION 2.22.0, and the hardware checks still owed

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
git switch worktree-unified-wheel && git merge --no-ff feat/wheel-8-docs -m "Merge feat/wheel-8-docs: docs, 2.22.0, HARDWARE_PENDING"
```

- [ ] **Step 6: Report**

Report, without claiming hardware verification: what was built, the test counts, the skipped count against P2, which screenshots were read and what they showed, the four items in `HARDWARE_PENDING.md` the user validates (including the selected-row size), and that the branch `worktree-unified-wheel` is ready to merge but not merged to `main`. Mention the collision with the main checkout's untracked `src/theme_assets/vaultec/`.
