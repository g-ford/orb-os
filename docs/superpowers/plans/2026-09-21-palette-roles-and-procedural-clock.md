# Palette roles, the built-in default, and the procedural clock: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An Orb with no theme selected (or the reserved `default`) draws a coherent, palette-driven look with a procedurally drawn clock (a dial and hands, no images), and a new theme can be just four colours.

**Architecture:** A pure `theme_roles.h` defines the eleven colour roles, the one derivation function and the built-in palette. `theme_style` gains a palette mode: the palette comes from `theme.json` (or is the built-in one when no theme is active), `$role` references in the style JSON are resolved once in `read_style_json()`, and role-bound colour options get their defaults from a new `theme_palette.cpp` before the JSON overrides are applied. `app_theme::palette()` reads the roles. The clock draws a dial, ticks and hands from the roles for any element the theme ships no image for. Shipped themes carry palettes and resolve to exactly the values they had before.

**Tech Stack:** C++17 firmware (LVGL 8.4, ArduinoJson 7, PlatformIO at `~/.platformio/penv/bin/pio`), Python 3 + PyYAML (builder and tests), the desktop simulator (`pio run -e native`, `--themeshot`) for looking at the result.

**Spec:** `docs/superpowers/specs/2026-09-20-theme-palette-fonts-builtin-default-design.md` (sections "Design 2. Palette roles", "Design 3. The built-in `default` look", "Naming", "Migrating the shipped themes"; rollout steps 2 and 3). Read it first. This plan does **not** retire the Default/Office skins (step 4) or delete the compiled art and fonts (step 5); the built-in look simply stops *using* the compiled dial and splash.

## Decisions taken while planning

These settle things the spec left open or that the code showed to be different. Each is a deliberate call, recorded so it can be reversed cheaply.

1. **Role names are camelCase in files:** `onPrimary`, not `on-primary`. Every other JSON key in the theme is camelCase.
2. **`roleDefaults`.** A theme's `theme.json` may say `"roleDefaults": false`. A theme with a `palette` and no such key gets role-bound defaults for every colour option it does not state (this is what makes "four colours" enough). Setting it `false` keeps the firmware's compiled values for unstated options. **Portal, Fallout and Elegant migrate with `roleDefaults: false`**, because they state only what differs from the compiled values; binding by default would silently restyle them, and pinning ~150 omitted colours by hand would be error-prone. Their `$role` references and their palette still work. This replaces the spec's "the migrated YAML states any colour where the binding would differ".
3. **Elegant stays explicit.** It is the exhaustive reference and is regenerated from the values the firmware reads, so its colours remain hex. It gains a hand-written `palette:` block and `roleDefaults: false`, which the generator preserves (as it already does `fonts:`).
4. **`$role` is resolved once,** in `read_style_json()`, by rewriting matching string values to integers before any merge runs. The ~400 existing `is<uint32_t>()` reads then need no change.
5. **The built-in palette is a full explicit constant** (all 11 roles), equal to today's `APP_THEME_DEFAULT` palette plus `muted` and `alert`, so a legacy theme's `AppPalette` is unchanged. `src/theme_assets/default/theme.yaml` lists all 11 and a test keeps it equal to that constant.
6. **The procedural clock ticks once a second and does not sweep.** It applies **per element**: no plate gives a drawn dial; a hand with no image gives a drawn hand; images always win. It is active only in palette mode, so a legacy theme with no plate still draws as it does today.
7. **`default` is reserved.** A saved slug of `default` means the built-in look; a card folder named `default` is ignored and unlisted; Settings > Design gets a first row "Default". `build_all_themes.py` and the font-coverage test skip the `default` folder.
8. **`FW_VERSION` becomes `2.19.0`.** `2.18.0` was already taken by the touch-swipe release on `main`.

## Global Constraints

- `THEME_CAPS` goes 52 -> 53 with a ledger entry in `theme_style.h`; never renumber or reuse.
- Roles, in enum order: `bg`, `primary`, `secondary`, `text` (picked, required in a palette), then `muted`, `dim`, `hairline`, `panel`, `highlight`, `onPrimary`, `alert` (derived unless stated). One C++ function derives them; the builder does no colour arithmetic.
- Derivation starting values, all integer RGB mixes rounded to nearest: `muted` = `text` 45% toward `bg`; `dim` = `primary` 55% toward `bg`; `hairline` = `primary` 80% toward `bg`; `panel` = `bg` 6% toward `text`; `highlight` = `bg` 25% toward `primary`; `onPrimary` = whichever of `bg` or `text` contrasts more with `primary` (a tie picks `bg`); `alert` = `0xE5484D`. The plan tunes these by eye with `--themeshot`.
- Built-in palette (explicit): `bg 0x000000`, `primary 0x1DFF86`, `secondary 0x9AFFC8`, `text 0xEAFFF3`, `muted 0x818C86`, `dim 0x5F7A6C`, `hairline 0x1C2620`, `panel 0x0C160F`, `highlight 0x232A36`, `onPrimary 0x05100A`, `alert 0xE5484D`.
- **Palette mode applies only** when `theme.json` has a `palette`, or when no theme is active (built-in). A theme with neither resolves to exactly the values it resolves to today; a golden proves it.
- Compiled art, the Default/Office skins and the compiled fonts are **not** deleted here.
- Firmware is not done until it has booted on a real Orb. There is none yet, so every firmware task adds to `docs/HARDWARE_PENDING.md` and nothing is called verified.
- No `git add -A`; explicit paths. Test before fix in commit order. One branch per part, merged `--no-ff`: **Tasks 1 to 8 on `feat/palette-roles`; Tasks 9 to 12 on `feat/procedural-clock`**, in one worktree with the branch switched between parts so a single ledger survives.
- Every commit ends with the trailer `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>` (a second `-m`).
- A fresh worktree has no `.pio/`: copy `libdeps` from the main checkout. Run wide test suites in the background and read the log; they are slow.
- Keep shell commands plain (no inline multi-line Python, no `cd &&` chains): the tool guard refuses them. Put scripts in files.

## Review Focus

Failure modes the spec implies that no headline test would catch. Each has a test in the task named.

1. **A legacy theme must not move.** A theme with no `palette` resolves to byte-identical values before and after the firmware learns palettes (Tasks 1, 4, 5, 8: the resolved-state golden).
2. **A text value that looks like a role reference.** `"$5.00"` must stay text; `"$primary"` in a text field must be refused by the builder rather than silently turned into a colour; `"$$5"` is the escape (Task 3, Task 4).
3. **A bad palette.** A missing base role, an unknown role, a colour that is not hex, a `$` inside the palette, and a role reference in a theme with no palette are all refused with the key path, and a `palette` never crashes the firmware if `theme.json` is hand-edited to nonsense (Task 3, Task 4).
4. **A theme with a palette but no images must still draw a clock; a theme with images must not get drawn hands on top of its sprites** (Task 10).
5. **Light and low-contrast palettes.** `onPrimary` must stay readable on a light primary, and no derived role may overflow or go negative (Task 2).
6. **The built-in palette and `default/theme.yaml` must not drift,** and a four-colour theme built from that file must resolve exactly like the built-in look (Task 6).

## File Structure

| File | Responsibility |
|---|---|
| `src/theme/core/theme_roles.h` (new) | Pure: the role list, `Palette`, `mix`, `resolve` (derivation), `BUILT_IN`. |
| `src/theme/core/theme_palette.{h,cpp}` (new) | The role bindings: which colour option takes which role by default; the built-in layout. |
| `src/theme/core/theme_style.{h,cpp}` | Palette mode, `$role` resolution, accessors, `THEME_CAPS` 53. |
| `src/theme/core/theme_select.{h,cpp}` | The reserved `default` slug. |
| `src/app/common/app_theme.cpp` | `AppPalette` from the roles. |
| `src/app/clock/clock_face.h` (new) | Pure geometry for the drawn dial (ticks, hand shapes). |
| `src/app/clock/clock_view.cpp` | The drawn dial, hands and date, per missing element; no sweep. |
| `src/theme/graphics/splash_art.cpp`, `src/app/ui/ui.cpp` | Splash without compiled art in palette mode. |
| `tools/build_theme.py` | `palette:`, `roleDefaults`, `$role` validation and pass-through. |
| `tools/resolved_golden.py`, `tools/palettize.py` (new) | Resolved-state goldens; a report of repeated colours. |
| `tools/dump_theme_defaults.cpp`, `tools/gen_elegant_theme.py` | Dump the palette; preserve Elegant's `palette:`. |
| `src/theme_assets/default/theme.yaml` (new) | The built-in palette, written down. |
| `tests/theme_roles_test.cpp`, `tests/clock_face_test.cpp`, `tests/test_theme_palette.py`, `tests/test_theme_resolved_golden.py`, `tests/test_default_palette.py`, `tests/golden/resolved_*.json` (new) | Tests and goldens. |

---

## Part A: Roles and the built-in look (spec step 2)

### Task 1: Resolved-state goldens, captured before anything changes

**Files:**
- Create: `tools/resolved_golden.py`, `tests/test_theme_resolved_golden.py`, `tests/golden/resolved_elegant.json`, `tests/golden/resolved_fallout.json`, `tests/golden/resolved_portal.json`

**Interfaces:**
- Consumes: `gen_elegant_theme.build_dumper(out) -> Path`, `build_folder(theme_dir, out) -> Path`, `run_dumper(binary, built) -> dict`, `GenError`; `tools/dump_theme_defaults.cpp` (links the real `theme_style.cpp`).
- Produces: `resolved_golden.KEEP` (the ten section keys), `resolved_golden.resolve_theme(theme_dir: Path, dumper: Path | None = None) -> dict`, `resolved_golden.diff_paths(a, b) -> list[str]` (`path: a -> b` lines). CLI: `python3 tools/resolved_golden.py <theme dir> [--write FILE]`.

The three shipped themes are captured **as they are on `main` now**, in legacy mode. Later tasks change the firmware and then the themes; the goldens are the fixed target that proves nothing moved.

- [ ] **Step 1: Write the failing test**

`tests/test_theme_resolved_golden.py`:
```python
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import gen_elegant_theme as gen  # noqa: E402
import resolved_golden  # noqa: E402

THEMES = ROOT / 'src' / 'theme_assets'
GOLDEN = ROOT / 'tests' / 'golden'


class DiffPathsTest(unittest.TestCase):
    def test_it_names_the_path_and_both_values(self):
        a = {'radar': {'sweepColor': '0x111111', 'zones': [{'r': 1}]}, 'apps': {'clock': True}}
        b = {'radar': {'sweepColor': '0x222222', 'zones': [{'r': 2}]}, 'apps': {'clock': True}}
        self.assertEqual(resolved_golden.diff_paths(a, b),
                         ["radar.sweepColor: '0x111111' -> '0x222222'", 'radar.zones[0].r: 1 -> 2'])

    def test_equal_values_have_no_differences(self):
        self.assertEqual(resolved_golden.diff_paths({'a': [1, {'b': 2}]}, {'a': [1, {'b': 2}]}), [])

    def test_a_missing_key_is_a_difference(self):
        self.assertEqual(resolved_golden.diff_paths({'a': 1}, {}), ['a: 1 -> <missing>'])


class ResolvedGoldenTest(unittest.TestCase):
    """A shipped theme must resolve to exactly the values it resolved to when the golden was captured. Compiled
    once per class: building the dumper is the slow part."""

    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        try:
            cls.dumper = gen.build_dumper(Path(cls._tmp.name) / 'dump_theme_defaults')
        except gen.GenError as e:
            cls._tmp.cleanup()
            raise unittest.SkipTest(str(e).splitlines()[0])

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def check(self, slug):
        got = resolved_golden.resolve_theme(THEMES / slug, self.dumper)
        want = json.loads((GOLDEN / f'resolved_{slug}.json').read_text(encoding='utf-8'))
        self.assertEqual(resolved_golden.diff_paths(want, got), [],
                         f'{slug} resolves differently from the golden captured before palettes existed')

    def test_elegant(self):
        self.check('elegant')

    def test_fallout(self):
        self.check('fallout')

    def test_portal(self):
        self.check('portal')


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run to verify it fails**

```bash
python3 -m unittest tests/test_theme_resolved_golden.py 2>&1 | tail -4
```
Expected: ERROR `ModuleNotFoundError: No module named 'resolved_golden'`.

- [ ] **Step 3: Implement `tools/resolved_golden.py`**

```python
#!/usr/bin/env python3
"""What the firmware resolves a theme to: every option's value, as JSON.

    python3 tools/resolved_golden.py src/theme_assets/portal
    python3 tools/resolved_golden.py src/theme_assets/portal --write tests/golden/resolved_portal.json

The theme is built with tools/build_theme.py and read by tools/dump_theme_defaults.cpp, which links the real
theme_style.cpp, so the numbers are what the firmware computes and not a copy of them. Only the theme's own
sections are kept, so the file does not move when the firmware learns a new top-level key (the `palette` the
dumper now reports). Used to prove a theme still resolves to exactly the same values after it moves to a palette.
"""
from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_elegant_theme as gen  # noqa: E402

KEEP = ('apps', 'names', 'clock', 'radar', 'weather', 'ticker', 'settings', 'menu', 'splash', 'intel')


def resolve_theme(theme_dir: Path, dumper: Path | None = None) -> dict:
    """Build `theme_dir` and return the kept sections of what the firmware makes of it. Pass a `dumper` binary to
    reuse one build across several themes."""
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        binary = dumper or gen.build_dumper(tmp / 'dump_theme_defaults')
        built = gen.build_folder(theme_dir, tmp / 'built')
        state = gen.run_dumper(binary, built)
    return {k: state[k] for k in KEEP}


def diff_paths(a, b, where: str = '') -> list:
    """Every place `a` and `b` differ, as `path: a -> b`, in a stable order."""
    out = []
    if isinstance(a, dict) and isinstance(b, dict):
        for k in sorted(set(a) | set(b)):
            path = f'{where}.{k}' if where else str(k)
            if k not in a:
                out.append(f'{path}: <missing> -> {b[k]!r}')
            elif k not in b:
                out.append(f'{path}: {a[k]!r} -> <missing>')
            else:
                out += diff_paths(a[k], b[k], path)
    elif isinstance(a, list) and isinstance(b, list) and len(a) == len(b):
        for i, (x, y) in enumerate(zip(a, b)):
            out += diff_paths(x, y, f'{where}[{i}]')
    elif a != b:
        out.append(f'{where}: {a!r} -> {b!r}')
    return out


def main(argv) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('theme', type=Path, help='a theme source folder (holds theme.yaml)')
    ap.add_argument('--write', type=Path, metavar='FILE', help='write the JSON here instead of printing it')
    args = ap.parse_args(argv[1:])
    try:
        text = json.dumps(resolve_theme(args.theme), indent=1, sort_keys=True) + '\n'
    except gen.GenError as e:
        print(f'error: {e}', file=sys.stderr)
        return 1
    if args.write:
        args.write.parent.mkdir(parents=True, exist_ok=True)
        args.write.write_text(text, encoding='utf-8')
        print(f'wrote {args.write}')
    else:
        sys.stdout.write(text)
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
```

- [ ] **Step 4: Capture the goldens from the themes as they are now**

```bash
python3 tools/resolved_golden.py src/theme_assets/elegant --write tests/golden/resolved_elegant.json
python3 tools/resolved_golden.py src/theme_assets/fallout --write tests/golden/resolved_fallout.json
python3 tools/resolved_golden.py src/theme_assets/portal --write tests/golden/resolved_portal.json
wc -l tests/golden/resolved_*.json
```
Expected: three `wrote ...` lines and non-trivial line counts (hundreds each). Fallout and Portal bake faces with `lv_font_conv`, so this needs `npx`.

- [ ] **Step 5: Run the tests**

```bash
python3 -m unittest tests/test_theme_resolved_golden.py 2>&1 | tail -3
```
Expected: `OK`.

- [ ] **Step 6: Commit**

```bash
git add tools/resolved_golden.py tests/test_theme_resolved_golden.py tests/golden/resolved_elegant.json tests/golden/resolved_fallout.json tests/golden/resolved_portal.json
git commit -m "Capture each shipped theme's resolved state before palettes exist" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 2: The roles, their derivation, and the built-in palette (`theme_roles.h`)

**Files:**
- Create: `src/theme/core/theme_roles.h`, `tests/theme_roles_test.cpp`, `tests/run_theme_roles_test.sh`
- Modify: `tests/run_host_tests.sh`

**Interfaces:**
- Produces (namespace `theme_roles`):
  - `THEME_ROLE_LIST(X)`: the macro the builder also reads.
  - `enum Role : uint8_t { R_bg, R_primary, R_secondary, R_text, R_muted, R_dim, R_hairline, R_panel, R_highlight, R_onPrimary, R_alert, ROLE_COUNT }`; `constexpr int BASE_ROLES = 4`; `const char *const NAMES[ROLE_COUNT]`.
  - `struct Palette { uint32_t v[ROLE_COUNT]; }`; `struct Input { uint32_t v[ROLE_COUNT]; bool set[ROLE_COUNT]; }`.
  - `int find_role(const char *name)` (index, or -1); `uint32_t mix(uint32_t a, uint32_t b, int pct)`; `int luma(uint32_t)`; `uint32_t on_colour(uint32_t fill, uint32_t a, uint32_t b)`; `void input_clear(Input &)`; `Palette resolve(const Input &)`; `inline constexpr Palette BUILT_IN`.

- [ ] **Step 1: Write the failing test**

