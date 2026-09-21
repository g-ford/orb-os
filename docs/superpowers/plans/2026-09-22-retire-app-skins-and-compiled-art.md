# Retire the app skins and delete the compiled art: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The whole device draws from the active theme's palette (or the built-in one) with no compiled Default/Office skins, and the compiled dials, hands, splash PNGs, Office sprites and bitmap fonts are gone from the firmware image, at least 1.3 MB smaller.

**Architecture:** Two parts, each its own branch. Part A (spec step 4) removes the `app_theme` skin API and every branch on it, leaving `app_theme::palette()` as the only entry point. Part B (spec step 5) deletes art that Part A and the existing code have already made unreachable, replaces the three compiled bitmap-font fallbacks with LVGL's Montserrat, and adds guards so the art cannot creep back.

**Tech Stack:** C++17 firmware (LVGL 8.4, PlatformIO at `~/.platformio/penv/bin/pio`), Python 3 unittest guards, the desktop simulator (`pio run -e native`, `--themeshot`) for before/after screenshots.

**Spec:** `docs/superpowers/specs/2026-09-20-theme-palette-fonts-builtin-default-design.md`, sections "Design 2. Palette roles" (the *Retired* bullet), "Design 4. Compiled art removed (step 5)", "Rollout" steps 4 and 5, "Testing". This plan follows plan 2 (`2026-09-21-palette-roles-and-procedural-clock.md`), which delivered spec steps 2 and 3.

## Decisions taken while planning

These settle things the spec left open or that the code showed to be different. Each is recorded so it can be reversed cheaply.

