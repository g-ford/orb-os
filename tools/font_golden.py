#!/usr/bin/env python3
"""Which font file each text slot resolves to in a BUILT theme, as `slot sha256` lines.

    python3 tools/font_golden.py build/themes/fallout
    python3 tools/font_golden.py build/themes/fallout --write tests/golden/fonts_fallout.txt

A slot resolves the way the firmware resolves it: through theme.json's `fonts` map, else font_<slot>.bin,
else nothing (the compiled face draws). The hash is of the file's bytes, so two builds match exactly when
the same typeface at the same size draws the same slot. Used to prove a theme moved to a `fonts:` block
still draws every slot with byte-identical glyphs.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_theme  # noqa: E402


def resolve(built: Path) -> dict:
    slots = build_theme.firmware_facts()['slots']
    mapping = {}
    theme_json = built / 'theme.json'
    if theme_json.exists():
        mapping = json.loads(theme_json.read_text(encoding='utf-8')).get('fonts', {})
    out = {}
    for slot in sorted(slots):
        path = built / mapping.get(slot, f'font_{slot}.bin')
        if path.is_file():
            out[slot] = hashlib.sha256(path.read_bytes()).hexdigest()
    return out


def render(resolved: dict) -> str:
    return ''.join(f'{slot} {digest}\n' for slot, digest in sorted(resolved.items()))


def main(argv) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('built', type=Path, help='a built theme folder (the output of tools/build_theme.py)')
    ap.add_argument('--write', type=Path, metavar='FILE', help='write the lines here instead of printing them')
    args = ap.parse_args(argv[1:])
    if not args.built.is_dir():
        print(f'error: {args.built} is not a folder', file=sys.stderr)
        return 2
    text = render(resolve(args.built))
    if args.write:
        args.write.parent.mkdir(parents=True, exist_ok=True)
        args.write.write_text(text, encoding='utf-8')
        print(f'wrote {args.write} ({len(text.splitlines())} slot(s))')
    else:
        sys.stdout.write(text)
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
