#!/usr/bin/env python3
"""What the firmware resolves a theme to: every option's value, as JSON.

    python3 tools/resolved_golden.py src/theme_assets/portal
    python3 tools/resolved_golden.py src/theme_assets/portal --write tests/golden/resolved_portal.json

The theme is built with tools/build_theme.py and read by tools/dump_theme_defaults.cpp, which links the real
theme_style.cpp, so the numbers are what the firmware computes and not a copy of them. Only the theme's own
sections are kept, so the file does not move when the firmware learns a new top-level key (the `palette` the
dumper now reports). Used to prove a theme still resolves to exactly the same values after it moves to a palette.
"""
from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_elegant_theme as gen  # noqa: E402

KEEP = ('apps', 'names', 'clock', 'radar', 'weather', 'ticker', 'splash', 'intel')


def resolve_theme(theme_dir: Path, dumper: Path | None = None) -> dict:
    """Build `theme_dir` and return the kept sections of what the firmware makes of it. Pass a `dumper` binary to
    reuse one build across several themes."""
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        binary = dumper or gen.build_dumper(tmp / 'dump_theme_defaults')
        built = gen.build_folder(theme_dir, tmp / 'built')
        state = gen.run_dumper(binary, built)
    return {k: state[k] for k in KEEP}


def diff_paths(a, b, where: str = '') -> list:
    """Every place `a` and `b` differ, as `path: a -> b`, in a stable order."""
    out = []
    if isinstance(a, dict) and isinstance(b, dict):
        for k in sorted(set(a) | set(b)):
            path = f'{where}.{k}' if where else str(k)
            if k not in a:
                out.append(f'{path}: <missing> -> {b[k]!r}')
            elif k not in b:
                out.append(f'{path}: {a[k]!r} -> <missing>')
            else:
                out += diff_paths(a[k], b[k], path)
    elif isinstance(a, list) and isinstance(b, list) and len(a) == len(b):
        for i, (x, y) in enumerate(zip(a, b)):
            out += diff_paths(x, y, f'{where}[{i}]')
    elif a != b:
        out.append(f'{where}: {a!r} -> {b!r}')
    return out


def main(argv) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('theme', type=Path, help='a theme source folder (holds theme.yaml)')
    ap.add_argument('--write', type=Path, metavar='FILE', help='write the JSON here instead of printing it')
    args = ap.parse_args(argv[1:])
    try:
        text = json.dumps(resolve_theme(args.theme), indent=1, sort_keys=True) + '\n'
    except gen.GenError as e:
        print(f'error: {e}', file=sys.stderr)
        return 1
    if args.write:
        args.write.parent.mkdir(parents=True, exist_ok=True)
        args.write.write_text(text, encoding='utf-8')
        print(f'wrote {args.write}')
    else:
        sys.stdout.write(text)
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