1. **Steps 4 and 5 as the spec words them (owner's call, 2026-09-22).** Step 4 is the skin logic and the API. The Office *art* is deleted in step 5, not step 4, so the Office sprite files stay in the tree, unreferenced, from Task 4 until Task 8.
2. **The Settings "selector row" is already gone.** `Display` lost its Theme row on 2026-09-14. What remains is an unreachable theme-picker page and restart-notice page (`MODE_THEME_SELECT`, `MODE_THEME_NOTICE`): nothing calls `show_page` with either. Task 5 deletes them. The live picker is Settings > Design, which is untouched.
3. **`CUSTOM_CLOCK.active` is a compiled `true`** (`src/theme/custom/custom_clock.h`), so `clockview::init()` always ends on `FACE_CUSTOM`. The Aviator, Imperial, Digital and Office faces, and everything they draw from (`dial_img.h`, `dial_avi.h`, `hand_*_img.h`, the Office sprites), are already unreachable. Deleting them changes what no theme draws. Task 8 removes the four faces and `s_face`.
4. **Two fallbacks are reachable and their removal is a visible change**, for a theme with **no `palette`** only (a user's own legacy theme; all three shipped themes have one):
   - no `clock_hand_*.png` in its folder: today it gets the compiled ornate hands (`CUSTOM_HOUR_PNG` etc., Task 9); afterwards it gets no hands, because the drawn hands are palette-mode only;
   - no `splash.png`: today it gets the compiled brown card (`SPLASH_PNG_DEFAULT`, Task 10); afterwards it gets its background and the text lines, with no card.
   Both are the spec's intent ("the compiled theme fallbacks are deleted"). Both are recorded in `docs/HARDWARE_PENDING.md`.
5. **The compiled `CUSTOM_HAS_HOUR/MINUTE/SECOND` flags and the pivots, blend and order macros stay.** `theme_style.cpp:121` reads the flags as the default for each hand's `show`, so setting them to 0 would silently hide hands on legacy themes. Only the PNG byte arrays go.
6. **The three bitmap-font fallbacks become Montserrat 44, 28 and 20** (menu name, radar text 1, radar text 2), the nearest sizes in `lv_conf.h` to the compiled 46, 27 and 21 px. 46 and 21 are equidistant between two Montserrat sizes; the narrower is chosen so a line cannot outgrow a layout tuned for the compiled face. This is done by re-pointing three macros in generated headers whose generator is not in this repo, which is the "small per-slot fallback table" the spec describes; `theme_font.cpp` is untouched.
7. **`app_theme::setRestartHook` goes with `set()`.** `theme_select` has its own hook and `sim_main.cpp` registers it separately (`sim_main.cpp:887`), so nothing is lost.
8. **The old NVS key `appTheme` is left in place and never read**, and the simulator's `/tmp/orb_sim_theme` state file is no longer read either. The namespace is permanent.
9. **Out of scope:** the radar's own compiled skins (`THEME_ORB/MILITARY/AVIATOR`, brown backdrop, no range rings): only `officeMode()` is removed from `radar_view.cpp`. `docs/HARDWARE_PENDING.md` currently says those skins are retired by "spec step 4"; that is wrong and Task 12 corrects it. Also out of scope: the slug `the-office` in comments and in `tests/theme_slug_policy_test.cpp` (it names Modern's former theme folder, which is still true), and the deferred review minors listed in `docs/HARDWARE_PENDING.md`.
10. **Tools whose input is deleted are deleted:** `tools/render_theme_bitmaps.py` (renders previews of the compiled art) and `tools/bake_dial.py` (makes the dials). The other `bake_*` helpers are generic and stay, with their Office comments corrected.
11. **Docs:** only the statements that Part B makes false are corrected (Task 12). The rest of spec step 6 stays a separate piece of work.
12. **Versions:** `FW_VERSION` 2.20.0 at the end of Part A, 2.21.0 at the end of Part B. `THEME_CAPS` does not change: no theme-format option is added or removed.

## Global Constraints

- Firmware is not done until it has booted on a real Orb. There is none yet, so every firmware task adds to `docs/HARDWARE_PENDING.md` and nothing is called verified.
- No `git add -A`: explicit paths. Test before fix in commit order, so no commit fails its own test. One branch per part, merged `--no-ff`: **Tasks 1 to 7 on `feat/retire-app-skins`; Tasks 8 to 12 on `feat/delete-compiled-art`**, then an integration branch `integrate/retire-skins-and-art` (Task 13). The final move of `main` is the owner's, from `/Users/geoffford/code/orb-os`: a session isolated to a worktree cannot run git against the shared checkout.
- Every commit ends with the trailer `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>` (a second `-m`).
- Personal permissions go in `.claude/settings.local.json`, never `settings.json`.
- **Both firmware builds must succeed after every firmware task:** device `~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175`, simulator `~/.platformio/penv/bin/pio run -e native`. Read warnings for the files you touched.
- **Baseline at `333dd7d` (recorded 2026-09-22):** `firmware.bin` 5,511,152 bytes (`Flash: used 5510792`); host tests all pass; `python3 -m unittest discover -s tests -p "test_*.py"` runs **203 tests, OK, 0 skipped**. A later count that is lower, or "OK (skipped=N)", is a regression to explain.
- Run the whole `tests/` directory before calling a branch green, not a hand-picked list (`python3 -m unittest discover -s tests -p "test_*.py"`, about two minutes, run it in the background and grep `^(FAIL|ERROR):|^Ran |^OK|^FAILED|skipped`). Also `bash tests/run_host_tests.sh` and both `pio run` envs.
- Keep shell commands plain: no `cd &&` chains, no shell variables in a command line, no inline multi-line Python. The tool guard refuses them. Put scripts in files (`tools/`, or the scratchpad for one-offs).
- A worktree has no `sim/` and its `.pio/` is cold on first use; the first build takes about a minute.
- `/tmp/orb_sim_theme_slug` and `/tmp/orb_sim_theme` are global to the machine, shared with other worktrees. `tools/themeshots.sh` puts the slug back; anything you write to `/tmp/orb_sim_theme` you must remove.

## Review Focus

Failure modes the spec implies that no headline test would catch. Each has a test or a check in the task named.

1. **A saved Office skin must change nothing.** An Orb that had `appTheme = 1` in NVS, or a sim with `1` in `/tmp/orb_sim_theme`, must boot to exactly what it would show with no saved skin. Nothing may read either any more (Task 6: the guard test forbids the key string; the sim check proves the pixels).
2. **A legacy theme (no `palette`) with no splash and no hand images** must draw a plainer screen, not crash, not allocate the 424 KB splash buffer, and not bring a brown card back (Tasks 9 and 10: wiring tests pin that there is no compiled fallback path at all).
3. **The three slots that lost a bitmap face** must draw Montserrat 44, 28 and 20 with no theme and no card, and a shipped theme must still get its own faces (Task 11: macro test; `tests/test_theme_font_coverage.py` stays green).
4. **A stale simulator source list is silent.** PlatformIO ignores a `+<file>` that does not exist, so a deleted file listed in `[env:native]` cannot fail the build. It already happens: `theme/custom_font1.c` and `custom_font2.c` are listed and are not where the list says. Task 11's guard fails on any `+<path>` that is missing.
5. **Deleting whole faces must not move what remains on the clock.** The sweep and cache logic tested `s_face != FACE_CUSTOM`. Task 8's test pins that no face variable is left, and the screenshots of every shipped theme's settings, menu and other apps must diff `same` after each firmware task.

## File Structure

| File | Change |
|---|---|
| `tools/themeshots.sh`, `tools/shot_diff.py` (new) | Photograph every app of a theme in the simulator; compare two folders of shots. The spec's regression net. |
| `src/app/common/app_theme.{h,cpp}` | Shrink to `AppPalette` and `palette()`. |
| `src/app/settings/settings_view.cpp` | Delete the unreachable theme picker and notice pages, the Office block in `init()`, and the `office` argument. |
| `src/app/ui/ui.cpp`, `src/app/spycam/spycam_view.cpp`, `src/app/radar/radar_view.cpp` | Delete the Office branches. |
| `src/app/clock/clock_view.cpp` | Task 4: the Office branch in `init()`. Task 8: the four compiled faces, `s_face`, the art includes. |
| `src/theme/graphics/splash_art.{h,cpp}` | Task 2: drop the `office` parameter. Task 10: drop the compiled fallback. |
| `src/app/common/custom_sprite.cpp`, `src/theme/custom/custom_hands.h` | Task 9: no flash fallback for any clock image. |
| `src/theme/custom/custom_menu.h`, `custom_radar.h` | Task 11: three font macros point at Montserrat. |
| `src/main.cpp`, `src/platform/sim/sim_main.cpp`, `src/theme/core/theme_select.h` | Drop `app_theme::init()` and the restart hook; fix comments. |
| `platformio.ini` | Drop the deleted sources from `[env:native]`. |
| `tests/test_app_skin_retired.py` (new), `tests/test_no_compiled_art.py` (new) | The two guards. |
| `tests/test_splash_wiring.py`, `tests/test_clock_wiring.py`, `tests/test_default_palette.py` | Follow the code. |
| Deleted in Part B | `dial_img.h`, `dial_avi.h`, `hand_img.h`, `hand_hour_img.h`, `hand_min_img.h`, `hand_hour_shadow_img.h`, `hand_min_shadow_img.h`, `office_sprite.{h,cpp}`, `office_{hour,minute}_png.h`, `office_{hour,minute}_img_meta.h`, `splash_png_default.h`, `splash_png_office.h`, ten `custom_*font*.c`, `tools/render_theme_bitmaps.py`, `tools/bake_dial.py`. |

---

## Part A: Retire Default/Office (spec step 4)

### Task 1: The screenshot tools, and a baseline to compare against

**Files:**
- Create: `tools/themeshots.sh` (already written in this worktree, untracked), `tools/shot_diff.py` (same)
- Test: none (the tools are exercised by every later task)

**Interfaces:**
- Produces: `bash tools/themeshots.sh <outdir> [slug ...]` writes `<outdir>/<slug>-<n>-<App>.bmp` and `<slug>-menu.bmp` for each of `default elegant fallout portal` (six each) and restores the sim's saved slug. `python3 tools/shot_diff.py [--skip-clock] <before> <after>` prints `same` or `CHANGED bbox=... pixels=...` per file and exits 1 on any difference. `--skip-clock` skips the first app of each theme, which shows the wall time and differs on every run.

- [ ] **Step 1: Create the branch and commit this plan**

Run from the worktree root (`/Users/geoffford/code/orb-os/.claude/worktrees/retire-default-office`):

```bash
git switch -c feat/retire-app-skins
git add docs/superpowers/plans/2026-09-22-retire-app-skins-and-compiled-art.md
git commit -m "Plan for spec steps 4 and 5: retire the app skins, delete the compiled art" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

- [ ] **Step 2: Build the simulator and take the baseline**

```bash
~/.platformio/penv/bin/pio run -e native
bash tools/themeshots.sh /private/tmp/claude-501/-Users-geoffford-code-orb-os/4fbd0a69-9a10-49d4-9ae4-2952002a44be/scratchpad/shots/before
```

Expected: `default: 6 screenshots` and the same for `elegant`, `fallout`, `portal` (about two minutes; run it in the background). If that folder already holds the 24 baseline shots from plan-writing, do not retake them: they were taken from `333dd7d`, which is what this branch starts from. Confirm the tools are deterministic: shoot `default` again into a second folder and `python3 tools/shot_diff.py --skip-clock <first> <second>` must print `same` for every non-clock shot and exit 0.

- [ ] **Step 3: Commit the tools**

```bash
git add tools/themeshots.sh tools/shot_diff.py
git commit -m "Tools: themeshots.sh and shot_diff.py, the before/after screenshot check" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

### Task 2: `splash_art_decode` stops taking a skin argument

**Files:**
- Modify: `src/theme/graphics/splash_art.h`, `src/theme/graphics/splash_art.cpp`, `src/app/ui/ui.cpp:1126-1134`, `src/app/settings/settings_view.cpp:890`
- Test: `tests/test_splash_wiring.py`

**Interfaces:**
- Produces: `bool splash_art_decode(lv_img_dsc_t *out);` (was `(bool office, lv_img_dsc_t *out)`). Callers: `ui_splash_show()` and `refresh_about()`.

- [ ] **Step 1: Rewrite the wiring test to the new shape (it must fail first)**

Replace the body of `tests/test_splash_wiring.py` after `code()` with:

```python
class SplashWiringTest(unittest.TestCase):
    def test_the_compiled_fallback_is_skipped_in_palette_mode(self):
        text = code('src/theme/graphics/splash_art.cpp')
        self.assertRegex(text, r'if \(!ok && !theme_style::paletteOn\(\)\) \{')

    def test_the_decoder_takes_no_skin_argument(self):
        self.assertIn('bool splash_art_decode(lv_img_dsc_t *out);', code('src/theme/graphics/splash_art.h'))
        self.assertIn('bool splash_art_decode(lv_img_dsc_t *out) {', code('src/theme/graphics/splash_art.cpp'))

    def test_nothing_is_allocated_when_nothing_will_be_decoded(self):
        """ensure() takes the 466x466 RGB565 decode buffer (about 424 KB of PSRAM) and it is never freed. In palette mode a
        theme with no splash of its own decodes nothing, so the function must return before that allocation, and after the
        pre-baked flash check, which needs no buffer either."""
        text = code('src/theme/graphics/splash_art.cpp')
        body = text[text.index('bool splash_art_decode('):]
        early = re.search(r'if \(theme_style::paletteOn\(\) && !\(slug\[0\] && theme_style::hasAsset\("splash\.png"\)\)\) return false;', body)
        self.assertIsNotNone(early, 'no early return for palette mode with no splash image of its own')
        self.assertLess(body.index('find_active("splash.png"'), early.start(), 'the pre-baked flash splash must still win')
        self.assertLess(early.start(), body.index('if (!ensure())'), 'the return must come before the allocation')

    def test_the_splash_background_is_the_palette_background(self):
        text = code('src/app/ui/ui.cpp')
        self.assertIn('theme_style::paletteOn() ? app_theme::palette().bg : lv_color_black()', text)
```

- [ ] **Step 2: Run it and see it fail**

`python3 -m unittest tests/test_splash_wiring.py -v` → 3 failures (the `office` spellings are still there).

- [ ] **Step 3: Edit `splash_art.cpp`**

Delete `#include "splash_png_office.h"`. Change `bool splash_art_decode(bool office, lv_img_dsc_t *out) {` to `bool splash_art_decode(lv_img_dsc_t *out) {`. Change the early return to:

```cpp
    if (theme_style::paletteOn() && !(slug[0] && theme_style::hasAsset("splash.png"))) return false;
```

Replace the whole "2) Fall back" comment and block with:

```cpp
    // 2) Fall back to whatever's flash-baked (a Launch Kit push) or, failing that, the stock art. In palette mode (the
    // built-in look, or a theme that has a palette) there is no compiled card: the splash is the palette's background
    // with its lines drawn over it.
    if (!ok && !theme_style::paletteOn()) {
#if CUSTOM_HAS_SPLASH
        ok = try_decode(CUSTOM_SPLASH_PNG, CUSTOM_SPLASH_PNG_LEN);
#else
        ok = try_decode(SPLASH_PNG_DEFAULT, SPLASH_PNG_DEFAULT_LEN);
#endif
    }
```

In `splash_art.h` change the declaration to `bool splash_art_decode(lv_img_dsc_t *out);` and reword the header comment so it no longer says "office/default" (it becomes "the stock art baked at splash_png_default.h").

- [ ] **Step 4: Edit the two callers**

`src/app/ui/ui.cpp`, in `ui_splash_show()`: delete `const bool office = app_theme::get() == APP_THEME_OFFICE;`; the next line becomes

```cpp
    lv_obj_set_style_bg_color(cont, theme_style::paletteOn() ? app_theme::palette().bg : lv_color_black(), 0);
```

and `if (splash_art_decode(office, &splashImg)) {` becomes `if (splash_art_decode(&splashImg)) {`.

`src/app/settings/settings_view.cpp:890`: `if (splash_art_decode(&aboutImg))`.

- [ ] **Step 5: Verify and commit**

`python3 -m unittest tests/test_splash_wiring.py -v` → OK. Build both envs. Then:

```bash
git add src/theme/graphics/splash_art.h src/theme/graphics/splash_art.cpp src/app/ui/ui.cpp src/app/settings/settings_view.cpp tests/test_splash_wiring.py
git commit -m "Splash: splash_art_decode no longer takes a skin argument" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

### Task 3: The radar loses `officeMode()`

**Files:**
- Modify: `src/app/radar/radar_view.cpp` (lines 334-340, 383-391, 1750-1764, 1771)

**Interfaces:**
- Consumes: nothing new. `orb()` and `aviator()` keep their names and meaning for every caller.

- [ ] **Step 1: Edit**

Replace the comment and three one-liners at `radar_view.cpp:334-340` with:

```cpp
// The scope's own skin. Orb's neon grid and Aviator's sepia dial are selected by s_theme; Military and anything else
// falls back to the plain ring/crosshair scope.
static inline bool orb() { return s_theme == THEME_ORB; }
static inline bool aviator() { return s_theme == THEME_AVIATOR; }
```

In `alt_color()`, delete the whole `if (officeMode()) { ... }` block (the first eight lines of the function body).

In `setTheme()`, replace the `if (officeMode()) { ... } else { switch ... }` with the switch alone, dedented one level:

```cpp
    switch (s_theme) {                          // pick the scope chrome palette
        case THEME_MILITARY:
            s_cRing = lv_color_hex(0x49C46B); s_cLead = lv_color_hex(0x76E08C);
            s_cInk  = lv_color_hex(0xE0FFE6); s_cSoft = lv_color_hex(0x9FD7A8); break;
        case THEME_AVIATOR:
            s_cRing = AVI_RING; s_cLead = AVI_LEAD; s_cInk = AVI_INK; s_cSoft = AVI_SOFT; break;
        default:                                // orb (uses its own colors elsewhere) / any invalid value
            s_cRing = COL_GREEN; s_cLead = COL_LEAD; s_cInk = COL_INK; s_cSoft = COL_SOFT; break;
    }
```

and change the background line to `lv_obj_set_style_bg_color(s_parent, aviator() ? AVI_BG : lv_color_black(), 0);`.

- [ ] **Step 2: Verify**

Both builds succeed with no new warnings in `radar_view.cpp`. `grep -n "officeMode\|APP_THEME" src/app/radar/radar_view.cpp` prints nothing.

- [ ] **Step 3: Commit**

```bash
git add src/app/radar/radar_view.cpp
git commit -m "Radar: remove the Office branches (officeMode)" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

### Task 4: The HUD, the spy camera and the clock lose their Office branches

**Files:**
- Modify: `src/app/ui/ui.cpp:46-60`, `src/app/spycam/spycam_view.cpp:513-578`, `src/app/clock/clock_view.cpp:1792-1852`

- [ ] **Step 1: `ui.cpp`**

In the comment above `ui_apply_theme`, delete the last three lines (the ones starting "The Office app theme (see app_theme.h) overrides all of this"). Delete the whole `if (app_theme::get() == APP_THEME_OFFICE) { ... return; }` block at the top of the function body.

- [ ] **Step 2: `spycam_view.cpp`**

Delete `const bool office = app_theme::get() == APP_THEME_OFFICE;`. Replace the three uses:
`office ? app_theme::palette().bg : lv_color_black()` becomes `lv_color_black()`; both `office ? app_theme::palette().soft : lv_color_hex(0x6A7078)` become `lv_color_hex(0x6A7078)`.

- [ ] **Step 3: `clock_view.cpp`, `clockview::init()` only**

The dead faces themselves go in Task 8. Here remove the skin logic:

```cpp
void clockview::init() {
    if (CUSTOM_CLOCK.active) s_face = FACE_CUSTOM;   // a pushed Launch Kit design wins over the theme default

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, COL_BLACK, 0);
```

Delete `const bool office = ...;`, `if (office) s_face = FACE_OFFICE;`, `(void)office;`, and the block at the end of `init()` that pre-decodes the Office minute sprite (the comment starting "OFFICE minute hand + rim glow sprite is pre-decoded here" and its `if (!office_minute_sprite()) Serial.println(...)`).

- [ ] **Step 4: Verify and commit**

Both builds succeed. Run `bash tools/themeshots.sh <after-dir>` after Task 5, not here (Task 5 is the last of the three view tasks and one screenshot pass covers them).

```bash
git add src/app/ui/ui.cpp src/app/spycam/spycam_view.cpp src/app/clock/clock_view.cpp
git commit -m "HUD, spy camera and clock: remove the Office branches" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

### Task 5: Settings loses the theme picker, the Office palette swap and the `office` argument

**Files:**
- Modify: `src/app/settings/settings_view.cpp`

Every edit below is a deletion or a comment fix, and every anchor is a name to search for, so it survives line drift.

- [ ] **Step 1: Mode enum and state**

Remove `MODE_THEME_SELECT, MODE_THEME_NOTICE,` from `enum Mode`. Delete `int s_themeSel = 0;` and the four declarations `s_themeSelPage`, `s_themeSelHl`, `s_themeSelItems[APP_THEME_COUNT + 1]`, `s_themeNoticePage`.

- [ ] **Step 2: The page and its handlers**

Delete: `refresh_themeSelect()` and the comment above it ("Theme picker: turning browses Default/Office..."); the two `lv_obj_add_flag(..., HIDDEN)` lines for `s_themeSelPage` and `s_themeNoticePage` in `show_page`; the two `else if (m == MODE_THEME_SELECT)` / `MODE_THEME_NOTICE` dispatch lines; the `else if (s_mode == MODE_THEME_SELECT) { s_themeSel += step; ... }` branch in the turn handler; the `MODE_THEME_SELECT` and `MODE_THEME_NOTICE` branches in the press handler (the one that calls `app_theme::set`); and, in `init()`, everything from `// --- theme picker page (Display > Theme) ---` up to (not including) `// --- design picker page`. In the turn handler change `s_mode == MODE_WIFI_STATUS || s_mode == MODE_THEME_NOTICE || s_mode == MODE_DESIGN_NOTICE` to `s_mode == MODE_WIFI_STATUS || s_mode == MODE_DESIGN_NOTICE`.

- [ ] **Step 3: The Office palette swap in `settingsview::init()`**

Delete the four-line block at the top of `init()`:

```cpp
    if (app_theme::get() == APP_THEME_OFFICE) {
        const AppPalette &p = app_theme::palette();
        C_WHITE = p.ink; C_GREY = p.soft; C_DIM = p.dim; C_ACCENT = p.accent;
        C_BG = p.bg; C_HL = p.highlight; C_TRACK = p.hairline;
    }
```

Change the comment above `C_WHITE` to `// The Settings pages' own colours. A theme's colours reach Settings through chrome(), below.`

- [ ] **Step 4: Comments that describe a picker that no longer exists**

Rewrite the "No 'Theme' row here any more" comment above `enum { DSP_SCREEN ...}` to two lines: the Display page has Screen and Brightness only; themes are chosen in Settings > Design. Rewrite the comment above `refresh_designSelect` to: `// Design picker (top-level "Design" item): turning browses the built-in look and whichever themes are installed on the SD card, pressing shows the restart notice and applies it (settingsview::onPress). Rescans the card every time this page is entered ...` keeping the rescan sentence as it is. In `init()`, rewrite the comment above the design picker page to drop "same shape as the theme picker above ... instead of a fixed APP_THEME_COUNT".

- [ ] **Step 5: Include**

`grep -n "app_theme" src/app/settings/settings_view.cpp`. If no `app_theme::` use is left, delete `#include "app_theme.h"`.

- [ ] **Step 6: Verify against the screenshots**

Build both envs, then:

```bash
~/.platformio/penv/bin/pio run -e native
bash tools/themeshots.sh /private/tmp/claude-501/-Users-geoffford-code-orb-os/4fbd0a69-9a10-49d4-9ae4-2952002a44be/scratchpad/shots/a5
python3 tools/shot_diff.py --skip-clock /private/tmp/claude-501/-Users-geoffford-code-orb-os/4fbd0a69-9a10-49d4-9ae4-2952002a44be/scratchpad/shots/before /private/tmp/claude-501/-Users-geoffford-code-orb-os/4fbd0a69-9a10-49d4-9ae4-2952002a44be/scratchpad/shots/a5
```

Expected: every line `same` (20 shots), exit 0. Then Read one clock shot from `before` and from `a5` (the default theme's) and confirm by eye that the dial, hands and date are the same drawing.

- [ ] **Step 7: Commit**

```bash
git add src/app/settings/settings_view.cpp
git commit -m "Settings: remove the unreachable Default/Office picker and the Office palette swap" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

### Task 6: `app_theme` shrinks to the palette, and a guard keeps it there

**Files:**
- Create: `tests/test_app_skin_retired.py`
- Modify: `src/app/common/app_theme.h`, `src/app/common/app_theme.cpp`, `src/main.cpp:2361`, `src/platform/sim/sim_main.cpp:884-886`, `src/theme/core/theme_select.h`, `tests/test_default_palette.py`, `tests/theme_roles_test.cpp:93`

**Interfaces:**
- Produces: `app_theme.h` exposes only `struct AppPalette` and `const AppPalette &app_theme::palette();`.

- [ ] **Step 1: Write the guard (it must fail first)**

Create `tests/test_app_skin_retired.py`:

```python
"""Spec step 4: the compiled Default/Office skins and the API that chose between them are gone. Rule 4: a second
palette system must not come back by accident, so this reads the whole source tree rather than sitting beside one call."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'src'

RETIRED = (
    r'APP_THEME_\w+',
    r'\bAppThemeId\b',
    r'app_theme::(get|set|name|init|setRestartHook)\b',
    r'\bofficeMode\b',
    r'MODE_THEME_(SELECT|NOTICE)',
    r'"appTheme"',        # the NVS key: left in place on old Orbs and never read
    r'orb_sim_theme"',    # the simulator's old skin state file (not orb_sim_theme_slug, which is the slug)
)


def strip_comments(text: str) -> str:
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def offences(text: str) -> list:
    code = strip_comments(text)
    return [p for p in RETIRED if re.search(p, code)]


class AppSkinRetiredTest(unittest.TestCase):
    def test_the_check_can_fail(self):
        self.assertEqual(offences('int x = app_theme::get();'), [r'app_theme::(get|set|name|init|setRestartHook)\b'])
        self.assertEqual(offences('// app_theme::get() is gone'), [])
        self.assertEqual(offences('p.getInt("appTheme", 0);'), [r'"appTheme"'])
        self.assertEqual(offences('static inline bool officeMode() { return false; }'), [r'\bofficeMode\b'])

    def test_no_source_file_refers_to_a_retired_skin(self):
        bad = {}
        for p in list(SRC.rglob('*.cpp')) + list(SRC.rglob('*.h')):
            found = offences(p.read_text(encoding='utf-8', errors='replace'))
            if found:
                bad[str(p.relative_to(ROOT))] = found
        self.assertEqual(bad, {})

    def test_the_header_offers_only_the_palette(self):
        code = strip_comments((SRC / 'app' / 'common' / 'app_theme.h').read_text(encoding='utf-8'))
        self.assertIn('const AppPalette &palette();', code)
        self.assertEqual(re.findall(r'^\s*(?:int|void|const char \*)\s+\w+\(', code, re.M), [])


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run it and see it fail**

`python3 -m unittest tests/test_app_skin_retired.py -v` → `test_no_source_file_refers_to_a_retired_skin` and `test_the_header_offers_only_the_palette` fail, naming `app_theme.h`, `app_theme.cpp`, `main.cpp`, `sim_main.cpp`, `theme_select.h`.

- [ ] **Step 3: Replace `app_theme.h`**

```cpp
#pragma once
// The nine-role palette every screen draws its chrome from. It is read off the active theme's colour roles
// (theme_roles.h), or the built-in night-vision set when no theme is selected or a theme has no palette of its own.
// There used to be a second, compiled palette here with two whole-device skins, Default and Office, chosen in
// Settings and stored under the NVS key "appTheme". Both are gone. The key is left in place on any Orb that wrote it
// and is never read: the namespace is permanent.
#include <lvgl.h>

struct AppPalette {
    lv_color_t bg;         // screen background
    lv_color_t panel;      // cards, tracks, the boot-splash halo
    lv_color_t highlight;  // selected menu row / active control fill
    lv_color_t ink;        // primary text
    lv_color_t soft;       // secondary text / labels
    lv_color_t dim;        // hints, disabled, least-emphasis text
    lv_color_t accent;     // clock hand, links, active state, radar blips
    lv_color_t hairline;   // list dividers
    lv_color_t onAccent;   // text drawn on top of an accent-filled control
};

namespace app_theme {

const AppPalette &palette();        // active palette (by const ref: no copies in draw callbacks)

} // namespace app_theme
```

- [ ] **Step 4: Replace `app_theme.cpp`**

```cpp
#include "app_theme.h"
#include "theme_style.h"

namespace app_theme {

// Everything reads the theme's roles (the built-in ones when the theme has no palette), so a themed Orb's menu,
// splash and Settings share its colours.
const AppPalette &palette() {
    static AppPalette s;
    const theme_roles::Palette &r = theme_style::palette();
    s.bg        = lv_color_hex(r.v[theme_roles::R_bg]);
    s.panel     = lv_color_hex(r.v[theme_roles::R_panel]);
    s.highlight = lv_color_hex(r.v[theme_roles::R_highlight]);
    s.ink       = lv_color_hex(r.v[theme_roles::R_text]);
    s.soft      = lv_color_hex(r.v[theme_roles::R_secondary]);
    s.dim       = lv_color_hex(r.v[theme_roles::R_dim]);
    s.accent    = lv_color_hex(r.v[theme_roles::R_primary]);
    s.hairline  = lv_color_hex(r.v[theme_roles::R_hairline]);
    s.onAccent  = lv_color_hex(r.v[theme_roles::R_onPrimary]);
    return s;
}

} // namespace app_theme
```

- [ ] **Step 5: The callers**

`src/main.cpp`: delete the line `app_theme::init();     // load the saved app skin (Default/Office) before any view reads it`.
`src/platform/sim/sim_main.cpp`: delete the three lines `app_theme::setRestartHook(sim_restart); ...`, `app_theme::init(); ...` and the `printf("[sim] app theme: ...")`; keep the `theme_select::` lines that follow.
`src/theme/core/theme_select.h`: three comments name `app_theme::`. Change the `init()` comment to `// load the saved slug from NVS; call once at boot, before any view reads theme data`, the `set()` comment to `// persists, then reboots/re-execs (device: ESP.restart(); sim: the restart hook)`, and the `setRestartHook` comment to `// Native only: sim_main.cpp registers its own re-exec here. A fresh process needs this file's static state reloaded from the persisted slug, not defaulted, right after the re-exec.`

- [ ] **Step 6: Re-point the palette test at literals**

In `tests/test_default_palette.py`, replace `test_the_built_in_palette_equals_todays_default_app_palette` with:

```python
    def test_the_built_in_palette_is_the_night_vision_set_every_screen_has_always_had(self):
        # These nine values were app_theme.cpp's APP_THEME_DEFAULT, the compiled palette retired in spec step 4. They
        # are written out here so the constants in theme_roles.h cannot drift from what the screens have always shown.
        b = built_in_from_header()
        want = {'bg': 0x000000, 'panel': 0x0C160F, 'highlight': 0x232A36, 'text': 0xEAFFF3, 'secondary': 0x9AFFC8,
                'dim': 0x5F7A6C, 'primary': 0x1DFF86, 'hairline': 0x1C2620, 'onPrimary': 0x05100A}
        self.assertEqual({k: b[k] for k in want}, want)
```

Remove now-unused imports if `re` is no longer used in that file. In `tests/theme_roles_test.cpp:93` change "(see app_theme.cpp)" to "(the palette app_theme.cpp used to hold)".

- [ ] **Step 7: Verify, including the saved-Office check**

`python3 -m unittest tests/test_app_skin_retired.py tests/test_default_palette.py tests/test_splash_wiring.py -v` → OK. Build both envs. Then prove a saved skin changes nothing:

```bash
echo 1 > /tmp/orb_sim_theme
~/.platformio/penv/bin/pio run -e native
bash tools/themeshots.sh /private/tmp/claude-501/-Users-geoffford-code-orb-os/4fbd0a69-9a10-49d4-9ae4-2952002a44be/scratchpad/shots/a6 default elegant
rm -f /tmp/orb_sim_theme
python3 tools/shot_diff.py --skip-clock /private/tmp/claude-501/-Users-geoffford-code-orb-os/4fbd0a69-9a10-49d4-9ae4-2952002a44be/scratchpad/shots/before_de /private/tmp/claude-501/-Users-geoffford-code-orb-os/4fbd0a69-9a10-49d4-9ae4-2952002a44be/scratchpad/shots/a6
```

(`before_de` is the subset of the baseline holding the `default` and `elegant` shots.) All ten non-clock lines must be `same`. Before this change the same run turned the whole device white.

- [ ] **Step 8: Commit**

```bash
git add tests/test_app_skin_retired.py tests/test_default_palette.py tests/theme_roles_test.cpp src/app/common/app_theme.h src/app/common/app_theme.cpp src/main.cpp src/platform/sim/sim_main.cpp src/theme/core/theme_select.h
git commit -m "app_theme: retire the Default/Office skins; only palette() is left" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

### Task 7: Part A closes: pending list, version, the whole suite

**Files:**
- Modify: `docs/HARDWARE_PENDING.md`, `src/config.h:10`

- [ ] **Step 1: Append to `docs/HARDWARE_PENDING.md`**

```markdown
## Retire Default/Office (plan `2026-09-22-retire-app-skins-and-compiled-art.md`, Part A, spec step 4)

Built, host-tested and screenshot-compared in the simulator only.

- [ ] An Orb that had the Office skin saved (`appTheme` = 1 in NVS from an older firmware) boots to its theme or the
      built-in look, not a white UI, with no error on the serial log. Nothing reads that key any more.
- [ ] Settings shows no Theme row under Display and the top-level Design picker still lists Default first and works.
- [ ] Flight Tracker, Spy Cam, the boot splash and Settings > About look as they did before.
```

- [ ] **Step 2: Bump the version**

`src/config.h`: `#define FW_VERSION "2.20.0"`.

- [ ] **Step 3: Run everything**

Run in the background and read the logs:

```bash
python3 -m unittest discover -s tests -p "test_*.py"
bash tests/run_host_tests.sh
~/.platformio/penv/bin/pio run -e native
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175
```

Expected: **Ran 207 tests** (203 plus 3 in the new `test_app_skin_retired.py` and 1 new test in `test_splash_wiring.py`; `test_default_palette.py` swaps one test for one), `OK`, no `skipped`; host tests `all host tests passed`; both builds succeed. A different count is something to explain, not to accept. Then one last screenshot pass and `shot_diff.py --skip-clock` against `before` for all four themes: all `same`.

- [ ] **Step 4: Commit**

```bash
git add docs/HARDWARE_PENDING.md src/config.h
git commit -m "Retire Default/Office: pending hardware checks, FW_VERSION 2.20.0" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

## Part B: Delete the compiled art and fonts (spec step 5)

Start it with `git switch -c feat/delete-compiled-art` from the tip of `feat/retire-app-skins`.

### Task 8: The clock keeps one face; the compiled dials, hands and Office sprites go

**Files:**
- Modify: `src/app/clock/clock_view.cpp`, `platformio.ini`, `src/app/common/curved_text.h` (comment), `src/theme/graphics/png_decode.h` (comment), `tools/bake_clock_custom.py` (comment)
- Delete: `src/theme/custom/dial_img.h`, `dial_avi.h`, `hand_img.h`, `hand_hour_img.h`, `hand_min_img.h`, `hand_hour_shadow_img.h`, `hand_min_shadow_img.h`; `src/theme/graphics/office_sprite.h`, `office_sprite.cpp`, `office_hour_png.h`, `office_minute_png.h`, `office_hour_img_meta.h`, `office_minute_img_meta.h`; `tools/render_theme_bitmaps.py`, `tools/bake_dial.py`
- Test: `tests/test_clock_wiring.py`

- [ ] **Step 1: Write the test (it must fail first)**

Append to `tests/test_clock_wiring.py`, before the `if __name__` line:

```python
class OneFaceTest(unittest.TestCase):
    """The clock has one face, the theme's. The compiled Aviator, Imperial, Digital and Office faces were unreachable
    once CUSTOM_CLOCK.active became a constant true, and they carried most of the firmware's compiled art."""
    RETIRED = ('FACE_AVIATOR', 'FACE_IMPERIAL', 'FACE_DIGITAL', 'FACE_OFFICE', 'FACE_CUSTOM', 's_face',
               'draw_aviator', 'draw_imperial', 'draw_digital', 'draw_office', 'DIAL_IMG', 'DIAL_AVI',
               'HAND_HOUR_IMG', 'HAND_MIN_IMG', 'office_', 'OFFICE_')

    def test_no_compiled_face_is_left(self):
        text = code()
        for word in self.RETIRED:
            self.assertNotIn(word, text, f'{word} is a compiled face or its art')

    def test_redraw_draws_the_themes_face_and_nothing_else(self):
        text = code()
        body = text[text.index('static void redraw('):]
        body = body[:body.index('\n}\n')]
        self.assertIn('draw_custom(ti);', body)
        self.assertNotIn('switch', body)

    def test_the_sweep_does_not_ask_which_face_is_showing(self):
        text = code()
        start = text.index('static bool sweep_possible()')
        self.assertNotIn('not a custom face', text[start:text.index('static lv_area_t second_box(', start)])
```

Run `python3 -m unittest tests/test_clock_wiring.py -v`: the three new tests fail.

- [ ] **Step 2: Confirm nothing else uses the art**

```bash
grep -rn "hand_img.h\|HAND_IMG\b" src tools
grep -rln "dial_img\|dial_avi\|DIAL_IMG\|DIAL_AVI\|hand_hour_img\|hand_min_img\|office_sprite\|OFFICE_" src tools tests
```

Expected: the first prints only the file's own definition (`hand_img.h` is not included by anything); the second lists `clock_view.cpp`, the art headers themselves, `curved_text.h`, `png_decode.h`, `tools/bake_*.py`, `tools/render_theme_bitmaps.py`. Anything else means stop and read it.

- [ ] **Step 3: Edit `clock_view.cpp`**

Work top to bottom.

1. Replace the four-line header comment with: `// Clock app for the shell. One face: the theme's own (plate, hands, overlay and text), with the built-in look drawn from the palette wherever a theme ships no image. See compose_custom() and the drawn face below.`
2. Delete the includes `office_sprite.h`, `office_minute_img_meta.h`, `office_hour_img_meta.h`, `dial_img.h`, `dial_avi.h`, `hand_hour_img.h`, `hand_min_img.h`, `hand_hour_shadow_img.h`, `hand_min_shadow_img.h`, and (after the compile check in Step 5) `custom_clock.h`.
3. Replace the block of colour constants under `// ---- palette` (every `COL_*` from `COL_HAND` to `COL_WK_OFF`) with the single line the drawn face still uses: `static const lv_color_t COL_BLACK = LV_COLOR_MAKE(0x00, 0x00, 0x00);`. Keep `CX`, `CY`, `DEG2RAD`.
4. Delete `DATE_WIN_X/Y`, `AVI_DATE_R/MID/STEP`, the `FACE_OFFICE ... FACE_CUSTOM` comment, `enum Face`, `s_face`, `s_hourImg`, `s_minImg`, `s_hourShadow`, `s_minShadow`, `HAND_SHADOW_DX`, `HAND_SHADOW_DY` and the comment above them. Keep `s_screen`, `s_canvas`, `s_buf`.
5. Drawing helpers: keep `P`, `draw_disc`, `draw_needle_at`. Delete `draw_hand_at`, `draw_hand_edged`, `draw_round_rect`.
6. **Move `unpack565`.** It is defined inside the Office region but the custom hand blender uses it (`clock_view.cpp` around `blend_custom_hand` and the plate rotation). Cut its four-line definition and paste it after `draw_needle_at`.
7. Delete everything from the line `// ---- IMPERIAL face` up to, and not including, `// ---- CUSTOM face (pushed from Launch Kit)`. That removes `draw_imperial`, `draw_aviator`, `draw_digit`, `draw_seg_cell`, `draw_digital`, the Office constants, `blend_office_sprite`, the two Office sprite helpers and `draw_office`.
8. `redraw()` becomes:

```cpp
static void redraw(const struct tm *ti) {
    if (!s_canvas || !s_buf) return;
    draw_custom(ti);
    lv_obj_invalidate(s_canvas);
}
```

9. In `sweep_possible()` delete the line `if (s_face != FACE_CUSTOM) return (s_sweepWhyNot = "not a custom face", false);  ...`.
10. In `apply_face()` delete the comment "Hand sprites belong to the aviator face only" and the `if (s_face != FACE_AVIATOR) { ... }` block under it.
11. `clockview::init()` becomes:

```cpp
void clockview::init() {
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, COL_BLACK, 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    // The canvas is NOT allocated here any more; onEnter() takes it when the app is shown
    // and onExit() gives it back. This is the boot app, so it is taken moments later
    // regardless, and the difference is that it is released the moment you leave.

    apply_face();
    s_tick = lv_timer_create(tick_cb, 1000, nullptr);
    retime();
}
```

(Everything between the canvas comment and `apply_face()` today, the shadow and hand image objects, is deleted.)

- [ ] **Step 4: Delete the files and fix the lists**

```bash
git rm src/theme/custom/dial_img.h src/theme/custom/dial_avi.h src/theme/custom/hand_img.h src/theme/custom/hand_hour_img.h src/theme/custom/hand_min_img.h src/theme/custom/hand_hour_shadow_img.h src/theme/custom/hand_min_shadow_img.h
git rm src/theme/graphics/office_sprite.h src/theme/graphics/office_sprite.cpp src/theme/graphics/office_hour_png.h src/theme/graphics/office_minute_png.h src/theme/graphics/office_hour_img_meta.h src/theme/graphics/office_minute_img_meta.h
git rm tools/render_theme_bitmaps.py tools/bake_dial.py
```

In `platformio.ini`, in `[env:native]`'s `build_src_filter`, delete `+<theme/graphics/office_sprite.cpp> `. Fix the three comments that name the Office files: `curved_text.h` ("custom_sprite/office_sprite emit" becomes "custom_sprite emits"), `png_decode.h` (drop "and office loaders" from the list), `tools/bake_clock_custom.py` line 6 ("that blend_office_sprite() rotates" becomes "that blend_custom_hand() rotates"), `tools/bake_png.py` line 5 ("like the Office splash" becomes "like a flat card").

- [ ] **Step 5: Compile-driven cleanup**

Build both envs. Every `unused function`, `unused variable` or `not declared` error in `clock_view.cpp` names something still to delete or to keep. Expect at most: a leftover colour or helper to delete, and `CUSTOM_CLOCK` now unused (delete its include if so). Do not silence a warning; remove its cause. Repeat until both builds are clean for that file.

- [ ] **Step 6: Verify**

`python3 -m unittest tests/test_clock_wiring.py -v` → OK. Take shots (`shots/b8`) and `shot_diff.py --skip-clock` against `before`: all `same`. Read the default clock shot from both folders and confirm by eye that the drawing is the same.

- [ ] **Step 7: Commit**

```bash
git add src/app/clock/clock_view.cpp src/app/common/curved_text.h src/theme/graphics/png_decode.h platformio.ini tools/bake_clock_custom.py tools/bake_png.py tests/test_clock_wiring.py
git commit -m "Clock: one face; delete the compiled dials, hands and Office sprites" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

### Task 9: No clock image has a compiled fallback

**Files:**
- Modify: `src/app/common/custom_sprite.cpp`, `src/theme/custom/custom_hands.h`
- Test: `tests/test_clock_wiring.py`

**Interfaces:**
- Produces: `custom_sprite.cpp`'s internal `bool decode_from_sd(const char *assetName, bool alpha, uint8_t *&out, int &w, int &h, const char *tag)` replaces `decode_sd_first(...)`. Public functions (`custom_plate`, `custom_overlay`, `custom_hand`, `splash_overlay`, `wind_*`) are unchanged.

- [ ] **Step 1: Replace the old guard with the new one (it must fail first)**

In `tests/test_clock_wiring.py`, delete the class `BuiltInLookHasNoCompiledArtTest` and add:

```python
class NoCompiledClockArtTest(unittest.TestCase):
    """Every clock image (plate, overlay, hands) comes through one decoder. It used to refuse its compiled flash fallback
    in the built-in mode; there is no fallback left to refuse."""

    def test_the_decoder_has_no_flash_fallback(self):
        text = re.sub(r'//[^\n]*', '', (ROOT / 'src' / 'app' / 'common' / 'custom_sprite.cpp').read_text(encoding='utf-8'))
        self.assertIn('bool decode_from_sd(const char *assetName, bool alpha,', text)
        self.assertNotIn('flashPng', text)
        self.assertNotRegex(text, r'CUSTOM_\w+_PNG')

    def test_the_hand_header_carries_options_not_pixels(self):
        text = (ROOT / 'src' / 'theme' / 'custom' / 'custom_hands.h').read_text(encoding='utf-8')
        self.assertNotRegex(text, r'_PNG(_LEN)?\b')
        # The compiled defaults for each hand's `show` are options and stay (theme_style.cpp reads them).
        self.assertIn('#define CUSTOM_HAS_HOUR 1', text)
        self.assertIn('#define CUSTOM_HAND_ORDER { 2, 0, 1 }', text)
```

`python3 -m unittest tests/test_clock_wiring.py -v` → the two new tests fail.

- [ ] **Step 2: Strip the byte arrays from `custom_hands.h`**

Write `/private/tmp/claude-501/-Users-geoffford-code-orb-os/4fbd0a69-9a10-49d4-9ae4-2952002a44be/scratchpad/strip_hand_pngs.py`:

```python
import re
from pathlib import Path

p = Path('src/theme/custom/custom_hands.h')
text = p.read_text(encoding='utf-8')
text = re.sub(r'static const uint8_t CUSTOM_(?:HOUR|MINUTE|SECOND|STATIC1|STATIC2)_PNG\[\d+\] = \{.*?\};\n', '', text, flags=re.S)
text = re.sub(r'static const uint32_t CUSTOM_\w+_PNG_LEN = \d+;\n', '', text)
p.write_text(text, encoding='utf-8')
```

Run `python3 /private/tmp/claude-501/-Users-geoffford-code-orb-os/4fbd0a69-9a10-49d4-9ae4-2952002a44be/scratchpad/strip_hand_pngs.py` from the worktree root. `wc -c src/theme/custom/custom_hands.h` drops from about 115,000 to under 3,000, and `grep -c "#define" src/theme/custom/custom_hands.h` still counts the defines (about 30).

- [ ] **Step 3: Edit `custom_sprite.cpp`**

1. Delete `#include "custom_plate.h"`, `"custom_overlay.h"`, `"custom_hands.h"`.
2. Rename and simplify the decoder. Its new signature and shape (the SD-reading body is unchanged; the flash tail goes):

```cpp
// The active theme's asset, read from the card and decoded into PSRAM. There is no compiled fallback: a theme that ships
// no file for a layer draws without that layer (the built-in look draws the missing pieces from the palette instead).
bool decode_from_sd(const char *assetName, bool alpha, uint8_t *&out, int &w, int &h, const char *tag) {
    const char *slug = theme_select::activeSlug();
    // Only read what the theme says it ships. ... (keep the existing comment and the three-way slug/hasAsset logging)
    ...
    return false;
}
```

Delete the `if (theme_style::paletteMode() == ... BuiltIn) flashPng = nullptr;` lines and their comment, and the `if (!flashPng) { ... }` / `return decode(flashPng, ...)` tail; end the function with `return false;` after the SD branches.
3. Call sites, each `decode_sd_first(` becomes `decode_from_sd(` with the two flash arguments removed:
   - `custom_plate()`: replace the `#if CUSTOM_HAS_PLATE ... #else ... #endif` with `if (decode_from_sd("clock_plate.png", false, o, w, h, "plate")) s_plate = (uint16_t *)o;`
   - `custom_overlay()`: likewise `decode_from_sd("clock_overlay.png", true, o, w, h, "overlay")`
   - `splash_overlay()`: `decode_from_sd("splash_overlay.png", true, o, w, h, "splash overlay")`
   - `load_wind()`: `decode_from_sd(name, true, o, fw, fh, tag)`
   - `wind_background()`: `decode_from_sd("wind_bg.png", false, o, fw, fh, "wind background")`
   - `custom_hand()`: delete `const uint8_t *png = nullptr; uint32_t len = 0;` and the five `#if CUSTOM_HAS_HOUR ... #endif` blocks; the call is `decode_from_sd(sdName[kind], true, o, w, h, "hand")`. Change the comment "SD first, flash as fallback, same contract as plate/overlay" to "From the card, same contract as plate/overlay."
4. Fix the header comment of `custom_sprite.h` if it says the sprites come from `custom_plate.h / custom_overlay.h / custom_hands.h`.

- [ ] **Step 4: Verify**

Both builds succeed. `python3 -m unittest tests/test_clock_wiring.py -v` → OK. Shots (`shots/b9`) diff `same` with `--skip-clock` for all four themes. Read `fallout` and `elegant` clock shots from before and after: hands, plate and overlay all present and identical in drawing. This is the check that the hands still come from the theme.

- [ ] **Step 5: Commit**

```bash
git add src/app/common/custom_sprite.cpp src/app/common/custom_sprite.h src/theme/custom/custom_hands.h tests/test_clock_wiring.py
git commit -m "Clock images: no compiled fallback; drop the hand PNG arrays" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

### Task 10: The splash has no compiled card

**Files:**
- Modify: `src/theme/graphics/splash_art.cpp`, `src/theme/graphics/splash_art.h`
- Delete: `src/theme/graphics/splash_png_default.h`, `src/theme/graphics/splash_png_office.h`
- Test: `tests/test_splash_wiring.py`

- [ ] **Step 1: Update the tests (they must fail first)**

In `tests/test_splash_wiring.py`, replace the first and third tests:

```python
    def test_there_is_no_compiled_splash(self):
        text = code('src/theme/graphics/splash_art.cpp')
        for word in ('SPLASH_PNG', 'CUSTOM_SPLASH', 'splash_png_', 'custom_splash.h'):
            self.assertNotIn(word, text)

    def test_nothing_is_allocated_when_nothing_will_be_decoded(self):
        """ensure() takes the 466x466 RGB565 decode buffer (about 424 KB of PSRAM) and it is never freed. A theme with no
        splash of its own decodes nothing in any mode, so the function must return before that allocation, and after the
        pre-baked flash check, which needs no buffer either."""
        text = code('src/theme/graphics/splash_art.cpp')
        body = text[text.index('bool splash_art_decode('):]
        early = re.search(r'if \(!\(slug\[0\] && theme_style::hasAsset\("splash\.png"\)\)\) return false;', body)
        self.assertIsNotNone(early, 'no early return for a theme with no splash image of its own')
        self.assertLess(body.index('find_active("splash.png"'), early.start(), 'the pre-baked flash splash must still win')
        self.assertLess(early.start(), body.index('if (!ensure())'), 'the return must come before the allocation')
```

(The other two tests in the file stay.) Run it: the new pair fails.

- [ ] **Step 2: Edit `splash_art.cpp`**

Delete `#include "splash_png_default.h"` and `#include "custom_splash.h"`. Replace everything from `const char *slug = theme_select::activeSlug();` to the closing `if (!ok) return false;` with:

```cpp
    const char *slug = theme_select::activeSlug();

    // A theme with no splash of its own decodes nothing, in any mode: the splash is then its background with the text
    // lines drawn over it (ui_splash_show). So do not take the 424 KB decode buffer for it: ensure() allocates once and
    // the buffer is never freed. tests/test_splash_wiring.py pins that this sits before ensure().
    if (!(slug[0] && theme_style::hasAsset("splash.png"))) return false;

    if (!ensure()) { Serial.printf("[splash] PSRAM alloc failed\n"); return false; }

    // The theme's splash on the card.
    char path[64];
    snprintf(path, sizeof(path), "/themes/%s/splash.png", slug);
    size_t sdLen = 0;
    uint8_t *sdBuf = theme_sd::read_whole(path, sdLen, SD_SPLASH_MAX_BYTES);
    if (!sdBuf) return false;
    const uint32_t t0 = millis();
    const bool ok = try_decode(sdBuf, (uint32_t)sdLen);
    theme_sd::free(sdBuf);   // only the raw compressed bytes: s_buf (decoded RGB565) stays alive for the caller either way
    if (!ok) return false;
    Serial.printf("[splash] decoded from SD %s (%u bytes) in %u ms\n", path, (unsigned)sdLen, (unsigned)(millis() - t0));
```

Rewrite the comment at the top of the function's file (`splash_art.h`) to: `Tries, in order: a pre-baked flash copy of the active theme's splash.png, then that file on the microSD card. A theme with no splash of its own decodes nothing: ui_splash_show() then draws its background and the text lines.`

- [ ] **Step 3: Delete the art and verify**

```bash
git rm src/theme/graphics/splash_png_default.h src/theme/graphics/splash_png_office.h
```

Both builds succeed. `python3 -m unittest tests/test_splash_wiring.py -v` → OK. Shots (`shots/b10`), `shot_diff.py --skip-clock` against `before`: all `same` for all four themes (every shipped theme is in palette mode or has its own splash, so none of them used the compiled card). Only the startup splash and Settings > About touch this code, and neither is among the shots, so the check for them is the wiring test plus the hardware list.

- [ ] **Step 4: Commit**

```bash
git add src/theme/graphics/splash_art.h src/theme/graphics/splash_art.cpp tests/test_splash_wiring.py
git commit -m "Splash: no compiled card; a theme with no splash.png decodes nothing" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

### Task 11: The compiled bitmap fonts go, and a guard keeps compiled art out

**Files:**
- Create: `tests/test_no_compiled_art.py`
- Modify: `src/theme/custom/custom_menu.h`, `src/theme/custom/custom_radar.h`, `platformio.ini`
- Delete: `src/theme/custom/custom_menu_font1.c`, `custom_menu_font2.c`, `custom_menu_font3.c`, `custom_radar_font1.c`, `custom_radar_font2.c`, `custom_radar_font3.c`, `custom_radar_font4.c`, `custom_settings_font1.c`, `custom_font1.c`, `custom_font2.c`

- [ ] **Step 1: Write the guard (it must fail first)**

Create `tests/test_no_compiled_art.py`:

```python
"""Spec step 5: no compiled theme art or bitmap font remains in the firmware image. Rule 4: this is the shared check that
makes the deletion stick, rather than a note beside each file. It counts numeric table entries, so a renamed dial or a
re-pasted sprite is caught by what it is, not by what it is called."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'src'
# A numeric table entry: 0x1F, or 217. Ordinary code has a few dozen of these; a compiled picture has tens of thousands.
DATA_TOKEN = re.compile(r'(?<![\w.])(?:0x[0-9A-Fa-f]{1,4}|\d{1,3})\s*,')
LIMIT = 1000
KEPT = ('src/theme/fonts/',)   # the Inter size ladder that the text-size sliders index into


def data_tokens(text: str) -> int:
    return len(DATA_TOKEN.findall(text))


def strip_comments(text: str) -> str:
    return re.sub(r'//[^\n]*', '', re.sub(r'/\*.*?\*/', '', text, flags=re.S))


class NoCompiledArtTest(unittest.TestCase):
    def test_the_check_can_fail(self):
        self.assertGreater(data_tokens('static const uint8_t X[] = {' + ','.join(['1'] * 2000) + ',};'), LIMIT)
        self.assertLess(data_tokens('int a[] = {1, 2, 3};'), LIMIT)

    def test_no_file_under_app_or_theme_carries_a_data_table(self):
        big = {}
        for base in ('src/app', 'src/theme'):
            for p in (ROOT / base).rglob('*'):
                rel = p.relative_to(ROOT).as_posix()
                if p.suffix in ('.h', '.c', '.cpp') and not rel.startswith(KEPT):
                    n = data_tokens(p.read_text(encoding='utf-8', errors='replace'))
                    if n > LIMIT:
                        big[rel] = n
        self.assertEqual(big, {})

    def test_the_compiled_bitmap_fonts_are_gone(self):
        self.assertEqual(sorted(p.name for p in (SRC / 'theme' / 'custom').glob('custom_*font*.c')), [])
        for p in list(SRC.rglob('*.h')) + list(SRC.rglob('*.cpp')):
            self.assertNotRegex(strip_comments(p.read_text(encoding='utf-8', errors='replace')), r'extern const lv_font_t custom_',
                                f'{p.relative_to(ROOT)} declares a compiled bitmap font')

    def test_the_three_slots_that_had_a_bitmap_face_fall_back_to_montserrat(self):
        menu = strip_comments((SRC / 'theme' / 'custom' / 'custom_menu.h').read_text(encoding='utf-8'))
        radar = strip_comments((SRC / 'theme' / 'custom' / 'custom_radar.h').read_text(encoding='utf-8'))
        self.assertIn('#define CUSTOM_MENU_CURRENT_FONT (&lv_font_montserrat_44)', menu)
        self.assertIn('#define CUSTOM_RTEXT1_FONT (&lv_font_montserrat_28)', radar)
        self.assertIn('#define CUSTOM_RTEXT2_FONT (&lv_font_montserrat_20)', radar)

    def test_the_simulator_source_list_names_only_files_that_exist(self):
        """PlatformIO ignores a +<file> that is not there, so a stale entry never fails a build. Two already had."""
        ini = (ROOT / 'platformio.ini').read_text(encoding='utf-8')
        line = next(l for l in ini[ini.index('[env:native]'):].splitlines() if l.startswith('build_src_filter'))
        missing = [p for sign, p in re.findall(r'([+-])<([^>]+)>', line) if sign == '+' and not (SRC / p).exists()]
        self.assertEqual(missing, [])


if __name__ == '__main__':
    unittest.main()
```

Run `python3 -m unittest tests/test_no_compiled_art.py -v`: `test_the_compiled_bitmap_fonts_are_gone`, `test_the_three_slots...` and `test_the_simulator_source_list...` fail (the first names `theme/custom_font1.c` and `theme/custom_font2.c`); `test_no_file_under_app_or_theme...` passes if Tasks 8 to 10 removed everything, and if it lists a file, that file is compiled art the earlier tasks missed: deal with it now.

- [ ] **Step 2: Re-point the three macros**

`src/theme/custom/custom_menu.h`: delete the `#ifdef __cplusplus / extern "C" { / #endif / extern const lv_font_t custom_menu_font1; / #ifdef __cplusplus / } / #endif` block and change

```cpp
#define CUSTOM_MENU_CURRENT_FONT (&custom_menu_font1)
```

to

```cpp
#define CUSTOM_MENU_CURRENT_FONT (&lv_font_montserrat_44)
```

`src/theme/custom/custom_radar.h`: delete the same kind of `extern "C"` block holding `custom_radar_font1` and `custom_radar_font2`, and change `CUSTOM_RTEXT1_FONT` to `(&lv_font_montserrat_28)` and `CUSTOM_RTEXT2_FONT` to `(&lv_font_montserrat_20)`. Add one comment line above each changed macro: `// was a compiled 46 px bitmap face; Montserrat 44 is the nearest size (spec step 5)` and likewise 27 to 28 and 21 to 20.

- [ ] **Step 3: Delete the font sources and fix the list**

```bash
git rm src/theme/custom/custom_menu_font1.c src/theme/custom/custom_menu_font2.c src/theme/custom/custom_menu_font3.c src/theme/custom/custom_radar_font1.c src/theme/custom/custom_radar_font2.c src/theme/custom/custom_radar_font3.c src/theme/custom/custom_radar_font4.c src/theme/custom/custom_settings_font1.c src/theme/custom/custom_font1.c src/theme/custom/custom_font2.c
```

In `platformio.ini`'s `[env:native]` `build_src_filter`, delete exactly `+<theme/custom_font1.c> +<theme/custom_font2.c> +<theme/custom/custom_menu_font1.c> +<theme/custom/custom_menu_font2.c> +<theme/custom/custom_menu_font3.c> +<theme/custom/custom_radar_font1.c> +<theme/custom/custom_radar_font2.c> +<theme/custom/custom_radar_font3.c> +<theme/custom/custom_radar_font4.c> +<theme/custom/custom_settings_font1.c> ` (keep the five `font_inter_*` entries after it).

- [ ] **Step 4: Verify**

Both builds succeed (a link error `undefined reference to custom_..._font` means something still names a deleted face: find it with `grep -rn "custom_menu_font\|custom_radar_font" src` and re-point it). `python3 -m unittest tests/test_no_compiled_art.py tests/test_theme_font_coverage.py -v` → OK (the coverage test must not skip: `lv_font_conv` is available through `npx`; if it does skip, install it and rerun, because this is the test that proves the shipped themes still supply their own faces).

Then the shots (`shots/b11`) with `shot_diff.py --skip-clock` against `before`. **Expected differences, and only these:** in `default-menu.bmp` and `default-*` screens that draw the menu name or radar text, the lettering is Montserrat instead of the compiled face. Every `elegant`, `fallout` and `portal` shot should still be `same` *if* the sim loaded their baked faces (it does since plan 1, when `theme_font::begin()` was added to `sim_main.cpp`); a change there means a slot fell through to the fallback, which is the coverage guarantee failing. Read the changed default shots and check that the menu name still fits its wrap width and the radar lines do not overlap.

- [ ] **Step 5: Commit**

```bash
git add tests/test_no_compiled_art.py src/theme/custom/custom_menu.h src/theme/custom/custom_radar.h platformio.ini
git commit -m "Delete the compiled bitmap fonts; the three fallbacks are Montserrat; guard against compiled art" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

### Task 12: Part B closes: docs, pending list, version, measurement

**Files:**
- Modify: `docs/HARDWARE_PENDING.md`, `docs/theme-yaml.md`, `src/theme_assets/README.md`, `src/config.h:10`

- [ ] **Step 1: Measure**

```bash
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175
ls -l .pio/build/esp32-s3-amoled-175/firmware.bin
~/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-nm -S -C .pio/build/esp32-s3-amoled-175/firmware.elf | grep -i -E "DIAL_|SPLASH_PNG|OFFICE|HAND_.*IMG|CUSTOM_.*_PNG"
```

Expected: the summary's `Flash: ... used N bytes`, and `firmware.bin` **at most 4,211,152 bytes** (baseline 5,511,152 minus the spec's 1.3 MB); the `nm | grep` prints nothing. If the saving is short of 1.3 MB, stop: something was left in (list the largest remaining symbols with `nm -S --size-sort -C` and read the top of it). Record the two numbers.

- [ ] **Step 2: `docs/HARDWARE_PENDING.md`**

Correct the two statements this work changes: in "Task 5: role bindings and the built-in layout" change "until spec step 4 retires those skins" to "until the radar's own compiled skins are retired (that is separate work and is not part of spec step 4 or 5)"; and replace the bullet under "Task 10" that begins "With no theme selected the clock uses no compiled bitmap at all" with "The firmware holds no compiled clock image of any kind (spec step 5), so a brown plate or ornate hands can never come from flash." Then append:

```markdown
## Delete the compiled art and fonts (plan `2026-09-22-retire-app-skins-and-compiled-art.md`, Part B, spec step 5)

Built, host-tested and screenshot-compared in the simulator only. `firmware.bin` went from 5,511,152 to <MEASURED> bytes
(app slot 6,553,600).

- [ ] With no theme and no card the Orb boots, the clock draws, and the startup splash is the flat card with its lines.
- [ ] The built-in look's menu name and Flight Tracker text are now Montserrat 44, 28 and 20. The menu name wraps inside its
      width and the radar lines do not overlap.
- [ ] Elegant, Fallout and Portal keep their own lettering (their faces load from flash with no card).
- [ ] **Behaviour change for a user's own legacy theme (no `palette:`)**: with no `splash.png` it now shows its background
      and text lines only; with no `clock_hand_*.png` it shows no hands. Neither may crash or log a decode error.
- [ ] An OTA update onto an Orb still on 2.19.x installs (the image is smaller, so it fits the slot it fitted before).
- [ ] `?orb mem` before and after: PSRAM is unchanged apart from the splash, which no longer allocates for a theme with no card art.
```

Fill `<MEASURED>` with the number from Step 1.

- [ ] **Step 3: The docs Part B makes false**

`docs/theme-yaml.md`: in the fonts section change "otherwise the compiled face" to "otherwise LVGL's Montserrat: 44 px for the menu's current name, 28 and 20 px for the first two radar text lines, and the default size elsewhere". `src/theme_assets/README.md`: delete the last paragraph (the previews of the art compiled into the firmware; the tool and the art are gone). Do a final `grep -rn "compiled fallback\|dial_img\|render_theme_bitmaps\|Default/Office\|Office skin" docs CLAUDE.md README.md src/theme_assets/README.md` outside `docs/superpowers/`; each hit is either now true or gets fixed.

- [ ] **Step 4: Version and the whole suite**

`src/config.h`: `#define FW_VERSION "2.21.0"`. Run the four commands from Task 7 Step 3 in the background. Expected: **Ran 216 tests** (207 at the end of Part A, plus 3 new in `OneFaceTest`, net +1 in `test_clock_wiring.py` where a one-test class became a two-test class, and 5 in the new `test_no_compiled_art.py`; `test_splash_wiring.py` keeps its four), `OK`, **0 skipped**; host tests pass; both builds succeed; all shots `same` except the intended `default` lettering.

- [ ] **Step 5: Commit**

```bash
git add docs/HARDWARE_PENDING.md docs/theme-yaml.md src/theme_assets/README.md src/config.h
git commit -m "Delete the compiled art: pending hardware checks, docs, FW_VERSION 2.21.0" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

## Landing

### Task 13: Integration branch and hand-over

- [ ] **Step 1:** From this worktree, create the integration branch and merge both parts with merge commits:

```bash
git switch -c integrate/retire-skins-and-art main
git merge --no-ff feat/retire-app-skins -m "Merge branch 'feat/retire-app-skins': retire the Default/Office skins, and FW_VERSION 2.20.0" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
git merge --no-ff feat/delete-compiled-art -m "Merge branch 'feat/delete-compiled-art': delete the compiled art and fonts, and FW_VERSION 2.21.0" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

- [ ] **Step 2:** Run the whole suite once more on the integration tip (Task 7 Step 3's four commands) and report the counts and the measured `firmware.bin`.

- [ ] **Step 3:** Tell the owner the branch is ready and that moving `main` is theirs, from `/Users/geoffford/code/orb-os`: check `git log main -1` is still `333dd7d`, then `git merge --ff-only integrate/retire-skins-and-art`. Also tell them the empty branches `feat/remove-office` and `worktree-retire-default-office` can be deleted, and that nothing has run on an Orb.
