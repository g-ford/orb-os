#!/usr/bin/env python3
"""Report the colours a theme repeats, and suggest four to make its palette. It only reports: it never rewrites a
file, because the YAML round trip would lose the comments a theme is full of.

    python3 tools/palettize.py src/theme_assets/portal

Then write the `palette:` block by hand and replace each repeated hex with its `$role`.
"""
from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

HEX = re.compile(r'(?<![\w$])(?:0x|#)([0-9A-Fa-f]{6})\b')
_NOT_A_HEX_COMMENT = re.compile(r' #(?![0-9A-Fa-f]{6}\b)')


def _code(line: str) -> str:
    """The line without its comment. ` #RRGGBB` is a colour written the short way, not a comment."""
    if line.lstrip().startswith('#'):
        return ''
    m = _NOT_A_HEX_COMMENT.search(line)
    return line[:m.start()] if m else line


def _palette_lines(text: str):
    """(in_palette, line) for every line: the palette block is the indented lines under a top-level `palette:`."""
    inside = False
    for line in text.split('\n'):
        if re.match(r'^palette:\s*$', line):
            inside = True
            yield True, line
            continue
        if inside and line and not line.startswith((' ', '\t', '#')):
            inside = False
        yield inside, line


def count_colours(text: str) -> Counter:
    """How often each colour is written outside the palette block and outside comments."""
    counts: Counter = Counter()
    for inside, line in _palette_lines(text):
        if inside:
            continue
        for m in HEX.finditer(_code(line)):
            counts[int(m.group(1), 16)] += 1
    return counts


def palette_colours(text: str) -> dict:
    """{role: colour} for the palette block, in file order."""
    out = {}
    for inside, line in _palette_lines(text):
        m = re.match(r'^\s+(\w+):\s*(?:0x|#)([0-9A-Fa-f]{6})\b', line) if inside else None
        if m:
            out[m.group(1)] = int(m.group(2), 16)
    return out


def _luma(c: int) -> int:
    return (((c >> 16) & 255) * 299 + ((c >> 8) & 255) * 587 + (c & 255) * 114) // 1000


def _saturation(c: int) -> float:
    hi = max((c >> 16) & 255, (c >> 8) & 255, c & 255)
    lo = min((c >> 16) & 255, (c >> 8) & 255, c & 255)
    return (hi - lo) / hi if hi else 0.0


def suggest(counts: Counter) -> dict:
    """A starting guess at bg, primary, secondary and text from how often each colour is used. Only a guess: a person
    picks the palette. Never suggests one colour for two roles, and leaves a role out rather than invent it."""
    ranked = [c for c, _ in counts.most_common()]
    out: dict = {}

    def take(role, wanted):
        for c in ranked:
            if wanted(c):
                out[role] = c
                ranked.remove(c)
                return

    take('bg', lambda c: _luma(c) < 60)
    top = ranked[:6]
    brightest = max((c for c in top if _luma(c) > 150), key=_luma, default=None)
    if brightest is not None:
        out['text'] = brightest
        ranked.remove(brightest)
    take('primary', lambda c: _saturation(c) > 0.4)
    take('secondary', lambda c: _saturation(c) > 0.2)
    if 'primary' not in out and ranked:          # nothing colourful: the most used remaining colour leads
        out['primary'] = ranked.pop(0)
    return {r: out[r] for r in ('bg', 'primary', 'secondary', 'text') if r in out}


def main(argv) -> int:
    if len(argv) != 2 or not (Path(argv[1]) / 'theme.yaml').is_file():
        print('usage: palettize.py <theme folder holding theme.yaml>', file=sys.stderr)
        return 2
    text = (Path(argv[1]) / 'theme.yaml').read_text(encoding='utf-8')
    counts = count_colours(text)
    print(f'{sum(counts.values())} colour(s) written, {len(counts)} distinct (outside the palette block):')
    for c, n in counts.most_common():
        print(f'  {n:3d}  0x{c:06X}  luma {_luma(c):3d}')
    print('suggested:', {r: f'0x{c:06X}' for r, c in suggest(counts).items()})
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
