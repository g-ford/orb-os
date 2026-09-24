# One wheel for the app picker and Settings

Status: design, awaiting review. Date: 2026-09-25.

## Intent

Settings and the app picker should look and behave as one control: the same wheel, dressed by the same theme
values, with no button-style background behind the selected row. Settings has more rows than the picker, so the
wheel must scale from five entries to as many as a list has. Nothing about a theme should be able to make the two
differ, and there should be no options left over that do nothing.

What was asked, in the user's words: Settings "should use the same theme settings as other apps", "no button
background", "very similar to the application picker"; then "remove the settings completely, no need to keep
vestigial options"; "happy for the settings wheel to stay as it"; "should use the same wheel as the app switcher
but with more available".

Decisions taken in review (all confirmed): one shared wheel (approach A); wheel geometry fixed in firmware, not a
theme option; the picker becomes a wheel of app names; the `settings:` and `menu:` theme blocks are removed
outright, not accepted and ignored.

Assumptions, not yet confirmed (see "Open for review"): the pills on the recovery pages go too; Settings opens on
its first row; the in-repo theme `vaultec` is migrated with the other four.

## What is there today

The picker ([menu_text.cpp](../../../src/app/settings/menu_text.cpp), driven from
[app_shell.cpp](../../../src/app/shell/app_shell.cpp)) and Settings
([settings_text.cpp](../../../src/app/settings/settings_text.cpp), `wheel_layout()` in
[settings_view.cpp](../../../src/app/settings/settings_view.cpp)) are two implementations of one idea:

| | Picker | Settings |
|---|---|---|
| Layout | three lines at theme-set x/y, `\|` breaks, `wrapWidth`, `lineStep` | curved, fading wheel, 10 rows, geometry from `settings:` |
| Glow | blurred stencil, dirty-rect repaint (fast) | 24 stamped copies per glyph (the approach `menu_text` measured at 358-829 ms) |
| Canvas | own ~868 KB PSRAM buffer | own ~868 KB PSRAM buffer |
| Colours, fonts | `menu:` block, slots `menu_current/prev/next` | `settings:` block, slots `settings`, `settings_sel` |
| Selected-row background | none | pill, on the main list and six sub-lists; also on the first-boot, network-list pages |
| Art | `menu_plate.png`, `menu_overlay.png` via `menu_sprite.cpp` | `settings_plate.png`, `settings_overlay.png` via `settings_sprite.cpp`, a near copy |

Two glyph blitters, two canvases, two glow implementations and two art loaders, kept in step by hand.

## Design

### The wheel module

New `src/app/wheel/` with a pure layout header and a drawing unit.

- `wheel_layout.h` (no LVGL, host-testable): given a row count, selected index and the constants below, returns
  each row's offset from the selection, screen x/y and opacity. Extracted from `wheel_layout()` and its row-fade
  rule, unchanged in behaviour. Also the ellipsis rule (cut to fit, end in `...`), currently duplicated in
  `settings_text::draw_item` and in a label helper.
- `wheel.h/.cpp`: owns the one canvas and the glyph and glow drawing. API sketch:
  - `wheel::acquire(parent)` / `release()`: allocate the canvas on showing the picker or Settings, free it on
    leaving. One buffer, replacing two. `available()` reports failure so callers keep a plain-label fallback.
  - `wheel::draw(const Row *rows, int count, int sel, Style style)`: clear, lay out, draw, invalidate only the
    union of this frame's and the last frame's written rectangle (the technique `menu_text` uses today).
  - `Style` is `THEMED` (palette colours, theme fonts) or `SYSTEM` (stock grey and white, Montserrat), which is
    today's `chrome()` distinction, kept because a theme must not be able to make WiFi setup illegible.
- Glow uses the blurred-stencil path from `menu_text`. One stencil per frame, for the selected row only, so cost
  does not grow with the list.

### Fixed values

Constants in `wheel.h`, taken from the current Settings defaults and tuned only if a screenshot shows a problem:
radius 170 px, horizontal bow 18 px, step 22 degrees, centre offset 0, fade 2.0, selected glow 8 px.

No theme option controls any of them.

### What a theme still controls

| Thing | Source |
|---|---|
| Selected row colour and glow colour | palette role `primary` |
| Other rows | palette role `muted` |
| Selected and other typefaces | two font slots, `wheel_sel` and `wheel_item` |
| Background and glass | the existing `menu_plate.png`/`menu_overlay.png` (picker) and `settings_plate.png`/`settings_overlay.png` (Settings) |

One art loader replaces `menu_sprite.cpp` and `settings_sprite.cpp`, called with an asset name. No theme artwork
changes.

Migration of font slots: `wheel_sel` takes the face each theme gave `menu_current`, `wheel_item` the face it gave
`settings`. `menu_prev`, `menu_next` and `settings_sel` go. The selected row therefore gets larger in Settings
(about 46 px against 27 today). The wheel's first-step spacing is 64 px, so it fits, but this is a look change to
check by eye and tune during implementation, not a settled value.

### Removing the pill

