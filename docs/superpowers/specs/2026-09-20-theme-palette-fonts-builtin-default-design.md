# Theme palette roles, shared fonts, and a built-in `default` theme

Date: 2026-09-20 (revised 2026-09-21). Status: design approved in conversation, awaiting
written-spec review. Supersedes the first version of this file, which put the default theme in a
flash partition image; that approach was dropped (see "Decisions").

## Summary

1. **Palette roles the firmware understands.** A theme picks four colours (`bg`, `primary`,
   `secondary`, `text`); the firmware derives the rest and gives every colour option a default role.
   A simple theme is just a palette.
2. **Fonts defined once.** `theme.yaml` gets a `fonts:` block of named faces and a slot-to-face map.
   The builder bakes each face once; the device loads each distinct file once and shares it.
3. **A built-in `default` theme.** Palette-driven and procedural: no compiled bitmaps, nothing to
   flash. It is what an Orb shows when no theme is selected.
4. **`default` becomes that built-in look; the current default theme is renamed `elegant`.**
   `elegant`, `portal` and `fallout` are all migrated to the new format with no visual change,
   except that Portal gets a chosen typeface (Barlow) with one face across its radar text.
5. **The compiled theme fallbacks are deleted**: the dials, hands, splash PNGs, Office sprites, the
   custom bitmap fonts, and the Default/Office skins.

## Why

- **Repetition.** Fallout writes 70 colour literals for 13 distinct colours, the current default
  109 for 37, Portal 69 for 22. Fallout documents its palette in a comment because the file has no
  way to state it. Themes really do use few colours: Fallout's top five cover 50 of its 70 uses,
  Portal's top five 42 of 69, and Portal's four core colours (orange `FF9A1F`, background `0B0E11`,
  white, light blue `82CEFF`) cover 38.
- **Fonts.** Each text slot loads its own file. Fallout ships 22 `font_*.bin` files of which only 10
  differ in content, and every one of the 22 is parsed into PSRAM.
- **Flash.** The compiled fallbacks are 1.36 MB of a 5.5 MB firmware image in a 6.25 MB app slot.
- **Two palette systems.** `app_theme.cpp` has a compiled 9-role `AppPalette` with Default and
  Office skins, separate from the theme system, and about 128 colour literals are hard-coded in
  `src/app` and `main.cpp`.

### Measurements

From the linked ELF and map of the device build at commit `f24a113` (2026-09-20).

| Compiled theme material | Flash bytes |
|---|---|
| `DIAL_IMG` + `DIAL_AVI`, one full 466x466 RGB565 plate each | 868,624 |
| `SPLASH_PNG_DEFAULT` + `SPLASH_PNG_OFFICE` | 223,664 |
| Hand sprites: `HAND_*_IMG_MAP` 125,520, `CUSTOM_HOUR/MINUTE_PNG` 30,469, `OFFICE_*_PNG` 48,186 | 204,175 |
| Compiled bitmap fonts: `custom_menu_font1` 48,092, `custom_radar_font1` 8,956, `custom_radar_font2` 5,504 | 62,552 |
| **Removable total** | **1,359,015** |

- App slot `0x640000` = 6,553,600 bytes; `firmware.bin` 5,499,520. Headroom today is
  **1,054,080 bytes**, about 2.4 MB after removal.
- **This is flash, not RAM.** Every one of these symbols is read-only flash data; all of
  `src/theme/*` puts 18,748 bytes into DRAM. The RAM saving in this work is shared fonts: 22 loads
  become 10 for Fallout, about 53 KB of PSRAM.
- The Inter files (`font_inter_12..20`, 27,807 bytes) are the compiled size ladder that the text
  size sliders index into. LVGL's Montserrat is what the menu's prev/next labels and Settings use
  (`CUSTOM_MENU_NEXT_FONT`, `CUSTOM_SETTINGS_FONT`). Neither is a fallback; both stay.

## Goals

- A simple theme is a `palette:` of four colours and a name, and every screen looks designed.
- A theme states each colour once and each typeface once.
- An Orb with no theme selected, no card, and no network shows a complete, coherent look.
- About 1.3 MB less compiled art in the app image, measured the same way as above.
- Every theme already on a card keeps working, unchanged, with no rebuild.

## Non-goals

