# Unverified on a real Orb

CLAUDE.md rule 1: firmware runs on a real Orb before it is called done. From 2026-09-21 the owner had no Orb to
hand, so the changes below built, passed the host tests and (where noted) ran in the simulator, but have **not**
booted on hardware. Work through this list when an Orb is available, and delete an entry only once it is checked.
Add to it with every firmware change made without a board.

## Boot bake: restored, and it no longer wipes the cache with no card (branch `fix/theme-bake-at-boot`)

Found by the whole-branch review of the font work, and both are **pre-existing**, not caused by it:

- `theme_art::bake_active_theme()` had **no caller** since `8b63b1d` removed `theme_manager::ensureDefaultBaked()`
  without a replacement, so nothing has written the flash cache at boot since. Every theme has been drawing from the
  SD card, and the shared-face / `fonts.map` path in the section below could never have run. The call is restored in
  `main.cpp` between `set_progress()` and `ui_splash_show()`, where the comment above it always said it belonged.
- With **no card** the theme's asset list is unreadable (fingerprint 0), so the old code skipped its "already baked"
  early-out, erased the flash index, read nothing and committed nothing: **a card-less boot destroyed the cache**. It is
  now `theme_bake_policy.h`'s `should_bake()`: no card, no bake.

Restoring the call changes boot on a device I cannot test, so this is on its own branch. Check on the Orb:

- [ ] First boot after flashing: the panel shows the bake progress, then the splash, then the clock; no reboot loop, no
      watchdog. Serial log: `[theme_art] baking '<slug>' into flash ...` and `committed N asset(s)`.
- [ ] A second boot logs `'<slug>' already baked and unchanged - nothing to do` and is noticeably quicker.
- [ ] **Pull the card and reboot twice.** Each logs `'<slug>': no card, leaving the flash cache alone`; the baked art and
      fonts are still drawn on the second boot (this is what would have failed before).
- [ ] Select a different theme, then back: both stay baked (the partition holds about two full themes; a third install wipes the others).
- [ ] Editing only a theme's `fonts.slots` (no file changes) now changes `assetsHash`, so it re-bakes and refreshes `fonts.map`.

## Theme font faces (spec `docs/superpowers/specs/2026-09-20-theme-palette-fonts-builtin-default-design.md`, plan 1)

`theme_font` is the file with a history of boot loops (a failed LVGL allocation once wrote through a null
pointer before any screen drew), so check this first.

### Task 5: shared faces, `fonts.map`, `THEME_CAPS` 52

Changed: `src/theme/core/theme_font.{h,cpp}`, `theme_font_resolve.h`, `theme_style.{h,cpp}`, `theme_art_bake.cpp`.
Built for both environments; the resolver is host-tested. Nothing below has run on a device.

- [ ] Boots with a legacy theme (one `font_<slot>.bin` per slot, no `fonts` map): every slot still loads its own file.
- [ ] Boots with a face-mapped theme on the card: the serial log shows the bake storing the faces and
      `[theme_art] stored fonts.map (N bytes)`, then `[theme_font] N of 27 slots loaded from the theme, M distinct face(s)`.
      Expected once the themes are migrated: Elegant 11 slots / 8 faces, Fallout 22 / 10, Portal 24 / 5.
- [ ] **Pull the SD card and reboot.** The same `[theme_font]` line must appear, the map coming from the flash
      `fonts.map` blob, and the lettering must be unchanged. This is the case the blob exists for.
- [ ] No `failed to parse` line for any face, and no reboot loop on the first boot after a theme is baked.
- [ ] `?orb mem` before and after selecting Fallout: PSRAM free is higher than on the previous firmware, by roughly
      the 53 KB that twelve fewer decoded faces should save. Record both numbers.
- [ ] A re-bake happens when a face changes (the `assetsHash` covers the face files) and the stale `fonts.map` from the
      earlier bake is gone afterwards.
- [ ] The web config / Stats screen reports the new `FW_VERSION`, and `THEME_CAPS` 52.

### Task 11: version bump and the migrated themes

- [ ] `FW_VERSION` is `2.19.0` (2.18.0 was already the touch-swipe release, so the fonts, boot-bake and palette work is
      2.19.0). It was bumped **without** a hardware check, so treat it as unreleased until this whole list is ticked;
      the web config page and the Stats screen should show it.
