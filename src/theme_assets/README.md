# Theme sources

One folder per theme, holding `theme.yaml` and the theme's images and fonts. The device does
not read these: `tools/build_theme.py` builds the folder that goes on the SD card. See
`docs/theme-yaml.md`.

The previews of the art compiled into the firmware (`tools/render_theme_bitmaps.py`) are
written to `build/theme_previews/`, not here.
