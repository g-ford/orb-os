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

### Flight Tracker: coastline, kite-shaped blips, idle auto-rotate (FW 2.23.0)

Built, host- and Python-tested, and screenshot-compared in the simulator (Tampa Bay coastline draws correctly on the
built-in look and Elegant; blips are vector kite/triangle shapes, not the uploaded icon). The idle auto-rotate's 30 s
timing could not be checked in the simulator — see below — so it is reasoned from code review and the pre-existing
(already shipped) `SELECT_IDLE_MS` mechanism it mirrors, not observed running.

- [ ] The scope's coastline actually appears for a real Orb's own location when one is near water, in the built-in
      look and in a theme that does not set `mapCoastOn` (it defaults on).
- [ ] The default look's blips are the vector triangle/kite shape (not the old icon sprite); `CUSTOM_BLIP_KITE_T 85`
      reads as "a plain isoceles triangle with a slight indent on the back edge", not a pronounced tail. Tune the
      value on the owner's own eye if it doesn't.
- [ ] With nothing selected and the knob untouched for 30 s, the scope starts stepping through in-range aircraft on
      its own, one every 30 s, looping continuously; a knob turn at any point hands control back immediately and
      picks up cycling from wherever it was.
- [ ] This never fires with no traffic in range (stays on the plain default view) and never fires while another app
      is on screen (confirm by idling on, say, Weather for well over 30 s, then opening Flight Tracker — it must not
      already be mid-rotation the instant it appears).
- **Not verified in the simulator.** `--themeshot`'s settle wait does not advance LVGL's tick (`lv_tick_inc` is never
  called in that loop), so a real-time wait there never reaches `AUTO_ROTATE_IDLE_MS`. The interactive capture path
  (`SIM_CLOCKSHOT`/`SIM_FRAMESHOT`) does advance it, but hung indefinitely under `SDL_VIDEODRIVER=dummy` for this
  build before it reached a capture — a pre-existing rough edge in the composite/chrome setup, not something this
  change touches. Worth fixing if idle timing on this screen needs checking this way again.

### App switcher: no more idle auto-commit (unreleased)

Built and self-tested only; the self-test always commits with an explicit press, so it could not have caught a
regression in the removed behaviour either way.

- [ ] Turning the knob to open the switcher and then leaving it alone sits there indefinitely, showing whatever app
      the cursor is on, until a press commits it — it must not open on its own after a couple of seconds the way it
      used to (`BROWSE_SETTLE_MS` removed from `app_shell.cpp`).

### Auto-cycle: Settings > Display > Auto-cycle (unreleased)

A new ambient-slideshow mode: Settings > Display gains a third row, cycling Off / 1 min / 5 min / 10 min / 15 min /
30 min (default Off). When set, `loop()` in `main.cpp` advances to the next app via `app_shell::next()` once the
device has gone that long with no knob, touch or motion input at all — reusing `display::inactiveMs()`, the same
inactivity clock the existing idle-dim feature reads. Built and host-tested (the `settings_store` guard covers the
new `autoCycleMs` key); the Settings UI row was exercised in the simulator's self-test (navigation only — the
simulator never builds `main.cpp`, so **the actual timed advance cannot run in the simulator at all**, same
limitation noted above for the radar's idle auto-rotate). The interval logic is reasoned from code review against
the idle-dim mechanism it mirrors, not observed running.

- [ ] Settings > Display shows "Auto-cycle   Off" by default; pressing it cycles Off → 1 min → 5 min → 10 min →
      15 min → 30 min → Off, same press-to-advance feel as the "Screen" row above it.
- [ ] With Auto-cycle set to 1 min (fastest, for a quick check) and the Orb left untouched, it slides to the next
      app roughly every minute, skipping any app hidden by the active theme, same as a manual `next()`.
- [ ] Any knob turn, touch or motion resets the countdown — confirm by nudging the knob partway through the
      interval and checking it does not advance until a full interval after that nudge.
- [ ] It never advances while the app switcher overlay is open (turn the knob to open it, then wait out the
      interval: the switcher must still be showing, not have silently committed and moved on).
- [ ] It never advances while the Orb is asleep face-down.
- [ ] Turn it back to Off and confirm the device stops cycling and stays on whatever app you leave it on.