`tests/theme_roles_test.cpp` (the expected values are computed independently, not by the formula under test):
```cpp
// Host test for src/theme/core/theme_roles.h.   tests/run_theme_roles_test.sh
#include "theme_roles.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

using namespace theme_roles;

static Input pick(uint32_t bg, uint32_t primary, uint32_t secondary, uint32_t text) {
    Input in;
    input_clear(in);
    in.v[R_bg] = bg;               in.set[R_bg] = true;
    in.v[R_primary] = primary;     in.set[R_primary] = true;
    in.v[R_secondary] = secondary; in.set[R_secondary] = true;
    in.v[R_text] = text;           in.set[R_text] = true;
    return in;
}

static void the_roles_are_the_eleven_the_builder_reads() {
    assert(ROLE_COUNT == 11);
    assert(strcmp(NAMES[R_bg], "bg") == 0);
    assert(strcmp(NAMES[R_onPrimary], "onPrimary") == 0);
    assert(strcmp(NAMES[R_alert], "alert") == 0);
    assert(BASE_ROLES == 4 && R_text == 3 && R_muted == 4);          // the four a theme picks come first
    assert(find_role("primary") == R_primary);
    assert(find_role("onPrimary") == R_onPrimary);
    assert(find_role("nope") == -1);
    assert(find_role("") == -1);
    assert(find_role("Primary") == -1);                              // case matters: it is a JSON key
}

static void mix_goes_from_a_to_b_per_channel() {
    assert(mix(0x102030, 0xA0B0C0, 0) == 0x102030);
    assert(mix(0x102030, 0xA0B0C0, 100) == 0xA0B0C0);
    assert(mix(0x000000, 0xFFFFFF, 50) == 0x808080);                 // (255*50 + 50) / 100 = 128
    assert(mix(0x102030, 0xA0B0C0, -5) == 0x102030);                 // clamped, never negative
    assert(mix(0x102030, 0xA0B0C0, 150) == 0xA0B0C0);                // clamped, never past b
    assert(mix(0xFFFFFF, 0xFFFFFF, 37) == 0xFFFFFF);                 // no channel can overflow
}

// Portal's four colours; the expected numbers come from an independent calculation.
static void a_dark_palette_derives_the_rest() {
    const Palette p = resolve(pick(0x0B0E11, 0xFF9A1F, 0x82CEFF, 0xFFFFFF));
    assert(p.v[R_bg] == 0x0B0E11 && p.v[R_primary] == 0xFF9A1F);
    assert(p.v[R_secondary] == 0x82CEFF && p.v[R_text] == 0xFFFFFF);
    assert(p.v[R_muted] == 0x919394);
    assert(p.v[R_dim] == 0x794D17);
    assert(p.v[R_hairline] == 0x3C2A14);
    assert(p.v[R_panel] == 0x1A1C1F);
    assert(p.v[R_highlight] == 0x483115);
    assert(p.v[R_onPrimary] == 0x0B0E11);                            // dark on orange
    assert(p.v[R_alert] == 0xE5484D);
}

// Review focus 5: a light theme. Derivation mixes toward bg, so it stays inside the palette.
static void a_light_palette_stays_readable() {
    const Palette p = resolve(pick(0xF4F5F7, 0x3B5BFF, 0x6E6E73, 0x1C1C1E));
    assert(p.v[R_muted] == 0x7D7E80);
    assert(p.v[R_dim] == 0xA1B0FB);
    assert(p.v[R_hairline] == 0xCFD6F9);
    assert(p.v[R_panel] == 0xE7E8EA);
    assert(p.v[R_highlight] == 0xC6CFF9);
    assert(p.v[R_onPrimary] == 0xF4F5F7);                            // light text on a mid-blue primary
}

static void on_colour_picks_the_one_that_contrasts_more() {
    assert(on_colour(0xFFFF00, 0x000000, 0xFFFFFF) == 0x000000);     // yellow: dark text
    assert(on_colour(0x101010, 0x000000, 0xFFFFFF) == 0xFFFFFF);     // near black: light text
    assert(on_colour(0x808080, 0x000000, 0xFFFFFF) == 0x000000);     // dead centre: a tie goes to the first
    assert(luma(0x808080) == 128 && luma(0x000000) == 0 && luma(0xFFFFFF) == 255);
}

static void a_stated_derived_role_beats_the_derivation() {
    Input in = pick(0x0B0E11, 0xFF9A1F, 0x82CEFF, 0xFFFFFF);
    in.v[R_muted] = 0x123456; in.set[R_muted] = true;
    in.v[R_alert] = 0xFF0000; in.set[R_alert] = true;
    const Palette p = resolve(in);
    assert(p.v[R_muted] == 0x123456 && p.v[R_alert] == 0xFF0000);
    assert(p.v[R_dim] == 0x794D17);                                  // the others still derive
}

static void a_base_role_left_out_falls_back_to_the_built_in_one() {
    Input in;
    input_clear(in);
    in.v[R_primary] = 0xFF9A1F; in.set[R_primary] = true;
    const Palette p = resolve(in);
    assert(p.v[R_primary] == 0xFF9A1F);
    assert(p.v[R_bg] == BUILT_IN.v[R_bg] && p.v[R_text] == BUILT_IN.v[R_text]);
    assert(p.v[R_secondary] == BUILT_IN.v[R_secondary]);
}

// The built-in palette is written out in full and equals today's night-vision AppPalette (see app_theme.cpp), so a
// theme with no palette of its own keeps the colours every screen has always had.
static void the_built_in_palette_is_todays_default() {
    assert(BUILT_IN.v[R_bg] == 0x000000 && BUILT_IN.v[R_primary] == 0x1DFF86);
    assert(BUILT_IN.v[R_secondary] == 0x9AFFC8 && BUILT_IN.v[R_text] == 0xEAFFF3);
    assert(BUILT_IN.v[R_muted] == 0x818C86 && BUILT_IN.v[R_dim] == 0x5F7A6C);
    assert(BUILT_IN.v[R_hairline] == 0x1C2620 && BUILT_IN.v[R_panel] == 0x0C160F);
    assert(BUILT_IN.v[R_highlight] == 0x232A36 && BUILT_IN.v[R_onPrimary] == 0x05100A);
    assert(BUILT_IN.v[R_alert] == 0xE5484D);
    for (int r = 0; r < ROLE_COUNT; ++r) assert(BUILT_IN.v[r] <= 0xFFFFFF);
}

int main() {
    the_roles_are_the_eleven_the_builder_reads();
    mix_goes_from_a_to_b_per_channel();
    a_dark_palette_derives_the_rest();
    a_light_palette_stays_readable();
    on_colour_picks_the_one_that_contrasts_more();
    a_stated_derived_role_beats_the_derivation();
    a_base_role_left_out_falls_back_to_the_built_in_one();
    the_built_in_palette_is_todays_default();
    printf("theme_roles: all tests passed\n");
    return 0;
}
```

`tests/run_theme_roles_test.sh`:
```bash
#!/bin/bash
# Builds and runs tests/theme_roles_test.cpp on the host. Needs no libraries.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc/theme/core tests/theme_roles_test.cpp -o "$OUT/theme_roles_test"
"$OUT/theme_roles_test"
```
Add to `tests/run_host_tests.sh` after the `theme bake policy` line:
```bash
run "theme roles"                        bash tests/run_theme_roles_test.sh
```

- [ ] **Step 2: Run to verify it fails**

```bash
chmod +x tests/run_theme_roles_test.sh
bash tests/run_theme_roles_test.sh 2>&1 | tail -3
```
Expected: FAIL to compile, `theme_roles.h: No such file or directory`.

- [ ] **Step 3: Implement `src/theme/core/theme_roles.h`**

```cpp
#pragma once
// Colour roles: the palette a theme picks, and the one place the rest is derived from it.
//
// A theme picks four colours (bg, primary, secondary, text); the other seven are mixes of those unless the theme
// states them. Every screen's colour options default to a role, so a theme that is only a palette still looks
// designed. Pure logic, no LVGL: tests/theme_roles_test.cpp runs it on the desktop, and tools/build_theme.py
// reads THEME_ROLE_LIST below for the names it accepts, so the builder cannot drift from the firmware.
//
// The mixing lives here and nowhere else. The builder does no colour arithmetic, on purpose: two languages
// computing "dim" would disagree in the last bit and nobody would know which was right.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// The order matters: the four a theme picks come first. Keep the X(...) entries one per name; the builder
// finds them with a regular expression.
#define THEME_ROLE_LIST(X) \
    X(bg) X(primary) X(secondary) X(text) \
    X(muted) X(dim) X(hairline) X(panel) X(highlight) X(onPrimary) X(alert)

namespace theme_roles {

enum Role : uint8_t {
#define X(n) R_##n,
    THEME_ROLE_LIST(X)
#undef X
    ROLE_COUNT
};

constexpr int BASE_ROLES = 4;   // bg, primary, secondary, text: what a theme picks

const char *const NAMES[ROLE_COUNT] = {
#define X(n) #n,
    THEME_ROLE_LIST(X)
#undef X
};

struct Palette { uint32_t v[ROLE_COUNT]; };
struct Input   { uint32_t v[ROLE_COUNT]; bool set[ROLE_COUNT]; };   // what a theme states

// The role with this exact name, or -1. Case matters: the names are JSON keys.
inline int find_role(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < ROLE_COUNT; ++i)
        if (strcmp(NAMES[i], name) == 0) return i;
    return -1;
}

// `pct` percent of the way from a to b, per channel, rounded to nearest. pct is clamped to 0..100 so a bad value
// can never push a channel past either end.
inline uint32_t mix(uint32_t a, uint32_t b, int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    uint32_t out = 0;
    for (int sh = 16; sh >= 0; sh -= 8) {
        const int ca = (int)((a >> sh) & 0xFF), cb = (int)((b >> sh) & 0xFF);
        out |= (uint32_t)((ca * (100 - pct) + cb * pct + 50) / 100) << sh;
    }
    return out;
}

// Perceived brightness, 0..255 (Rec. 601 weights, no gamma: this only has to rank two colours).
inline int luma(uint32_t c) {
    return (int)((((c >> 16) & 0xFF) * 299 + ((c >> 8) & 0xFF) * 587 + (c & 0xFF) * 114) / 1000);
}

// Whichever of a and b contrasts more with `fill`; a tie goes to a.
inline uint32_t on_colour(uint32_t fill, uint32_t a, uint32_t b) {
    const int d = luma(fill);
    return abs(luma(a) - d) >= abs(luma(b) - d) ? a : b;
}

inline void input_clear(Input &in) {
    memset(in.v, 0, sizeof(in.v));
    memset(in.set, 0, sizeof(in.set));
}

// What an Orb shows when no theme is selected, and what a base role a theme leaves out falls back to. All eleven
// written out: the night-vision green set every screen has always had (it equals APP_THEME_DEFAULT in
// app_theme.cpp), so a theme with no palette of its own is unchanged. src/theme_assets/default/theme.yaml lists the
// same eleven and a test keeps the two equal.
inline constexpr Palette BUILT_IN = {{
    0x000000,   // bg
    0x1DFF86,   // primary
    0x9AFFC8,   // secondary
    0xEAFFF3,   // text
    0x818C86,   // muted
    0x5F7A6C,   // dim
    0x1C2620,   // hairline
    0x0C160F,   // panel
    0x232A36,   // highlight
    0x05100A,   // onPrimary
    0xE5484D,   // alert
}};

// The palette a theme resolves to. The four base roles are taken from `in`, or from BUILT_IN when left out; the
// other seven derive from them unless `in` states them. Starting ratios, tuned by eye with --themeshot.
inline Palette resolve(const Input &in) {
    Palette p = BUILT_IN;
    for (int r = 0; r < BASE_ROLES; ++r)
        if (in.set[r]) p.v[r] = in.v[r] & 0xFFFFFF;
    const uint32_t bg = p.v[R_bg], pr = p.v[R_primary], tx = p.v[R_text];
    p.v[R_muted]     = mix(tx, bg, 45);
    p.v[R_dim]       = mix(pr, bg, 55);
    p.v[R_hairline]  = mix(pr, bg, 80);
    p.v[R_panel]     = mix(bg, tx, 6);
    p.v[R_highlight] = mix(bg, pr, 25);
    p.v[R_onPrimary] = on_colour(pr, bg, tx);
    p.v[R_alert]     = 0xE5484D;
    for (int r = BASE_ROLES; r < ROLE_COUNT; ++r)
        if (in.set[r]) p.v[r] = in.v[r] & 0xFFFFFF;
    return p;
}

} // namespace theme_roles
```

- [ ] **Step 4: Run to verify it passes**

```bash
bash tests/run_theme_roles_test.sh 2>&1 | tail -3
```
Expected: `theme_roles: all tests passed`, no compiler warnings.

- [ ] **Step 5: Commit**

```bash
git add src/theme/core/theme_roles.h tests/theme_roles_test.cpp tests/run_theme_roles_test.sh tests/run_host_tests.sh
git commit -m "Add theme_roles.h: the colour roles, their derivation and the built-in palette" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 3: The builder understands `palette:`, `roleDefaults` and `$role`

**Files:**
- Modify: `tools/build_theme.py`
- Test: `tests/test_build_theme.py` (append a class)

**Interfaces:**
- Consumes: the role list in `theme_roles.h` (Task 2), read as the `X(name)` entries of `THEME_ROLE_LIST`.
- Produces: `firmware_facts()["roles"]` (the ordered list of role names); `theme.json["palette"]`, a mapping of each *stated* role to an integer; `theme.json["roleDefaults"]` (bool, when stated); role references written by a theme as `$primary` are passed through in the style JSON as the string `"$primary"` for the firmware to resolve. `$$x` is written as the text `$x`.

- [ ] **Step 1: Write the failing tests**

Append to `tests/test_build_theme.py`, above `if __name__ == '__main__':`:
```python
PALETTE_YAML = """slug: sample
palette:
  bg: 0x0B0E11
  primary: 0xFF9A1F
  secondary: #82CEFF
  text: 0xFFFFFF
radar:
  sweepColor: $secondary
ticker:
  upColor: 0x1FA2FF
"""


class PaletteBuildTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = Path(self._tmp.name)
        self.src = self.tmp / 'sample'
        self.src.mkdir()
        self.out = self.tmp / 'out'

    def write(self, yaml_text):
        (self.src / 'theme.yaml').write_text(yaml_text, encoding='utf-8')

    def built(self, name):
        return json.loads((self.out / 'sample' / name).read_text(encoding='utf-8'))

    def refused(self, yaml_text, needle):
        self.write(yaml_text)
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 1, msg=result.stdout)
        self.assertIn(needle, result.stderr)
        self.assertNotIn('Traceback', result.stderr)
        return result

    def test_the_palette_reaches_theme_json_as_integers(self):
        self.write(PALETTE_YAML)
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertEqual(self.built('theme.json')['palette'],
                         {'bg': 0x0B0E11, 'primary': 0xFF9A1F, 'secondary': 0x82CEFF, 'text': 0xFFFFFF})

    def test_a_role_reference_passes_through_as_a_string_and_hex_stays_a_number(self):
        self.write(PALETTE_YAML)
        run(self.src, self.out)
        self.assertEqual(self.built('radar_style.json')['sweepColor'], '$secondary')
        self.assertEqual(self.built('ticker_style.json')['upColor'], 0x1FA2FF)

    def test_role_references_work_inside_lists_and_flow_mappings(self):
        self.write(PALETTE_YAML + 'menu:\n  current: {color: $text, glowColor: $primary}\n'
                                  'settings:\n  hlColor: $primary\n')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertEqual(self.built('menu_style.json')['current'], {'color': '$text', 'glowColor': '$primary'})
        self.assertEqual(self.built('settings_style.json')['hlColor'], '$primary')

    def test_a_palette_may_state_a_derived_role(self):
        self.write(PALETTE_YAML.replace('  text: 0xFFFFFF\n', '  text: 0xFFFFFF\n  muted: 0x123456\n  onPrimary: 0x000000\n'))
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertEqual(self.built('theme.json')['palette']['muted'], 0x123456)
        self.assertEqual(self.built('theme.json')['palette']['onPrimary'], 0)

    def test_a_missing_base_role_is_an_error_naming_it(self):
        self.refused(PALETTE_YAML.replace('  text: 0xFFFFFF\n', ''), 'needs text')

    def test_an_unknown_role_in_the_palette_is_an_error_that_lists_the_real_ones(self):
        result = self.refused(PALETTE_YAML.replace('  text: 0xFFFFFF\n', '  text: 0xFFFFFF\n  accent: 0x123456\n'),
                              'palette.accent')
        self.assertIn('onPrimary', result.stderr)

    def test_a_palette_colour_must_be_hex(self):
        self.refused(PALETTE_YAML.replace('0xFF9A1F', 'orange'), 'palette.primary')

    def test_a_role_reference_inside_the_palette_is_an_error(self):
        self.refused(PALETTE_YAML.replace('0xFF9A1F', '$text'), 'palette.primary')

    def test_an_unknown_role_reference_is_an_error_with_its_path(self):
        self.refused(PALETTE_YAML.replace('$secondary', '$nope'), 'radar.sweepColor')

    def test_a_role_reference_needs_a_palette(self):
        self.refused('slug: sample\nradar:\n  sweepColor: $primary\n', 'needs a palette')

    def test_a_dollar_amount_is_just_text(self):
        self.write(PALETTE_YAML + 'menu:\n  current: {fmt: "$5.00 {name}"}\n  prev: {fmt: "$5"}\n')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertEqual(self.built('menu_style.json')['current']['fmt'], '$5.00 {name}')
        self.assertEqual(self.built('menu_style.json')['prev']['fmt'], '$5')

    def test_double_dollar_is_a_literal_dollar(self):
        self.write(PALETTE_YAML + 'menu:\n  current: {fmt: "$$5"}\n')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertEqual(self.built('menu_style.json')['current']['fmt'], '$5')

    def test_a_literal_that_would_read_as_a_role_is_refused(self):
        self.refused(PALETTE_YAML + 'menu:\n  current: {fmt: "$$primary"}\n', 'would be read as')

    def test_role_defaults_is_carried_through(self):
        self.write(PALETTE_YAML + 'roleDefaults: false\n')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIs(self.built('theme.json')['roleDefaults'], False)

    def test_role_defaults_without_a_palette_warns(self):
        self.write('slug: sample\nroleDefaults: false\n')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn('roleDefaults', result.stderr)

    def test_role_defaults_must_be_a_boolean(self):
        self.refused(PALETTE_YAML + 'roleDefaults: maybe\n', 'roleDefaults')

    def test_no_palette_builds_exactly_as_before(self):
        self.write('slug: sample\nradar:\n  rangeKm: 30\n')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertNotIn('palette', self.built('theme.json'))
        self.assertNotIn('roleDefaults', self.built('theme.json'))

    def test_the_role_names_come_from_the_firmware_header(self):
        sys.path.insert(0, str(ROOT / 'tools'))
        import build_theme
        self.assertEqual(build_theme.firmware_facts()['roles'],
                         ['bg', 'primary', 'secondary', 'text', 'muted', 'dim', 'hairline', 'panel', 'highlight',
                          'onPrimary', 'alert'])
