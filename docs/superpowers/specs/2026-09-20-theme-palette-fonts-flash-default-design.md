# Theme palette, shared fonts, and a default theme that lives in flash

Date: 2026-09-20. Status: design approved in conversation, awaiting written-spec review.

## Summary

Three changes to the theme system, shipped as six independent steps:

1. **Palette.** `theme.yaml` gets a `palette:` block. Colour values elsewhere in the file can say
   `$name` instead of repeating a hex code. Resolved by the builder; the firmware never sees a name.
2. **Fonts defined once.** `theme.yaml` gets a `fonts:` block of named faces and a slot-to-face
   map. The builder bakes each face once, and the device loads each distinct file once and shares it.
3. **No compiled theme fallbacks.** The `default` theme is baked into the `themeart` flash partition
   at install time (the theme's JSON as well as its art and fonts), so an Orb always has a theme.
   The compiled dials, hands, splashes, fonts and the Default/Office skins are deleted.

## Why

- **Repetition.** Fallout writes 70 colour literals for 13 distinct colours, Default 109 for 37,
  Portal 69 for 22. Fallout's `theme.yaml` already documents its palette in a comment, because the
  file has no way to state it.
- **Fonts.** Each text slot loads its own file. Fallout ships 22 `font_*.bin` files of which only 10
  differ in content. Every one of the 22 is parsed into PSRAM by `lv_font_load`.
- **Flash.** The compiled fallbacks are 1.36 MB of a 5.5 MB firmware image in a 6.25 MB app slot.
- **The guarantee is only half true today.** `main.cpp` says the Orb "really does run without a
  card, on the flash-baked artwork". Only art and fonts are baked. The theme's JSON (positions,
  colours, app list, hand geometry) is read from the SD card only (`theme_style.cpp`, the reader at
  line ~450). An Orb with no card draws baked art under compiled option values.

### Measurements

Taken from the linked ELF and map of the device build at commit `f24a113` (2026-09-20), not
estimated.

| Compiled theme material | Flash bytes |
|---|---|
| `DIAL_IMG` + `DIAL_AVI`, one full 466x466 RGB565 plate each | 868,624 |
| `SPLASH_PNG_DEFAULT` + `SPLASH_PNG_OFFICE` | 223,664 |
| Hand sprites: `HAND_*_IMG_MAP` 125,520, `CUSTOM_HOUR/MINUTE_PNG` 30,469, `OFFICE_*_PNG` 48,186 | 204,175 |
| Compiled fonts: `custom_menu_font1` 48,092, `custom_radar_font1` 8,956, `custom_radar_font2` 5,504 | 62,552 |
| **Removable total** | **1,359,015** |

- App slot is `0x640000` = 6,553,600 bytes; `firmware.bin` is 5,499,520. Headroom today is
  **1,054,080 bytes**, and would be about 2.4 MB after removal.
- **This is flash, not RAM.** Every one of these symbols is read-only data in flash. All of
  `src/theme/*` puts 18,748 bytes into DRAM. The RAM saving in this work is the shared fonts:
  22 loads become 10 for Fallout, about 53 KB of PSRAM (Default: 11 files, 8 distinct, about 14 KB).
- The Inter files (`font_inter_12..20`, 27,807 bytes) are the compiled size ladder that the text
  size sliders index into. They are **not** fallbacks and stay.

## Goals

- A theme states each colour once and each typeface once.
- A factory-fresh Orb with no card and no network still shows a complete, correctly themed UI.
- About 1.3 MB less compiled art in the app image, measured the same way as above.
- Every theme already installed on a card or in flash keeps working with no rebuild.

## Non-goals

- Runtime font scaling. LVGL fonts are bitmaps baked at one size, and `curved_text.cpp` reads the
  4-bit glyph bitmaps straight from the file.
- Palette arithmetic, alpha, or derived colours.
- Replacing the 128 hard-coded colour literals in `src/app` and `main.cpp` (51 of them in
  `radar_view.cpp`). Follow-up, as each screen is touched.
