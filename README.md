# The Orb OS

<p align="center">
  <img src="https://img.shields.io/badge/board-ESP32--S3%20round%20AMOLED-E7352C?logo=espressif&logoColor=white" alt="Board: ESP32-S3 round AMOLED">
  <a href="LICENSE"><img src="https://img.shields.io/badge/code-MIT-2088FF" alt="License: MIT"></a>
</p>

Firmware for **The Orb**, a round-AMOLED desk instrument: a clock, a live flight tracker,
a weather screen and a news screen, all dressed by SD-card themes written as `theme.yaml` files.

The name stands for Occasionally Relevant Ball: open firmware, open themes, occasionally
relevant.

<!-- There are no photographs here yet. The ones that used to sit in this place were Quique
     Tortosa's, of his own device (an ADS-B radar with four fixed skins), and are no longer in
     the repository. When there are photographs of an Orb wearing a theme somebody designed,
     they go here. -->

## What it does

Five screens. Rock the knob to open the app picker, a wheel of the app names, and turn to choose:

- **Clock**: an analogue clock. It draws the theme's own dial and hands, or a dial and hands drawn from the palette when the theme ships none, with optional date and second banners, hand shadows, a plate that can turn with a hand, and a chime on the hour if you turn that on.
- **Flight tracker**: live traffic from [adsb.lol](https://api.adsb.lol), a sweep, trails, coastlines, roads and airports, with a card for the selected aircraft and up to three readout lines the theme composes itself.
- **Weather**: the temperature and outlook, a rain radar over the map, and a seven-day forecast; turn the knob to move between the three.
- **News**: headlines from BBC, the Guardian or NASA. Turn to move the highlight, press to read the story's own summary in the same band the list was in.
- **Settings**: display, location, sound, units, range, WiFi, theme, About and reset, on the same wheel as the app picker.

A stock ticker and a camera view are in the tree but compiled out of it (`APPS_LAUNCH_ONE` in [`src/config.h`](src/config.h)), so they are absent from the picker rather than present and switched off.

## Themes

Every screen is dressed by a **theme**: a folder on the SD card, built from a `theme.yaml` by `tools/build_theme.py` (see [`docs/theme-yaml.md`](docs/theme-yaml.md)). A theme can be as small as four colours, its **palette**: the firmware works out the rest, so a theme that is only a palette still looks designed. Backgrounds, glass and CRT overlays, typefaces, colours, opacity, glow and layout are the theme's to choose beyond that. With no theme selected the Orb draws a built-in sky-blue look.

Four themes ship in [`src/theme_assets/`](src/theme_assets/): **Elegant** (also the reference listing every option), **Fallout**, **Portal** and **Vault-Tec**. Switch themes on the device under **Settings → Theme**, with no computer needed.

The app picker and every list in Settings are one wheel with a fixed shape. A theme dresses it through its palette (the selected row takes `primary`, the others `muted`), two typefaces and its background art; there is no selection bar and nothing else to configure.

The firmware refuses a design its own build cannot render, rather than installing it and quietly drawing something else. `THEME_CAPS` in [`src/theme/core/theme_style.h`](src/theme/core/theme_style.h) is the ledger of what each level added.

## Hardware

Waveshare **ESP32-S3-Touch-AMOLED-1.75**: ESP32-S3R8 (8 MB PSRAM, 16 MB flash), **CO5300** AMOLED over QSPI, **CST9217** touch, **QMI8658** IMU, **PCF85063** RTC, **AXP2101** PMIC, **ES8311** audio + speaker, microSD. All pins are in [`src/config.h`](src/config.h), taken from the board definition rather than guessed.

The knob is the interface. Touch adds swipes only: sideways between apps, up and down between an app's own screens (Weather's Now, Radar and 7-Day). Nothing can be tapped.

## Build and flash

```bash
pio run -e esp32-s3-amoled-175 -t upload     # build + flash over USB-C
pio device monitor -b 115200                  # serial log
```

If `pio` is not on your `PATH`, PlatformIO's own copy is at `~/.platformio/penv/bin/pio`. On a first flash you may need to hold **BOOT** then tap **RESET**. On first boot the Orb asks for your WiFi on its own screen, and you pick the network and type the password with the knob. If you would rather use a phone, it also opens a network called **The Orb Setup** with a setup page.

The Orb is always flashed over USB. Wireless updates are compiled out (`ORB_OTA_ENABLED` in [`src/main.cpp`](src/main.cpp)) because the second app slot they need was given to theme art in [`partitions_16MB_themeart.csv`](partitions_16MB_themeart.csv); turn the flag and the partition table back on together, never one alone.

A firmware change is not finished when it compiles: boot it on an Orb. [`docs/HARDWARE_PENDING.md`](docs/HARDWARE_PENDING.md) lists what has only been checked in the simulator.

## Desktop simulator

