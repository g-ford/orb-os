# Unverified on a real Orb

CLAUDE.md rule 1: firmware runs on a real Orb before it is called done. From 2026-09-21 the owner had no Orb to
hand, so the changes below built, passed the host tests and (where noted) ran in the simulator, but have **not**
booted on hardware. Work through this list when an Orb is available, and delete an entry only once it is checked.
Add to it with every firmware change made without a board.

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