- Removing compiled *option values* (positions, colours a theme omits). They cost almost nothing
  and are what keeps already-installed partial themes working.
- Migrating themes other than Fallout, which serves as the proof.
- The stale `tools/orb-deploy.sh`, which still describes the Launch Kit flow.

## Design

### 1. Theme format (host side, `tools/build_theme.py`)

Two new allowed top-level keys: `palette` and `fonts`.

```yaml
palette:                        # name -> colour; hex only
  bright: 0x1BFF80
  mid:    0x11B25A
  tube:   0x021A0C
  # the nine AppPalette roles are ordinary names here: bg, panel, highlight,
  # ink, soft, dim, accent, hairline, onAccent

radar:
  sweepColor: $bright           # plain scalar, so it also works inside [..] and {..}

fonts:
  faces:                        # each defined once
    label:    {src: source/ShareTechMono-Regular.ttf, size: 16}
    title:    {src: source/ShareTechMono-Regular.ttf, size: 32}
    prebaked: {src: font_old.bin}          # a .bin is copied verbatim
  slots:                        # slot names are theme_font.cpp's own
    radar2: label
    radar3: label
    intel_title: title
```

**Palette rules**

- Names match `[a-z][a-z0-9_]*`. Values are `0xRRGGBB` or `#RRGGBB`; nothing else.
- `$name` anywhere a scalar is expected is replaced by its integer when the builder parses the YAML,
  so the built JSON is byte-for-byte what the firmware reads today.
- An undefined `$name` **fails the build**, naming the key path. An unused palette entry is a
  warning. `$$` gives a literal `$` in a text field. A `$` not followed by a letter (`$5`) is text.
- `tools/palettize.py <theme>` is report-only: it lists hex values used two or more times. It never
  rewrites, because the YAML parser loses comments.

**Font rules**

- A `.ttf`/`.otf` face is baked once with the pinned `lv_font_conv@1.5.3 --bpp 4 --no-compress`,
  reusing `converter()` and the default character ranges from `tools/fallout_fonts.py`, and written
  as `font_<face>.bin`. A `.bin` face is copied verbatim.
- The built `theme.json` gains `"fonts": {"<slot>": "<file>"}`.
- The builder reads the slot list from `theme_font.cpp`'s `SLOT_FILE` (the same source
  `tests/test_fallout_theme.py` already parses), so a misspelt slot fails the build rather than
  silently doing nothing.
- Size lives in the face. There is no runtime scaling and no size override on a slot.
- A theme with no `fonts` block builds exactly as it does today.

### 2. Firmware side

**Fonts** (`THEME_CAPS` 51 -> 52, with a ledger entry in `theme_style.h`; never renumber or reuse)

- `theme_style::load()` reads the `fonts` map from `theme.json` into a slot-to-filename table.
- `theme_font::begin()` loads each **distinct** filename once and gives every slot that names it the
  same `lv_font_t`. Fallout: 22 `lv_font_load` calls become 10.
- A slot with no map entry tries the legacy `font_<slot>.bin` name, so themes built earlier keep
  working. Failing that it gets `LV_FONT_DEFAULT`.
- `CUSTOM_*_FONT` fallbacks are removed. The `*_has_font` predicates stay: they still answer "did
  this slot load?" for callers that honour a size control.

**Palette roles** (`THEME_CAPS` 52 -> 53, second ledger entry)

- The builder copies the nine `AppPalette` role names, when the theme defines them, into a `palette`
  object in `theme.json`. Every other palette entry is build-time only.
- `app_theme::palette()` returns the theme's roles. A role a theme omits gets the corresponding
  value of **today's `APP_THEME_DEFAULT` palette** (the night-vision green set in `app_theme.cpp`),
  so a theme that never defined roles looks as it does now under Default. One neutral set, no
  compiled skins.
