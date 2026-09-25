# Setup

## Toolchain
- **PlatformIO** (VS Code extension or CLI). If `pio` is not on your `PATH`, PlatformIO's own copy is
  `~/.platformio/penv/bin/pio`.
- USB-C cable. On first flash you may need to hold **BOOT** then tap **PWR/RST**. The Orb is always flashed
  over USB: wireless update is compiled out (see `ORB_OTA_ENABLED` in `src/main.cpp`).
- For the desktop simulator: SDL2 (`brew install sdl2`).

## Build, flash, simulate
```
pio run -e esp32-s3-amoled-175                 # build the firmware
pio run -e esp32-s3-amoled-175 -t upload       # flash
pio device monitor -b 115200                   # serial log
pio run -e native -t exec                      # desktop simulator (same LVGL UI, virtual knob)
```

## Tests
```
bash tests/run_host_tests.sh                                    # pure-logic host tests, no board
python3 -m unittest discover -s tests -p "test_*.py"            # theme builder and firmware-facing checks
```
The host tests need the native environment's libraries once (`pio run -e native`).

## Pins and LVGL
Every pin is already in `src/config.h`, taken from the board definition; see [HARDWARE.md](HARDWARE.md). LVGL's
configuration is `include/lv_conf.h` (v8, 16-bit colour, PSRAM draw buffers), reached through
`-DLV_CONF_INCLUDE_SIMPLE`. Keep the LVGL version in `platformio.ini` and `lv_conf.h` matched.

## First boot: WiFi and location
No secrets are committed. On first boot the Orb asks for WiFi on its own screen (you pick the network and
type the password with the knob), or opens a network called **The Orb Setup** if you would rather use a phone.
The first location comes from an IP lookup and can be changed under **Settings > Location** or on the web page
at `http://theorb.local/`. There is no baked-in home position (`HOME_LAT_DEFAULT` is 0).

## Themes
A theme is a folder on the SD card, built from a `theme.yaml` by `tools/build_theme.py`; see
[theme-yaml.md](theme-yaml.md). With no theme selected the Orb draws its built-in look.

## HTTPS note
The aircraft feed is fetched over plain HTTP (see the note above `ADSB_PRIMARY_HOST` in `src/config.h`), and
the other HTTPS clients use `WiFiClientSecure::setInsecure()`. For a hobby build that is a documented choice;
for anything more, pin the root CA.