- [ ] **Install path.** `python3 tools/build_all_themes.py --out /Volumes/ORB/themes` (building Fallout and Portal
      needs `npx`/`lv_font_conv`), copy `elegant` to the card too if it still holds the old `default` folder, then select
      each theme in Settings. The saved slug `default` now selects the built-in look (it no longer means the old default
      theme, which is `elegant`), so an Orb that had `default` saved boots to the built-in look until you pick another.
- [ ] **Elegant and Fallout must look exactly as before** (their per-slot fonts were replaced by shared faces that a golden
      test proves byte-identical, but only the device shows the glyphs actually drawn).
- [ ] **Portal's lettering is new (Barlow) and was judged only in the simulator.** Check menu, Settings, headlines, ticker
      and the flight tracker on the Orb. Two things the simulator could not show: the radar's *selected-aircraft readout
      pills* at the single 20 px size (a `--themeshot` selects no aircraft), and Settings' selected-row glow, which draws a faint
      offset duplicate of the word (present with and without Barlow, stronger at 28 px).
- [ ] The Weather "Now" and "7-Day" screens take their fonts from `ui.cpp`/`wx_screens.cpp`, not the theme, so they are
      unchanged by any of this; confirm they still draw.

## Palette roles and the built-in look (plan 2)

Plan `docs/superpowers/plans/2026-09-21-palette-roles-and-procedural-clock.md`. Built and host-tested only.

### Task 4: palette mode in `theme_style`, `THEME_CAPS` 53

- [ ] `theme_style::load()` runs with no theme and with a palette theme without a boot loop or watchdog; the serial log
      shows no `is not a colour role` line for a healthy theme.
- [ ] A theme with `$role` strings draws those colours (Fallout and Portal after Task 8).

### Task 5: role bindings and the built-in layout

- [ ] With no theme selected the Orb boots to the built-in look: green on black, no compiled images, on every screen
      **except the Flight Tracker**, which still draws the radar's compiled Aviator skin (brown backdrop, no range rings,
      oversized disc blips) until the radar's own compiled skins are retired (that is separate work and is not part of
      spec step 4 or 5): see "Known limit" in `docs/theme-yaml.md`. Do not fail this line for that screen.
- [ ] A theme that is only a palette (four colours) looks designed on every screen except the Flight Tracker's scope
      chrome, not just the clock.
- [ ] **Flash cache.** The old `default` slug's entries in the `themeart` partition (from before the rename to `elegant`)
      are orphaned. Confirm the next install or boot bake evicts them cleanly rather than leaving the partition full.

### Task 6: the reserved `default` slug and Settings > Design

- [ ] Settings > Design lists **Default** first, and choosing it reboots into the built-in look and stays there across
      reboots (it must not switch to the first card theme).
- [ ] With Default active, choosing Default again does not reboot. Choosing an installed theme and then Default works.
- [ ] A card folder called `default` is not listed.

### Task 7: `AppPalette` from the roles

- [ ] The app-switcher menu, Settings and About take a palette theme's colours; a theme with no palette still shows the
      familiar night-vision green.

### Task 8: Elegant, Fallout and Portal carry palettes

- [ ] Every screen of each shipped theme looks as it did before. The resolved-state goldens prove the *style options* did
      not move, but they do not cover `AppPalette`, so the Settings/About chrome and the app-switcher menu **do** now follow
      each theme's palette (they were always night-vision green): check they read well on Elegant (green on black),
      Fallout (phosphor green) and Portal (orange on steel).

### Task 10: the drawn clock face

The drawn face was judged only in the simulator, and it is the biggest unknown in the spec: how long the canvas calls
take on the device.

- [ ] With the built-in look the clock shows the ring, ticks, hands, hub and date, and the hands advance once a second.
- [ ] The redraw is smooth: no visible tearing, no watchdog reset, no dropped knob turns while the clock is on screen.
      Measure the time one redraw takes (add a temporary `millis()` pair around `draw_custom()`); if it is over about
      50 ms, cache the dial (ring, ticks) in the PSRAM canvas and redraw only the hands.
- [ ] The firmware holds no compiled clock image of any kind (spec step 5), so a brown plate or ornate hands can never
      come from flash.
- [ ] A theme with images (Fallout, Portal, Elegant) still shows only its own art, with no drawn hub or ticks on top.
- [ ] A theme with a plate but no hand images draws hands over the plate; a theme with hand images but no plate draws the
      dial under them.

### Task 11: the built-in splash

- [ ] The startup splash with no theme is a flat dark card with the version, the network line, the credits and the
      theme's name ("Default") in the palette's colours; nothing brown.
