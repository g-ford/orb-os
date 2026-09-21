#!/usr/bin/env python3
"""Bake one typeface at one size into the lv_font_conv binary the firmware loads.

The format is fixed by the firmware, not chosen: `--bpp 4 --no-compress`, because the curved-text and
menu renderers read the 4-bit glyph bitmaps straight out of the file (see curved_text.cpp).

Needs lv_font_conv (npm). It is found, in order, from $LV_FONT_CONV, from PATH, or fetched by
`npx --yes lv_font_conv@1.5.3`. Pinned: a different version could lay glyphs out differently.
"""
from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path

CONVERTER_VERSION = '1.5.3'

# ASCII, then what real text on these screens carries beyond it: degrees and plus-minus for weather,
# a middle dot the theme's own weather line uses, and the curly quotes, dashes, ellipsis and bullet
# a headline is full of (they are not sanitised on the way in, and a glyph the face lacks draws blank).
DEFAULT_RANGES = '0x20-0x7E,0xB0,0xB1,0xB7,0x2013,0x2014,0x2018,0x2019,0x201C,0x201D,0x2022,0x2026'


class FontBakeError(Exception):
    pass


def converter() -> list[str]:
    env = os.environ.get('LV_FONT_CONV')
    if env:
        return [env]
    if shutil.which('lv_font_conv'):
        return ['lv_font_conv']
    if shutil.which('npx'):
        return ['npx', '--yes', f'lv_font_conv@{CONVERTER_VERSION}']
    raise FontBakeError('lv_font_conv is needed to bake a .ttf face: npm install -g lv_font_conv, '
                        'or set $LV_FONT_CONV to its path')


def bake_face(src: Path, size: int, out: Path, ranges: str = DEFAULT_RANGES) -> None:
    cmd = converter() + ['--font', str(src), '--size', str(size), '--bpp', '4', '--no-compress',
                         '--format', 'bin', '--range', ranges, '-o', str(out)]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
    except OSError as e:
        raise FontBakeError(f'cannot run lv_font_conv ({cmd[0]}): {e}')
    if result.returncode != 0 or not out.exists() or out.stat().st_size == 0:
        detail = (result.stderr or result.stdout).strip() or 'no output'
        raise FontBakeError(f'lv_font_conv failed for {src.name} at {size}px: {detail}')
