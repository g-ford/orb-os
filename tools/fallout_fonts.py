#!/usr/bin/env python3
"""Bakes the Fallout theme's typeface into src/theme_assets/fallout/font_*.bin.

    python3 tools/fallout_fonts.py

The face is Share Tech Mono (SIL Open Font License, source/OFL.txt): a clean techno monospace, the
closest free thing to the terminal face on the games' wrist computer. The firmware cannot scale a
font, only load one baked at a single size, so one file is written per text slot at the size that
slot's layout was tuned for (the sizes the default theme bakes, or the ones the firmware draws at
when it has no face of its own). A slot that is not listed here draws with the compiled font.

The format is fixed by the firmware, not chosen: `--bpp 4 --no-compress`, because the curved-text and
menu renderers read the 4-bit glyph bitmaps straight out of the file (see curved_text.cpp).

Needs lv_font_conv (npm). It is found, in order, from $LV_FONT_CONV, from PATH, or fetched by
`npx --yes lv_font_conv@1.5.3`. Pinned: a different version could lay glyphs out differently.
"""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / 'src' / 'theme_assets' / 'fallout'
FACE = OUT / 'source' / 'ShareTechMono-Regular.ttf'
sys.path.insert(0, str(Path(__file__).resolve().parent))
import font_bake  # noqa: E402

# ASCII, then what real text on these screens carries beyond it: degrees and plus-minus for weather,
# a middle dot the theme's own weather line uses, and the curly quotes, dashes, ellipsis and bullet
# a headline is full of (they are not sanitised on the way in, and a glyph the face lacks draws blank).
RANGES = font_bake.DEFAULT_RANGES

# slot file -> pixel size. Keep in step with theme_font.cpp's slot list (the test checks the names).
SLOTS = {
    'font_menu_current.bin': 46,        # the current app's name, large across the middle
    'font_settings.bin': 27,
    'font_radar1.bin': 22, 'font_radar2.bin': 16, 'font_radar3.bin': 16, 'font_radar4.bin': 16,
    'font_weather1.bin': 22, 'font_weather2.bin': 22, 'font_weather3.bin': 16, 'font_weather4.bin': 16,
    'font_intel_title.bin': 32, 'font_intel_text.bin': 22, 'font_intel_source.bin': 12,
    'font_intel_age.bin': 12, 'font_intel_brief.bin': 18,
    'font_ticker_name.bin': 16, 'font_ticker_price.bin': 40, 'font_ticker_change.bin': 22,
    'font_ticker_strip.bin': 16,
    'font_wind_title.bin': 28, 'font_wind_ask.bin': 20, 'font_wind_turns.bin': 16,
}


def converter() -> list[str]:
    try:
        return font_bake.converter()
    except font_bake.FontBakeError as e:
        sys.exit(f'fallout_fonts.py: {e}')


def bake(out_dir: Path) -> None:
    if not FACE.exists():
        sys.exit(f'missing {FACE.relative_to(REPO)}')
    cmd = converter()
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, size in SLOTS.items():
        subprocess.run(cmd + ['--font', str(FACE), '--size', str(size), '--bpp', '4', '--no-compress',
                              '--format', 'bin', '--range', RANGES, '-o', str(out_dir / name)], check=True)


def main(argv) -> int:
    bake(OUT)
    print(f'wrote {len(SLOTS)} fonts to {OUT}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