- [ ] Settings > About shows the same, and a theme with its own `splash.png` still shows that.
- [ ] **Memory.** With no theme, `splash_art_decode()` returns before `ensure()`, so the 434,312-byte (466x466x2) decode
      buffer is never allocated. Compare free PSRAM after boot with the built-in look against a theme that has a
      `splash.png`: the built-in should have about 424 KB more. This is by construction, not measured.

## Retire Default/Office (plan `2026-09-22-retire-app-skins-and-compiled-art.md`, Part A, spec step 4)

Built, host-tested and screenshot-compared in the simulator only.

- [ ] An Orb that had the Office skin saved (`appTheme` = 1 in NVS from an older firmware) boots to its theme or the
      built-in look, not a white UI, with no error on the serial log. Nothing reads that key any more.
- [ ] Settings shows no Theme row under Display and the top-level Theme picker (MODE_DESIGN_SELECT internally) still
      lists Default first and works.
- [ ] Flight Tracker, Spy Cam, the boot splash and Settings > About look as they did before.

## Delete the compiled art and fonts (plan `2026-09-22-retire-app-skins-and-compiled-art.md`, Part B, spec step 5)

Built, host-tested and screenshot-compared in the simulator only. `firmware.bin` went from 5,511,152 to 4,142,752 bytes
(app slot 6,553,600).

- [ ] With no theme and no card the Orb boots, the clock draws, and the startup splash is the flat card with its lines.
- [ ] The built-in look's menu name and Flight Tracker text are now Montserrat 44, 26 and 20. The menu name wraps inside its
      width and the radar lines do not overlap.
- [ ] Elegant, Fallout and Portal keep their own lettering (their faces load from flash with no card).
- [ ] **Behaviour change for a user's own legacy theme (no `palette:`)**: with no `splash.png` it now shows its background
      and text lines only; with no `clock_hand_*.png` it shows no hands. Neither may crash or log a decode error.
- [ ] An OTA update onto an Orb still on 2.19.x installs (the image is smaller, so it fits the slot it fitted before).
- [ ] `?orb mem` before and after: PSRAM is unchanged apart from the splash, which no longer allocates for a theme with no card art.
- **Not cruft — leave them.** `src/theme/custom/custom_splash.h`, `custom_plate.h` and `custom_overlay.h` (each a ~90-byte
  `#define CUSTOM_HAS_* 0` stub) are referenced by nothing under `src/` after this branch's clock/splash changes, but the
  Launch Kit push server (outside this repo) still emits all three on every push and expects them to exist. Do not delete
  them as leftover dead code; a future push would just regenerate them.

### The default palette went from green to sky blue (FW 2.21.1)

Judged only in the simulator and by contrast arithmetic (primary 9.3:1, secondary 10.7:1 and text 16.5:1 against the navy).

- [ ] The built-in look reads well on the real panel: sky-blue primary, rose second accent, text and small `dim`/`muted`
      captions on the navy `0x0D1220`. Navy is not true black, so check it does not look washed out on the AMOLED, and
      that the radar tile and the weather tile (which mask their edges in true black) show no visible seam against it.
- [ ] Settings, the app-switcher menu and the clock all follow the new palette; the Orb and Military radar
      scopes keep their own green on purpose (they are scope skins, not the default palette). Settings' own background
      now explicitly follows `app_theme::palette().bg` in built-in mode (it used to default to plain black, which only
      matched by coincidence when the built-in background was itself black) — check it and the menu read as one
      continuous navy, not two different darks.
- [ ] The web config page, the firmware-update page, the "Saved, restarting" page and the WiFi setup portal are
      sky-blue on navy in a phone browser. The portal's `.q` signal icon used to be recoloured with
      `hue-rotate(90deg)` to suit the green; that filter is gone, so check the icon still looks right.

### The app picker and Settings are one wheel (FW 2.22.0)

Not booted on an Orb. Simulator-verified only: the navigation self-test matches its pre-change baseline (8 of 8), and the
picker, the Settings lists, the network list and first-boot page, and the no-canvas fallback were read from screenshots.

- [ ] One 868 KB PSRAM canvas (`src/platform/wheel`) is taken on entering either screen and given back on leaving. Read the
      `[wheel]` and `[menu] art:` log lines for the PSRAM figure, and confirm Flight Tracker still allocates its own overlays
      after visiting Settings and the picker.
- [ ] The picker opening over Settings hands it the canvas (the wheel moves it to the overlay), and closing the picker back
      onto Settings gives Settings a fresh one (`onEnter`). Open the picker from Settings, settle on Settings, and check the
      wheel is drawn; then settle on another app and check nothing is left behind.