- Retired: `AppThemeId`, `name()`, `set()` and the saved choice; the Default/Office selector row in
  Settings; and the branches on `app_theme::get()`: `ui.cpp` (2), `settings_view.cpp` (3),
  `spycam_view.cpp`, `radar_view.cpp` (`officeMode()`), `clock_view.cpp`. There are 8 sites at the
  time of writing; the 12 `palette()` call sites stay and now read the theme.
- The old `appTheme` NVS key is left in place and never read. The namespace is permanent.
- An Orb that had Office selected uses whatever theme it has selected, or `default`.

**Compiled art removed**

- `dial_img.h`, `dial_avi.h` and the `memcpy` paths at `clock_view.cpp:199` and `:247`; the compiled
  hand headers and `custom_sprite.cpp`'s PNG fallbacks; `office_sprite.*` and the `office_*` PNG
  headers; `splash_png_default.h` and `splash_png_office.h`; the compiled `custom_*font*.c` files.
- Kept: compiled option values, the Inter ladder, LVGL's Montserrat, and the recovery screens.
- With no theme at all (themeart erased, no card) screens draw plain: flat colours and Montserrat.
  That is a degenerate case, no longer the normal one.

### 3. The flash-resident default theme

**JSON in flash.** The bake stores each theme JSON file as an `FMT_RAW` blob alongside art and
fonts. `theme_style`'s reader (`/themes/<slug>/<name>`) becomes **SD first, flash second**. SD
first preserves the edit-a-colour-and-reboot loop; flash is what an Orb gets with no card or no
folder. Known limitation: the two copies can disagree, and SD wins. The flash copy refreshes only
on a bake or a re-flash of the image.

**The image.** `tools/bake_theme_partition.py` turns the built `default` theme into a partition
image in the existing ORBT layout (`theme_art.cpp`: `MAGIC` `'ORBT'`, `VERSION` 6, 8 KB index of
64-byte entries, sector-aligned blobs), sized to the used bytes. A new PlatformIO target,
`pio run -t uploadtheme`, flashes it at the `themeart` offset read from
`partitions_16MB_themeart.csv` (`0x650000`, size `0x9A0000`). Plain `upload` still writes only the
app and leaves `themeart` alone. The boot log and the Stats screen report `default theme: baked` or
`missing`.

**Drift guard.** A host script and the device firmware read and write one binary format. To make
the format's single definition enforced rather than commented:

- The pure layout (`Header`, `Entry`, `VERSION`, index parse, lookup, and the keep-set selection
  below) moves out of the `#ifdef ARDUINO` block into `theme_art_index.{h,cpp}`, compiled for both
  device and host.
- The Python tool reads `VERSION` from that header.
- A host test looks up every asset of a Python-made image through the C++ code and checks width,
  height, format and bytes, and checks that host-baked RGB565 equals the firmware's own decoder
  output (`png_decode.cpp`) for the default theme's images.

**Protecting `default`.** Today `install_begin()` wipes *all* themes when under 4 MB
(`FULL_THEME_BYTES`) would remain (`theme_art.cpp`, line ~207). New rule, inside the shared
function: `default` is always in the keep-set. On a shortage every *other* theme is evicted; if the
new theme still does not fit, the install fails with a message. A `default` install replaces its own
entry as any theme does. `removeInstalled()` and `wipeAll()` act on SD folders only.

**`activeSlug()`** never returns `""`. A saved slug that is neither baked nor installed resolves to
`default`. Callers that test `activeSlug()[0]` to mean "no theme" are audited in the plan.

**Capacity.** The code comment puts a rich theme at about 3.8 MB against a 9.6 MiB partition; that
figure is unmeasured for `default`. Pinning `default` leaves room for `default` plus one more theme
rather than two or three. The plan's first task measures it.

**Simulator.** The native `theme_art::find_blob` (a stub today) reads
`sim/sdcard/themes/<slug>/<file>` into a cached heap buffer that is never freed, as a mapping would
not be. Fonts and JSON then load from the folder, and the compiled font `.c` files leave the native
env's `build_src_filter`. A missing `sim/sdcard/themes/default` is an error that names
`tools/build_all_themes.py`.