`style_highlight()` and all ten pill objects go (`s_hl`, `s_lmHl`, `s_fbHl`, `s_wifiHl`, `s_dspHl`, `s_sndHl`,
`s_chimeSelHl`, `s_designHl`, `s_rangeHl`, `s_unitsHl`). The selected row is marked by colour, size and glow.

On the recovery pages (two-row first-boot, network list, password) the pill is also the only non-colour selection
cue. They stay unthemed: stock colours, no pill, selected row white against grey as they are already coloured.
This is the item most worth checking on a real Orb.

### The picker

`app_shell` shows the overlay through `wheel::draw` with the visible apps as rows. It keeps its own concerns: the
settle timer, the detent accumulator and `next_visible()`. Names are single-line and clipped with `...`; the `|`
break marker has no meaning any more.

### Schema removal

Firmware:
- Delete the `Menu`, `MenuText` and `Settings` structs, their `s_menu`/`s_settings` state, their JSON merge code
  and the `menu_style.json`/`settings_style.json` reads in `theme_style.cpp/.h`.
- Delete the corresponding lines in `theme_palette.cpp`, and the `apply_role_defaults` parameters.
- Delete `custom_menu.h` and `custom_settings.h` (compiled fallbacks) and the now-unused `custom_*_plate.h` /
  `_overlay.h` guards only where nothing else includes them.
- `theme_font.cpp`: slots `S_MENU_CUR/PREV/NEXT/SETTINGS/SETTINGS_SEL` become `S_WHEEL_SEL`, `S_WHEEL_ITEM`,
  compiled fallback Montserrat.
- `Settings > default selection` (`defaultSel`) goes with the block. Settings opens on its first row.

Builder and theme format:
- `tools/build_theme.py`: drop `settings` and `menu` from `SECTIONS`. A theme that still has either key, or a
  font slot that no longer exists, fails with a message naming the replacement (font slots already fail this way,
  since the builder reads the slot list from `theme_font.cpp`).
- Bump `THEME_CAPS` from 53 to 54 with a ledger entry saying what was removed. A card already holding an old build
  keeps a `settings_style.json` the firmware no longer reads; nothing loads it.

Themes: migrate `default`, `elegant`, `fallout`, `portal`, `vaultec`, and regenerate the three golden files in
`tests/golden/`.

### Testing

- New host test for `wheel_layout.h`: row positions and opacities at several counts and selections against
  values captured from today's `wheel_layout()` first, so "unchanged geometry" is checked rather than asserted;
  ellipsis cases including a string that fits exactly and a single very wide glyph.
- Update `test_theme_style_isolation.py`, `test_theme_palette.py`, `test_no_compiled_art.py`,
  `test_theme_font_coverage.py` and the three goldens for the removed blocks. Add a builder test that the old keys
  are refused with the replacement named.
- Simulator: `--settingsshot` and `--themeshot` for each of the five themes, plus a picker capture. `--rockshot` and
  `--knobshot` drive the live app switcher; whether either photographs the picker overlay is to be checked first,
  and a `--menushot` added alongside the others only if neither does. Screenshots are read by eye; they are not a pass
  criterion.
- Full run: `bash tests/run_host_tests.sh`, and the whole Python test directory (memory: a guard has been left red
  before, and the skipped count matters).
- Performance guard: the picker must not regress to a full-screen repaint per detent. The dirty-rect behaviour
  is kept and asserted where it can be tested on the host.

### Order of work

One branch per logical change, each tested, merged `--no-ff`:
1. `wheel_layout.h` and its test, geometry captured from current code.
2. `wheel` module and merged art loader.
3. Settings onto the wheel; pills deleted.
4. Picker onto the wheel.
5. Schema, builder and caps removal; theme migration; goldens; docs (`theme-yaml.md`, `adding-a-screen.md`,
   `ARCHITECTURE.md`).
6. `FW_VERSION` 2.22.0 and a `docs/HARDWARE_PENDING.md` entry.

Steps 3 and 4 change what appears on the glass but nothing has run on an Orb since 2026-09-21. This will be
simulator-verified only, and recorded as unverified on hardware, per CLAUDE.md rule 1.

## Out of scope

- The README and NOTICE refresh and the whole-project review are requested separately and follow this work, so
  they describe the finished state rather than a moving one.
- Non-wheel Settings pages beyond removing their pill (the WiFi password entry, Reset, About).
- Any change to theme art, palettes or the other apps.

## Open for review

1. Pills removed on the recovery pages as well as the wheels (above). Alternative: keep a pill on those three
   pages only. It costs consistency and keeps a background the request said should not exist.
2. Settings opens on its first row now that `defaultSel` is gone. Themes used it to open on a favourite row.
3. `vaultec` is now tracked in this branch's base (it was untracked in the main checkout). The main checkout still
   has an untracked `src/theme_assets/vaultec/` that will collide when this lands; the untracked copy is left
   alone and will need reconciling by hand. `Untitled theme.orb` is untouched.
4. The selected-row size jump in Settings (27 to ~46 px) is a default to be tuned by eye, not a decision.