- Runtime font scaling. LVGL fonts are bitmaps baked at one size, and `curved_text.cpp` reads the
  4-bit glyph bitmaps straight from the file.
- Palette alpha, or arbitrary user-defined colour names or arithmetic. Roles are a fixed list.
- Replacing all 128 hard-coded colour literals in one go. The binding table grows screen by screen.
- Putting theme JSON, or a rich default theme, into flash. Follow-up if wanted: an Orb with no card
  and a *rich* theme selected still gets its options from compiled values, as it does today.
- The stale `tools/orb-deploy.sh`, which still describes the Launch Kit flow.

## Naming

- **`default`** is the built-in look and a **reserved slug**. Selecting it, or selecting nothing,
  loads no folder. A card folder named `default` is ignored and not listed.
- `src/theme_assets/default/` is a new, palette-only `theme.yaml` (four colours and a name). It is
  the template for a new theme and the worked example. A host test asserts its palette equals the
  firmware's built-in palette, so the two cannot drift.
- **`elegant`** is the current default theme (the Studio-imported look), renamed: folder, `slug`,
  `name`. It remains the exhaustive reference that lists every option a theme can set.
- The `default:` key in `theme.json` is not read by the firmware (no code references it), so it is
  cosmetic: `true` for the new `default`, `false` for the rest.
- An Orb whose saved slug is `default` (today's Studio look) becomes the built-in look after the
  upgrade. The owner copies `elegant` to `/themes/elegant/` and selects it once.

## Design

### 1. Fonts

**Theme format** (`tools/build_theme.py`): a new top-level `fonts:` key.

```yaml
fonts:
  faces:                        # each defined once
    label:    {src: source/ShareTechMono-Regular.ttf, size: 16}
    title:    {src: source/ShareTechMono-Regular.ttf, size: 32}
    prebaked: {src: font_old.bin}           # a .bin is copied verbatim
  slots:                        # slot names are theme_font.cpp's own
    radar2: label
    radar3: label
    intel_title: title
```

- A `.ttf`/`.otf` face is baked once with the pinned `lv_font_conv@1.5.3 --bpp 4 --no-compress`,
  reusing `converter()` and the default character ranges from `tools/fallout_fonts.py`, and written
  as `font_<face>.bin`. A `.bin` face is copied verbatim.
- The built `theme.json` gains `"fonts": {"<slot>": "<file>"}`.
- The builder reads the slot list from `theme_font.cpp`'s `SLOT_FILE` (the source
  `tests/test_fallout_theme.py` already parses), so a misspelt slot fails the build.
- Size lives in the face. There is no runtime scaling and no size override on a slot.
- A theme with no `fonts` block builds exactly as it does today.

**Firmware** (`THEME_CAPS` 51 -> 52, with a ledger entry in `theme_style.h`; never renumber or reuse):

- `theme_style::load()` reads the `fonts` map into a slot-to-filename table.
- `theme_font::begin()` loads each **distinct** filename once and gives every slot that names it the
  same `lv_font_t`. Fallout: 22 `lv_font_load` calls become 10.
- A slot with no map entry tries the legacy `font_<slot>.bin` name (themes built earlier keep
  working), and failing that gets `LV_FONT_DEFAULT`.
- The compiled `CUSTOM_*_FONT` bitmap faces are removed in step 5. The `*_has_font` predicates stay.

**Coverage rule** (rule 4: a guard, not a comment). Only three faces are compiled bitmap fonts:
menu current (`custom_menu_font1`, 46 px) and radar text 1 and 2 (`custom_radar_font1`,
`custom_radar_font2`). A host test asserts that every shipped theme supplies a face for those three
slots. Without it, deleting the compiled fonts silently changes a theme's lettering. The other
compiled mappings are LVGL's own Montserrat (radar text 3 = 20, menu prev/next = 16, Settings = 20)
and are not compiled theme material; step 5 keeps them as a small per-slot table, so those slots draw
exactly as they do today.

### 2. Palette roles

**Roles.** A theme picks four; every other role is derived unless the theme sets it. One C++
function does every mix (an RGB interpolation), used by every consumer. The ratios are starting
values, tuned by eye with `--themeshot` in the plan.

