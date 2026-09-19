#!/usr/bin/env python3
"""Fit artwork from an image generator (or any editor) to what the firmware can draw.

    python3 tools/fit_theme_art.py master.png src/theme_assets/portal/clock_plate.png
    python3 tools/fit_theme_art.py hand.png out.png --size 26x156

Does no drawing: it only resamples and converts. The firmware's PNG decoders draw an image at its
own pixel size and accept only 8-bit RGBA (anything else is silently drawn as black), and a plate
is drawn onto a 466x466 screen. Generators usually give a big square RGB image, so this scales it
down (Lanczos) and adds an opaque alpha channel. Keep the big original next to the theme as the
master, in the theme's source/ folder, and fit from that, so a plate can be redone at any time.

Needs Pillow (pip3 install pillow).
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit('fit_theme_art.py needs Pillow:  pip3 install pillow')

SCREEN = 466            # the round AMOLED is 466x466


def size_arg(text: str) -> tuple[int, int]:
    try:
        w, h = text.lower().split('x')
        return int(w), int(h)
    except ValueError:
        raise argparse.ArgumentTypeError('use WIDTHxHEIGHT, e.g. 466x466')


def main(argv) -> int:
    ap = argparse.ArgumentParser(description='Resample and convert art so the firmware can draw it.')
    ap.add_argument('source', type=Path)
    ap.add_argument('dest', type=Path)
    ap.add_argument('--size', type=size_arg, default=(SCREEN, SCREEN), metavar='WxH',
                    help=f'target size (default {SCREEN}x{SCREEN}, a full-screen plate)')
    ap.add_argument('--stretch', action='store_true',
                    help='allow a different aspect ratio (otherwise it is an error: it would distort)')
    args = ap.parse_args(argv[1:])

    if args.source.resolve() == args.dest.resolve():
        print('error: the source and destination are the same file; keep the master somewhere else '
              '(a source/ folder in the theme) so it is not overwritten', file=sys.stderr)
        return 2
    img = Image.open(args.source)
    tw, th = args.size
    if not args.stretch and abs(img.width / img.height - tw / th) > 0.01:
        print(f'error: {args.source.name} is {img.width}x{img.height}, which is not the aspect ratio of '
              f'{tw}x{th}; crop it first, or pass --stretch', file=sys.stderr)
        return 1
    had_alpha = 'A' in img.getbands()
    if img.size != (tw, th):
        img = img.convert('RGBA').resize((tw, th), Image.LANCZOS)
    img = img.convert('RGBA')
    args.dest.parent.mkdir(parents=True, exist_ok=True)
    img.save(args.dest, optimize=True)
    print(f'{args.source.name} -> {args.dest}: {tw}x{th} RGBA'
          f'{"" if had_alpha else " (opaque: the source had no alpha channel)"}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
