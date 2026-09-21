#!/usr/bin/env python3
"""Compare two folders of --themeshot images (see tools/themeshots.sh).

    python3 tools/shot_diff.py [--skip-clock] before after

Prints one line per screenshot: `same`, or the bounding box of the pixels that changed and how many changed, so a
reviewer can check that only the region a change was meant to touch has moved. Exits 1 if anything differs, or if a
screenshot exists in only one folder.

--skip-clock leaves out the first app of every theme (`<slug>-0-*.bmp`). That is the clock in all four shipped themes,
and the simulator draws the wall time and a running second hand, so it differs on every run. Look at it instead.
"""
import re
import sys
from pathlib import Path

from PIL import Image, ImageChops

FIRST_APP = re.compile(r'^[^-]+-0-')


def diff(a: Path, b: Path):
    ia, ib = Image.open(a).convert('RGB'), Image.open(b).convert('RGB')
    if ia.size != ib.size:
        return f'SIZE {ia.size} -> {ib.size}', True
    d = ImageChops.difference(ia, ib).convert('L').point(lambda v: 255 if v else 0)
    box = d.getbbox()
    if box is None:
        return 'same', False
    changed = sum(1 for v in d.getdata() if v)
    return f'CHANGED bbox={box} pixels={changed}', True


def main(before: str, after: str, skip_clock: bool) -> int:
    fa, fb = Path(before), Path(after)
    names_a = {p.name for p in fa.glob('*.bmp')}
    names_b = {p.name for p in fb.glob('*.bmp')}
    bad = False
    for name in sorted(names_a | names_b):
        if name not in names_a or name not in names_b:
            print(f'{name}: ONLY IN {"after" if name in names_b else "before"}')
            bad = True
            continue
        if skip_clock and FIRST_APP.match(name):
            print(f'{name}: skipped (the clock shows the wall time)')
            continue
        text, differs = diff(fa / name, fb / name)
        bad = bad or differs
        print(f'{name}: {text}')
    return 1 if bad else 0


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if a != '--skip-clock']
    if len(args) != 2:
        sys.exit(__doc__)
    sys.exit(main(args[0], args[1], '--skip-clock' in sys.argv[1:]))