| Role | Picked or derived | Starting rule |
|---|---|---|
| `bg`, `primary`, `secondary`, `text` | picked (required in palette mode) | |
| `muted` | derived | `text` mixed 45% toward `bg` |
| `dim` | derived | `primary` mixed 55% toward `bg` |
| `hairline` | derived | `primary` mixed 80% toward `bg` |
| `panel` | derived | `bg` mixed 6% toward `text` |
| `highlight` | derived | `bg` mixed 25% toward `primary` |
| `on-primary` | derived | whichever of `bg` or `text` contrasts more with `primary` |
| `alert` | derived | fixed red (`0xE5484D`) unless set |

The nine `AppPalette` roles fold into this set (`ink` = `text`, `accent` = `primary`, and so on), so
there is one palette system, not two. The radar's six altitude bands default to a ramp:
`muted`, `secondary`, the midpoint of `secondary` and `primary`, `primary`, `primary` toward `text`,
`text`. Any band can still be overridden.

**In `theme.yaml`:**

```yaml
palette:
  bg:        0x0B0E11
  primary:   0xFF9A1F
  secondary: 0x82CEFF
  text:      0xFFFFFF
  # any other role may be set here to override its derivation

radar:
  sweepColor: $secondary        # a role reference; the firmware resolves it
  ringColor:  0x1E3A2E          # explicit hex always wins
```

- A colour value is an integer (hex) or a role name (`$role`). The builder validates role names
  against a list read from firmware source (`theme_roles.h`), the way slot names are validated; an
  unknown role fails the build with the key path. `$role` is passed through as a string, and hex as
  an integer. The builder does no colour arithmetic, so the mixing exists in exactly one language.
- `tools/palettize.py <theme>` is report-only: it lists repeated hex values and suggests a role for
  each. It never rewrites a file, because the YAML parser loses comments.

**Firmware** (`THEME_CAPS` 52 -> 53, second ledger entry):

- `theme.json` carries a `palette` object. Load order: read `theme.json`; resolve the roles;
  initialise every role-bound colour option from one table (option -> role); then apply the screen
  JSON overrides, each an integer or a role name. The colour fields stay `uint32_t`, so no renderer
  changes. The table is sparse: an option with no binding keeps its compiled value, so adoption is
  incremental. The plan starts with radar, weather, ticker, headlines, settings, menu and splash.
- **Palette mode applies only when `theme.json` has a `palette`.** Without one, a theme keeps
  today's compiled values exactly. Portal and Fallout state only what differs from those values, so
  binding by default would silently restyle them.
- `app_theme::palette()` returns the resolved roles. In legacy mode it returns the built-in palette.
- The built-in palette is today's night-vision `APP_THEME_DEFAULT`: `bg` `0x000000`, `primary`
  `0x1DFF86`, `text` `0xEAFFF3`, and `secondary` `0x9AFFC8` (today's `soft`; a starting value).
- Retired: `AppThemeId`, `name()`, `set()` and the saved choice; the Default/Office selector row in
  Settings; and the branches on `app_theme::get()`: `ui.cpp` (2), `settings_view.cpp` (3),
  `spycam_view.cpp`, `radar_view.cpp` (`officeMode()`), `clock_view.cpp`. There are 8 sites at the
  time of writing. The old `appTheme` NVS key is left in place and never read; the namespace is
  permanent.

### 3. The built-in `default` look

- **Procedural clock**, used only where the theme ships no image (per element; images always win).
  The dial is drawn once into the clock's existing canvas: a `primary` ring, 12 major ticks in
  `primary`, 48 minor ticks in `dim`, fill `bg`, numerals off by default. The hour and minute hands
  are tapered polygons in `primary`, the second hand in `secondary`, drawn with the same
  `lv_canvas_draw_polygon` that `clock_view.cpp:151` already uses. Date and time text use `text`.
  Redraw cost on the device is unmeasured. If per-frame polygons are too slow, hands redraw once a
  second and sweeping seconds are off in procedural mode.
- **Splash and menu**, with no image: the splash is flat `bg` plus the theme name in `primary`; the
  menu uses `bg` with `text` and `muted`. Both use the theme's font slot if it loaded, else
  Montserrat.
- **Radar, weather, ticker, headlines, settings** already draw in colour and take their colours from
  the binding table. Their plates are optional pictures, absent in the built-in look.
- `activeSlug()` returning `""`, or `default`, means the built-in look. Nothing is baked or flashed.

### 4. Compiled art removed (step 5)

`dial_img.h`, `dial_avi.h` and the `memcpy` paths at `clock_view.cpp:199` and `:247`; the compiled
hand headers and `custom_sprite.cpp`'s PNG fallbacks; `office_sprite.*` and the `office_*` PNG
headers; `splash_png_default.h` and `splash_png_office.h`; the compiled `custom_*font*.c` bitmap
faces. Kept: compiled option values, the Inter ladder, LVGL's Montserrat (with the small per-slot
fallback table described under the coverage rule), and the recovery screens.