```

- [ ] **Step 2: Run to verify they fail**

```bash
python3 -m unittest tests.test_build_theme.PaletteBuildTest > /tmp/t3-red.log 2>&1; tail -4 /tmp/t3-red.log
```
Expected: FAILED, most tests failing because the top-level key `palette` is refused; the two "unchanged" tests (`no_palette_builds_exactly_as_before`, and a refusal that happens to match) may pass.

- [ ] **Step 3: Implement in `tools/build_theme.py`**

Put this in a script file `patch_build_theme.py` (not committed), run it once, then delete it. Every replacement asserts it matched exactly once.
```python
import pathlib
import re

p = pathlib.Path('tools/build_theme.py')
s = p.read_text(encoding='utf-8')


def sub(old, new):
    global s
    assert s.count(old) == 1, (s.count(old), old[:70])
    s = s.replace(old, new)


# roleDefaults is a manifest key, like apps and names
sub("MANIFEST_KEYS = ('slug', 'name', 'author', 'version', 'default', 'apps', 'names')",
    "MANIFEST_KEYS = ('slug', 'name', 'author', 'version', 'default', 'apps', 'names', 'roleDefaults')")

sub("FACE_KEYS = {'src', 'size', 'ranges'}\n", """FACE_KEYS = {'src', 'size', 'ranges'}
PALETTE_KEY = 'palette'
BASE_ROLES = ('bg', 'primary', 'secondary', 'text')     # a theme picks these; the rest derive (theme_roles.h)
ROLE_REF = re.compile(r'^\\$([A-Za-z_][A-Za-z0-9_]*)$')  # "$primary": a reference the firmware resolves
""")

# the role list, read out of the firmware header
sub("    font, style = read(CORE / 'theme_font.cpp'), read(CORE / 'theme_style.cpp')\n",
    "    font, style = read(CORE / 'theme_font.cpp'), read(CORE / 'theme_style.cpp')\n"
    "    roles_h = read(CORE / 'theme_roles.h')\n")
sub("""    slots = {n[len('font_'):-len('.bin')] for n in fonts}
    return {'images': images, 'fonts': fonts, 'slots': slots, 'aliases': aliases,
            'max_json': int(limit.group(1)), 'keys': keys}""", """    slots = {n[len('font_'):-len('.bin')] for n in fonts}
    block = re.search(r'#define THEME_ROLE_LIST\\(X\\)(.*?)\\n\\n', roles_h, re.S)
    roles = re.findall(r'X\\((\\w+)\\)', block.group(1)) if block else []
    if tuple(roles[:len(BASE_ROLES)]) != BASE_ROLES:
        raise BuildError('could not read THEME_ROLE_LIST out of src/theme/core/theme_roles.h; '
                         'the firmware source has changed shape and this script needs updating')
    return {'images': images, 'fonts': fonts, 'slots': slots, 'roles': roles, 'aliases': aliases,
            'max_json': int(limit.group(1)), 'keys': keys}""")

# the two new functions, above the build banner
FUNCS = '''def build_palette(block, roles: list) -> dict:
    """Validate the `palette:` block and return {role: int}. A theme picks the four base roles and may state any
    other; nothing else is a role. Values are colours, never references: a palette that pointed at itself would
    have no answer."""
    if not isinstance(block, dict):
        raise BuildError('palette: must be a mapping of role names to colours')
    out = {}
    for name, value in block.items():
        if name not in roles:
            raise BuildError(f'palette.{name}: not a role (roles: {", ".join(roles)})')
        if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= 0xFFFFFF:
            raise BuildError(f'palette.{name}: must be a colour written 0xRRGGBB or #RRGGBB, not {value!r} '
                             f'(a palette states colours; only the sections use $role)')
        out[name] = value
    missing = [r for r in BASE_ROLES if r not in out]
    if missing:
        raise BuildError(f'palette: needs {", ".join(missing)} (a theme picks bg, primary, secondary and text; '
                         f'the other roles are derived)')
    return out


def check_refs(node, where: str, roles: list, has_palette: bool):
    """Check every role reference in a section and return what to write. `$primary` is passed through as it is, for
    the firmware to resolve; `$$x` is the text `$x`. Anything else that starts with `$` (a price, say) is plain text."""
    if isinstance(node, dict):
        return {k: check_refs(v, f'{where}.{k}', roles, has_palette) for k, v in node.items()}
    if isinstance(node, list):
        return [check_refs(v, f'{where}[{i}]', roles, has_palette) for i, v in enumerate(node)]
    if isinstance(node, str) and node.startswith('$'):
        if node.startswith('$$'):
            text = node[1:]
            m = ROLE_REF.match(text)
            if m and m.group(1) in roles:
                raise BuildError(f'{where}: {node!r} would be read as the role reference {text}; change the text')
            return text
        m = ROLE_REF.match(node)
        if m:
            if m.group(1) not in roles:
                raise BuildError(f'{where}: {node} is not a role (roles: {", ".join(roles)})')
            if not has_palette:
                raise BuildError(f'{where}: {node} needs a palette: block')
    return node


'''
i = s.index('# ---- build ---')
s = s[:i] + FUNCS + s[i:]

# inside _build
sub("    font_block = data.pop(FONTS_KEY, None)\n",
    "    font_block = data.pop(FONTS_KEY, None)\n    palette_block = data.pop(PALETTE_KEY, None)\n")
sub("""    for section in SECTIONS:
        if section in data and not isinstance(data[section], dict):
            raise BuildError(f'{section}: must be a mapping of options')
""", """    for section in SECTIONS:
        if section in data and not isinstance(data[section], dict):
            raise BuildError(f'{section}: must be a mapping of options')
    roles = facts['roles']
    palette = build_palette(palette_block, roles) if palette_block is not None else None
    for section in SECTIONS:
        if section in data:
            data[section] = check_refs(data[section], section, roles, palette is not None)
    if 'roleDefaults' in data:
        if not isinstance(data['roleDefaults'], bool):
            raise BuildError('roleDefaults: must be true or false')
        if palette is None:
            warnings.append('roleDefaults: has no effect without a palette:')
""")
sub("    if font_map:\n        theme['fonts'] = font_map\n",
    "    if font_map:\n        theme['fonts'] = font_map\n    if palette:\n        theme['palette'] = palette\n")
p.write_text(s, encoding='utf-8')
print('patched')
```
Run it:
```bash
python3 patch_build_theme.py
rm patch_build_theme.py
```

- [ ] **Step 4: Run the tests**

```bash
python3 -m unittest tests/test_build_theme.py tests/test_font_bake.py tests/test_build_all_themes.py > /tmp/t3-green.log 2>&1; tail -4 /tmp/t3-green.log
```
Expected: `OK`.

- [ ] **Step 5: Commit**

```bash
git add tools/build_theme.py tests/test_build_theme.py
git commit -m "Builder: a palette block, roleDefaults, and \$role references" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 4: `theme_style` gets a palette mode; `$role` resolves; the dumper reports it

**Files:**
- Modify: `src/theme/core/theme_style.h`, `src/theme/core/theme_style.cpp`, `tools/dump_theme_defaults.cpp`
- Create: `tests/test_theme_palette.py`
- Modify: `docs/HARDWARE_PENDING.md`

**Interfaces:**
- Consumes: `theme_roles::{Palette, Input, resolve, find_role, NAMES, ROLE_COUNT, BUILT_IN}` (Task 2); the built theme.json of Task 3.
- Produces (namespace `theme_style`): `enum class PaletteMode : uint8_t { Legacy, Theme, BuiltIn }`; `PaletteMode paletteMode()`; `bool roleDefaults()`; `const theme_roles::Palette &palette()`. The dumper's JSON gains a top-level `palette`: `{ "mode": "legacy"|"theme"|"builtin", "roleDefaults": bool, "<role>": "0xRRGGBB", ... }`.
- Behaviour: no theme active -> `BuiltIn`; theme.json has a `palette` object -> `Theme`; otherwise `Legacy`. In `Theme` and `BuiltIn` mode a string value `"$<role>"` anywhere in a style file resolves to that role's colour before any read. Role-bound defaults arrive in Task 5; this task only wires the mode, the palette and the references.

- [ ] **Step 1: Write the failing tests**

`tests/test_theme_palette.py`:
```python
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import gen_elegant_theme as gen  # noqa: E402

BUILT_IN = {'bg': '0x000000', 'primary': '0x1DFF86', 'secondary': '0x9AFFC8', 'text': '0xEAFFF3',
            'muted': '0x818C86', 'dim': '0x5F7A6C', 'hairline': '0x1C2620', 'panel': '0x0C160F',
            'highlight': '0x232A36', 'onPrimary': '0x05100A', 'alert': '0xE5484D'}
PORTAL_DERIVED = {'bg': '0x0B0E11', 'primary': '0xFF9A1F', 'secondary': '0x82CEFF', 'text': '0xFFFFFF',
                  'muted': '0x919394', 'dim': '0x794D17', 'hairline': '0x3C2A14', 'panel': '0x1A1C1F',
                  'highlight': '0x483115', 'onPrimary': '0x0B0E11', 'alert': '0xE5484D'}
PORTAL = """slug: sample
palette:
  bg: 0x0B0E11
  primary: 0xFF9A1F
  secondary: 0x82CEFF
  text: 0xFFFFFF
"""


def roles(state):
    return {k: v for k, v in state['palette'].items() if k not in ('mode', 'roleDefaults')}


class DumperCase(unittest.TestCase):
    """Builds tools/dump_theme_defaults.cpp once (it links the real theme_style.cpp) and runs it on throwaway themes,
    so what is asserted is what the firmware computes."""

    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        try:
            cls.dumper = gen.build_dumper(Path(cls._tmp.name) / 'dump_theme_defaults')
        except gen.GenError as e:
            cls._tmp.cleanup()
            raise unittest.SkipTest(str(e).splitlines()[0])

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def dump(self, yaml_text=None):
        """What the firmware resolves `yaml_text` to. None means no theme at all: the built-in look."""
        if yaml_text is None:
            return gen.run_dumper(self.dumper)
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / 'sample'
            src.mkdir()
            (src / 'theme.yaml').write_text(yaml_text, encoding='utf-8')
            return gen.run_dumper(self.dumper, gen.build_folder(src, Path(tmp) / 'built'))


class PaletteModeTest(DumperCase):
    def test_no_theme_is_the_built_in_palette(self):
        state = self.dump()
        self.assertEqual(state['palette']['mode'], 'builtin')
        self.assertEqual(roles(state), BUILT_IN)

    def test_a_theme_with_no_palette_is_legacy_and_keeps_the_built_in_roles(self):
        state = self.dump('slug: sample\nradar:\n  rangeKm: 30\n')
        self.assertEqual(state['palette']['mode'], 'legacy')
        self.assertEqual(roles(state), BUILT_IN)

    def test_a_palette_puts_a_theme_in_palette_mode_and_derives_the_rest(self):
        state = self.dump(PORTAL)
        self.assertEqual(state['palette']['mode'], 'theme')
        self.assertEqual(roles(state), PORTAL_DERIVED)
        self.assertIs(state['palette']['roleDefaults'], True)

    def test_role_defaults_false_is_carried(self):
        self.assertIs(self.dump(PORTAL + 'roleDefaults: false\n')['palette']['roleDefaults'], False)


class RoleReferenceTest(DumperCase):
    def test_a_role_reference_resolves_to_the_role_colour(self):
        state = self.dump(PORTAL + 'radar:\n  sweepColor: $secondary\n  sweepLeadColor: $muted\nticker:\n  upColor: $alert\n')
        self.assertEqual(state['radar']['sweepColor'], '0x82CEFF')
        self.assertEqual(state['radar']['sweepLeadColor'], '0x919394')      # a derived role
        self.assertEqual(state['ticker']['upColor'], '0xE5484D')

    def test_explicit_hex_still_wins_and_nested_references_resolve(self):
        state = self.dump(PORTAL + 'radar:\n  sweepColor: 0x123456\n  rtext:\n    - {color: $text}\n    - {color: $primary}\n')
        self.assertEqual(state['radar']['sweepColor'], '0x123456')
        self.assertEqual(state['radar']['rtext'][0]['color'], '0xFFFFFF')
        self.assertEqual(state['radar']['rtext'][1]['color'], '0xFF9A1F')

    def test_text_that_starts_with_a_dollar_is_not_touched(self):
        state = self.dump(PORTAL + 'menu:\n  current: {fmt: "$5 {name}"}\n')
        self.assertEqual(state['menu']['current']['fmt'], '$5 {name}')

    def test_a_reference_in_a_theme_with_no_palette_never_reaches_the_firmware(self):
        # the builder refuses it (Task 3); this is what a hand-edited card would do: the read ignores a wrong-typed value
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / 'sample'
            src.mkdir()
            (src / 'theme.yaml').write_text('slug: sample\nradar:\n  rangeKm: 30\n', encoding='utf-8')
            built = gen.build_folder(src, Path(tmp) / 'built')
            (built / 'radar_style.json').write_text('{"sweepColor":"$primary","rangeKm":30}', encoding='utf-8')
            state = gen.run_dumper(self.dumper, built)
        self.assertEqual(state['palette']['mode'], 'legacy')
        self.assertNotEqual(state['radar']['sweepColor'], '0x1DFF86')      # legacy: the string is left alone and ignored


class HandEditedThemeTest(DumperCase):
    """Review focus 3: a palette that is nonsense must not crash the firmware."""

    def dump_theme_json(self, theme_json):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / 'sample'
            src.mkdir()
            (src / 'theme.yaml').write_text('slug: sample\n', encoding='utf-8')
            built = gen.build_folder(src, Path(tmp) / 'built')
            (built / 'theme.json').write_text(theme_json, encoding='utf-8')
            return gen.run_dumper(self.dumper, built)

    def test_a_palette_that_is_not_an_object_is_ignored(self):
        self.assertEqual(self.dump_theme_json('{"slug":"sample","palette":7}')['palette']['mode'], 'legacy')

    def test_wrong_typed_roles_fall_back_and_do_not_crash(self):
        state = self.dump_theme_json('{"slug":"sample","palette":{"bg":"red","primary":-1,"text":1.5e40}}')
        self.assertEqual(state['palette']['mode'], 'theme')
        self.assertEqual(state['palette']['bg'], BUILT_IN['bg'])          # not a number: the built-in one stands
        self.assertEqual(state['palette']['text'], BUILT_IN['text'])


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run to verify it fails**

```bash
python3 -m unittest tests/test_theme_palette.py > /tmp/t4-red.log 2>&1; grep -E "^(FAIL|ERROR):|^Ran |^FAILED|KeyError" /tmp/t4-red.log | head -8
```
Expected: FAILED, `KeyError: 'palette'` (the dumper reports no palette yet).

- [ ] **Step 3: `theme_style.h`**

Replace
```
#include "theme_font_resolve.h"
```
with
```
#include "theme_font_resolve.h"
#include "theme_roles.h"
```
Add the ledger entry and bump the constant. Replace
```
//      those slots.
constexpr int THEME_CAPS = 52;
```
with
```
//      those slots.
//  53  colour roles. theme.json's `palette` picks bg, primary, secondary and text; the firmware derives seven
//      more (muted, dim, hairline, panel, highlight, onPrimary, alert) and resolves "$role" strings in the style
//      files to those colours. Unless theme.json says roleDefaults:false, every colour option a theme leaves out
//      takes its default from a role, so a theme can be only a palette. With no theme active the Orb draws the
//      built-in palette. An Orb below this level ignores the palette and reads "$role" as a wrong-typed value, so
//      such a colour keeps its compiled default.
constexpr int THEME_CAPS = 53;
```
Declare the accessors. Replace
```
theme_font::FontMap &fontMap();
```
with
```
theme_font::FontMap &fontMap();

