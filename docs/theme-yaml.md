# Writing a theme in YAML

A theme is a folder: `theme.yaml` plus its images and fonts. `tools/build_theme.py` turns
that folder into the one the Orb reads from its SD card. The device never sees the YAML; it
reads the same JSON files it always has (`theme_style.cpp`).

```
pip3 install pyyaml                                  # once
python3 tools/build_theme.py src/theme_assets/default # -> build/themes/default/
python3 tools/build_theme.py src/theme_assets/default --out /Volumes/ORB/themes
```

Copy `<out>/<slug>/` to `/themes/<slug>/` on the card and pick it in Settings > Design.
Rebuilding replaces the target folder, so build straight to the card if you like.

To build every theme in `src/theme_assets/` at once, whatever is there, use
`python3 tools/build_all_themes.py`. It goes to the simulator's SD card (`sim/sdcard/themes`) by default;
`--out /Volumes/ORB/themes` puts them on a real card, and folder names limit it
(`build_all_themes.py fallout portal`). One theme failing does not stop the rest, but the exit code says so.

## Starting a new theme

Copy `src/theme_assets/default/` and edit it. Its `theme.yaml` lists every option a theme can
set, with the value the default theme gives it, so it is both the reference and the starting
point; delete whatever you do not want to change.

The default theme's folder is where it lives. To make it a different design from a packed
`.orb` bundle, run
`python3 tools/gen_default_theme.py --from-orb "My theme.orb"`: that unpacks it, replaces the
folder, and writes `theme.yaml` from what the firmware itself makes of the theme, so every option
is listed and every colour is hex. Editing `theme.yaml` by hand is fine too, but run
`python3 tools/gen_default_theme.py` afterwards (comments are not kept) or the tests fail; they
fail as well when the firmware learns an option the default theme does not yet list.

The default theme is not the firmware's compiled fallback. An Orb with no theme on its card shows
the values compiled in (`theme_style.h` and the `CUSTOM_*` macros), and an option a theme leaves
out keeps THAT value, not the one the default theme states.

`src/theme_assets/portal/` is a worked example of a fully dressed theme: it states only what
differs from those compiled values. Its artwork is drawn by `tools/portal_art.py` (needs
`pip3 install pillow numpy`), so change the script and re-run it rather than editing the PNGs.

`src/theme_assets/fallout/` is a second one, and the one to copy if you want a theme whose art is
all drawn by a script: green phosphor on black, every plate, hand and blip from `tools/fallout_art.py`
(which borrows Portal's drawing helpers), and a typeface baked by `tools/fallout_fonts.py`. It is also the
one to copy for fonts: one `font_*.bin` per text slot at the size that slot is laid out for, made with
`lv_font_conv --bpp 4 --no-compress`. `tests/test_fallout_theme.py` also fails on any amber.

**Check plates with the firmware's own decoder.** The device's PNG decoder mis-reads some perfectly valid
streams: from one column to the end of a row it reads every channel a byte out of step, so black comes out
pure red. Whether a stream triggers it depends only on how it was compressed (the same pixels saved at
another `compress_level` decode fine), so Pillow opening the file proves nothing. Fallout's test decodes
every image through `png_decode.cpp` and compares; the simulator (`--themeshot`) shows it too. The simulator
does not load baked fonts (`theme_art::find_blob` is a stub natively), so fonts are checked through LVGL's
`lv_font_load` in the same test.

## The one rule: YAML keys are the JSON keys

A top-level section named after a screen becomes that screen's file, verbatim:

| YAML section | file | | YAML section | file |
|---|---|---|---|---|
| `clock:` | `clock_style.json` | | `weather:` | `weather_style.json` |
| `radar:` | `radar_style.json` | | `ticker:` | `ticker_style.json` |
| `settings:` | `settings_style.json` | | `menu:` | `menu_style.json` |
| `splash:` | `splash_style.json` | | `intel:` | `intel_style.json` |

`slug`, `name`, `author`, `version`, `default`, `apps` and `names` become `theme.json`.
Nothing else is allowed at the top level.

Nothing is whitelisted, so every option `theme_style.cpp` reads can be set, and new options
work as soon as the firmware reads them. To find an option's name, read the `merge_*`
functions and `load()` in `src/theme/core/theme_style.cpp`, or the structs in
`theme_style.h`. Leave out anything you do not want to change: the compiled default stands.

```yaml
slug: example
name: Example
author: Orb OS
apps:
  weather: false            # hide the Weather app from the knob menu

clock:
  bg: 0x000000
  text1:
    show: true
    x: 233
    y: 300
    color: #F2F5F9          # 0xF2F5F9 means the same thing
    fmt: "%H:%M"
  hands:
    order: [3, 4, 0, 1, 2]
    shadow: {on: true, dx: 2, dy: 3}

radar:
  sweepEnabled: true
  zones:
    - r: 120
    - rect: true
      w: 100
      h: 80
```

## Values

- Colours are numbers. Write `0xRRGGBB`, or `#RRGGBB` when the value starts the line's value
  (`color: #F2F5F9`, not inside `[...]` or `{...}`). Both become the integer the firmware wants.
- `true` / `false` are the only booleans. `on`, `off`, `yes` and `no` are text, which is what
  lets `hands.shadow.on` be written as-is.
- Numbers with a `.` are decimals, everything else is an integer. `90` and `90.0` both work
  for the options that take degrees.
- Quote a string that starts with a special character or looks like a number: `fmt: "%H:%M"`.

## What the build checks

These fail silently on the device, so the script stops or says so:

- **error** — duplicate key, unknown top-level key, a slug the card cannot use, or a JSON
  file over the size the firmware will read (8192 bytes; it ignores a bigger one without a word).
- **warning** — a key `theme_style.cpp` never reads (a typo; it would do nothing), a key
  with no value (left out), an image the firmware does not load, or two files that claim the
  same asset name.

The build is made beside the target and swapped in, so a failed build leaves the previous
one alone, and `_installed` is written last, which is how the Orb decides a folder is complete.

## Images, fonts and baking

Put the PNGs and fonts in the theme folder under the names the firmware loads
(`clock_plate.png`, `clock_hand_hour.png`, `splash.png`, `font_clock1.bin`, ...). The older
names in `LEGACY_NAMES` at the top of `tools/build_theme.py` (`dial_img.png`,
`custom_hour_png.png`, ...) are renamed on the way through. The device never sees them.

**Do not write an `assets:` list.** The device bakes only what `theme.json` lists, so the
script works the list out from the files actually in the folder. It also writes
`assetsHash`, which covers the files' contents: that is what makes the Orb re-bake an image
you replaced without renaming.

The bake itself (PNG to RGB565 in the `themeart` flash partition) happens on the device, on
the first boot after the theme is selected, and again whenever `assetsHash` changes. The
build script cannot do that part; it makes sure the device has what it needs to.

## Omitting a section

A section you leave out writes no file. Because the build replaces the whole
`<out>/<slug>/` folder, a file from an earlier build does not survive in the new one.