### 5. Simulator

The native `theme_art::find_blob` is a stub today, so the simulator gets its fonts from the compiled
files. Step 5 makes it read `sim/sdcard/themes/<slug>/<file>` into a cached heap buffer (never
freed, as a mapping is not), and the compiled font `.c` files leave the native env's
`build_src_filter`. The built-in `default` needs no folder, so the simulator starts with none.

## Migrating the shipped themes

`elegant`, `portal` and `fallout` all move to the new format: a `palette:` block, `$role`
references in place of repeated hex, and a `fonts:` block. **Invariant: no visual change**, with
one deliberate exception: Portal's typeface (D1, below).

- **Golden first.** Before any theme is edited, a host tool resolves each theme through the real
  C++ code and records every colour field's value and every slot's font file to
  `tests/golden/<theme>.txt`. That commit comes first, so no commit fails its own test. After
  migration the tool must reproduce the file byte for byte.
- **Portal and Fallout omit many options** and rely on compiled values. In palette mode those
  options would resolve through role bindings and could shift, so the migrated YAML states any
  colour where the binding would differ, and the golden test proves it.
- **`elegant`** ships 11 pre-baked `.bin` fonts and no typeface source, so its faces are `.bin`
  passthrough (11 files become 8 distinct faces, by content). It already covers menu current and
  radar text 1-2, so the coverage rule needs nothing more from it.
- **`fallout`** already has its TTF and `tools/fallout_fonts.py`; the script's slot table becomes the
  `fonts:` block (22 slots, 10 faces).
- **`portal`** ships no fonts at all: its lettering is the compiled `custom_menu_font1` (46 px) and
  radar faces, generated from a temporary TTF that is not in the repo. **Decision D1 (resolved)**:
  Portal gets a deliberately chosen typeface, **Barlow**, baked from a TTF committed under
  `src/theme_assets/portal/source/` with its OFL licence text. Barlow is an open-licence
  (SIL OFL) DIN-style sans, chosen because Portal's test chamber is clinical and industrial and the
  face stays legible at 16-20 px on the AMOLED. It is confirmed against the licence file that ships
  with the font when it is added, and against a `--themeshot` specimen before it is committed. This
  is a deliberate change of Portal's lettering, so Portal's fonts golden is regenerated; its colours
  golden is not. Portal gets Barlow for every slot Fallout themes (menu, Settings, weather,
  headlines, ticker, wind), at the same slot sizes as `tools/fallout_fonts.py`'s table, with one
  exception: **all four radar text slots map to a single face at a single size** (starting value
  20 px, what radar text 3 draws today), so the callsign line is no longer larger than the others.
  The radar's pill layout is re-checked with `--themeshot` at that size.
- `tools/gen_default_theme.py` is retargeted at `elegant`; `tests/test_default_theme.py` becomes
  `tests/test_elegant_theme.py`. The other references to sweep (counted at `f24a113`):
  `docs/theme-yaml.md` (10), `docs/adding-a-screen.md` (3), `tools/render_theme_bitmaps.py` (3),
  `tools/dump_theme_defaults.cpp` (3), `tools/build_theme.py` (2, docstrings),
  `tools/fallout_fonts.py` (1), `tests/test_portal_theme.py` (1), `tests/test_fallout_theme.py` (1),
  and the header comments of the Portal and Fallout `theme.yaml`.
- `tests/test_fallout_theme.py` fails on any colour whose red or blue exceeds its green. Once
  colours can be role names it must read the *resolved* values, from the same host tool.

## Rollout

Each step is its own branch, tested, merged `--no-ff`, explicit staging. Each leaves the Orb
bootable. `FW_VERSION` is bumped in the steps that ship firmware.

