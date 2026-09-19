#!/usr/bin/env python3
"""Regenerate src/theme_assets/default from the firmware's own defaults.

    python3 tools/gen_default_theme.py            # write theme.yaml and the clock images
    python3 tools/gen_default_theme.py --check    # exit 1 if theme.yaml is out of date

The default theme is not written by hand. tools/dump_theme_defaults.cpp is built against the
real src/theme/core/theme_style.cpp and asked what the firmware does with no theme at all
(seed_defaults(), which is the struct defaults in theme_style.h overridden by the CUSTOM_*
macros in src/theme/custom). Whatever it says is written out as YAML, every option, so the file
is both the default look and a template listing every option a theme can set.

Because it is generated, the --check mode (and tests/test_default_theme.py) fail when a firmware
default changes and the file was not regenerated, which a comment saying "keep this in step"
would not do.

What makes it the aviator theme, and the only places it differs from the firmware's defaults:
  * the clock plate is the aviator dial compiled into the firmware (dial_avi.h), and the hands
    are the compiled hour/minute/second art from custom_hands.h, exported as PNGs;
  * the second hand is centred on the dial's sub-dial (AVI_SUB_X/Y in dial_avi.h), which is
    where the built-in aviator face draws its seconds, instead of on the dial centre.
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
THEME_DIR = REPO / 'src' / 'theme_assets' / 'default'
DUMPER_SRC = REPO / 'tools' / 'dump_theme_defaults.cpp'
PREVIEWS = REPO / 'build' / 'theme_previews' / 'default'

SLUG, NAME, AUTHOR = 'default', 'Default', 'Orb OS'
SECTIONS = ('clock', 'radar', 'weather', 'ticker', 'settings', 'menu', 'splash', 'intel')

# firmware export (render_theme_bitmaps.py names it by symbol) -> the name a theme uses
IMAGES = {
    'dial_avi.png':          'clock_plate.png',
    'custom_hour_png.png':   'clock_hand_hour.png',
    'custom_minute_png.png': 'clock_hand_minute.png',
    'custom_second_png.png': 'clock_hand_second.png',
}

NOTES = {
    'clock.hands.second.centerX': 'the aviator dial keeps its seconds in the sub-dial (AVI_SUB_X/Y in dial_avi.h)',
    'radar.selOpa': "theme_style.cpp loads this key into the SETTINGS selection opacity, not the radar's "
                    "(a likely slip in load(); it is written here because that is where it is read)",
    'radar.zones': 'up to 6 masks: {x, y, r} is a circle, {x, y, w, h, rect: true} a rectangle; '
                   'invert: true hides everything outside it (at most one)',
    'weather.zones': 'same shape as radar.zones',
}

_COLOR = re.compile(r'0x[0-9A-F]{6}')


class GenError(Exception):
    pass


# ---- running the firmware --------------------------------------------------------------

def _includes() -> list[str]:
    libs = REPO / '.pio' / 'libdeps' / 'native'
    lvgl, aj = libs / 'lvgl', libs / 'ArduinoJson' / 'src'
    if not lvgl.exists() or not aj.exists():
        raise GenError('the LVGL / ArduinoJson sources are not in .pio/libdeps/native; '
                       'run `pio run -e native` (or `pio pkg install -e native`) once')
    dirs = ['include', 'src', 'src/app', 'src/app/common', 'src/app/intel', 'src/core', 'src/platform',
            'src/platform/storage', 'src/theme', 'src/theme/core', 'src/theme/graphics', 'src/theme/custom',
            'src/theme/fonts']
    return (['-DLV_CONF_INCLUDE_SIMPLE'] + [f'-I{REPO / d}' for d in dirs] +
            [f'-I{lvgl}', f'-I{lvgl / "src"}', f'-I{aj}'])


def build_dumper(out: Path) -> Path:
    compiler = shutil.which('g++') or shutil.which('clang++')
    if not compiler:
        raise GenError('no C++ compiler (g++ or clang++) on the PATH')
    cmd = [compiler, '-std=gnu++17', *_includes(), str(DUMPER_SRC),
           str(REPO / 'src' / 'theme' / 'core' / 'theme_style.cpp'), '-o', str(out)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        raise GenError(f'could not build the dumper:\n{r.stderr}')
    return out


def run_dumper(binary: Path, theme_dir: Path | None = None) -> dict:
    r = subprocess.run([str(binary)] + ([str(theme_dir)] if theme_dir else []), capture_output=True, text=True)
    if r.returncode:
        raise GenError(f'the dumper failed:\n{r.stderr}')
    # theme_style.cpp logs to stdout too ("[theme_style] theme declares N asset(s)"); the JSON is last.
    return json.loads(r.stdout.strip().splitlines()[-1])


def aviator_sub_dial() -> tuple[int, int]:
    text = (REPO / 'src' / 'theme' / 'custom' / 'dial_avi.h').read_text(encoding='utf-8', errors='ignore')[:2000]
    x, y = re.search(r'AVI_SUB_X\s+(\d+)', text), re.search(r'AVI_SUB_Y\s+(\d+)', text)
    if not (x and y):
        raise GenError('AVI_SUB_X/Y not found in dial_avi.h')
    return int(x.group(1)), int(y.group(1))


def with_aviator(defaults: dict) -> dict:
    """The firmware's defaults, with the one change that makes the clock use the aviator dial."""
    out = json.loads(json.dumps(defaults))
    second = out['clock']['hands']['second']
    second['centerX'], second['centerY'] = aviator_sub_dial()
    return out


