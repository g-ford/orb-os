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

- [ ] `FW_VERSION` is `2.18.0`. It was bumped **without** a hardware check, so treat 2.18.0 as unreleased until this
      whole list is ticked; the web config page and the Stats screen should show it.
- [ ] **Install path.** `python3 tools/build_all_themes.py --out /Volumes/ORB/themes` (building Fallout and Portal
      needs `npx`/`lv_font_conv`), copy `elegant` to the card too if it still holds the old `default` folder, then select
      each theme in Settings. The saved slug `default` no longer matches anything after the rename.
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

- [ ] With no theme selected the Orb boots to the built-in look: green on black, no images, on every screen.
- [ ] A theme that is only a palette (four colours) looks designed on every screen, not just the clock.

### Task 6: the reserved `default` slug and Settings > Design

- [ ] Settings > Design lists **Default** first, and choosing it reboots into the built-in look and stays there across
      reboots (it must not switch to the first card theme).
- [ ] With Default active, choosing Default again does not reboot. Choosing an installed theme and then Default works.
- [ ] A card folder called `default` is not listed.