### App settings registry: the Flight Tracker submenu and the generic web settings page (FW 2.27.0)

Plan `docs/superpowers/sdd/2026-10-05-app-settings-registry`. Apps now declare their settings once,
as a `SettingDescriptor` array, and both the on-device Settings menu and the web config page render
that same data instead of each having its own hand-written controls. Radar is the pilot: its five
settings (max aircraft, units, hide ground, military only, large text) move from web-only routes to
a dynamic "Flight Tracker" row in the on-device Settings menu (opening a generic Toggle/Slider/Enum
submenu built once and re-labelled per group) and to a single generic `POST /setting` route plus one
card renderer on the web page, replacing the five old routes (`/units`, `/milonly`, `/bigtext`,
`/maxac`, `/ground`) and their bespoke HTML/JS. Built, host- and Python-tested (228 Python tests,
all host tests including the new descriptor/registry/radar-settings suites), and both PlatformIO
environments build clean; the native sim's self-test still passes its existing navigation checks
(the dynamic top-level row count now includes one row per registered group) but does not walk into
the Flight Tracker submenu itself or exercise a web request.

**Flashed to a real Orb and confirmed on-device and on the web portal, 2026-10-05.** The known
theme-override limitation below still needs its own explicit check (it requires switching to a
theme that opines on these settings, which the general confirmation above didn't necessarily cover).

- [x] Settings shows one new "Flight Tracker" row (singular — Radar is the only registered group so
      far) below the fixed Display/Location/Sound/.../Reset rows; selecting it opens a submenu
      listing Max aircraft, Units, Hide ground, Military only and Large text, in that order.
- [x] Each row's control behaves correctly on the real knob: Max aircraft is a slider, Units cycles
      Aviation/Metric/Imperial, and the three toggles flip on a press — not just that they draw, but
      that turning/pressing one actually changes the stored value (watch the serial log or come back
      into the submenu after leaving).
- [x] Each change takes effect on the Flight Tracker screen itself: Max aircraft changes how many
      blips can show, Units changes the readout units immediately, Hide ground and Military only
      filter the aircraft on screen, and Large text changes the Flight Tracker type size.
- [x] **Large text reboots the device** (`g_rebootAtMs = millis() + 1200`, the same mechanism the old
      web-only toggle used to give the HTTP response time to reach the browser). On-device there is no
      browser response to wait for — confirm the 1.2 s delay still reboots cleanly from a knob press,
      with no watchdog trip and no corrupted NVS write caught mid-flight.
- [x] Back out of the Flight Tracker submenu to the top-level Settings menu and confirm the row order
      and count above/below it (Reset, Range, etc.) are unchanged — this plan made that row count
      dynamic, and a miscount here would be the kind of thing the simulator's self-test already
      caught once as a segfault during development (see commit `fbd3aa8`), now fixed, but worth a
      real-device look.
- [x] Load the web config page over the Orb's own WiFi: a settings card for "Flight Tracker" renders
      the same five controls, and changing one through the browser (a POST to `/setting`) both
      persists and live-applies on the Orb — including watching Large text actually trigger the
      device reboot from a phone browser, not just the simulator's mocked HTTP path.
- [x] An unknown/stale setting key (e.g. a bookmarked old `/maxac`-style request, or a stale page open
      from before this update) 404s rather than silently doing nothing, as the new generic route is
      designed to.
- [ ] **Theme-override display, fixed in code and flashed (FW 2.27.0, then a follow-up fix for the
      sim's own stale mirror) — blocked on an SD card, not yet checked on real hardware.**
      A theme whose `theme.yaml` sets `maxAircraft` or `hideGround` (Elegant does: `maxAircraft: 5`,
      `hideGround: true`) used to make both renderers show the stored NVS value instead of the
      theme-forced effective one. Fixed with a `readLive` accessor on `SettingDescriptor` (both
      renderers now call `settings::display_int(d)`, which prefers the live value when one exists);
      see `docs/superpowers/specs/2026-10-05-app-settings-registry-design.md`'s "Resolved limitation"
      section. Host- and Python-tested, both PlatformIO environments build clean, code-reviewed
      (twice — the first pass caught the native sim's own copy of this bug, in its separate
      theme-preview path, fixed in the same branch).

      **This Orb currently has no SD card installed, and themes live on the card** — with no card,
      the active theme is the built-in fallback, which has no opinion on `maxAircraft`/`hideGround`
      (`applyThemeSettings()`'s guards `rs.maxAircraft > 0` / `rs.hideGround >= 0` both skip), so
      there is nothing for the two settings to diverge from and this check cannot be performed yet.
      Once a card with Elegant installed is available: switch to Elegant and confirm the Flight
      Tracker submenu and the web card both show "Hide ground: On" and "Max aircraft: 5" (not
      whatever's separately stored in NVS), and that pressing the on-device Hide ground toggle once
      actually flips what's drawn (not silently no-op for one press — the on-device cycle handler
      was also changed to advance from the displayed value, not the stored one).

      Same no-SD-card block now also applies to `rangeKm` and `minAltFt` (FW 2.28.0 gave both a
      `readLive` too — see the entry below). Elegant sets `rangeKm: 30` and `minAltFt: 1250`;
      once a card is available, also confirm the Flight Tracker submenu and the web card show
      those theme-forced values rather than whatever's separately stored in NVS, and that they
      stay within each setting's slider range (10..150 km, 0..60000 ft) even if a theme file sets
      something outside it — `applyThemeSettings()` now clamps both the same way it already
      clamped `maxAircraft`.

### App settings registry: radar's remaining six settings + Range page removal (FW 2.28.0)

Finishes the migration above: Aircraft trails, Minimum altitude, Show radar sweep, Show airports,
Screen rotation and Display range move from the old Settings > Range page and bespoke web routes
(`/range`, `/sweep`, `/airports`, `/altmin`, `/trail`, `/rotate`, now deleted) into the same
Flight Tracker group, bringing it to 11 settings. The on-device Range page (`MODE_RANGE`,
`ITEM_RANGE`) is deleted entirely — `ITEM_FIXED_COUNT` drops from 9 to 8. `RANGE_KM` changes type
from `Float` to `Int` (lo/hi 10..150, matching `ADSB_QUERY_MAX_KM`); a device holding an old
float-typed `"rangeKm"` reads back the default once via NVS's own type-mismatch fallback. A new
`step` field lets a Slider advance by more than 1 per knob press (Minimum altitude: 5000 ft,
Screen rotation: 15°, Display range: 10 km). `ROT_DEG` gained a `readLive` hook
(`radar_read_rot_deg_live`) reporting the display driver's own read-back, because
`display::setRotation()` can silently normalize a requested angle to 0 when there's no PSRAM for
it — without this, the stored/requested angle and the one actually drawn could diverge with
nothing saying so. `applyThemeSettings()`'s theme-forced `rangeKm`/`minAltFt` are now clamped to
their descriptors' ranges the same way `maxAircraft` already was, so a theme (or a stale one
written before a range tightened) can't report a `readLive` value outside what either renderer's
control can represent. Built, host- and Python-tested, both PlatformIO environments build clean;
the native sim's self-test was updated for the new `ITEM_FIXED_COUNT` and walks the Flight Tracker
group instead of the deleted Range page. Code-reviewed (twice — the second pass caught the
sim's own rotDeg stub briefly regressing in the fix round, since fixed); all Important findings
(the two above, plus the web page's Sliders now showing a live numeric readout via an `<output>`
element, and the `RANGE_KM` ceiling landing at 150 instead of a stricter 100) were fixed before
merge.

**Flashed to the real Orb (`/dev/cu.usbmodem2101`), 2026-10-05.** This device was running FW
2.27.0 beforehand (the pilot commit, `RANGE_KM` still a `Float`), so the boot itself exercises the
NVS type-mismatch fallback this plan relied on. What was checked without the knob (serial log +
the web config page over the device's own WiFi, since there is no SD card to swipe through
on-device screens with and the knob isn't reachable from here):

- [x] Clean boot, no crash/watchdog reset: `[diag] boot #18`, `setup done (firmware 2.28.0, first
      boot after an update)`, WiFi connected, `[theme] applied: rangeKm=30 maxAircraft=12
      minAltFt=0 hideGround=0 ...` — the built-in fallback theme (no SD card), so none of the
      theme-override settings have anything to diverge from here (same caveat as FW 2.27.0 below).
- [x] **The old-float-to-new-int NVS migration, on the one device that could actually test it**:
      booting this build against this device's real, previously-`Float`-typed `"rangeKm"` key
      produced `rangeKm=30` (the declared default), not a crash or a garbage value — confirms the
      checklist item this replaces, for real, not just via the host test's `FakePrefs`.
- [x] Loaded `http://192.168.1.242/`: the Flight Tracker card renders all 11 controls, in the
      documented order, with no separate "Range" card. Each Slider's initial `<output>` matches
      its `value` attribute (`Max aircraft` 12, `Minimum altitude (ft)` 5000, `Screen rotation
      (deg)` 2, `Display range (km)` 30) and the `min`/`max` attributes are exactly 1..12, 0..60000,
      0..359, 10..150.
- [x] `POST /setting` round-trips: `key=rangeKm&value=50` → `200 ok`, and a re-fetch of `/` shows
      `<output>50`. `key=rangeKm&value=999` → `200 ok` but the stored/displayed value clamps to
      `150` — confirms the `applyThemeSettings()`-style clamp added in the fix round actually
      reaches a real NVS write, not just the in-RAM theme path it was written for. Restored to 30
      afterward.
- [x] `key=bogusKey&value=1` → `404 unknown key`; `GET /range` (one of the six deleted routes) →
      `404 Not found: /range` — the generic route and the old routes' removal both hold on real
      hardware, not just in the simulator's mocked HTTP path.

**Still needs the physical knob** (no SD card and no hands-on-device access from here — this is
the part only a person at the Orb can do):

- [ ] Settings > Flight Tracker lists all 11 rows and the group's own Back row returns to the main
      Settings menu; the top-level Settings menu no longer has a "Range" row.
- [ ] Each of the six new rows' control behaves correctly on the real knob: Aircraft trails cycles
      Off/Short/Medium/Long, Minimum altitude/Screen rotation/Display range are sliders that step
      by 5000/15/10 per press and wrap at their ends, Show radar sweep/Show airports toggle.
- [ ] Each change takes effect on the Flight Tracker screen itself: trail length, altitude floor
      filtering, sweep animation, airport markers, screen rotation (confirm the display actually
      physically rotates on a quarter-turn value, not just the stored number changing), and
      display range (the range rings/labels update).
- [ ] **Screen rotation specifically**: set it to a quarter-turn value (90/180/270) and confirm the
      screen visibly rotates. If this Orb's build has no PSRAM scratch buffer for rotation (check
      serial log for `display::setRotation` falling back), the fix in this branch means the
      Settings row should itself show 0° afterward (via the new `readLive`), not the angle you
      requested — if it instead still shows your requested angle, the `readLive` wiring is broken.
      A designed-in consequence of that same fix: the knob's Slider now advances from whatever
      `readLive` reports, so on a build stuck at 0° the knob can't step past it either (every
      press reads 0, adds 15, writes 15, and the display normalizes straight back to 0) — only
      the web slider can still set a non-zero value in that state. If you see "stuck at 0° from
      the knob," that's this, not a new bug.

### App settings registry: Weather units moves into a registered group; WX_ZOOM's dead code deleted (FW 2.29.0)

`WX_UNITS` (Auto/Metric/Imperial) moves from a bespoke fixed "Units" page (on-device) and a
hand-coded web card + `/wxunits` route into a new registered "Weather" settings_registry group
(`src/app/weather/weather_settings.h/.cpp`), same shape as Flight Tracker's group. The top-level
Settings menu's `ITEM_FIXED_COUNT` drops from 8 to 7 with the Units page gone — WiFi, Theme,
About and Reset each sit one row higher than before. Separately, `WX_ZOOM`'s dead settings
plumbing (a weather-map zoom tier a prior "lean redesign" already fixed at tier 0 everywhere,
with every live setter removed) is deleted outright: the NVS key, `host_wx_zoom_set()`/
`host_wx_zoom_tier()` (confirmed zero callers), and the flag whose only setter was
`host_wx_zoom_set`. No behavior change from that half — the weather map was already always at
its 50mi tier. Built, host- and Python-tested (231 host-test cases including a new
`weather_settings_test.cpp`, 230 Python tests), both PlatformIO environments build clean; the
native sim's self-test gained a Settings > Weather block exercising multi-group navigation for
the first time. Code-reviewed; the `snprintf` format/argument audit for the deleted web card was
independently re-derived (29→28, balanced).

**Flashed to the real Orb (`/dev/cu.usbmodem2101`), 2026-10-06.** This device was running FW
2.28.0 beforehand, so the boot exercises the real upgrade path, not just a host test. Checked
without the knob (serial log + the web config page over the device's own WiFi, same limits as
the prior entry — no SD card, no hands-on access from here):

- [x] Clean boot, no crash/watchdog reset: `setup done (firmware 2.29.0, ...)`, WiFi connected,
      weather fetched successfully. `[theme] applied: rangeKm=30 maxAircraft=12 ...` — the
      built-in fallback theme (no SD card), so this doesn't exercise Auto-mode resolution
      against a theme-set location, only the default/no-theme path.
- [x] Loaded `http://192.168.1.242/`: card order is Display, Location, Sound, WiFi, Theme,
      About, Reset, **Flight Tracker, Weather** — no "Units" card anywhere. The Weather card
      renders exactly one control, `<label>Weather units</label>` with Auto/Metric/Imperial
      options and the stored one selected.
- [x] `POST /setting` round-trips for `wxUnits`: `value=2` (Imperial) → `200 ok`, a re-fetch of
      `/` shows Imperial selected; restored to `value=0` (Auto) afterward.
- [x] `GET /wxunits` (the deleted route) → `404 Not found: /wxunits`.

**Still needs the physical knob** (no SD card and no hands-on-device access from here):

- [ ] The top-level Settings menu no longer has a "Units" row; WiFi, Theme, About and Reset sit
      one position higher than FW 2.28.0.
- [ ] Settings > Weather lists its one row ("Weather units") and the group's own Back row
      returns to the main Settings menu, not the app switcher.
- [ ] Pressing the row cycles Auto → Metric → Imperial → Auto on the real knob, and the Weather
      screen's temperature/distance readouts change accordingly within each mode. In Auto mode,
      the row only shows "Auto" now, not the resolved units the old page showed alongside it
      (e.g. "Auto (F, mi)") — confirmed deliberate (see `weather_settings.h`'s comment), not a
      regression to chase.

### App settings registry: sound settings split across Flight Tracker, Clock and a new "System" group (FW 2.30.0)

The old hand-coded "Sound" page (on-device) and web card bundled settings owned by three
different subsystems. Each setting now lives with the thing it actually affects, confirmed by
tracing every call site rather than assumed from the old page's grouping:

- **Flight Tracker** gains `SND_RADAR` ("Radar sounds") and `ALERT_MODE` ("Alert on") --
  both gate only the radar-proximity/new-aircraft notifier. Alert on gets on-device UI for the
  first time (it was web-only before).
- **Clock** gets its first registered settings group ever, one entry: `SND_CHIME` ("Clock
  chime") -- gates only the on-the-hour chime.
- **"System"**, a new kind of group: registered directly via
  `settings_registry::register_group()` rather than through `app_shell::add()`, since it isn't
  owned by any one app and has no screen to attach to (see `system_settings.h`). One entry:
  `MUTE` ("Mute alerts") -- confirmed genuinely cross-cutting (`audio_play()` applies it to
  every cue uniformly: radar beeps, the clock chime, test pings). Mute had no on-device UI at
  all before this -- it's a new capability, not just a port.

Two settings were deliberately **not** migrated, each for a reason specific to it, not a
blanket policy:

- **Volume (`VOL`)** keeps its own dedicated page. Its knob-turn-to-adjust-with-live-audio-
  preview interaction has no equivalent in the generic settings_registry group, which only
  supports press-to-cycle. Forcing it in would trade a direct, continuous "turn the knob to hear
  it change" feel for "press repeatedly to step by 10" -- a real interaction downgrade, not a
  cosmetic one, so it was kept bespoke instead.
- **Proximity alert (`PROX_KM`)** keeps its bespoke web-only route. Its options are shown in
  whichever distance unit the owner has chosen (nm/km/mi), which the generic descriptor model
  has no field to express; migrating it would mean losing that per-unit relabeling, not just a
  hint the way Weather's "Auto" label was.
- **Chime selection (`CHIME_IDX`)** stays its own bespoke picker, same reason Theme selection
  isn't in the registry: its option count varies at runtime (one entry per installed theme plus
  the flash built-ins), which the registry's fixed-size `Enum` can't represent. Its on-device
  entry point moved to jump directly from the shrunk "Sound" page, alongside Volume.

The on-device "Sound" page still exists as a fixed top-level item, now holding only Volume and
Chime sound (`ITEM_FIXED_COUNT` is unchanged at 7 -- no fixed row was removed, only trimmed).
The web "Sound" card shrinks to Volume, Proximity alert, Test ping and Chime sound; Mute, Alert
on, Radar sounds and Clock chime move to their new cards. `handleSound()` and the `/sound`
route are deleted outright (both toggles it handled are now reached through the generic
`POST /setting` route instead). Built, host- and Python-tested (all host test scripts pass,
including two new ones, `run_clock_settings_test.sh` and `run_system_settings_test.sh`; all 230
Python tests pass), both PlatformIO environments build clean; the native sim's self-test gained
Settings > Clock and Settings > System blocks (four groups now navigate correctly in sequence:
Clock, Flight Tracker, Weather, System).

**Flashed to the real Orb (`/dev/cu.usbmodem2101`), 2026-10-07.** This device was running FW
2.29.0 beforehand, so the boot exercises the real upgrade path. Checked without the knob (serial
log + the web config page over the device's own WiFi, same limits as prior entries -- no SD
card, no hands-on access from here):

- [x] Clean boot, no crash/watchdog reset: `setup done (firmware 2.30.0, first boot after an
      update)`, WiFi connected, weather fetched -- confirms the FW 2.29.0-or-earlier upgrade path
      item above.
- [x] Loaded `http://192.168.1.242/`: card order is Display, Location, Sound, WiFi, Theme,
      About, Reset, **Clock, Flight Tracker, Weather, System** -- matching registration order.
      The Sound card shows exactly Volume, Proximity alert, Test ping, Chime sound (no Mute,
      Alert on, Radar sounds, Clock chime there). The new Clock card shows exactly one row
      ("Clock chime"). The new System card shows exactly one row ("Mute alerts"). Flight
      Tracker's card shows all 13 rows, including "Radar sounds" and "Alert on" (Off/Emergencies
      only/New aircraft + emergencies, the last one selected by default).
- [x] `POST /setting` round-trips for all four new/moved keys: `mute`, `sndChime`, `sndRadar`,
      `alertmode` each returned `200 ok`, and a re-fetch of `/` showed the written value
      reflected (confirmed for `mute`: toggled to `checked`, then restored to unchecked).
      Restored all four to their pre-test values afterward.
- [x] `GET /sound` (the deleted route) → `404 Not found: /sound`.

**Still needs the physical knob** (no SD card and no hands-on-device access from here -- and no
way to confirm audio content remotely, so the chime-preview-sounds-correct checks stay here too):

- [ ] The on-device Sound page now lists only "Volume" and "Chime sound" + Back (no more Radar
      sounds / Clock chime rows there).
- [ ] Settings > Flight Tracker's two new rows behave correctly on the real knob. "Alert on"
      showing "New aircraft + emergencies" is the longest label any registry row has carried (37
      characters) -- confirm the wheel doesn't truncate or overlap it on the real 466px panel
      (the already-shipped "Large text (restarts the device)" row is 38 characters and renders
      fine, so this is expected to be okay, but hasn't been seen on this specific row).
- [ ] Settings > Clock's row toggles on the knob and the group's own Back row returns to the
      main menu, not the app switcher. Toggling it on previews the actual selected chime
      immediately via `chime_library::playSelected()` (fixed in this migration -- the old page
      always played the hardcoded flash Westminster regardless of which chime was selected;
      confirm a non-default theme chime now previews correctly, not Westminster).
- [ ] Settings > System's row toggles on the knob and its Back row returns to the main menu.
      Toggling it actually mutes/unmutes every sound (radar beep, clock chime, test ping), not
      just a cosmetic checkbox.
- [ ] Toggling "Clock chime" on from the web now audibly previews on the device (new -- the old
      `/sound` route never did this; the registry's generic onChanged path does, matching the
      on-device behavior). The `POST /setting` call succeeded (confirmed above); whether it
      actually made a sound on this device could not be confirmed remotely.