**Boot order** (as of `f24a113`, `main.cpp`): `sdcard::begin` 2334; `app_theme::init` 2349;
`theme_select::init` 2350; `theme_art::begin` 2359; `ui_splash_show` 2391; `theme_font::begin`
2395. Art and JSON are reachable before the splash. Fonts load after it, so the splash's text
falls back to Montserrat unless `theme_font::begin` moves earlier or the splash text is taken from
a font loaded before it. A freshly pushed theme bakes its fonts *between* those two lines, so any
move must keep font load after that bake. Task 1 of the plan settles this.

## Rollout

Each step is its own branch, tested, merged `--no-ff`, explicit staging. Each leaves the Orb
bootable. `FW_VERSION` is bumped in the steps that ship firmware.

1. **Palette.** Builder only: `palette:`, `$name`, `palettize.py`, tests. No firmware change.
2. **Fonts.** Builder `fonts:` block and slot map; firmware map and shared loading; Fallout
   migrated as the proof. Legacy per-slot names still load.
3. **Flash-resident theme.** JSON blobs, SD-first loading, `theme_art_index`, the host baker,
   `uploadtheme`, the `default` guard, `activeSlug()` never empty, simulator folder loader. The
   compiled fallbacks are still present, so the card can be pulled to prove it on hardware.
4. **Palette roles.** `app_theme` reads the theme; Default/Office retired.
5. **Delete the compiled art and fonts.** Re-run the ELF measurement. Target: at least 1.3 MB less.
6. **Docs.** `docs/theme-yaml.md`, `docs/adding-a-screen.md`, and every "compiled fallback"
   statement in `CLAUDE.md` and `README.md`. Step 5 makes them false, so they change with it.

## Testing

- **Builder** (`tests/test_build_theme.py`): undefined `$name` is an error with the key path;
  unused entry is a warning; `$$`; references inside `[..]` and `{..}`; built JSON never contains
  `$`; a misspelt slot, or a face whose `src` is missing, is an error; one `.bin` per face; a `.bin`
  face is copied verbatim; a theme with no `fonts` block builds as before.
- **Host C++**: the `fonts` map parse; shared loading (10 loads for Fallout, not 22); a Python-made
  image round-trips through `theme_art_index`; host-baked pixels equal the firmware decoder's; the
  keep-set never evicts `default`; a missing saved slug resolves to `default`.
- **`tests/test_default_theme.py`**: `default`'s YAML becomes hand-authored, so the test compares
  *resolved* values. `gen_default_theme.py --from-orb` stays as an import tool; its rewrite-in-place
  mode stops overwriting the file.
- **Regression net**: `--themeshot` for default, fallout and portal before and after each step.
  Only the expected regions may change.
- **Hardware** (CLAUDE.md rule 1; `?orb mem` before and after each): boot with the card pulled;
  boot with `themeart` erased and no card, expecting plain screens and the no-SD notice, no crash;
  upgrade an Orb that had Office selected; splash text; the app-slot size delta from the ELF.
  None of this is done until it has booted on a real Orb.

## Risks and open items

1. `default`'s baked size is unmeasured, so capacity is unconfirmed until step 3.
2. The splash's fonts load after it draws (see Boot order).
3. SD and flash JSON can disagree; SD wins. A JSON-only edit followed by removing the card shows
   the older flash copy.
4. A firmware-only flash onto blank `themeart`, with no card, leaves the Orb plain until
   `uploadtheme` is run once. Every Orb is flashed over USB, so this is a documented step, not a
   field problem.
5. If the host baker and the firmware's PNG decoder disagree on a stream (the decoder is known to
   misread some valid PNGs), the parity test flags it. The fix is to recompress that PNG, as was done
   for Fallout.
6. Themes on a card that lack a `palette` object change appearance only if they relied on the Office
   skin; those on Default (the shipped default) do not.
