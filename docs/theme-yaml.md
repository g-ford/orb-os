# Writing a theme in YAML

A theme is a folder: `theme.yaml` plus its images and fonts. `tools/build_theme.py` turns
that folder into the one the Orb reads from its SD card. The device never sees the YAML; it
reads the same JSON files it always has (`theme_style.cpp`).

```
pip3 install pyyaml                                  # once
python3 tools/build_theme.py src/theme_assets/elegant # -> build/themes/elegant/
python3 tools/build_theme.py src/theme_assets/elegant --out /Volumes/ORB/themes
```

Copy `<out>/<slug>/` to `/themes/<slug>/` on the card and pick it in Settings > Design.
Rebuilding replaces the target folder, so build straight to the card if you like.

To build every theme in `src/theme_assets/` at once, whatever is there, use
`python3 tools/build_all_themes.py`. It goes to the simulator's SD card (`sim/sdcard/themes`) by default;
`--out /Volumes/ORB/themes` puts them on a real card, and folder names limit it
(`build_all_themes.py fallout portal`). One theme failing does not stop the rest, but the exit code says so.

## Starting a new theme

The quickest start is a palette: copy `src/theme_assets/default/`, give it your own `slug:`, change its four colours and
delete the seven lines below its divider, and every screen follows (see "Palette" below). Copy `src/theme_assets/elegant/` instead when you want every option in front of you. Its
`theme.yaml` lists every option a theme can
set, with the value Elegant gives it, so it is both the reference and the starting
point; delete whatever you do not want to change.

Elegant's folder is where it lives. To make it a different design from a packed
`.orb` bundle, run
`python3 tools/gen_elegant_theme.py --from-orb "My theme.orb"`: that unpacks it, replaces the
folder, and writes `theme.yaml` from what the firmware itself makes of the theme, so every option
is listed and every colour is hex. Editing `theme.yaml` by hand is fine too, but run
`python3 tools/gen_elegant_theme.py` afterwards (comments are not kept) or the tests fail; they
fail as well when the firmware learns an option Elegant does not yet list.

Elegant is not the firmware's built-in look. An Orb with no theme selected draws the built-in palette
(`theme_roles.h`), with every colour option defaulting to one of its roles (`theme_palette.cpp`). An option a
theme leaves out never takes the value Elegant states: in a theme with no `palette:` it keeps the value compiled in
(`theme_style.h` and the `CUSTOM_*` macros), and in a theme with a palette it takes its role's colour unless the
theme says `roleDefaults: false` (see "Palette" below).

`src/theme_assets/portal/` is a worked example of a fully dressed theme: it states only what
differs from those compiled values. Its artwork is drawn by `tools/portal_art.py` (needs
`pip3 install pillow numpy`), so change the script and re-run it rather than editing the PNGs.

