# Portal artwork

How art gets into this theme:

```
master (any size, from your image generator)  ->  tools/fit_theme_art.py  ->  this folder  ->  tools/build_theme.py
```

Keep each big original in `source/` and fit from it, so a plate can be redone at any size. `fit_theme_art.py`
only resamples and converts; it draws nothing. The build refuses what the firmware cannot draw:

- **8-bit RGBA only.** RGB, palette and greyscale PNGs are drawn as solid black, with nothing in the log.
- **Plates are exactly 466x466** (every `*_plate.png`, `splash.png`, `wind_bg.png`): they are drawn at their own
  pixel size on the 466x466 round screen. Corners outside the circle should be black.
- Sprites (hands, blip) can be any size, but must point **straight up** and pivot on the numbers in `theme.yaml`.

## The reference: `clock_plate.png`

Fitted from `source/clock_plate.png` (1254x1254). Everything else should look like it belongs next to it:
photoreal, worn and dirty concrete chamber panels with fine seams and grime (`#A6A8AD`-`#B6B6BA`), dark weathered
steel bezel, black hour bars (`#141313`), and glowing plasma portals with a hot rim and light spilling onto the wall:
orange (rim around `#FF9A1F`, void `#B94802`) and blue (rim around `#0176D4`, void `#0046A9`).

## What is still a placeholder

Drawn flat by `tools/portal_art.py`; replace them with generated art in the same style.

| file | final size | pivot (in `theme.yaml`) | what to leave alone |
|---|---|---|---|
| `clock_hand_hour.png` | 26x156 | (13, 132) | tapered blade, pointing up, tail below the pivot |
| `clock_hand_minute.png` | 20x220 | (10, 198) | same, longer |
| `clock_hand_second.png` | 14x246 | (7, 206) | thin needle with a counterweight tail |
| `radar_blip.png` | 26x26 | (13, 13) | a sentry droid seen from above, eye at the top; keep its own colours (tint is off) |
| `radar_plate.png`, `weather_plate.png` | 466x466 | | scope: keep the middle clear. The bezel starts at radius 217 and aircraft are masked outside radius 214 (`zones`); change both together |
| `menu_plate.png` | 466x466 | | the current app's name is drawn large across the middle: keep x 45-421 and y 195-270 calm |
| `settings_plate.png` | 466x466 | | list of rows around the centre with a 260x40 highlight bar: keep the centre column clear |
| `splash.png` | 466x466 | | version, network, credits and theme name are drawn at y 321-419: keep that band dark |
| `intel_plate.png` | 466x466 | | title at y 65 between two rules; text margins 68 px; the age line at y 409 |
| `ticker_plate.png` | 466x466 | | name y 186, price y 222, change y 286; the scrolling strip runs on a curve of radius 196 |

Generators handle tall thin sprites badly. Make a hand at 4x on a transparent background (for example 104x624),
then `python3 tools/fit_theme_art.py hour.png src/theme_assets/portal/clock_hand_hour.png --size 26x156`.
If the pivot in the new art is not where the table says, change `pivotX`/`pivotY` in `theme.yaml`: the firmware
turns the sprite about that point, so a pivot 10 px off makes the hand wobble instead of turn.

`test_portal_theme.py` checks the plates' size and that each pivot sits inside its image.

## Notes

- The iris in the middle of the clock plate looks like a well-known company logo. Fine for a device of your own;
  worth removing before sharing the theme.
- `tools/portal_art.py` no longer draws the clock plate and will not overwrite it.

## Typeface

Barlow Regular (Copyright 2017 The Barlow Project Authors, SIL Open Font License 1.1; the licence text is
`source/OFL.txt`), baked by `tools/build_theme.py` from the `fonts:` block in `theme.yaml`. Five sizes are
shared by 24 slots. Add a size only where a `--themeshot` shows a layout needs it: every extra size is another
face in flash and in PSRAM.