The whole UI is portable LVGL and runs on a computer over SDL2, with a virtual knob, so a screen can be built and photographed without touching hardware:

```bash
pio run -e native -t exec     # 466x466 window (needs SDL2: brew install sdl2)
```

It reads the same theme folders from `sim/sdcard/themes/`, makes the same network requests, and has headless capture modes used to check a screen without a photograph:

```bash
.pio/build/native/program --themeshot out     # what the device renders, active theme
.pio/build/native/program --settingsshot out  # the settings wheel and theme picker
.pio/build/native/program --rockshot out      # the app picker, opened over Settings
.pio/build/native/program --wifishot out      # the first-boot and WiFi pages
.pio/build/native/program --newsshot out      # the news list and a briefing
.pio/build/native/program --bakeshot out      # the artwork-preparing screen
.pio/build/native/program --readyshot out     # the post-update notice
SIM_SELFTEST=1 .pio/build/native/program      # headless knob and navigation checks
```

`tools/themeshots.sh` photographs every app of one or more themes, and `tools/shot_diff.py` compares two such sets, which is the regression net for a change to how something is drawn.

## Tests

```bash
bash tests/run_host_tests.sh                              # pure logic on the desktop, no board
python3 -m unittest discover -s tests -p "test_*.py"      # the theme builder, goldens and firmware-facing checks (about two minutes)
```

Host tests need the native environment's libraries once (`pio run -e native`). They cover the parts of the firmware that are pure enough to run on a desktop; display, audio, WiFi, NVS and the two cores together only show themselves on an Orb.

## Configuration

`http://theorb.local/` on the same WiFi, or the device's IP, for centre point, range, brightness, sound and WiFi reset, and it is where a theme's files are sent to the card. Settings live in NVS under the `capsuleradar` namespace, which keeps its old name deliberately: renaming it would make every existing Orb look factory reset.

## Repo layout

```
src/
  main.cpp             boot, tasks, WiFi/NTP, the web config page
  config.h             pins, hostname, user agent, tunables, FW_VERSION
  app/                 one folder per screen, plus the shell
    shell/             the app list, the picker overlay, and the input router
    clock/  radar/  weather/  intel/  settings/  ticker/  spycam/  photo/  route/
    common/            shared drawing helpers, and wheel_look (a theme, made into a wheel)
    boot/  ui/         the hello screen, the main UI (tileview, radar list, detail card) and the update screen
  core/                feed client, geometry, aircraft model, GPS, IMU, swipe recogniser (no LVGL where possible)
  platform/            the board and the host
    display/  input/  audio/  rtc/  storage/   panel and touch, the knob, sound, clock, SD card
    net/               HTTP for the simulator only (the device uses WiFiClientSecure)
    wheel/             the one knob-driven list renderer, theme-free
    sim/               the SDL simulator and its capture modes
  theme/               the theme engine
    core/              the style model and THEME_CAPS, palette roles, fonts, art baking, theme selection
    graphics/  custom/  fonts/   image decoding and sprites, compiled fallbacks, LVGL fonts
  theme_assets/        the shipped themes (theme.yaml, plates, typefaces)
include/lv_conf.h      LVGL v8 config
tools/                 the theme builder, generators, the screenshot tools
tests/                 host tests and the Python suite
web/flash/             browser web flasher (ESP Web Tools)
docs/                  architecture, memory, hardware, and the theme format
```

Adding or changing a screen? Read [`docs/adding-a-screen.md`](docs/adding-a-screen.md) first: it is the checklist of the standard parts every screen has. [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) has the rest.

## Community ports and forks

- **[Capsule Radar for the Waveshare ESP32-S3-Touch-LCD-2.1](https://github.com/alexzogh/capsule-radar/tree/port/esp32-s3-lcd-21)** by **@alexzogh (STLWarehouse)**: a port of the upstream project to the 2.1" round LCD (ST7701), with double-tap aircraft tracking, an idle clock face, and a busy-airspace query-radius fix that was merged back upstream.

## Data and licence

**Code: [MIT](LICENSE).** Fork it and build on it, keeping the notice. Who wrote what, and the third-party typefaces the themes ship, are in [`NOTICE`](NOTICE).

The Orb OS began as a fork of [Quique Tortosa's Capsule Radar](https://github.com/socquique/capsule-radar) and carries his copyright alongside Zion Brock's. See [`LICENSE`](LICENSE) for what came from where.

Aircraft data from **adsb.lol**, free and non-commercial. First location from **ip-api.com** and city search from **Open-Meteo**'s geocoding. Forecasts from **Open-Meteo.com** (CC BY 4.0), credited on the Weather screens where it cannot be switched off; rain radar from **RainViewer**, credited on the radar. Map data **© OpenStreetMap contributors**, ODbL, credited on the Orb's own About screen where it cannot be switched off. Headlines from **BBC**, **The Guardian** and **NASA** RSS.