0. **Rename `default` to `elegant`.** Folder, slug, name, the reference sweep, the retargeted
   tool and test, docs. No firmware change. Note for the owner: copy `elegant` to the card and
   select it, because the saved slug `default` now points at the built-in.
1. **Fonts.** Builder `fonts:` block and slot map; firmware shared loading; the coverage test;
   `elegant`, `fallout` and `portal` migrated, Portal with its new Barlow faces and a single
   radar face. `THEME_CAPS` 52.
2. **Roles and the built-in look.** `theme_roles.h`, resolver and derivation, binding table,
   reserved `default` slug, the palette-only `default` folder and its drift test, `AppPalette` from
   roles, the host dump tool and goldens (goldens committed first), and the palette migration of the
   three themes. `THEME_CAPS` 53. Acceptance: a `--themeshot` of every screen from the four-colour
   `default` theme.
3. **Procedural clock face.** Proven for speed on hardware.
4. **Retire Default/Office.** The 8 sites and the Settings selector row.
5. **Delete the compiled art and fonts**, add the simulator folder font loader, re-measure the ELF.
   Target: at least 1.3 MB less.
6. **Docs.** `docs/theme-yaml.md`, `docs/adding-a-screen.md`, and every "compiled fallback"
   statement in `CLAUDE.md` and `README.md`. Step 5 makes them false, so they change with it.

## Testing

- **Builder** (`tests/test_build_theme.py`): `$role` passes through as a string; hex as an integer;
  an unknown role is an error with the key path; a misspelt slot, or a face whose `src` is missing,
  is an error; one `.bin` per face; a `.bin` face is copied verbatim; a theme with no `fonts` or
  `palette` builds as before.
- **Host C++**: the derivation function (determinism, and the `on-primary` contrast rule); every
  bound option resolves; the int-or-role JSON parse; shared font loading (10 loads for Fallout, not
  22); the `fonts` map parse.
- **Goldens**: each migrated theme resolves byte-identical to its pre-migration golden (Portal's
  fonts golden is regenerated on purpose, D1; its colours golden is not); a legacy theme (no
  `palette`) is unchanged from before this work.
- **Naming**: `activeSlug()` of `""` or `default` is the built-in look; a card folder named
  `default` is ignored and unlisted; `default/theme.yaml`'s palette equals the firmware's.
- **Coverage**: every shipped theme covers every slot that would reach a compiled bitmap face.
- **Regression net**: `--themeshot` for default, elegant, fallout and portal before and after each
  step; only the expected regions may change.
- **Hardware** (CLAUDE.md rule 1; `?orb mem` before and after each): boot with no card and no theme
  selected, expecting the built-in look and no crash; procedural clock frame time; upgrade an Orb
  that had `default` (the Studio look) selected; splash text; the app-slot size delta from the ELF.
  None of this is done until it has booted on a real Orb.

## Decisions and risks

**D1 (resolved by the owner, 2026-09-21): Portal's typeface.** Portal never chose one; it inherited
compiled faces. It gets a deliberately chosen open-licence face (Barlow, see "Migrating the shipped
themes"), and its radar text slots share one face. The alternatives considered were extracting the
compiled faces into `.bin` files (exact lettering, but a one-off converter) and keeping the compiled
faces (62,552 bytes, 4.6% of the saving).

1. The procedural clock's speed and looks are the biggest unknown.
2. Portal's radar text drops from three sizes to one, so the pills may need their padding adjusted.
   This is the reading of "one font across all radar text lines" that was taken: one typeface at one
   size. If one typeface at several sizes was meant, only the size in the radar face entries changes.
3. The derivation ratios need tuning by eye, especially for light or low-saturation themes.
4. Two default sets (legacy compiled values, role-bound) coexist until legacy themes are migrated.
   All three shipped themes migrate here, so only user themes remain in legacy mode.
5. Existing Orbs: the saved slug `default` becomes the built-in look. The old baked Studio art stays
   in `themeart` under the slug `default`, as another theme's entry, until an install runs short of
   room and wipes it. Harmless, and self-clearing.
6. An Orb with no card and a *rich* theme selected still gets its options from compiled values, as
   today (non-goal above).
7. The splash draws before `theme_font::begin()` (`main.cpp` 2391 vs 2395), so its text is
   Montserrat unless fonts load earlier. Acceptable for the built-in look; step 1 checks the rest.
