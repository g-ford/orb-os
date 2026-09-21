#!/usr/bin/env python3
"""Build every theme in src/theme_assets/ in one go.

    python3 tools/build_all_themes.py                       # all of them -> sim/sdcard/themes
    python3 tools/build_all_themes.py fallout portal        # just these (folder names)
    python3 tools/build_all_themes.py --out /Volumes/ORB/themes    # onto the SD card itself

A theme is any folder in src/theme_assets/ holding a theme.yaml, so adding one needs no change here.
The default output is the desktop simulator's stand-in SD card (gitignored), which is what
`.pio/build/native/program` reads its themes from; pick one there with T, or in Settings.

Each theme is built by tools/build_theme.py, exactly as `build_theme.py <folder> --out <out>` would.
A theme that fails does not stop the others, and the exit code says so (1). Rebuilding replaces
<out>/<slug>/, so a folder there is only ever the last build of its source.

It does not touch which theme the simulator is wearing, and it does not remove themes from <out>
whose source has gone: delete those by hand.
"""
from __future__ import annotations

import argparse
import contextlib
import io
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_theme  # noqa: E402

REPO = Path(__file__).resolve().parents[1]
DEFAULT_SRC = REPO / 'src' / 'theme_assets'
DEFAULT_OUT = REPO / 'sim' / 'sdcard' / 'themes'


def slug_of(folder: Path) -> str:
    """The slug build_theme will use, so two folders claiming one can be told apart before either builds.
    A theme that cannot be read falls back to its folder name; the real build reports what is wrong."""
    try:
        data = build_theme.load_yaml(folder / 'theme.yaml')
    except (build_theme.BuildError, OSError):
        return folder.name
    return str(data.get('slug') or folder.name)


def main(argv) -> int:
    ap = argparse.ArgumentParser(description='Build every theme in src/theme_assets/ into one folder.')
    ap.add_argument('names', nargs='*', help='folder names to build (default: all of them)')
    ap.add_argument('--src', type=Path, default=DEFAULT_SRC, help='where the theme folders are (default: src/theme_assets)')
    ap.add_argument('--out', type=Path, default=DEFAULT_OUT,
                    help='folder to build into (default: sim/sdcard/themes, the simulator\'s SD card)')
    args = ap.parse_args(argv)

    if not args.src.is_dir():
        print(f'error: {args.src} is not a folder', file=sys.stderr)
        return 2
    children = sorted(p for p in args.src.iterdir() if p.is_dir())
    # `default` is the built-in look, not a card theme: it has no folder on the card (see theme_slug_policy.h).
    themes = [p for p in children if (p / 'theme.yaml').is_file() and p.name != 'default']
    for p in children:
        if p not in themes:
            print(f'skipped  {p.name}  (no theme.yaml)')
    if not themes:
        print(f'error: no theme folders (holding a theme.yaml) in {args.src}', file=sys.stderr)
        return 2

    if args.names:
        unknown = [n for n in args.names if n not in {p.name for p in themes}]
        if unknown:
            print(f'error: no such theme: {", ".join(unknown)} (have: {", ".join(p.name for p in themes)})',
                  file=sys.stderr)
            return 2
        themes = [p for p in themes if p.name in args.names]

    args.out.mkdir(parents=True, exist_ok=True)
    width = max(len(p.name) for p in themes)
    built, failed = 0, 0
    claimed: dict[str, str] = {}
    for folder in themes:
        slug = slug_of(folder)
        if slug in claimed:
            failed += 1
            print(f'FAILED   {folder.name.ljust(width)}  slug {slug!r} is already claimed by {claimed[slug]!r}; '
                  'building it would overwrite that theme', file=sys.stderr)
            continue
        claimed[slug] = folder.name
        warnings: list = []
        captured = io.StringIO()
        try:
            with contextlib.redirect_stdout(captured):
                build_theme.build(folder, args.out, warnings)
        except (build_theme.BuildError, OSError) as e:
            failed += 1
            print(f'FAILED   {folder.name.ljust(width)}  {e}', file=sys.stderr)
            continue
        built += 1
        counts = (captured.getvalue().splitlines() or ['', ''])[-1].strip()      # "12 image(s), 22 font(s), ..."
        print(f'built    {folder.name.ljust(width)}  {counts}')
        for w in warnings:
            print(f'           warning: {w}', file=sys.stderr)

    print(f'\n{built} built, {failed} failed -> {args.out}')
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