// Colour roles, THEME_CAPS 53. Legacy: the theme has no palette, so every colour option is its compiled default and
// palette() is the built-in one (which is what app_theme.cpp has always drawn with). Theme: theme.json has a
// palette. BuiltIn: no theme is active. In the last two, a "$role" string in a style file reads as that colour.
enum class PaletteMode : uint8_t { Legacy, Theme, BuiltIn };
PaletteMode paletteMode();
inline bool paletteOn() { return paletteMode() != PaletteMode::Legacy; }
bool roleDefaults();                      // false when theme.json says "roleDefaults": false
const theme_roles::Palette &palette();
```

- [ ] **Step 4: `theme_style.cpp`**

Use a patch script file as before (each `sub` asserts one match).

(a) State. Replace `Names    s_names;\n` with:
```
Names    s_names;
PaletteMode s_paletteMode = PaletteMode::Legacy;
bool        s_roleDefaults = true;
theme_roles::Palette s_palette = theme_roles::BUILT_IN;
```

(b) The reference resolver, above `read_style_json`. Replace the line
```
// Reads /themes/<slug>/<name> into `doc`. Returns false (doc left empty) if the
```
with:
```
// A string value that is exactly "$<role>" becomes that role's colour, as an integer, wherever it sits in the
// document. Done once, right after parsing, so the hundreds of is<uint32_t>() reads below need no change. Only
// exact matches are touched, so text such as "$5.00" is never a colour. A name that looks like a role but is not
// one is left as a string (a read of the wrong type is ignored, as any is) and logged.
void resolve_role_refs(JsonVariant v) {
    if (v.is<JsonObject>()) {
        for (JsonPair kv : v.as<JsonObject>()) resolve_role_refs(kv.value());
    } else if (v.is<JsonArray>()) {
        for (JsonVariant e : v.as<JsonArray>()) resolve_role_refs(e);
    } else if (v.is<const char *>()) {
        const char *s = v.as<const char *>();
        if (!s || s[0] != '$' || !s[1]) return;
        const int r = theme_roles::find_role(s + 1);
        if (r >= 0) {
            v.set((uint32_t)s_palette.v[r]);            // s is dead after this: it pointed into the value we replaced
            return;
        }
        bool word = (s[1] >= 'A' && s[1] <= 'Z') || (s[1] >= 'a' && s[1] <= 'z') || s[1] == '_';
        for (const char *c = s + 2; word && *c; ++c)
            word = (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_';
        if (word) printf("[theme_style] '%s' is not a colour role; ignored\n", s);
    }
}

// Reads /themes/<slug>/<name> into `doc`. Returns false (doc left empty) if the
```

(c) The parse hook. Replace
```
    const DeserializationError err = deserializeJson(doc, buf, len);
    theme_sd::free(buf);
    return !err;
```
with
```
    const DeserializationError err = deserializeJson(doc, buf, len);
    theme_sd::free(buf);
    if (err) return false;
    if (s_paletteMode != PaletteMode::Legacy) resolve_role_refs(doc.as<JsonVariant>());
    return true;
```

(d) `load()`. Replace
```
void load() {
    seed_defaults();

    const char *slug = theme_select::activeSlug();
    if (!slug || !slug[0]) return;   // no theme active (stock build) — compiled defaults stand
```
with
```
void load() {
    seed_defaults();
    s_paletteMode = PaletteMode::Legacy;
    s_roleDefaults = true;
    s_palette = theme_roles::BUILT_IN;

    const char *slug = theme_select::activeSlug();
    if (!slug || !slug[0]) {
        // No theme is active: the built-in look. Its palette is the constant, and its colour options take their
        // defaults from it (Task 5 adds that; until then the compiled defaults stand).
        s_paletteMode = PaletteMode::BuiltIn;
        return;
    }

    {   // The palette comes first, because every read below may name a role.
        JsonDocument doc;
        if (read_style_json(slug, "theme.json", doc)) {
            JsonObjectConst pal = doc["palette"].as<JsonObjectConst>();
            if (!pal.isNull()) {
                theme_roles::Input in;
                theme_roles::input_clear(in);
                for (int r = 0; r < theme_roles::ROLE_COUNT; ++r) {
                    JsonVariantConst v = pal[theme_roles::NAMES[r]];
                    if (v.is<uint32_t>()) { in.v[r] = v.as<uint32_t>() & 0xFFFFFF; in.set[r] = true; }
                }
                s_palette = theme_roles::resolve(in);
                s_paletteMode = PaletteMode::Theme;
                if (doc["roleDefaults"].is<bool>()) s_roleDefaults = doc["roleDefaults"].as<bool>();
            }
        }
    }
```
(e) The accessors. Replace `theme_font::FontMap &fontMap() { return s_fontMap; }\n` with:
```
theme_font::FontMap &fontMap() { return s_fontMap; }
PaletteMode paletteMode() { return s_paletteMode; }
bool roleDefaults() { return s_roleDefaults; }
const theme_roles::Palette &palette() { return s_palette; }
```

- [ ] **Step 5: The dumper**

In `tools/dump_theme_defaults.cpp`, replace `    dump_splash(root["splash"].to<JsonObject>());\n` with:
```
    dump_splash(root["splash"].to<JsonObject>());
    {
        // The palette the firmware resolved, and which mode it is in. Not a theme section: tests read it, and
        // tools/resolved_golden.py drops it, so the goldens do not move when the firmware learns palettes.
        JsonObject o = root["palette"].to<JsonObject>();
        const PaletteMode m = paletteMode();
        o["mode"] = m == PaletteMode::BuiltIn ? "builtin" : m == PaletteMode::Theme ? "theme" : "legacy";
        o["roleDefaults"] = roleDefaults();
        for (int r = 0; r < theme_roles::ROLE_COUNT; ++r) o[theme_roles::NAMES[r]] = hex(palette().v[r]);
    }
```

- [ ] **Step 6: Run the tests**

```bash
python3 -m unittest tests/test_theme_palette.py tests/test_theme_resolved_golden.py > /tmp/t4-green.log 2>&1; grep -E "^(FAIL|ERROR):|^Ran |^OK|^FAILED" /tmp/t4-green.log
```
Expected: `OK`. The golden test passing here is **Review Focus 1**: the three shipped themes resolve exactly as before with the new firmware. If it fails, its message lists the option paths that moved; the firmware change broke a legacy theme, and that is a bug in this task.

- [ ] **Step 7: Build both firmware environments and the host suite**

```bash
~/.platformio/penv/bin/pio run -e native > /tmp/t4-native.log 2>&1; tail -3 /tmp/t4-native.log
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 > /tmp/t4-device.log 2>&1; tail -3 /tmp/t4-device.log
bash tests/run_host_tests.sh 2>&1 | tail -3
```
Expected: `SUCCESS` twice (the device build is cold in a fresh worktree: allow several minutes) and `all host tests passed`.

- [ ] **Step 8: Record it as unverified, and commit**

Add to `docs/HARDWARE_PENDING.md`, under a new heading `## Palette roles and the built-in look (plan 2)`:
```markdown
### Task 4: palette mode in `theme_style`, `THEME_CAPS` 53

- [ ] `theme_style::load()` runs with no theme and with a palette theme without a boot loop or watchdog; the serial log
      shows no `is not a colour role` line for a healthy theme.
- [ ] A theme with `$role` strings draws those colours (Fallout and Portal after Task 8).
```
```bash
git add src/theme/core/theme_style.h src/theme/core/theme_style.cpp tools/dump_theme_defaults.cpp tests/test_theme_palette.py docs/HARDWARE_PENDING.md
git commit -m "theme_style: a palette mode, \$role references, and the built-in palette" -m "THEME_CAPS 53. No theme active is the built-in mode; a theme.json palette is theme mode; anything else is legacy and resolves exactly as before (the resolved-state golden proves it). Built for both environments; NOT verified on hardware, see docs/HARDWARE_PENDING.md." -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 5: The role bindings and the built-in look's layout

**Files:**
- Create: `src/theme/core/theme_palette.h`, `src/theme/core/theme_palette.cpp`
- Modify: `src/theme/core/theme_style.cpp`, `tools/gen_elegant_theme.py` (the dumper's compile line), `platformio.ini` (the native source list)
- Test: `tests/test_theme_palette.py` (append a class)

**Interfaces:**
- Consumes: `theme_roles::{Palette, R_*, mix}` (Task 2); `theme_style::{Clock, Radar, Weather, Ticker, Menu, Settings, Splash, Intel, ClockText, TextSlot, MenuText, SplashText}` (the structs in `theme_style.h`); the `PaletteMode`, `s_palette`, `s_roleDefaults` state of Task 4.
- Produces: `theme_palette::apply_role_defaults(const Palette &, Clock &, Radar &, Weather &, Ticker &, Menu &, Settings &, Splash &, Intel &)`. It gives every role-bound colour option its palette default and, because a drawn face needs them, sets the clock's three hands to show in order with no sweep and no shadow, and shows the splash's theme-name line. `theme_style::load()` calls it in the built-in mode, and in theme mode unless `roleDefaults` is false; the theme's own JSON is applied **after**, so anything a theme states wins.

- [ ] **Step 1: Write the failing tests**

Append to `tests/test_theme_palette.py`:
```python
KEEP = ('apps', 'names', 'clock', 'radar', 'weather', 'ticker', 'settings', 'menu', 'splash', 'intel')


def sections(state):
    return {k: state[k] for k in KEEP}


class RoleDefaultsTest(DumperCase):
    """A theme that is only a palette gets a designed look: every colour option it leaves out takes its role."""

    def test_the_options_a_palette_theme_leaves_out_take_their_role(self):
        s = self.dump(PORTAL)
        self.assertEqual(s['radar']['sweepColor'], '0xFF9A1F')              # primary
        self.assertEqual(s['radar']['sweepLeadColor'], '0xFFFFFF')          # text
        self.assertEqual(s['radar']['blipAltGround'], '0x919394')           # muted
        self.assertEqual(s['radar']['blipAltLow'], '0x82CEFF')              # secondary
        self.assertEqual(s['radar']['blipAltMid'], '0xC1B48F')              # halfway from secondary to primary
        self.assertEqual(s['radar']['blipAltHigh'], '0xFF9A1F')
        self.assertEqual(s['radar']['blipAltCruise'], '0xFFC279')           # primary 40% toward text
        self.assertEqual(s['radar']['blipAltJet'], '0xFFFFFF')
        self.assertEqual(s['radar']['card']['color'], '0x1A1C1F')           # panel
        self.assertEqual([t['color'] for t in s['radar']['rtext']], ['0xFFFFFF', '0xFF9A1F', '0x82CEFF', '0x919394'])
        self.assertEqual(s['ticker']['upColor'], '0xFF9A1F')
        self.assertEqual(s['ticker']['downColor'], '0xE5484D')              # alert
        self.assertEqual(s['ticker']['flatColor'], '0x919394')
        self.assertEqual(s['ticker']['priceColor'], '0xFFFFFF')
        self.assertEqual(s['settings']['selColor'], '0xFF9A1F')
        self.assertEqual(s['settings']['itemColor'], '0x919394')
        self.assertEqual(s['settings']['hlColor'], '0x483115')              # highlight
        self.assertEqual(s['intel']['staleColor'], '0xE5484D')
        self.assertEqual(s['intel']['selBarColor'], '0x483115')
        self.assertEqual(s['weather']['ringColor'], '0x3C2A14')             # hairline
        self.assertEqual(s['weather']['credit']['color'], '0x919394')
        self.assertEqual(s['menu']['current']['color'], '0xFFFFFF')
        self.assertEqual(s['menu']['prev']['color'], '0x919394')
        self.assertEqual(s['clock']['bg'], '0x0B0E11')
        self.assertEqual(s['clock']['windRingFill'], '0xFF9A1F')
        self.assertEqual(s['splash']['theme']['color'], '0xFF9A1F')
        self.assertEqual(s['splash']['network']['color'], '0x919394')

    def test_a_drawn_face_gets_the_three_hands_ticking(self):
        s = self.dump(PORTAL)
        hands = s['clock']['hands']
        self.assertEqual([hands[h]['show'] for h in ('hour', 'minute', 'second')], [True, True, True])
        self.assertEqual([hands[h]['show'] for h in ('static1', 'static2')], [False, False])
        self.assertEqual(hands['order'], [0, 1, 2])
        self.assertIs(s['clock']['secondSweep'], False)
        self.assertIs(s['splash']['theme']['show'], True)                   # the theme's name is on the splash

    def test_what_a_theme_states_beats_the_role_default(self):
        s = self.dump(PORTAL + 'radar:\n  sweepColor: 0x123456\nticker:\n  upColor: $alert\n')
        self.assertEqual(s['radar']['sweepColor'], '0x123456')
        self.assertEqual(s['ticker']['upColor'], '0xE5484D')
        self.assertEqual(s['radar']['sweepLeadColor'], '0xFFFFFF')          # the others still default

    def test_role_defaults_false_keeps_every_compiled_default(self):
        # Review focus 1: a theme that only wants the palette's $role references must not be restyled.
        legacy = sections(self.dump('slug: sample\n'))
        kept = sections(self.dump(PORTAL + 'roleDefaults: false\n'))
        self.assertEqual(kept, legacy)

    def test_role_references_work_with_role_defaults_off(self):
        s = self.dump(PORTAL + 'roleDefaults: false\nradar:\n  sweepColor: $secondary\n')
        self.assertEqual(s['radar']['sweepColor'], '0x82CEFF')
        self.assertEqual(s['radar']['sweepLeadColor'], sections(self.dump('slug: sample\n'))['radar']['sweepLeadColor'])

    def test_a_legacy_theme_is_untouched_by_role_defaults(self):
        legacy = self.dump('slug: sample\nradar:\n  sweepColor: 0x0A0B0C\n')
        self.assertEqual(legacy['palette']['mode'], 'legacy')
        self.assertEqual(legacy['radar']['sweepColor'], '0x0A0B0C')
        self.assertNotEqual(legacy['radar']['sweepLeadColor'], '0xEAFFF3')   # not the built-in text colour: still compiled


class BuiltInLookTest(DumperCase):
    def test_no_theme_draws_the_built_in_palette_everywhere(self):
        s = self.dump()
        self.assertEqual(s['palette']['mode'], 'builtin')
        self.assertEqual(s['radar']['sweepColor'], '0x1DFF86')
        self.assertEqual(s['radar']['blipAltGround'], '0x818C86')
        self.assertEqual(s['ticker']['downColor'], '0xE5484D')
        self.assertEqual(s['settings']['hlColor'], '0x232A36')
        self.assertEqual(s['clock']['bg'], '0x000000')
        self.assertEqual([s['clock']['hands'][h]['show'] for h in ('hour', 'minute', 'second')], [True, True, True])
        self.assertIs(s['splash']['theme']['show'], True)
```
Add a line to `tests/test_theme_palette.py` imports if absent: nothing new is needed.

- [ ] **Step 2: Run to verify they fail**

```bash
python3 -m unittest tests.test_theme_palette.RoleDefaultsTest tests.test_theme_palette.BuiltInLookTest > /tmp/t5-red.log 2>&1; grep -E "^(FAIL|ERROR):|^Ran |^FAILED" /tmp/t5-red.log | head -10
```
Expected: FAILED (the compiled `sweepColor` is not `0xFF9A1F`, and so on). `test_role_defaults_false_keeps_every_compiled_default` and the legacy test may already pass, which is right: they guard behaviour that must not change.

- [ ] **Step 3: `src/theme/core/theme_palette.h`**

```cpp
#pragma once
#include "theme_roles.h"
#include "theme_style.h"

// The role bindings: which colour option takes which role by default.
//
// theme_style::load() calls this before it reads a theme's own JSON, so a theme that is only a palette gets a
// designed look and anything a theme states wins. It is a function of explicit assignments on purpose: the
// compiler checks every field name, a reader can see the whole mapping on one screen, and moving a colour to a
// different role is a one-line change that shows in a diff.
namespace theme_palette {

void apply_role_defaults(const theme_roles::Palette &p,
                         theme_style::Clock &clock, theme_style::Radar &radar, theme_style::Weather &weather,
                         theme_style::Ticker &ticker, theme_style::Menu &menu, theme_style::Settings &settings,
                         theme_style::Splash &splash, theme_style::Intel &intel);

} // namespace theme_palette
```

- [ ] **Step 4: `src/theme/core/theme_palette.cpp`**

```cpp
#include "theme_palette.h"

using namespace theme_roles;

namespace theme_palette {
namespace {

// The readout lines a screen draws for the selected item, in order: the name, two facts, a note.
constexpr Role LINE_COLOUR[4] = { R_text, R_primary, R_secondary, R_muted };

template <typename Slot>
void lines(Slot (&s)[4], const Palette &p) {
    for (int i = 0; i < 4; ++i) {
        s[i].color     = p.v[LINE_COLOUR[i]];
        s[i].glowColor = p.v[R_primary];
        s[i].bg        = p.v[R_panel];      // only visible if the theme gives the line a plate (bgOpa > 0)
    }
}

} // namespace

void apply_role_defaults(const Palette &p,
                         theme_style::Clock &clock, theme_style::Radar &radar, theme_style::Weather &weather,
                         theme_style::Ticker &ticker, theme_style::Menu &menu, theme_style::Settings &settings,
                         theme_style::Splash &splash, theme_style::Intel &intel) {
    const uint32_t bg = p.v[R_bg], primary = p.v[R_primary], secondary = p.v[R_secondary], text = p.v[R_text],
                   muted = p.v[R_muted], dim = p.v[R_dim], hairline = p.v[R_hairline], panel = p.v[R_panel],
                   highlight = p.v[R_highlight], alert = p.v[R_alert];

    // ---- Clock. The winding screen's colours, the two text banners, and the layout a DRAWN face needs: all three
    // hands, in order, ticking once a second with no shadow. A theme with images states its own hands and wins.
    clock.bg = bg;
    clock.windBg = bg;
    clock.windRingTrack = hairline;
    clock.windRingFill = primary;
    clock.windTitleCol = text;
    clock.windAskCol = muted;
    clock.windTurnsCol = dim;
    for (theme_style::ClockText *t : { &clock.text1, &clock.text2 }) {
        t->color = text;
        t->glowColor = primary;
        t->bg = panel;
    }
    for (int i = 0; i < 3; ++i) clock.hand[i].show = true;
    clock.hand[3].show = false;
    clock.hand[4].show = false;
    clock.orderN = 3;
    clock.order[0] = 0;
    clock.order[1] = 1;
    clock.order[2] = 2;
    clock.secondSweep = false;
    clock.shadowOn = false;

    // ---- Flight Tracker
    radar.sweepColor = primary;
    radar.sweepLeadColor = text;
    radar.blipFixedColor = primary;
    // Altitude, low to high: a ramp through the palette rather than six unrelated hues.
    radar.blipAltGround = muted;
    radar.blipAltLow = secondary;
    radar.blipAltMid = mix(secondary, primary, 50);
    radar.blipAltHigh = primary;
    radar.blipAltCruise = mix(primary, text, 40);
    radar.blipAltJet = text;
    radar.blipGlowColor = text;
    radar.selColor = secondary;
    radar.selGlowColor = secondary;
    radar.offRangeColor = muted;
    radar.centerColor = primary;
    radar.centerInnerColor = bg;
    radar.mapRoadColor = dim;
    radar.mapAirportColor = muted;
    radar.sweepHubColor = primary;
    radar.sweepHubGlowColor = primary;
    radar.card.color = panel;
    radar.card.borderColor = primary;
    lines(radar.rtext, p);

    // ---- Weather map
    weather.bg = bg;
    weather.sweepColor = secondary;
    weather.sweepLeadColor = text;
    weather.ringColor = hairline;
    weather.roadColor = dim;
    weather.coastColor = dim;
    lines(weather.text, p);
    weather.credit.color = muted;
    weather.credit.bg = bg;

    // ---- Stock ticker
    ticker.bg = bg;
    ticker.upColor = primary;
    ticker.downColor = alert;
    ticker.flatColor = muted;
    ticker.nameColor = muted;
    ticker.priceColor = text;
    ticker.stripColor = text;

    // ---- App switcher
    menu.current.color = text;
    menu.prev.color = muted;
    menu.next.color = muted;
    for (theme_style::MenuText *t : { &menu.current, &menu.prev, &menu.next }) t->glowColor = primary;

    // ---- Settings
    settings.selColor = primary;
    settings.itemColor = muted;
    settings.glowColor = primary;
    settings.selGlowColor = primary;
    settings.itemGlowColor = muted;
    settings.hlColor = highlight;

    // ---- Splash: the version, the network line, the credits and the theme's name
    splash.version.color = text;
    splash.network.color = muted;
    splash.credits.color = dim;
    splash.theme.color = primary;
    for (theme_style::SplashText *t : { &splash.version, &splash.network, &splash.credits, &splash.theme }) {
        t->glowColor = primary;
        t->bg = panel;
    }
    splash.theme.show = true;

    // ---- Headlines
    intel.bg = bg;
    intel.titleColor = muted;
    intel.textColor = text;
    intel.sourceColor = muted;
    intel.staleColor = alert;
    intel.selColor = text;
    intel.selBarColor = highlight;
    intel.briefColor = text;
    intel.ageColor = dim;
    intel.ageGlowColor = dim;
    intel.ageBg = bg;
}

} // namespace theme_palette
```
If the compiler rejects a field name (a struct member the search above missed), read that struct in `theme_style.h`, fix the name, and keep the role; do not drop the binding.

- [ ] **Step 5: Call it from `theme_style.cpp`, and build it everywhere**

In `src/theme/core/theme_style.cpp`, add the include beside the others: after `#include "theme_style.h"` add `#include "theme_palette.h"`. Then edit `load()`:

Replace
```
        // No theme is active: the built-in look. Its palette is the constant, and its colour options take their
        // defaults from it (Task 5 adds that; until then the compiled defaults stand).
        s_paletteMode = PaletteMode::BuiltIn;
        return;
```
with
```
        // No theme is active: the built-in look. Its palette is the constant, and every colour option takes its
        // default from it.
        s_paletteMode = PaletteMode::BuiltIn;
        theme_palette::apply_role_defaults(s_palette, s_clock, s_radar, s_weather, s_ticker, s_menu, s_settings,
                                           s_splash, s_intel);
        snprintf(s_names.theme, sizeof(s_names.theme), "Default");
        return;
```
and replace
```
                if (doc["roleDefaults"].is<bool>()) s_roleDefaults = doc["roleDefaults"].as<bool>();
```
with
```
                if (doc["roleDefaults"].is<bool>()) s_roleDefaults = doc["roleDefaults"].as<bool>();
                // Defaults first, so everything the theme's own files state (read below) wins over them.
                if (s_roleDefaults)
                    theme_palette::apply_role_defaults(s_palette, s_clock, s_radar, s_weather, s_ticker, s_menu,
                                                       s_settings, s_splash, s_intel);
```
Give the dumper the new file: in `tools/gen_elegant_theme.py`'s `build_dumper`, replace
```python
           str(REPO / 'src' / 'theme' / 'core' / 'theme_style.cpp'), '-o', str(out)]
```
with
```python
           str(REPO / 'src' / 'theme' / 'core' / 'theme_style.cpp'),
           str(REPO / 'src' / 'theme' / 'core' / 'theme_palette.cpp'), '-o', str(out)]
```
Give the simulator's native build the file: in `platformio.ini` replace `+<theme/core/theme_style.cpp>` with `+<theme/core/theme_palette.cpp> +<theme/core/theme_style.cpp>` (it appears once, in the native `build_src_filter`).

- [ ] **Step 6: Run the tests, the golden and both builds**

```bash
python3 -m unittest tests/test_theme_palette.py tests/test_theme_resolved_golden.py tests/test_elegant_theme.py tests/test_portal_theme.py tests/test_fallout_theme.py > /tmp/t5-green.log 2>&1; grep -E "^(FAIL|ERROR):|^Ran |^OK|^FAILED" /tmp/t5-green.log
~/.platformio/penv/bin/pio run -e native > /tmp/t5-native.log 2>&1; tail -3 /tmp/t5-native.log
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 > /tmp/t5-device.log 2>&1; tail -3 /tmp/t5-device.log
bash tests/run_host_tests.sh 2>&1 | tail -3
```
Expected: `OK` (the goldens still pass: a legacy theme is untouched), `SUCCESS` twice, `all host tests passed`.

- [ ] **Step 7: Record it as unverified, and commit**

Append to the `Palette roles` section of `docs/HARDWARE_PENDING.md`:
```markdown
### Task 5: role bindings and the built-in layout

- [ ] With no theme selected the Orb boots to the built-in look: green on black, no images, on every screen.
- [ ] A theme that is only a palette (four colours) looks designed on every screen, not just the clock.
```
```bash
git add src/theme/core/theme_palette.h src/theme/core/theme_palette.cpp src/theme/core/theme_style.cpp tools/gen_elegant_theme.py platformio.ini tests/test_theme_palette.py docs/HARDWARE_PENDING.md
git commit -m "Role bindings: every colour option takes its role, and the built-in look is a palette" -m "Built for both environments; NOT verified on hardware, see docs/HARDWARE_PENDING.md." -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 6: The built-in `default`: a reserved slug, a template folder, and a row in Settings

**Files:**
- Create: `src/theme/core/theme_slug_policy.h`, `tests/theme_slug_policy_test.cpp`, `tests/run_theme_slug_policy_test.sh`, `src/theme_assets/default/theme.yaml`, `tests/test_default_palette.py`
- Modify: `src/theme/core/theme_select.h`, `src/theme/core/theme_select.cpp`, `src/theme/core/theme_style.cpp` (`labelFor`), `src/app/settings/settings_view.cpp`, `tools/build_all_themes.py`, `tests/test_theme_font_coverage.py`, `tests/run_host_tests.sh`, `docs/HARDWARE_PENDING.md`

**Interfaces:**
- Consumes: `theme_roles::BUILT_IN` (Task 2), the palette mode and dumper of Tasks 4 and 5.
- Produces: `theme_select::BUILTIN_SLUG` (`"default"`), `theme_select::is_builtin(const char *) -> bool`, `theme_select::listable(const char *leaf) -> bool` (a folder name a card scan may offer: non-empty, not a dotfile, not the reserved slug). A saved slug of `default` boots to the built-in look (`activeSlug()` returns `""`, and this is *not* "nothing chosen", so the first card theme is not worn instead). `labelFor("default")` is `Default`. Settings > Design lists Default first.

- [ ] **Step 1: Write the failing tests**

`tests/theme_slug_policy_test.cpp`:
```cpp
// Host test for src/theme/core/theme_slug_policy.h.   tests/run_theme_slug_policy_test.sh
#include "theme_slug_policy.h"

#include <assert.h>
#include <stdio.h>

using namespace theme_select;

static void the_reserved_slug_is_default() {
    assert(strcmp(BUILTIN_SLUG, "default") == 0);
    assert(is_builtin("default"));
    assert(!is_builtin("elegant"));
    assert(!is_builtin("Default"));            // slugs are lowercase; a folder called Default is an ordinary theme
    assert(!is_builtin(""));
    assert(!is_builtin(nullptr));
}

// A card folder called `default` must not appear in the list: it would be a second, dead "Default".
static void a_card_scan_never_offers_the_reserved_slug_or_dotfiles() {
    assert(listable("elegant"));
    assert(listable("the-office"));
    assert(!listable("default"));
    assert(!listable(""));
    assert(!listable(nullptr));
    assert(!listable("."));
    assert(!listable(".."));
    assert(!listable(".Trashes"));
    assert(listable("default2"));              // only the exact name is reserved
}

int main() {
    the_reserved_slug_is_default();
    a_card_scan_never_offers_the_reserved_slug_or_dotfiles();
    printf("theme_slug_policy: all tests passed\n");
    return 0;
}
```
`tests/run_theme_slug_policy_test.sh`:
```bash
#!/bin/bash
# Builds and runs tests/theme_slug_policy_test.cpp on the host. Needs no libraries.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc/theme/core tests/theme_slug_policy_test.cpp -o "$OUT/theme_slug_policy_test"
"$OUT/theme_slug_policy_test"
```
Add to `tests/run_host_tests.sh` after the `theme roles` line: `run "theme slug policy"                  bash tests/run_theme_slug_policy_test.sh`

`tests/test_default_palette.py` (review focus 6):
```python
import re
import sys
import tempfile
import unittest
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests'))
import build_theme  # noqa: E402
import gen_elegant_theme as gen  # noqa: E402
from test_theme_palette import DumperCase, KEEP  # noqa: E402

DEFAULT_YAML = ROOT / 'src' / 'theme_assets' / 'default' / 'theme.yaml'


def built_in_from_header() -> dict:
    """The eleven values of theme_roles::BUILT_IN, in role order, read out of the header so this cannot drift."""
    text = (ROOT / 'src' / 'theme' / 'core' / 'theme_roles.h').read_text(encoding='utf-8')
    block = re.search(r'inline constexpr Palette BUILT_IN = \{\{(.*?)\}\};', text, re.S).group(1)
    values = [int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]{6}),', block)]
    return dict(zip(build_theme.firmware_facts()['roles'], values))


class DefaultFolderTest(unittest.TestCase):
    def test_the_default_folder_lists_exactly_the_built_in_palette(self):
        palette = yaml.safe_load(DEFAULT_YAML.read_text(encoding='utf-8'))['palette']
        self.assertEqual(palette, built_in_from_header())

    def test_the_folder_says_it_is_the_built_in(self):
        data = yaml.safe_load(DEFAULT_YAML.read_text(encoding='utf-8'))
        self.assertEqual((data['slug'], data['name'], data['default']), ('default', 'Default', True))

    def test_the_built_in_palette_equals_todays_default_app_palette(self):
        # legacy themes keep the colours every screen has always had: APP_THEME_DEFAULT in app_theme.cpp
        text = (ROOT / 'src' / 'app' / 'common' / 'app_theme.cpp').read_text(encoding='utf-8')
        block = text[text.index('APP_THEME_DEFAULT'):text.index('APP_THEME_OFFICE')]
        got = dict(re.findall(r'/\*(\w+)\*/\s+lv_color_hex\(0x([0-9A-Fa-f]{6})\)', block))
        b = built_in_from_header()
        want = {'bg': b['bg'], 'panel': b['panel'], 'highlight': b['highlight'], 'ink': b['text'], 'soft': b['secondary'],
                'dim': b['dim'], 'accent': b['primary'], 'hairline': b['hairline'], 'onAccent': b['onPrimary']}
        self.assertEqual({k: int(v, 16) for k, v in got.items()}, want)


class FourColoursEqualTheBuiltInTest(DumperCase):
    def test_a_theme_built_from_the_default_file_resolves_like_the_built_in_look(self):
        # the whole path (builder, theme.json, palette mode, role defaults) against the compiled built-in mode
        text = DEFAULT_YAML.read_text(encoding='utf-8').replace('slug: default', 'slug: paldemo', 1)
        from_file = self.dump(text)
        built_in = self.dump()
        self.assertEqual(from_file['palette']['mode'], 'theme')
        self.assertEqual(built_in['palette']['mode'], 'builtin')
        self.assertEqual({k: from_file['palette'][k] for k in built_in['palette'] if k not in ('mode',)},
                         {k: v for k, v in built_in['palette'].items() if k != 'mode'})
        self.assertEqual({k: from_file[k] for k in KEEP}, {k: built_in[k] for k in KEEP})


if __name__ == '__main__':
    unittest.main()
```
Also make the coverage guard and the all-themes build skip the reserved folder. In `tests/test_theme_font_coverage.py` replace
```python
        themes = sorted(p for p in THEMES.iterdir() if (p / 'theme.yaml').exists())
```
with
```python
        # `default` is the built-in look, palette-only and drawn in LVGL's Montserrat: it ships no fonts on purpose.
        themes = sorted(p for p in THEMES.iterdir() if (p / 'theme.yaml').exists() and p.name != 'default')
```

- [ ] **Step 2: Run to verify they fail**

```bash
chmod +x tests/run_theme_slug_policy_test.sh
bash tests/run_theme_slug_policy_test.sh 2>&1 | tail -2
python3 -m unittest tests/test_default_palette.py 2>&1 | tail -3
```
Expected: the slug policy fails to compile (`theme_slug_policy.h` missing); `test_default_palette` errors (`No such file ... default/theme.yaml`).

- [ ] **Step 3: The policy header and the folder**

`src/theme/core/theme_slug_policy.h`:
```cpp
#pragma once
#include <string.h>

// Which theme slugs mean something special. Pure, so it is tested on the desktop; theme_select.cpp uses it.
namespace theme_select {

// The built-in look. A saved slug of "default" selects it, no folder is read for it, and a card folder called
// "default" is ignored (it would be a second, dead "Default" in the list).
constexpr const char *BUILTIN_SLUG = "default";

inline bool is_builtin(const char *slug) { return slug && strcmp(slug, BUILTIN_SLUG) == 0; }

// A folder name a card scan may offer as a theme: non-empty, not a dotfile, not the reserved slug.
inline bool listable(const char *leaf) { return leaf && leaf[0] && leaf[0] != '.' && !is_builtin(leaf); }

} // namespace theme_select
```
`src/theme_assets/default/theme.yaml`:
```yaml
# The built-in look, written down: the palette an Orb draws when no theme is selected, and the template for a
# theme that is only colours. Copy this folder, change the four colours, and every screen follows.
#
# This file is not installed on a card (the slug `default` is reserved for the built-in); tests/test_default_palette.py
# keeps it equal to theme_roles::BUILT_IN in src/theme/core/theme_roles.h.
slug: default
name: Default
author: Orb OS
version: 1
default: true

palette:
  bg:        0x000000
  primary:   0x1DFF86
  secondary: 0x9AFFC8
  text:      0xEAFFF3
  # The other roles derive from these four unless a theme states them (theme_roles.h). The built-in look states
  # all of them, so it matches the firmware's constant exactly.
  muted:     0x818C86
  dim:       0x5F7A6C
  hairline:  0x1C2620
  panel:     0x0C160F
  highlight: 0x232A36
  onPrimary: 0x05100A
  alert:     0xE5484D
```

- [ ] **Step 4: Wire the reserved slug into `theme_select`, `labelFor` and the tools**

`src/theme/core/theme_select.h`: add `#include "theme_slug_policy.h"` beside the other includes (after `#include <stddef.h>`).

`src/theme/core/theme_select.cpp` (each `sub` asserts one match):
- Arduino scan: replace `            if (leaf[0] && leaf[0] != '.') {` with `            if (listable(leaf)) {`.
- Native scan: replace `        if (e->d_name[0] == '.') continue;   // skip ".", "..", dotfiles` with `        if (!listable(e->d_name)) continue;   // ".", "..", dotfiles, and the reserved built-in slug`.
- `init()`: replace
```
    if (!s_slug[0]) {
        static char slugs[MAX_THEMES][MAX_SLUG_LEN];
```
with
```
    // The reserved slug is a choice, the built-in look: no folder, and not "nothing chosen" (which would wear the
    // first theme on the card instead of what was picked).
    const bool builtinChosen = is_builtin(s_slug);
    if (builtinChosen) s_slug[0] = 0;
    if (!s_slug[0] && !builtinChosen) {
        static char slugs[MAX_THEMES][MAX_SLUG_LEN];
```
`src/theme/core/theme_style.cpp`, `labelFor`: replace `    if (!out || !cap) return;\n    snprintf(out, cap, "%s", (slug && slug[0]) ? slug : "");   // slug is the fallback label` with
```
    if (!out || !cap) return;
    if (theme_select::is_builtin(slug)) { snprintf(out, cap, "Default"); return; }   // the built-in has no folder to read
    snprintf(out, cap, "%s", (slug && slug[0]) ? slug : "");   // slug is the fallback label
```
`src/app/settings/settings_view.cpp`: in `refresh_designSelect()` replace
```
        s_designCount = theme_select::listInstalled(s_designSlugs);
        if (s_designCount > theme_select::MAX_THEMES) s_designCount = theme_select::MAX_THEMES;
```
with
```
        s_designCount = theme_select::listInstalled(s_designSlugs);
        // The built-in look is always the first choice: nothing to install, and the way back from any theme.
        if (s_designCount > theme_select::MAX_THEMES - 1) s_designCount = theme_select::MAX_THEMES - 1;
        for (int i = s_designCount; i > 0; --i)
            memcpy(s_designSlugs[i], s_designSlugs[i - 1], theme_select::MAX_SLUG_LEN);
        strncpy(s_designSlugs[0], theme_select::BUILTIN_SLUG, theme_select::MAX_SLUG_LEN - 1);
        s_designSlugs[0][theme_select::MAX_SLUG_LEN - 1] = 0;
        ++s_designCount;
```
and in the press handler replace `strcmp(s_designSlugs[s_designSel], theme_select::activeSlug()) != 0` with `strcmp(s_designSlugs[s_designSel], theme_select::activeSlug()[0] ? theme_select::activeSlug() : theme_select::BUILTIN_SLUG) != 0` (`activeSlug()` is `""` for the built-in, so without this, choosing Default while already on it would reboot for nothing).

`tools/build_all_themes.py`: replace
```python
    themes = [p for p in children if (p / 'theme.yaml').is_file()]
```
with
```python
    # `default` is the built-in look, not a card theme: it has no folder on the card (see theme_slug_policy.h).
    themes = [p for p in children if (p / 'theme.yaml').is_file() and p.name != 'default']
```

- [ ] **Step 5: Run the tests, the golden and both builds**

```bash
bash tests/run_theme_slug_policy_test.sh 2>&1 | tail -2
python3 -m unittest tests/test_default_palette.py tests/test_theme_font_coverage.py tests/test_build_all_themes.py tests/test_theme_resolved_golden.py > /tmp/t6-green.log 2>&1; grep -E "^(FAIL|ERROR):|^Ran |^OK|^FAILED" /tmp/t6-green.log
~/.platformio/penv/bin/pio run -e native > /tmp/t6-native.log 2>&1; tail -3 /tmp/t6-native.log
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 > /tmp/t6-device.log 2>&1; tail -3 /tmp/t6-device.log
bash tests/run_host_tests.sh 2>&1 | tail -3
```
Expected: `theme_slug_policy: all tests passed`, `OK`, `SUCCESS` twice, `all host tests passed`. `test_a_theme_built_from_the_default_file_resolves_like_the_built_in_look` is the end-to-end proof that a four-colour theme equals the built-in look; if it differs, its message names the option, and the difference is a real inconsistency between the two modes to fix here.

- [ ] **Step 6: Try it in the simulator**

```bash
echo default > /tmp/orb_sim_theme_slug
```
Run the simulator headless as in the plan-1 recipe (`SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SETTLE_MS=1500 .pio/build/native/program --themeshot /tmp/builtin`, wait about 45 seconds, `kill -9`). The log line `[sim] theme slug:` must print an empty slug, and the screenshots use the built-in colours. (The clock has no dial yet: that is Part B.) Restore the slug file afterwards.

- [ ] **Step 7: Record it as unverified, and commit**

Append to `docs/HARDWARE_PENDING.md`:
```markdown
### Task 6: the reserved `default` slug and Settings > Design

- [ ] Settings > Design lists **Default** first, and choosing it reboots into the built-in look and stays there across
      reboots (it must not switch to the first card theme).
- [ ] With Default active, choosing Default again does not reboot. Choosing an installed theme and then Default works.
- [ ] A card folder called `default` is not listed.
```
```bash
git add src/theme/core/theme_slug_policy.h tests/theme_slug_policy_test.cpp tests/run_theme_slug_policy_test.sh src/theme_assets/default/theme.yaml tests/test_default_palette.py src/theme/core/theme_select.h src/theme/core/theme_select.cpp src/theme/core/theme_style.cpp src/app/settings/settings_view.cpp tools/build_all_themes.py tests/test_theme_font_coverage.py tests/run_host_tests.sh docs/HARDWARE_PENDING.md
git commit -m "The built-in default: a reserved slug, a template folder, and the first row in Settings > Design" -m "Built for both environments; NOT verified on hardware, see docs/HARDWARE_PENDING.md." -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 7: `AppPalette` reads the roles

**Files:**
- Modify: `src/app/common/app_theme.cpp`, `docs/HARDWARE_PENDING.md`

**Interfaces:**
- Consumes: `theme_style::palette()` (Task 4), `theme_roles::R_*`.
- Produces: `app_theme::palette()` returns the theme's roles as the nine `AppPalette` colours (`ink` = `text`, `soft` = `secondary`, `accent` = `primary`, `onAccent` = `onPrimary`), except when the Office skin is selected, which keeps its compiled palette until it is retired. For a theme with no palette these are the built-in values, which equal the old Default palette (Task 6's `test_the_built_in_palette_equals_todays_default_app_palette` proves it), so a legacy theme's menu, settings and splash do not change.

There is no separate unit test: the mapping is nine assignments, its legacy equivalence is already guarded by Task 6, and its effect is the app-switcher menu's colours, which the simulator shows.

- [ ] **Step 1: Edit `app_theme.cpp`**

Add `#include "theme_style.h"` after `#include "settings_store.h"`. Replace
```cpp
const AppPalette &palette() {
    return PALETTES[s_theme];
}
```
with
```cpp
// Office keeps its compiled palette until that skin is retired. Everything else reads the theme's roles (the built-in
// ones when the theme has no palette, which equal APP_THEME_DEFAULT above), so a themed Orb's menu, splash and
// Settings share its colours instead of always being night-vision green.
const AppPalette &palette() {
    if (s_theme == APP_THEME_OFFICE) return PALETTES[APP_THEME_OFFICE];
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
```

- [ ] **Step 2: Build both environments and run the suites**

```bash
~/.platformio/penv/bin/pio run -e native > /tmp/t7-native.log 2>&1; tail -3 /tmp/t7-native.log
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 > /tmp/t7-device.log 2>&1; tail -3 /tmp/t7-device.log
bash tests/run_host_tests.sh 2>&1 | tail -3
python3 -m unittest tests/test_default_palette.py 2>&1 | tail -3
```
Expected: `SUCCESS` twice, `all host tests passed`, `OK`.

- [ ] **Step 3: Look at it**

Run the simulator headless with the slug file set to `portal` (build Portal into `sim/sdcard/themes` first with `tools/build_theme.py`; it has no palette yet, so this must look **exactly as before**), then with the slug set to `default`. The `--themeshot` menu image (`<prefix>-menu.bmp`) is the app switcher: with `default` it is the built-in green; with Portal (legacy) it is still the built-in green, unchanged. Restore the slug file.

- [ ] **Step 4: Record it as unverified, and commit**

Append to `docs/HARDWARE_PENDING.md`:
```markdown
### Task 7: `AppPalette` from the roles

- [ ] The app-switcher menu, Settings and About take a palette theme's colours; a theme with no palette still shows the
      familiar night-vision green.
```
```bash
git add src/app/common/app_theme.cpp docs/HARDWARE_PENDING.md
git commit -m "AppPalette reads the theme's roles; Office keeps its compiled skin" -m "Built for both environments; NOT verified on hardware, see docs/HARDWARE_PENDING.md." -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 8: Migrate Fallout, Portal and Elegant to palettes, with nothing moving

**Files:**
- Create: `tools/palettize.py`, `tests/test_palettize.py`, `tests/test_shipped_palettes.py`
- Modify: `src/theme_assets/fallout/theme.yaml`, `src/theme_assets/portal/theme.yaml`, `src/theme_assets/elegant/theme.yaml`, `tools/gen_elegant_theme.py` (`PRESERVED_KEYS`), `tests/test_fallout_theme.py`, `tests/test_elegant_theme.py`

**Interfaces:**
- Consumes: the `palette:` / `roleDefaults` builder (Task 3), the resolved-state goldens (Task 1).
- Produces: `palettize.count_colours(text) -> Counter[int]`, `palettize.suggest(counts) -> dict[str, int]` (role -> colour, for `bg`, `primary`, `secondary`, `text`); CLI `python3 tools/palettize.py <theme dir>` that only **reports**. Each shipped theme gains a `palette:` block and `roleDefaults: false`; Fallout and Portal replace each repeated hex with its `$role`; Elegant keeps its generated explicit hex (decision 3). **The goldens are the acceptance test: nothing may resolve differently.**

- [ ] **Step 1: Write the failing tests**

`tests/test_palettize.py`:
```python
import sys
import unittest
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import palettize  # noqa: E402


class CountColoursTest(unittest.TestCase):
    def test_it_counts_both_spellings_and_ignores_comments(self):
        text = 'a: 0xFF9A1F\nb: #ff9a1f\nc: {color: 0x0B0E11, bg: 0xFF9A1F}  # 0x123456 is not counted\n# 0xABCDEF\n'
        self.assertEqual(palettize.count_colours(text), Counter({0xFF9A1F: 3, 0x0B0E11: 1}))

    def test_a_palette_block_is_not_counted(self):
        text = 'palette:\n  bg: 0x000000\n  primary: 0x1DFF86\nradar:\n  sweepColor: 0x1DFF86\n'
        self.assertEqual(palettize.count_colours(text), Counter({0x1DFF86: 1}))


class SuggestTest(unittest.TestCase):
    def test_portals_colours_suggest_portals_palette(self):
        counts = Counter({0xFF9A1F: 16, 0x0B0E11: 8, 0xFFFFFF: 7, 0x82CEFF: 7, 0x1FA2FF: 4, 0x8794A3: 3})
        self.assertEqual(palettize.suggest(counts),
                         {'bg': 0x0B0E11, 'primary': 0xFF9A1F, 'secondary': 0x82CEFF, 'text': 0xFFFFFF})

    def test_it_never_suggests_one_colour_for_two_roles(self):
        counts = Counter({0x000000: 5, 0xFFFFFF: 4})
        got = palettize.suggest(counts)
        self.assertEqual(len(set(got.values())), len(got))

    def test_a_theme_with_almost_no_colours_gets_a_partial_suggestion_not_a_crash(self):
        self.assertEqual(palettize.suggest(Counter()), {})
        self.assertEqual(palettize.suggest(Counter({0x808080: 2})), {'primary': 0x808080})     # grey: not a bg, not bright, not colourful


if __name__ == '__main__':
    unittest.main()
```
`tests/test_shipped_palettes.py`:
```python
import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests'))
import palettize  # noqa: E402
from font_facts import converter_available  # noqa: E402

THEMES = ROOT / 'src' / 'theme_assets'
BASE = ('bg', 'primary', 'secondary', 'text')


def build(slug: str):
    tmp = tempfile.TemporaryDirectory()
    r = subprocess.run([sys.executable, str(ROOT / 'tools' / 'build_theme.py'), str(THEMES / slug), '--out', tmp.name],
                       capture_output=True, text=True)
    if r.returncode != 0:
        tmp.cleanup()
        if 'lv_font_conv' in r.stderr and not converter_available():
            raise unittest.SkipTest("lv_font_conv (or npx) is needed to bake this theme's faces")
        raise AssertionError(r.stderr)
    return tmp, Path(tmp.name) / slug


def refs_in(built: Path) -> int:
    """How many "$role" strings the built style files carry."""
    n = 0

    def walk(node):
        nonlocal n
        if isinstance(node, dict):
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)
        elif isinstance(node, str) and re.fullmatch(r'\$[A-Za-z_]\w*', node):
            n += 1

    for f in built.glob('*_style.json'):
        walk(json.loads(f.read_text(encoding='utf-8')))
    return n


class ShippedPalettesTest(unittest.TestCase):
    def check_palette(self, slug):
        tmp, built = build(slug)
        self.addCleanup(tmp.cleanup)
        theme = json.loads((built / 'theme.json').read_text(encoding='utf-8'))
        self.assertTrue(all(r in theme['palette'] for r in BASE), f'{slug} does not state bg, primary, secondary and text')
        self.assertIs(theme['roleDefaults'], False, f'{slug} states only what differs from the compiled values')
        return built, theme

    def test_fallout_states_its_palette_and_uses_it(self):
        built, theme = self.check_palette('fallout')
        self.assertEqual({k: theme['palette'][k] for k in BASE},
                         {'bg': 0x021A0C, 'primary': 0x1BFF80, 'secondary': 0x11B25A, 'text': 0xB6FFD2})
        self.assertGreaterEqual(refs_in(built), 30)

    def test_portal_states_its_palette_and_uses_it(self):
        built, theme = self.check_palette('portal')
        self.assertEqual({k: theme['palette'][k] for k in BASE},
                         {'bg': 0x0B0E11, 'primary': 0xFF9A1F, 'secondary': 0x82CEFF, 'text': 0xFFFFFF})
        self.assertGreaterEqual(refs_in(built), 25)

    def test_elegant_states_a_palette_but_keeps_its_colours_explicit(self):
        built, theme = self.check_palette('elegant')
        self.assertEqual(refs_in(built), 0)          # it is the exhaustive reference, regenerated from resolved values

    def test_no_colour_is_written_twice(self):
        # "a theme states each colour once": a hex value equal to a palette colour should be its $role
        for slug in ('fallout', 'portal'):
            text = (THEMES / slug / 'theme.yaml').read_text(encoding='utf-8')
            palette = set(palettize.palette_colours(text).values())
            outside = set(palettize.count_colours(text))
            self.assertEqual(sorted(hex(c) for c in outside & palette), [],
                             f'{slug}: these colours are in the palette but still written as hex')


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run to verify they fail**

```bash
python3 -m unittest tests/test_palettize.py tests/test_shipped_palettes.py 2>&1 | tail -4
```
Expected: ERROR `ModuleNotFoundError: No module named 'palettize'`.

- [ ] **Step 3: Implement `tools/palettize.py`**

```python
#!/usr/bin/env python3
"""Report the colours a theme repeats, and suggest four to make its palette. It only reports: it never rewrites a
file, because the YAML round trip would lose the comments a theme is full of.

    python3 tools/palettize.py src/theme_assets/portal

Then write the `palette:` block by hand and replace each repeated hex with its `$role`.
"""
from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

HEX = re.compile(r'(?<![\w$])(?:0x|#)([0-9A-Fa-f]{6})\b')
_NOT_A_HEX_COMMENT = re.compile(r' #(?![0-9A-Fa-f]{6}\b)')


def _code(line: str) -> str:
    """The line without its comment. ` #RRGGBB` is a colour written the short way, not a comment."""
    if line.lstrip().startswith('#'):
        return ''
    m = _NOT_A_HEX_COMMENT.search(line)
    return line[:m.start()] if m else line


def _palette_lines(text: str):
    """(in_palette, line) for every line: the palette block is the indented lines under a top-level `palette:`."""
    inside = False
    for line in text.split('\n'):
        if re.match(r'^palette:\s*$', line):
            inside = True
            yield True, line
            continue
        if inside and line and not line.startswith((' ', '\t', '#')):
            inside = False
        yield inside, line


def count_colours(text: str) -> Counter:
    """How often each colour is written outside the palette block and outside comments."""
    counts: Counter = Counter()
    for inside, line in _palette_lines(text):
        if inside:
            continue
        for m in HEX.finditer(_code(line)):
            counts[int(m.group(1), 16)] += 1
    return counts


def palette_colours(text: str) -> dict:
    """{role: colour} for the palette block, in file order."""
    out = {}
    for inside, line in _palette_lines(text):
        m = re.match(r'^\s+(\w+):\s*(?:0x|#)([0-9A-Fa-f]{6})\b', line) if inside else None
        if m:
            out[m.group(1)] = int(m.group(2), 16)
    return out


def _luma(c: int) -> int:
    return (((c >> 16) & 255) * 299 + ((c >> 8) & 255) * 587 + (c & 255) * 114) // 1000


def _saturation(c: int) -> float:
    hi = max((c >> 16) & 255, (c >> 8) & 255, c & 255)
    lo = min((c >> 16) & 255, (c >> 8) & 255, c & 255)
    return (hi - lo) / hi if hi else 0.0


def suggest(counts: Counter) -> dict:
    """A starting guess at bg, primary, secondary and text from how often each colour is used. Only a guess: a person
    picks the palette. Never suggests one colour for two roles, and leaves a role out rather than invent it."""
    ranked = [c for c, _ in counts.most_common()]
    out: dict = {}

    def take(role, wanted):
        for c in ranked:
            if wanted(c):
                out[role] = c
                ranked.remove(c)
                return

    take('bg', lambda c: _luma(c) < 60)
    top = ranked[:6]
    brightest = max((c for c in top if _luma(c) > 150), key=_luma, default=None)
    if brightest is not None:
        out['text'] = brightest
        ranked.remove(brightest)
    take('primary', lambda c: _saturation(c) > 0.4)
    take('secondary', lambda c: _saturation(c) > 0.2)
    if 'primary' not in out and ranked:          # nothing colourful: the most used remaining colour leads
        out['primary'] = ranked.pop(0)
    return {r: out[r] for r in ('bg', 'primary', 'secondary', 'text') if r in out}


def main(argv) -> int:
    if len(argv) != 2 or not (Path(argv[1]) / 'theme.yaml').is_file():
        print('usage: palettize.py <theme folder holding theme.yaml>', file=sys.stderr)
        return 2
    text = (Path(argv[1]) / 'theme.yaml').read_text(encoding='utf-8')
    counts = count_colours(text)
    print(f'{sum(counts.values())} colour(s) written, {len(counts)} distinct (outside the palette block):')
    for c, n in counts.most_common():
        print(f'  {n:3d}  0x{c:06X}  luma {_luma(c):3d}')
    print('suggested:', {r: f'0x{c:06X}' for r, c in suggest(counts).items()})
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
```

- [ ] **Step 4: A one-off script to replace hex with `$role`**

Not committed. Save as `pin_refs.py` in the scratch area and run it on each theme that is migrated:
```python
"""Replace each hex colour equal to a palette role with `$role`, outside the palette block and comments.
usage: python3 pin_refs.py <theme.yaml>"""
import re
import sys
from pathlib import Path

sys.path.insert(0, 'tools')
import palettize  # noqa: E402

path = Path(sys.argv[1])
text = path.read_text(encoding='utf-8')
by_value = {}
for role, colour in palettize.palette_colours(text).items():      # file order: the first role for a value wins
    by_value.setdefault(colour, role)
n = 0


def swap(m):
    global n
    role = by_value.get(int(m.group(1), 16))
    if role is None:
        return m.group(0)
    n += 1
    return '$' + role


out = []
for inside, line in palettize._palette_lines(text):
    if inside or line.lstrip().startswith('#'):
        out.append(line)
        continue
    m = palettize._NOT_A_HEX_COMMENT.search(line)
    code, comment = (line[:m.start()], line[m.start():]) if m else (line, '')
    out.append(palettize.HEX.sub(swap, code) + comment)
path.write_text('\n'.join(out), encoding='utf-8')
print(n, 'replaced')
```

- [ ] **Step 5: Run the palettize tests**

```bash
python3 -m unittest tests/test_palettize.py 2>&1 | tail -3
```
Expected: `OK`.

- [ ] **Step 6: Migrate Fallout**

Insert this after the `default: false` line of `src/theme_assets/fallout/theme.yaml` (that line appears once):
```yaml

roleDefaults: false        # colours this theme does not state keep the firmware's compiled values

palette:
  bg:        0x021A0C      # tube
  primary:   0x1BFF80      # bright
  secondary: 0x11B25A      # mid
  text:      0xB6FFD2      # pale
  muted:     0x0B7A3E      # low
  dim:       0x064021
  hairline:  0x0B5A2E
  panel:     0x010D06
  highlight: 0x0A3A1E
```
Then replace the repeated hex and prove nothing moved:
```bash
python3 pin_refs.py src/theme_assets/fallout/theme.yaml
python3 -m unittest tests/test_theme_resolved_golden.py 2>&1 | tail -3
```
Expected: `N replaced` with N of about 40, and the golden `OK`: every option Fallout resolves to is exactly what it was. If the golden fails, its message lists the option paths that moved: a hex was swapped for the wrong role, so fix the palette or the line, not the golden.

- [ ] **Step 7: Migrate Portal**

After the `default: false` line of `src/theme_assets/portal/theme.yaml`:
```yaml

roleDefaults: false        # colours this theme does not state keep the firmware's compiled values

palette:
  bg:        0x0B0E11
  primary:   0xFF9A1F      # the orange portal
  secondary: 0x82CEFF      # the blue portal's light
  text:      0xFFFFFF
  muted:     0x8794A3
  dim:       0x6E8FA8
  alert:     0xFF2D2D
```
```bash
python3 pin_refs.py src/theme_assets/portal/theme.yaml
python3 -m unittest tests/test_theme_resolved_golden.py 2>&1 | tail -3
```
Expected: about 45 replaced, golden `OK`.

- [ ] **Step 8: Migrate Elegant**

Elegant is regenerated from the values the firmware reads, so it keeps every colour as hex (decision 3); it only gains the two hand-written blocks, which the generator must now preserve. First the generator: in `tools/gen_elegant_theme.py` replace `PRESERVED_KEYS = ('fonts',)` with `PRESERVED_KEYS = ('fonts', 'palette', 'roleDefaults')`. Add to `PreservedBlocksTest` in `tests/test_elegant_theme.py`:
```python
    def test_a_palette_block_and_role_defaults_survive(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / 'theme.yaml'
            p.write_text('slug: x\nradar:\n  rangeKm: 1\npalette:\n  bg: 0x000000\n  primary: 0x1DFF86\nroleDefaults: false\n',
                         encoding='utf-8')
            self.assertEqual(gen.preserved_blocks(p), 'palette:\n  bg: 0x000000\n  primary: 0x1DFF86\nroleDefaults: false\n')
```
Run it and watch it pass once the generator change is in. Then ask for a suggestion for Elegant's four colours:
```bash
python3 tools/palettize.py src/theme_assets/elegant
```
Write the four it suggests (adjust by eye if a suggestion is a poor bg or text) as a palette at the end of `src/theme_assets/elegant/theme.yaml`, after its `fonts:` block:
```yaml

palette:
  bg:        0x......
  primary:   0x......
  secondary: 0x......
  text:      0x......

roleDefaults: false
```
Regenerate and confirm the file did not change, the form check passes and the golden holds:
```bash
python3 tools/gen_elegant_theme.py
git diff --stat src/theme_assets/elegant/theme.yaml
python3 tools/gen_elegant_theme.py --check
python3 -m unittest tests/test_theme_resolved_golden.py 2>&1 | tail -3
```
Expected: the diff shows only the appended palette block, `elegant theme is up to date`, and the golden `OK`.

- [ ] **Step 9: Update Fallout's colour test**

Its `test_every_colour_in_the_yaml_is_green` asserts more than 20 hex colours are written, to prove the palette is stated. The palette is now a block and the uses are `$role`s, so in `tests/test_fallout_theme.py` replace
```python
        self.assertGreater(len(colours), 20, 'the palette should be stated, not left to the defaults')
```
with
```python
        self.assertGreater(len(colours), 8, 'the palette should be stated, not left to the defaults')
```
(the palette block holds nine, and every one is still checked for green).

- [ ] **Step 10: Run the whole theme suite in the background, and commit**

```bash
python3 -m unittest tests/test_palettize.py tests/test_shipped_palettes.py tests/test_theme_resolved_golden.py tests/test_theme_palette.py tests/test_default_palette.py tests/test_elegant_theme.py tests/test_portal_theme.py tests/test_fallout_theme.py tests/test_build_theme.py tests/test_theme_fonts_golden.py tests/test_theme_font_coverage.py tests/test_build_all_themes.py tests/test_boot_wiring.py > /tmp/t8-suite.log 2>&1; grep -E "^(FAIL|ERROR):|^Ran |^OK|^FAILED" /tmp/t8-suite.log
```
Expected: `OK`. Then:
```bash
git add tools/palettize.py tests/test_palettize.py tests/test_shipped_palettes.py tools/gen_elegant_theme.py src/theme_assets/fallout/theme.yaml src/theme_assets/portal/theme.yaml src/theme_assets/elegant/theme.yaml tests/test_fallout_theme.py tests/test_elegant_theme.py
git commit -m "Fallout, Portal and Elegant carry palettes; nothing they resolve to has moved" -m "Fallout and Portal replace each repeated hex with its role; Elegant keeps its generated explicit colours. All three set roleDefaults false, so options they leave out keep the compiled values. The resolved-state goldens, captured before palettes existed, still pass." -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

## Part B: The procedural clock and the built-in splash (spec step 3)

Cut the second branch from the end of Part A: `git switch -c feat/procedural-clock`.

### Task 9: The drawn face's geometry, as pure math

**Files:**
- Create: `src/app/clock/clock_face.h`, `tests/clock_face_test.cpp`, `tests/run_clock_face_test.sh`
- Modify: `tests/run_host_tests.sh`

**Interfaces:**
- Produces (namespace `clock_face`): `struct Layout { float cx, cy, ringR, ringW, majorLen, minorLen, majorW, minorW; }`; `constexpr Layout DEFAULT_LAYOUT`; `struct Segment { float x0, y0, x1, y1; }`; `bool is_major(int)`; `Segment tick(const Layout &, int i)` (i = 0..59, 0 at 12, clockwise); `struct Blade { float x[4], y[4]; }`; `Blade blade(cx, cy, angDeg, len, tail, hw)` (four corners, clockwise from the tip); `struct Angles { float hour, minute, second; }`; `Angles angles(int hour, int minute, float second)`.

- [ ] **Step 1: Write the failing test**

`tests/clock_face_test.cpp`:
```cpp
// Host test for src/app/clock/clock_face.h.   tests/run_clock_face_test.sh
#include "clock_face.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

using namespace clock_face;

static bool near(float a, float b, float eps = 0.01f) { return fabsf(a - b) < eps; }
static float length(const Segment &s) { return hypotf(s.x1 - s.x0, s.y1 - s.y0); }

static void twelve_ticks_are_major() {
    int major = 0;
    for (int i = 0; i < 60; ++i) if (is_major(i)) ++major;
    assert(major == 12);
    assert(is_major(0) && is_major(15) && is_major(55) && !is_major(1) && !is_major(59));
}

static void ticks_sit_at_the_right_angles_and_lengths() {
    const Layout &L = DEFAULT_LAYOUT;
    const Segment top = tick(L, 0);                         // 12 o'clock: straight up from the centre
    assert(near(top.x0, L.cx) && near(top.y0, L.cy - 222.0f));
    assert(near(length(top), L.majorLen));
    const Segment right = tick(L, 15);                      // 3 o'clock
    assert(near(right.x0, L.cx + 222.0f) && near(right.y0, L.cy));
    const Segment down = tick(L, 30);
    assert(near(down.x0, L.cx) && near(down.y0, L.cy + 222.0f));
    const Segment left = tick(L, 45);
    assert(near(left.x0, L.cx - 222.0f) && near(left.y0, L.cy));
    assert(near(length(tick(L, 1)), L.minorLen));           // a minor tick is the short one
}

static void every_tick_stays_inside_the_ring() {
    const Layout &L = DEFAULT_LAYOUT;
    for (int i = 0; i < 60; ++i) {
        const Segment s = tick(L, i);
        assert(hypotf(s.x0 - L.cx, s.y0 - L.cy) < L.ringR);
        assert(hypotf(s.x1 - L.cx, s.y1 - L.cy) < hypotf(s.x0 - L.cx, s.y0 - L.cy));      // runs inward
        assert(s.x0 >= 0 && s.x0 <= 466 && s.y0 >= 0 && s.y0 <= 466);                     // and on the 466 px screen
    }
}

static void a_hand_at_twelve_points_straight_up() {
    const Blade b = blade(100, 100, 0, 50, 10, 4);
    assert(near(b.x[0], 100) && near(b.y[0], 50));           // the tip
    assert(near(b.x[1], 104) && near(b.y[1], 92));           // the shoulder, 16% of the way out, to the right
    assert(near(b.x[2], 100) && near(b.y[2], 110));          // the tail, behind the pivot
    assert(near(b.x[3], 96) && near(b.y[3], 92));            // the shoulder on the left
}

static void a_hand_at_three_points_right() {
    const Blade b = blade(100, 100, 90, 50, 10, 4);
    assert(near(b.x[0], 150) && near(b.y[0], 100));
    assert(near(b.x[2], 90) && near(b.y[2], 100));
}

static void the_hour_hand_creeps_and_the_minute_hand_follows_the_seconds() {
    Angles a = angles(3, 0, 0);
    assert(near(a.hour, 90) && near(a.minute, 0) && near(a.second, 0));
    a = angles(12, 30, 0);
    assert(near(a.hour, 15) && near(a.minute, 180));         // half past twelve: the hour hand is half way to one
    a = angles(15, 45, 30);                                   // 24-hour input wraps to 3:45:30
    assert(near(a.hour, 112.75f) && near(a.minute, 273.0f) && near(a.second, 180));
    a = angles(0, 0, 0);
    assert(near(a.hour, 0));
}

int main() {
    twelve_ticks_are_major();
    ticks_sit_at_the_right_angles_and_lengths();
    every_tick_stays_inside_the_ring();
    a_hand_at_twelve_points_straight_up();
    a_hand_at_three_points_right();
    the_hour_hand_creeps_and_the_minute_hand_follows_the_seconds();
    printf("clock_face: all tests passed\n");
    return 0;
}
```
`tests/run_clock_face_test.sh`:
```bash
#!/bin/bash
# Builds and runs tests/clock_face_test.cpp on the host. Needs no libraries.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc/app/clock tests/clock_face_test.cpp -o "$OUT/clock_face_test"
"$OUT/clock_face_test"
```
Add to `tests/run_host_tests.sh` after the `theme slug policy` line: `run "clock face geometry"                bash tests/run_clock_face_test.sh`

- [ ] **Step 2: Run to verify it fails**

```bash
chmod +x tests/run_clock_face_test.sh
bash tests/run_clock_face_test.sh 2>&1 | tail -2
```
Expected: FAIL to compile, `clock_face.h: No such file or directory`.

- [ ] **Step 3: Implement `src/app/clock/clock_face.h`**

```cpp
#pragma once
// The geometry of the drawn clock face. Pure math with no LVGL, so the layout is tested on the desktop and the
// drawing in clock_view.cpp only has to put pixels where this says.
#include <math.h>

namespace clock_face {

constexpr float DEG2RAD = 0.017453292519943295f;

struct Layout {
    float cx, cy;              // dial centre
    float ringR;               // radius of the ring's centre line
    float ringW;               // ring stroke
    float majorLen, minorLen;  // tick lengths, measured inward from the ring
    float majorW, minorW;      // tick stroke
};

// The 466x466 round screen: a ring hugging the bezel, twelve long ticks and forty-eight short ones.
constexpr Layout DEFAULT_LAYOUT = { 233.0f, 233.0f, 226.0f, 4.0f, 24.0f, 10.0f, 5.0f, 2.0f };

struct Segment { float x0, y0, x1, y1; };

inline bool is_major(int i) { return i % 5 == 0; }

// Tick i (0..59, 0 at 12 o'clock, clockwise), running inward from just inside the ring.
inline Segment tick(const Layout &L, int i) {
    const float a = (float)i * 6.0f * DEG2RAD;
    const float sx = sinf(a), sy = -cosf(a);
    const float outer = L.ringR - L.ringW * 0.5f - 2.0f;
    const float inner = outer - (is_major(i) ? L.majorLen : L.minorLen);
    return { L.cx + outer * sx, L.cy + outer * sy, L.cx + inner * sx, L.cy + inner * sy };
}

struct Blade { float x[4], y[4]; };

// A tapered hand pivoting at (cx, cy): the tip `len` out, the tail `tail` behind the pivot, and `hw` half-width at
// the shoulder, which is 16% of the way out. Four corners, clockwise from the tip. The same shape clock_view.cpp's
// draw_hand_at() has always drawn for the compiled faces.
inline Blade blade(float cx, float cy, float angDeg, float len, float tail, float hw) {
    const float a = angDeg * DEG2RAD;
    const float dx = sinf(a), dy = -cosf(a);      // along the hand
    const float qx = cosf(a), qy = sinf(a);       // across it
    const float sx = cx + len * 0.16f * dx, sy = cy + len * 0.16f * dy;
    return { { cx + len * dx, sx + hw * qx, cx - tail * dx, sx - hw * qx },
             { cy + len * dy, sy + hw * qy, cy - tail * dy, sy - hw * qy } };
}

struct Angles { float hour, minute, second; };   // degrees clockwise from 12

// The hour hand creeps with the minutes and the minute hand with the seconds, as on a real watch.
inline Angles angles(int hour, int minute, float second) {
    const float mins = (float)minute + second / 60.0f;
    const float hrs = (float)(hour % 12) + mins / 60.0f;
    return { hrs * 30.0f, mins * 6.0f, second * 6.0f };
}

} // namespace clock_face
```

- [ ] **Step 4: Run to verify it passes**

```bash
bash tests/run_clock_face_test.sh 2>&1 | tail -2
```
Expected: `clock_face: all tests passed`, no warnings.

- [ ] **Step 5: Commit**

```bash
git add src/app/clock/clock_face.h tests/clock_face_test.cpp tests/run_clock_face_test.sh tests/run_host_tests.sh
git commit -m "Add clock_face.h: the drawn dial's ticks, hand shapes and angles, as pure math" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 10: Draw the dial and hands from the palette, per missing element

**Files:**
- Modify: `src/app/clock/clock_view.cpp`, `docs/HARDWARE_PENDING.md`
- Create: `tests/test_clock_wiring.py`

**Interfaces:**
- Consumes: `clock_face::*` (Task 9); `theme_style::paletteOn()` and `theme_style::palette()` (Task 4); the existing helpers `draw_disc`, `draw_needle_at`, `P`, `s_canvas`, `s_noTime`, `CX`, `CY` in `clock_view.cpp`.
- Produces: in `compose_custom`, a drawn dial when there is **no plate** and the theme is in palette mode; a drawn hand for hour, minute or second when there is **no sprite for that hand** and the theme is in palette mode; a hub after any drawn hand; the date under the hub. `sweep_possible()` refuses to sweep a second hand that is drawn. **Images always win**, per element; a legacy theme (no palette) with no plate draws exactly as it does today.

The drawing itself cannot be unit-tested on the desktop (it is LVGL canvas calls), so this task is guarded two ways: a source-structure test that pins the "images win, per element" wiring (review focus 4), and looking at the result in the simulator.

- [ ] **Step 1: Write the failing wiring test**

`tests/test_clock_wiring.py`:
```python
"""The drawn face is only drawn where there is no image, and only in palette mode. That is a property of how
compose_custom is wired, and the drawing needs LVGL, so it is pinned here on the source."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'src' / 'app' / 'clock' / 'clock_view.cpp'


def code() -> str:
    return re.sub(r'//[^\n]*', '', SRC.read_text(encoding='utf-8'))


def compose_custom(text: str) -> str:
    start = text.index('static void compose_custom(')
    return text[start:text.index('static void draw_custom(', start)]


class DrawnFaceWiringTest(unittest.TestCase):
    def test_the_dial_is_drawn_only_when_there_is_no_plate_and_the_theme_has_a_palette(self):
        body = compose_custom(code())
        self.assertRegex(body, r'else\s*\{\s*lv_canvas_fill_bg\([^;]*\);\s*if \(theme_style::paletteOn\(\)\) draw_palette_dial\(ti\);')

    def test_a_hand_is_drawn_only_when_it_has_no_sprite(self):
        body = compose_custom(code())
        self.assertRegex(body, r'if \(spr\.data\) blend_custom_hand\([^;]*\);\s*else if \(k < 3 && theme_style::paletteOn\(\)\) \{')

    def test_the_drawn_pieces_are_defined(self):
        text = code()
        for name in ('draw_palette_dial', 'draw_palette_hand', 'draw_palette_hub'):
            self.assertIn(f'static void {name}(', text)

    def test_a_drawn_second_hand_never_sweeps(self):
        text = code()
        start = text.index('static bool sweep_possible()')
        body = text[start:text.index('static lv_area_t second_box(', start)]
        self.assertIn('custom_hand(2).data', body)


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run to verify it fails**

```bash
python3 -m unittest tests/test_clock_wiring.py 2>&1 | grep -E "^(FAIL|ERROR)|^Ran|^FAILED" | head -6
```
Expected: FAILED (nothing is drawn yet).

- [ ] **Step 3: The drawn face, in `clock_view.cpp`**

Use a patch script file; each `sub` asserts one match.

(a) The include. After `#include "custom_sprite.h"  // custom_plate()/custom_overlay()/custom_hand()` add `#include "clock_face.h"     // the drawn face's geometry, pure math (tests/clock_face_test.cpp)`.

(b) The drawing functions. Before the line `static void compose_custom(const struct tm *ti, bool skipSecond, bool withOverlay) {` insert:
```cpp
// ---- the drawn face ---------------------------------------------------------
// For any element a palette-mode theme ships no image for (see compose_custom): the dial when there is no plate,
// a hand when there is no sprite for it. Colours are the theme's roles, so a theme that is only a palette has a
// clock that matches the rest of its screens. It ticks once a second: no sweep, no cache, nothing to keep in step.
static inline lv_color_t role_colour(theme_roles::Role r) { return lv_color_hex(theme_style::palette().v[r]); }

static void draw_palette_dial(const struct tm *ti) {
    const clock_face::Layout &L = clock_face::DEFAULT_LAYOUT;
    {   // the ring: a circle outline, drawn as a rectangle whose corners are the whole radius
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        d.bg_opa = LV_OPA_TRANSP;
        d.border_opa = LV_OPA_COVER;
        d.border_color = role_colour(theme_roles::R_primary);
        d.border_width = (lv_coord_t)lroundf(L.ringW);
        d.radius = LV_RADIUS_CIRCLE;
        lv_canvas_draw_rect(s_canvas, (lv_coord_t)lroundf(L.cx - L.ringR), (lv_coord_t)lroundf(L.cy - L.ringR),
                            (lv_coord_t)lroundf(2 * L.ringR), (lv_coord_t)lroundf(2 * L.ringR), &d);
    }
    for (int i = 0; i < 60; ++i) {          // sixty ticks, every fifth long
        const clock_face::Segment t = clock_face::tick(L, i);
        lv_point_t pts[2] = { P(t.x0, t.y0), P(t.x1, t.y1) };
        lv_draw_line_dsc_t ld;
        lv_draw_line_dsc_init(&ld);
        const bool major = clock_face::is_major(i);
        ld.color = role_colour(major ? theme_roles::R_primary : theme_roles::R_dim);
        ld.width = (lv_coord_t)lroundf(major ? L.majorW : L.minorW);
        ld.opa = LV_OPA_COVER;
        lv_canvas_draw_line(s_canvas, pts, 2, &ld);
    }
    if (!s_noTime) {                        // the day and date, low on the dial where the hands are furthest away
        char ds[16];
        strftime(ds, sizeof(ds), "%a %d", ti);
        lv_draw_label_dsc_t ld;
        lv_draw_label_dsc_init(&ld);
        ld.color = role_colour(theme_roles::R_muted);
        ld.font = &lv_font_montserrat_18;
        ld.align = LV_TEXT_ALIGN_CENTER;
        lv_canvas_draw_text(s_canvas, (lv_coord_t)lroundf(L.cx - 60), (lv_coord_t)lroundf(L.cy + 112), 120, &ld, ds);
    }
}

// A tapered blade with an outline in the background colour so it reads against the ticks.
static void draw_blade(float ang, float len, float tail, float hw, lv_color_t fill, lv_color_t edge) {
    for (int pass = 0; pass < 2; ++pass) {
        const float grow = pass == 0 ? 1.4f : 0.0f;
        const clock_face::Blade b = clock_face::blade(CX, CY, ang, len + grow, tail + grow, hw + grow);
        lv_point_t pts[4] = { P(b.x[0], b.y[0]), P(b.x[1], b.y[1]), P(b.x[2], b.y[2]), P(b.x[3], b.y[3]) };
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        d.bg_color = pass == 0 ? edge : fill;
        d.bg_opa = LV_OPA_COVER;
        lv_canvas_draw_polygon(s_canvas, pts, 4, &d);
    }
}

// k: 0 hour, 1 minute, 2 second. The lengths fit inside the ticks (the ring's inner edge is at about 198 px).
static void draw_palette_hand(int k, float angDeg) {
    if (k == 2) {
        draw_needle_at(CX, CY, angDeg, 200, 40, 3, role_colour(theme_roles::R_secondary));
        return;
    }
    const lv_color_t fill = role_colour(theme_roles::R_primary), edge = role_colour(theme_roles::R_bg);
    if (k == 0) draw_blade(angDeg, 120, 22, 8.0f, fill, edge);
    else        draw_blade(angDeg, 186, 26, 6.0f, fill, edge);
}

static void draw_palette_hub() {
    draw_disc(CX, CY, 9, role_colour(theme_roles::R_primary));
    draw_disc(CX, CY, 3, role_colour(theme_roles::R_bg));
}

```
(c) The dial. Replace
```
    else lv_canvas_fill_bg(s_canvas, lv_color_hex(theme_style::clock().bg), LV_OPA_COVER);
```
with
```
    else {
        lv_canvas_fill_bg(s_canvas, lv_color_hex(theme_style::clock().bg), LV_OPA_COVER);
        if (theme_style::paletteOn()) draw_palette_dial(ti);   // no plate: draw the dial from the palette
    }
```
(d) The hands. Replace
```
        CustomSprite spr = custom_hand(k);
        if (spr.data) blend_custom_hand(spr.data, spr.w, spr.h, hd.pivotX, hd.pivotY,
                                        (float)hd.centerX, (float)hd.centerY, ang[k], hd.blend);
    }
    if (cs.textOverHands) draw_banners();
```
with
```
        CustomSprite spr = custom_hand(k);
        if (spr.data) blend_custom_hand(spr.data, spr.w, spr.h, hd.pivotX, hd.pivotY,
                                        (float)hd.centerX, (float)hd.centerY, ang[k], hd.blend);
        else if (k < 3 && theme_style::paletteOn()) {          // no image for this hand: draw it
            draw_palette_hand(k, ang[k]);
            drewHand = true;
        }
    }
    if (drewHand) draw_palette_hub();
    if (cs.textOverHands) draw_banners();
```
and declare the flag: replace `    bool sawSecond = false;\n    for (int i = 0; i < cs.orderN; ++i) {\n        const int k = cs.order[i];\n        if (k < 0 || k > 4) continue;` with the same preceded by `    bool drewHand = false;\n`. (The `bool sawSecond = false;` line that starts the hand loop appears once; the shadow loop above uses `sawSecondSh`.)

(e) No sweep for a drawn second hand. In `sweep_possible()`, after
```
    if (!cs.hand[2].show) return (s_sweepWhyNot = "the second hand is hidden", false);
```
add
```
    if (!custom_hand(2).data) return (s_sweepWhyNot = "the second hand is drawn, not an image", false);
```

- [ ] **Step 4: Run the wiring test, the suites and both builds**

```bash
python3 -m unittest tests/test_clock_wiring.py 2>&1 | tail -3
~/.platformio/penv/bin/pio run -e native > /tmp/t10-native.log 2>&1; tail -3 /tmp/t10-native.log
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 > /tmp/t10-device.log 2>&1; tail -3 /tmp/t10-device.log
bash tests/run_host_tests.sh 2>&1 | tail -3
```
Expected: `OK`, `SUCCESS` twice, `all host tests passed`. A compile error naming an LVGL call means the LVGL 8.4 signature differs from what is written here: read it in `.pio/libdeps/native/lvgl/src/extra/widgets/../draw/lv_draw_rect.h` and fix the call, keeping the drawing.

- [ ] **Step 5: Look at it (this is the acceptance for the clock)**

Write `bmp2png.py` in the scratch area:
```python
import glob
import sys

from PIL import Image

for f in glob.glob(sys.argv[1] + '*.bmp'):
    Image.open(f).save(f[:-4] + '.png')
print('converted')
```
Then, from the repo root:
```bash
echo default > /tmp/orb_sim_theme_slug
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SETTLE_MS=1500 .pio/build/native/program --themeshot /tmp/builtin > /tmp/builtin.log 2>&1 &
```
Wait about 45 seconds (the simulator ignores SIGTERM: `kill -9 %1`), then `python3 bmp2png.py /tmp/builtin` and open `/tmp/builtin-0-*.png`. **Expected:** a dark screen with a green ring, sixty ticks (twelve longer), an hour and minute hand in `primary` outlined in the background colour, a thin `secondary` seconds needle, a hub, and a small muted date under it; nothing brown, no photograph. Then do the same with `echo fallout > /tmp/orb_sim_theme_slug` (build Fallout into `sim/sdcard/themes` first): its clock must still be its own image with its own hands and **no** drawn ring, ticks or hub on top (review focus 4). Restore the slug file when done. If the drawn face looks wrong (hands too thick, ticks too dim), adjust the constants in `clock_face.h` (and the test that pins them) or the hand sizes in `draw_palette_hand`, and look again.

- [ ] **Step 6: Record it as unverified, and commit**

Append to `docs/HARDWARE_PENDING.md`:
```markdown
### Task 10: the drawn clock face

The drawn face was judged only in the simulator, and it is the biggest unknown in the spec: how long the canvas calls
take on the device.

- [ ] With the built-in look the clock shows the ring, ticks, hands, hub and date, and the hands advance once a second.
- [ ] The redraw is smooth: no visible tearing, no watchdog reset, no dropped knob turns while the clock is on screen.
      Measure the time one redraw takes (add a temporary `millis()` pair around `draw_custom()`); if it is over about
      50 ms, cache the dial (ring, ticks) in the PSRAM canvas and redraw only the hands.
- [ ] A theme with images (Fallout, Portal, Elegant) still shows only its own art, with no drawn hub or ticks on top.
- [ ] A theme with a plate but no hand images draws hands over the plate; a theme with hand images but no plate draws the
      dial under them.
```
```bash
git add src/app/clock/clock_view.cpp tests/test_clock_wiring.py docs/HARDWARE_PENDING.md
git commit -m "Clock: draw the dial and hands from the palette wherever the theme ships no image" -m "Per element, only in palette mode, ticking once a second. Looked at in the simulator; NOT verified on hardware, see docs/HARDWARE_PENDING.md." -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 11: The splash draws no compiled art in palette mode

**Files:**
- Modify: `src/theme/graphics/splash_art.cpp`, `src/app/ui/ui.cpp`, `docs/HARDWARE_PENDING.md`
- Create: `tests/test_splash_wiring.py`

**Interfaces:**
- Consumes: `theme_style::paletteOn()`, `app_theme::palette()` (Task 7).
- Produces: in palette mode (the built-in look, or a theme with a palette) with no splash image of the theme's own, `splash_art_decode()` returns `false` instead of decoding the compiled brown PNG, and the splash background is the palette's `bg`. The splash's version, network and credits lines and the theme's name are drawn by the existing `splash_lines` in the palette's colours (Task 5). The Office skin still uses its compiled art.

- [ ] **Step 1: Write the failing wiring test**

`tests/test_splash_wiring.py`:
```python
"""The compiled splash PNG must not be drawn in palette mode: it is the brown card the built-in look replaces."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def code(path: str) -> str:
    return re.sub(r'//[^\n]*', '', (ROOT / path).read_text(encoding='utf-8'))


class SplashWiringTest(unittest.TestCase):
    def test_the_compiled_fallback_is_skipped_in_palette_mode_unless_office(self):
        text = code('src/theme/graphics/splash_art.cpp')
        self.assertRegex(text, r'if \(!ok && !\(theme_style::paletteOn\(\) && !office\)\) \{')

    def test_the_splash_background_is_the_palette_background(self):
        text = code('src/app/ui/ui.cpp')
        self.assertIn('(office || theme_style::paletteOn()) ? app_theme::palette().bg : lv_color_black()', text)


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run to verify it fails**

```bash
python3 -m unittest tests/test_splash_wiring.py 2>&1 | grep -E "^(FAIL|ERROR)|^Ran|^FAILED" | head -4
```
Expected: FAILED (neither edit exists yet).

- [ ] **Step 3: The edits**

`src/theme/graphics/splash_art.cpp`: replace the line
```
    if (!ok) {
#if CUSTOM_HAS_SPLASH
```
with
```
    // In palette mode (the built-in look, or a theme that has a palette) there is no compiled card: the splash is the
    // palette's background with its lines drawn over it. The Office skin still has its own.
    if (!ok && !(theme_style::paletteOn() && !office)) {
#if CUSTOM_HAS_SPLASH
```
(the `if (!ok) return false;` a few lines later is what then returns).

`src/app/ui/ui.cpp`: in `ui_splash_show()` replace
```
    lv_obj_set_style_bg_color(cont, office ? app_theme::palette().bg : lv_color_black(), 0);
```
with
```
    lv_obj_set_style_bg_color(cont, (office || theme_style::paletteOn()) ? app_theme::palette().bg : lv_color_black(), 0);
```
If `ui.cpp` does not already include it, add `#include "theme_style.h"` beside its other theme includes (`grep -n '#include "theme_' src/app/ui/ui.cpp`).

- [ ] **Step 4: Run the test, the suites and both builds**

```bash
python3 -m unittest tests/test_splash_wiring.py tests/test_clock_wiring.py 2>&1 | tail -3
~/.platformio/penv/bin/pio run -e native > /tmp/t11-native.log 2>&1; tail -3 /tmp/t11-native.log
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 > /tmp/t11-device.log 2>&1; tail -3 /tmp/t11-device.log
bash tests/run_host_tests.sh 2>&1 | tail -3
```
Expected: `OK`, `SUCCESS` twice, `all host tests passed`.

- [ ] **Step 5: Look at the rest of the built-in look**

With `echo default > /tmp/orb_sim_theme_slug`, run the `--themeshot` as in Task 10 and open every `/tmp/builtin-*.png`: the flight tracker, weather, headlines, ticker, settings and the app-switcher menu. **Expected:** green on black throughout, nothing brown, no stray compiled art. The startup splash is not in `--themeshot`, so it is checked on the Orb (below). Note anything that looks wrong (an unreadable colour, a plate-less screen with an odd background) and fix it by changing that option's role in `theme_palette.cpp` and adding a line to `RoleDefaultsTest`, rather than special-casing a screen.

- [ ] **Step 6: Record it as unverified, and commit**

Append to `docs/HARDWARE_PENDING.md`:
```markdown
### Task 11: the built-in splash

- [ ] The startup splash with no theme is a flat dark card with the version, the network line, the credits and the
      theme's name ("Default") in the palette's colours; nothing brown.
- [ ] Settings > About shows the same, and a theme with its own `splash.png` still shows that.
```
```bash
git add src/theme/graphics/splash_art.cpp src/app/ui/ui.cpp tests/test_splash_wiring.py docs/HARDWARE_PENDING.md
git commit -m "Splash: no compiled art in palette mode; the background is the palette's" -m "Built for both environments; NOT verified on hardware, see docs/HARDWARE_PENDING.md." -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 12: Docs, the version, and the whole suite

**Files:**
- Modify: `docs/theme-yaml.md`, `docs/adding-a-screen.md`, `src/config.h`, `docs/HARDWARE_PENDING.md`

- [ ] **Step 1: Document the palette**

In `docs/theme-yaml.md`, insert this section immediately before the heading `## Fonts: define each face once`:
````markdown
## Palette: four colours and a look

A theme can be nothing but colours. Pick four; the firmware derives the rest and gives every colour option on every
screen a default from them:

```yaml
palette:
  bg:        0x0B0E11
  primary:   0xFF9A1F      # the accent: sweep, selection, the clock's hands
  secondary: 0x82CEFF      # a second accent: selected aircraft, the seconds hand
  text:      0xFFFFFF
```

Copy `src/theme_assets/default/` to start: it is the built-in look written down, with all eleven roles listed.

- The seven roles you do not pick are mixes of those four: `muted` (text toward the background), `dim` and `hairline`
  (primary toward the background), `panel`, `highlight` and `onPrimary` (whichever of background or text reads on
  primary). State any of them in the palette to override the mix. `alert` is a fixed red unless you state it.
- Any colour option can name a role instead of a number: `sweepColor: $secondary`. Only whole values are read as
  roles, so text such as `"$5.00"` is untouched; write `$$` for a literal `$`, and `$$primary` is refused.
- An option you state always wins. One you leave out takes its role's colour, **unless** the theme says
  `roleDefaults: false`, which keeps the firmware's compiled value for every option you do not state. Themes written
  before palettes existed (Fallout, Portal, Elegant) use it: they state only what differs from the compiled values.
- A theme with no images at all gets a drawn clock (a ring, sixty ticks, hands and the date) in these colours. An
  image always wins, per element: a plate but no hands, or hands but no plate, is drawn only where it is missing.
- With no theme selected, or the reserved theme `default`, the Orb draws the built-in palette. Settings > Design lists
  it first. Do not put a folder called `default` on the card: it is ignored.

````
Then, in the same file, add a sentence to the top of "Starting a new theme": replace
```
Copy `src/theme_assets/elegant/` and edit it. Its `theme.yaml` lists every option a theme can
```
with
```
The quickest start is a palette: copy `src/theme_assets/default/`, change its four colours, and every screen follows
(see "Palette" below). Copy `src/theme_assets/elegant/` instead when you want every option in front of you. Its
`theme.yaml` lists every option a theme can
```
In `docs/adding-a-screen.md`, replace the bullet
```
- **Colour**, and a **stale/alternate colour** if the element can go out of date.
```
with
```
- **Colour**, and a **stale/alternate colour** if the element can go out of date. Give it a default in
  `src/theme/core/theme_palette.cpp` (a role such as `text` or `primary`, not a hex), so a theme that is only a palette
  still colours it.
```

- [ ] **Step 2: Bump the version**

`2.18.0` was already the touch-swipe release on `main`, so this work is `2.19.0`. In `src/config.h` change `"2.18.0"` to `"2.19.0"` on the `FW_VERSION` line. In `docs/HARDWARE_PENDING.md` replace
```
- [ ] `FW_VERSION` is `2.18.0`. It was bumped **without** a hardware check, so treat 2.18.0 as unreleased until this
      whole list is ticked; the web config page and the Stats screen should show it.
```
with
```
- [ ] `FW_VERSION` is `2.19.0` (2.18.0 was already the touch-swipe release, so the fonts, boot-bake and palette work is
      2.19.0). It was bumped **without** a hardware check, so treat it as unreleased until this whole list is ticked;
      the web config page and the Stats screen should show it.
```
Then confirm nothing else quotes the old number as this work's version:
```bash
grep -rn '2\.18\.0' src docs/HARDWARE_PENDING.md docs/theme-yaml.md docs/adding-a-screen.md README.md CLAUDE.md
```
Expected: no output (the touch-swipe docs may legitimately mention 2.18.0; leave those).

- [ ] **Step 3: Everything, on the final tree**

```bash
bash tests/run_host_tests.sh 2>&1 | tail -4
python3 -m unittest tests/test_build_theme.py tests/test_font_bake.py tests/test_theme_fonts_golden.py tests/test_theme_font_coverage.py tests/test_boot_wiring.py tests/test_clock_wiring.py tests/test_splash_wiring.py tests/test_palettize.py tests/test_shipped_palettes.py tests/test_theme_resolved_golden.py tests/test_theme_palette.py tests/test_default_palette.py tests/test_elegant_theme.py tests/test_portal_theme.py tests/test_fallout_theme.py tests/test_build_all_themes.py > /tmp/t12-suite.log 2>&1; grep -E "^(FAIL|ERROR):|^Ran |^OK|^FAILED" /tmp/t12-suite.log
~/.platformio/penv/bin/pio run -e native > /tmp/t12-native.log 2>&1; tail -3 /tmp/t12-native.log
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 > /tmp/t12-device.log 2>&1; tail -3 /tmp/t12-device.log
```
Run the Python suite in the background and read its log (it is slow). Expected: `all host tests passed`, `OK`, and `SUCCESS` twice.

- [ ] **Step 4: Commit**

```bash
git add docs/theme-yaml.md docs/adding-a-screen.md src/config.h docs/HARDWARE_PENDING.md
git commit -m "Docs for palettes and the built-in default; bump FW_VERSION to 2.19.0" -m "2.18.0 was already the touch-swipe release. Unverified on hardware: see docs/HARDWARE_PENDING.md." -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

- [ ] **Step 5: HARDWARE GATE: an agent cannot pass this alone**

Nothing in Tasks 4 to 11 has run on a real Orb. **Do not call this work done until the owner has ticked the palette, boot-bake and clock sections of `docs/HARDWARE_PENDING.md`,** and report which were and were not checked. The two riskiest lines are the drawn clock's redraw time (the spec's biggest unknown) and a boot with no theme (the built-in look must not depend on anything on the card).

---

## Follow-on plans (not in this plan)

Spec step 4 (retire the Default/Office skins and the eight `app_theme::get()` branches) and step 5 (delete the compiled dials, hands, splashes, compiled bitmap fonts and the native font sources, then re-measure the ELF), then the final docs pass. Step 5 is what the built-in look was built for: after this plan nothing draws the compiled splash in the built-in look, and the compiled dials look unreachable already (`CUSTOM_CLOCK.active` is `true`, which forces the custom face). Step 5's plan must first confirm that, and decide what a **legacy theme with no `splash.png`** gets, because that still falls back to the compiled splash card.