# ---- YAML ------------------------------------------------------------------------------

def scalar(v) -> str:
    if isinstance(v, bool):
        return 'true' if v else 'false'
    if isinstance(v, (int, float)):
        return repr(v)
    if isinstance(v, str):
        return v if _COLOR.fullmatch(v) else json.dumps(v, ensure_ascii=False)
    raise GenError(f'cannot write {v!r}')


def _is_scalar(v) -> bool:
    return not isinstance(v, (dict, list))


def emit(node: dict, indent: int, path: str, out: list[str]):
    pad = ' ' * indent
    unset = node.get('_unset')
    for key, value in node.items():
        if key == '_unset':
            continue
        here = f'{path}.{key}' if path else key
        note = f'  # {NOTES[here]}' if here in NOTES else ''
        if isinstance(value, dict):
            if not value:
                out.append(f'{pad}{key}: {{}}{note}')
                continue
            out.append(f'{pad}{key}:{note}')
            emit(value, indent + 2, here, out)
        elif isinstance(value, list):
            if not value:
                out.append(f'{pad}{key}: []{note}')
            elif all(_is_scalar(v) for v in value):
                out.append(f'{pad}{key}: [{", ".join(scalar(v) for v in value)}]{note}')
            else:
                out.append(f'{pad}{key}:{note}')
                for item in value:
                    lines: list[str] = []
                    emit(item, indent + 4, here + '[]', lines)
                    out.append(f'{pad}  - {lines[0].lstrip()}')
                    out.extend(lines[1:])
        else:
            out.append(f'{pad}{key}: {scalar(value)}{note}')
    if unset:
        out.append(f'{pad}# unset by default, the firmware decides: {", ".join(unset)}')


def render_yaml(defaults: dict) -> str:
    out = [
        '# The default theme, and a template listing every option a theme can set.',
        '# GENERATED by tools/gen_default_theme.py from the firmware\'s own defaults: do not edit this',
        '# file, edit a copy. Any option left out of a theme keeps the value shown here.',
        '# See docs/theme-yaml.md.',
        f'slug: {SLUG}',
        f'name: {NAME}',
        f'author: {AUTHOR}',
        'version: 1',
        'default: true',
    ]
    for key in ('apps', 'names'):
        out.append('')
        out.append(f'{key}:')
        emit(defaults[key], 2, key, out)
    for section in SECTIONS:
        out.append('')
        out.append(f'{section}:')
        emit(defaults[section], 2, section, out)
    return '\n'.join(out) + '\n'


# ---- images ----------------------------------------------------------------------------

def export_images(dest: Path):
    subprocess.run([sys.executable, str(REPO / 'tools' / 'render_theme_bitmaps.py')],
                   check=True, capture_output=True)
    dest.mkdir(parents=True, exist_ok=True)
    for src_name, theme_name in IMAGES.items():
        src = PREVIEWS / src_name
        if not src.exists():
            raise GenError(f'render_theme_bitmaps.py did not produce {src_name}')
        shutil.copyfile(src, dest / theme_name)


# ---- main ------------------------------------------------------------------------------

def generate() -> str:
    with tempfile.TemporaryDirectory() as tmp:
        binary = build_dumper(Path(tmp) / 'dump_theme_defaults')
        return render_yaml(with_aviator(run_dumper(binary)))


def main(argv) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--check', action='store_true', help='fail if theme.yaml does not match the firmware')
    args = ap.parse_args(argv[1:])
    try:
        text = generate()
        target = THEME_DIR / 'theme.yaml'
        if args.check:
            if not target.exists() or target.read_text(encoding='utf-8') != text:
                print(f'{target.relative_to(REPO)} is out of date: run python3 tools/gen_default_theme.py',
                      file=sys.stderr)
                return 1
            print('default theme is up to date')
            return 0
        THEME_DIR.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding='utf-8')
        export_images(THEME_DIR)
        print(f'wrote {target.relative_to(REPO)} and {len(IMAGES)} images')
        return 0
    except GenError as e:
        print(f'error: {e}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
