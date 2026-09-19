# Writing a theme in YAML

A theme is a folder: `theme.yaml` plus its images and fonts. `tools/build_theme.py` turns
that folder into the one the Orb reads from its SD card. The device never sees the YAML; it
reads the same JSON files it always has (`theme_style.cpp`).

```
pip3 install pyyaml                                  # once
python3 tools/build_theme.py src/theme_assets/office # -> build/themes/office/
python3 tools/build_theme.py src/theme_assets/office --out /Volumes/ORB/themes
```

Copy `<out>/<slug>/` to `/themes/<slug>/` on the card and pick it in Settings > Design.
Rebuilding replaces the target folder, so build straight to the card if you like.

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
slug: office
name: Office
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
`office_hour_png.png`, ...) are renamed on the way through. The device never sees them.

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
