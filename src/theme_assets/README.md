# Theme asset previews

These PNG previews are exported once per embedded asset, using the same canonical symbol names as the C++ source headers. This avoids duplicate renders from alternate naming schemes like `foo.png` and `foo_png.png`.

- default/ : custom/default theme artwork
- office/ : office theme artwork

Each folder also holds a `theme.yaml`. The device does not read it: `tools/build_theme.py`
builds the folder that goes on the SD card. See `docs/theme-yaml.md`.