`src/theme_assets/fallout/` is a second one, and the one to copy if you want a theme whose art is
all drawn by a script: green phosphor on black, every plate, hand and blip from `tools/fallout_art.py`
(which borrows Portal's drawing helpers), and a typeface baked from `source/` by its `fonts:` block. It is
also the one to copy for fonts: ten faces, one per size, shared by twenty-two slots. `tests/test_fallout_theme.py` also fails on any amber.

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
| `splash:` | `splash_style.json` | | `intel:` | `intel_style.json` |

`slug`, `name`, `author`, `version`, `default`, `apps` and `names` become `theme.json`.
Nothing else is allowed at the top level. (`settings:` and `menu:` used to be sections. See [The wheel](#the-wheel).)

Nothing is whitelisted, so every option `theme_style.cpp` reads can be set, and new options
work as soon as the firmware reads them. To find an option's name, read the `merge_*`
functions and `load()` in `src/theme/core/theme_style.cpp`, or the structs in
`theme_style.h`. Leave out anything you do not want to change: it keeps its default, which is the compiled value, or its
role's colour in a theme with a palette (see "Palette" below).

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

## The wheel

The app picker and every list in Settings are one wheel. Its shape is fixed in the firmware, and a theme dresses it
with:

- the palette: `primary` is the selected row and its glow, `muted` is every other row;
- the font slots `wheel_sel` (the selected row) and `wheel_item` (every other row);
- the artwork `menu_plate.png` / `menu_overlay.png` (the picker) and `settings_plate.png` / `settings_overlay.png`
  (Settings).

There is no `settings:` or `menu:` block, no highlight bar behind the selected row, and no default row: THEME_CAPS
54 removed them, and the builder refuses a theme that still has one, naming what replaced it. The setup pages
(first boot, the network list, the password entry) are not themed: they use stock colours so a theme cannot make
the screen somebody fixes their WiFi on illegible.

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

Copy `src/theme_assets/default/` to start: it is the built-in look written down, with all eleven roles listed. Give
the copy its own `slug:` (`default` is reserved: the builder refuses it) and change the four colours. Then delete the
seven lines under the file's "delete from here" divider. They are the built-in's own tuned values, so if you leave them
in they stay the built-in's blue whatever four you picked; without them the firmware derives them from your four.

- The seven roles you do not pick are derived from those four: `muted` (the text colour mixed toward the background),
  `dim` and `hairline` (primary mixed toward the background), `panel` (the background with a little text in it),
  `highlight` (the background with some primary in it) and `onPrimary` (whichever of background or text reads better on
  primary). State any of them in the palette to override the derivation. `alert` is a fixed red unless you state it.
- Any colour option can name a role instead of a number: `sweepColor: $secondary`. Only whole values are read as
  roles, so text such as `"$5.00"` is untouched; write `$$` for a literal `$`, and `$$primary` is refused.
- An option you state always wins. One you leave out takes its role's colour, **unless** the theme says
  `roleDefaults: false`, which keeps the firmware's compiled value for every option you do not state. Themes written
  before palettes existed (Fallout, Portal, Elegant) use it: they state only what differs from the compiled values.
- A theme with no images at all gets a drawn clock (a ring, sixty ticks, hands and the date) in these colours. An
  image always wins, per element: a plate but no hands, or hands but no plate, is drawn only where it is missing.
  A hand with `show: false` is never drawn, whether or not it has an image. A hand that is shown but has no image is
  drawn, so a theme with hour and minute images and no second image gets a drawn seconds hand (with a small hub) unless
  it says `hands: second: {show: false}`. A palette theme that leaves `show` out gets all three hands on.
- With no theme selected, or the reserved theme `default`, the Orb draws the built-in palette. Settings > Design lists
  it first. Do not put a folder called `default` on the card: it is ignored.
- **Known limit.** The Flight Tracker's scope chrome (its background, range rings and crosshair) still comes from the
  radar's own compiled Orb/Military/Aviator skin, not from options a theme can set, so a palette colours the sweep,
  blips and text but does not draw a scope. A theme with no radar plate shows that skin's backdrop until those skins
  are retired.

## Fonts: define each face once

A face is one typeface at one size, baked into one `.bin`. Every text slot that wants it points at it by name,
and the Orb loads each face once however many slots share it:

```yaml
fonts:
  faces:
    label:    {src: source/ShareTechMono-Regular.ttf, size: 16}
    title:    {src: source/ShareTechMono-Regular.ttf, size: 32}
    prebaked: {src: fonts/old.bin}          # a .bin is copied as it is
  slots:
    radar2: label
    radar3: label
    intel_title: title
```

- `src` is a `.ttf` or `.otf` inside the theme folder (baked with `lv_font_conv`, which the build fetches with
  `npx` if it is not installed) or a `.bin` that is already baked. `size` is in pixels and belongs to a
  `.ttf`/`.otf` only; `ranges` optionally replaces the default characters (`lv_font_conv --range` form). Keep a
  `.bin` face's file out of the top of the folder (for example in `fonts/`): the top level is scanned for
  legacy per-slot files.
- Slot names are the firmware's slot files without `font_` and `.bin`: `radar2`, `intel_title`,
  `wheel_sel`, and so on. The build lists them when you get one wrong.
- A face name is at most 14 characters and cannot be a slot name.
- **Sizes cost memory.** Every distinct typeface-and-size is a separate face in flash and in PSRAM, so reuse a
  size unless a layout genuinely needs another. The build prints the count and warns above 10.
- A slot you leave out keeps loading `font_<slot>.bin` from the folder if there is one (how themes were made
  before this), otherwise LVGL's Montserrat: 44 px for the wheel's selected row (`wheel_sel`), 20 px for its other rows
  (`wheel_item`), 26 and 20 px for the first two radar text lines, and the default size elsewhere.
- The Orb stores the slot-to-face map in flash when it bakes the theme, so it keeps its typeface with no card.

## Omitting a section

A section you leave out writes no file. Because the build replaces the whole
`<out>/<slug>/` folder, a file from an earlier build does not survive in the new one.
