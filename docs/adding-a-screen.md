# Adding a screen

A screen is more than what it shows. Every screen has a set of standard parts, and it is not
finished until it has them. This is the checklist for building one so it arrives complete
instead of being repaired over several days.

## Why this file exists

The Headlines (Intel) screen was built a piece at a time in August 2026, and every piece
that got missed had already existed on every other screen for months. In order, the owner
had to ask for: a background image, the glass and CRT layer, a typeface for each text
element, and top and bottom margins. None of these were new ideas. They were the standard
equipment of a screen, and shipping without them was not a smaller version of the feature,
it was a broken one.

The rule this file encodes: **anywhere a screen has text, a picture, or a background, it
gets the same controls every other screen has for that thing.** Screens differ in what
they show. They do not differ in how their text is set.

## The standard parts

### Every screen

| Part | What it needs |
|---|---|
| Background | colour, plus `<screen>_plate.png` decoded flash-first then SD |
| Glass / CRT | `<screen>_overlay.png`, composited over everything |
| Capability level | a `THEME_CAPS` bump **and** a ledger entry per feature (`src/theme/core/theme_style.h`) |
| Memory | attach art on enter, release on exit (see [memory.md](memory.md)) |
| Swipes | if the screen has several screens the knob steps between, register a pager (`app_shell::setPager`) so up/down swipes work too; see "Touch: swipes only" in [ARCHITECTURE.md](ARCHITECTURE.md) |

Ship the plate only when the design actually uses a picture. Baked art is raw RGB565, so a
466x466 plate of flat colour costs 424 KB of the `themeart` partition to say what one JSON
field already says. The glass is already skipped when it is switched off.

### Every text element

This is the part that kept getting shortchanged. A text element means **all** of these,
not a subset:

- **Words**: static text, or a format string with a token for anything live
  (`{t}`, `{callsign}`). Any text that can come from outside must be ASCII-folded before it
  reaches the device: the font has no fallback, so a curly apostrophe draws an empty box.
- **Typeface and weight**: declared as a face in the theme's `fonts:` block and mapped to a slot
  in `theme_font.cpp`. A converted face is baked at one size by `lv_font_conv`, so the size
  control is what gets baked rather than something the device varies afterwards. Give the
  screen an `intel_has_font`-style predicate if it needs to know which it got.
- **Size**: from the compiled ladder only (`lv_conf.h`). LVGL fonts are glyph bitmaps, not
  outlines: a size the binary was not built with cannot be drawn at any quality. Offer a
  slider over ladder positions, never a free pixel value, and snap unknown values to the
  default rather than to the nearest: nearest silently redesigns the theme.
- **Colour**, and a **stale/alternate colour** if the element can go out of date.
- **Glow** and glow colour.
- **Across / Down**: absolute screen px, hidden when the element is curved.
- **Curve**: on, radius, angle, via `curved_text::draw_arc`. Do not write a fourth copy of
  the glyph-rotation maths; it lives in `src/app/common/curved_text.cpp` and both the clock
  and the scope already call it.
- **Show / hide.**

### Every image element

Source, on/off, zoom, dim, position. Decoded flash-first then SD, released on exit, and
declared in the theme's asset list so a file left behind by an older push is not drawn.

### Layout

Left, right, **top and bottom** margins. Top and bottom were the ones missed on Intel: the
band was worked out from whatever sat above and below, which is a fine default and a poor
ceiling. Default them to 0 meaning "work it out", and let a stated value win.

Where a screen stacks layers, it gets the same layer ordering the Flight Tracker has, with
the background pinned at the bottom and the glass pinned at the top.

### Screens fed by live data

- Poll interval as theme data, not a `#define`.
- An age or freshness readout, and a colour that changes when it stops being current.
- Honest empty states that say **which** thing is unwell: no WiFi, not asked yet, and the
  service not answering are three different sentences.
- If more can arrive than fits, the knob scrolls: press takes the knob, turn moves, press
  again or an idle timeout releases. Use the Flight Tracker's grammar rather than a new one.
- Separate **how many are fetched** from **how many are shown**. They are different
  questions and welding them together caps the design to the dial.
- Hold the data in one place and read narrow windows out of it. At twenty items an
  `IntelSnapshot` is over 2 KB, and copying that onto a task stack is not something this
  device's internal RAM can absorb.
- Build widgets for what can be **seen**, not for what can be **held**.

## Before calling it done

- [ ] Both firmware targets build (`native` and `esp32-s3-amoled-175`).
- [ ] `bash tests/run_host_tests.sh` passes.
- [ ] `THEME_CAPS` bumped and a ledger entry written for each feature.
- [ ] Elegant is regenerated (`python3 tools/gen_elegant_theme.py`) and
      `tests/test_elegant_theme.py` passes: a brand new theme must look identical to what
      the screen drew before it was themeable.
- [ ] Every option that indexes a lookup table has an entry in it, with a test that walks
      the option's full range. A new font size with no line-height entry lays two lines of
      text on top of each other.
- [ ] The screen was looked at in a simulator screenshot, not judged by expectation.
- [ ] Nothing new is drawn on a resting screen that a person did not ask for. An indicator
      that sits inside a line of text becomes punctuation.
- [ ] PSRAM taken on enter is given back on exit.
- [ ] It has booted on a real Orb (CLAUDE.md rule 1).

## Where things live

| | |
|---|---|
| Screen view | `src/app/<screen>/<screen>_view.cpp` |
| Its art loader | `src/app/<screen>/<screen>_sprite.cpp` (model on `src/app/intel/intel_sprite.cpp`, the smallest) |
| Theme data | `src/theme/core/theme_style.{h,cpp}` |
| Font slots | `src/theme/core/theme_font.{h,cpp}` |
| Curved text | `src/app/common/curved_text.cpp`: shared, do not copy |
| Elegant theme (the reference) | `src/theme_assets/elegant/theme.yaml`, generated by `tools/gen_elegant_theme.py` |
| Tests | `tests/`, run together with `bash tests/run_host_tests.sh` |