- [ ] Detent latency in the picker and in Settings. The picker's dirty-rectangle repaint was kept: a turn must not repaint the
      whole panel (`[perf]` lines).
- [ ] The recovery pages (first boot, network list, password) no longer have a selection pill. Check on the glass that the
      selected row is unmistakable, especially the network list at its 28 px selected / 24 px other sizes.
- [ ] Settings' selected row is the theme's `wheel_sel` face, about 46 px on the four themes that ship one (it was 27 px).
      Check it fits and reads well on all five themes. This is a starting value for you to tune by eye.
- [ ] The picker is now a ring of every app with the current one in the middle and the rest fading away; check it wraps
      correctly at the first and last app, and that names are legible on each theme.
- [ ] A card written by an older build, with `settings_style.json`, `menu_style.json` and an old `fonts.map`, must still boot
      into a working Settings and picker (those files are simply never read).
- **Not cruft.** `custom_menu*.h` and `custom_settings*.h` were deleted with this change. The Launch Kit push server (outside
  this repo) has emitted such stubs on every push; nothing under `src/` includes them any more, so a push regenerating them is
  harmless, but if a push ever fails on their absence, that is why.

### One sprite loader, and the clock's hand shadows (unreleased)

Not booted on an Orb. The simulator's self-test gained a check that hand shadows reload after the clock releases its
sprites, and it failed on the old code and passes on the new.

- [ ] On a theme that ships hand shadows (only Elegant does), visit another app and come back to the clock several times:
      the three shadows must still be there every time. Before this change they vanished after the first release until a
      reboot. This may be the cause of the "shadow silently gone missing right after a theme install" reports noted in
      `clock_view.cpp`, which were put down to memory pressure.
- [ ] The Flight Tracker layers, the News plate and glass, and the clock's plate and overlay now load through
      `plate_sprite`. Log lines are tagged `[radar_plate]`, `[intel_plate]`, `[custom_sprite plate]` and so on. Check
      each still loads from flash on a theme that has been baked, and from the card when it has not.

### `radar_view.cpp` split into five files (unreleased)

Not booted on an Orb. The device firmware builds, and in the simulator the Flight Tracker's frames are pixel-identical
before and after on every stock and migrated theme, but nothing measured the frame rate on the board.

`radar_view.cpp` (3300 lines) became `radar_view.cpp`, `radar_sweep.cpp`, `radar_aircraft.cpp`, `radar_flatbg.cpp`,
`radar_select.cpp` and the private `radar_internal.h`. The file-level state that was `static` is now an ordinary
global in `namespace radar_impl`, and a handful of small helpers (`show`, `alt_color`, `canvas_acquire`) that the compiler
could once inline into their callers now sit in another translation unit.

- [ ] Flight Tracker's `[rframe]` line (printed every ten seconds) should match the figures from before the split, for the
      grid, sweep and aircraft phases: a call across files is a real call now, and the sweep and blip draw callbacks
      run every frame. If a phase regressed, move the offending helper into `radar_internal.h` as `static inline`.
- [ ] The profiler's timing array is now a C++17 `inline` variable in the header so every radar file adds to the same
      one; confirm `[rframe]` still shows non-zero times for all four phases (`grid`, `sweep`, `aircraft`, `wx`).

### `settings_view.cpp` split into four files (unreleased)

Not booted on an Orb. The device firmware builds, the simulator's self-test output is identical, and the Settings screenshots
(main wheel, theme picker, WiFi setup) are pixel-identical before and after.

`settings_view.cpp` (2050 lines) became `settings_view.cpp` (state, the knob dispatch, `init()`), `settings_pages.cpp`
(display, sound, chime, theme, range, units, volume, About, reset), `settings_location.cpp` (location, recents, city search)
and `settings_wifi.cpp` (first-boot choice, phone path, no-SD notice, network list, password strip, connect status), sharing a
private `settings_internal.h`. The state moved out of an anonymous namespace into `settings_impl` so the files can share it.

- [ ] Walk the first-boot path on a wiped Orb: no-card notice (if the card is out), the WiFi choice, scan, pick, password, connect.
      It is the path a stranger takes and the one with the least room for a regression.
- [ ] Location search still finds a city while typing (its `search_tick` timer moved to `settings_location.cpp`), and the
      WiFi connect status still times out after 20 s (`wifi_tick`, now in `settings_wifi.cpp`).
