#!/usr/bin/env python3
"""Write src/theme_assets/default: the theme every Orb starts from, and the template for new ones.

    python3 tools/gen_default_theme.py --from-orb "Some theme.orb"   # make the default this theme
    python3 tools/gen_default_theme.py                               # re-list every option, keep the values
    python3 tools/gen_default_theme.py --check                       # exit 1 if theme.yaml is not in that form

The default theme is a design made in Orb Studio, like any other, and the folder is where it
lives: theme.yaml plus its images and fonts. What this script adds is the form of theme.yaml.
tools/dump_theme_defaults.cpp is built against the real src/theme/core/theme_style.cpp and asked
what the firmware makes of a theme folder, and whatever it says is written out as YAML, every
option and every colour as 0xRRGGBB. So theme.yaml is both the default look and a template
listing every option a theme can set, whichever way the theme stated them.

--from-orb unpacks the .orb, resolves it that way, and replaces the folder's contents. With no
argument the folder is resolved as it stands, which is what to run after editing theme.yaml by
hand (comments are not kept) or after the firmware learns a new option. --check, and
tests/test_default_theme.py, fail when theme.yaml is not what that produces: a hand edit that
was not re-listed, or an option the firmware added that the default theme does not mention.

The firmware's compiled defaults (seed_defaults(): the struct defaults in theme_style.h and the
CUSTOM_* macros in src/theme/custom) are what an Orb shows with no theme on its card, and they
are NOT this theme. They are the fallback, and the Portal theme is written against them.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
THEME_DIR = REPO / 'src' / 'theme_assets' / 'default'
DUMPER_SRC = REPO / 'tools' / 'dump_theme_defaults.cpp'

SLUG, NAME, AUTHOR = 'default', 'Default', 'Orb OS'
SECTIONS = ('clock', 'radar', 'weather', 'ticker', 'settings', 'menu', 'splash', 'intel')

# What a theme folder holds besides theme.yaml. Anything else in an .orb (Studio's own
# studio.json, the device's _installed marker, the generated *_style.json) is not the theme.
ART = re.compile(r'[a-z0-9_]+\.(?:png|bin)')

NOTES = {
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


def unpack_orb(orb: Path, dest: Path):
    """Write every file in a .orb into dest, which is what Studio would have sent over the cable."""
    spec = importlib.util.spec_from_file_location('read_orb_bundle', REPO / 'tools' / 'read-orb-bundle.py')
    reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    try:
        _, files = reader.unpack(str(orb))
    except (SystemExit, OSError, struct.error, UnicodeDecodeError) as e:
        raise GenError(f'{orb}: {e}')
    dest.mkdir(parents=True, exist_ok=True)
    for name, data in files:
        # the name comes from inside the file, so it must not be able to leave dest
        if Path(name).name != name or name in ('', '.', '..'):
            raise GenError(f'{orb}: refusing a file named {name!r}')
        (dest / name).write_bytes(data)


def build_folder(theme: Path, out: Path) -> Path:
    """The folder the Orb reads for a theme source folder, made by tools/build_theme.py."""
    r = subprocess.run([sys.executable, str(REPO / 'tools' / 'build_theme.py'), str(theme), '--out', str(out)],
                       capture_output=True, text=True)
    if r.returncode:
        raise GenError(f'build_theme.py failed on {theme}:\n{r.stderr}')
    if r.stderr:
        raise GenError(f'build_theme.py warned about {theme}:\n{r.stderr}')
    return out / theme_slug(theme)


def theme_slug(theme: Path) -> str:
    m = re.search(r'^slug:\s*(\S+)', (theme / 'theme.yaml').read_text(encoding='utf-8'), re.M)
    if not m:
        raise GenError(f'{theme}/theme.yaml has no slug')
    return m.group(1)


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
        '# Designed in Orb Studio. tools/gen_default_theme.py writes this file: from a .orb, or, after',
        '# a hand edit, by re-listing every option (edit, then run it, or the tests fail).',
        '# An option left out of ANOTHER theme keeps the firmware\'s compiled value, which is not',
        '# necessarily the value shown here. See docs/theme-yaml.md.',
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


# ---- main ------------------------------------------------------------------------------

def generate(theme: Path = THEME_DIR) -> str:
    """theme.yaml as it should read for the theme folder `theme`: what the firmware makes of it."""
    with tempfile.TemporaryDirectory() as tmp:
        binary = build_dumper(Path(tmp) / 'dump_theme_defaults')
        return render_yaml(run_dumper(binary, build_folder(theme, Path(tmp) / 'built')))


def import_orb(orb: Path):
    """Replace the default theme's folder with the theme in a .orb."""
    with tempfile.TemporaryDirectory() as tmp:
        unpacked = Path(tmp) / 'orb'
        unpack_orb(orb, unpacked)
        binary = build_dumper(Path(tmp) / 'dump_theme_defaults')
        text = render_yaml(run_dumper(binary, unpacked))
        art = sorted(p for p in unpacked.iterdir() if ART.fullmatch(p.name))

        THEME_DIR.mkdir(parents=True, exist_ok=True)
        for old in THEME_DIR.iterdir():
            if old.name == 'theme.yaml' or ART.fullmatch(old.name):
                old.unlink()
        (THEME_DIR / 'theme.yaml').write_text(text, encoding='utf-8')
        for p in art:
            shutil.copyfile(p, THEME_DIR / p.name)
    # the folder must resolve to what was just written, or theme.yaml would not be the theme
    if generate() != text:
        raise GenError('the folder written from the .orb does not resolve to the same theme.yaml')
    print(f'wrote {(THEME_DIR / "theme.yaml").relative_to(REPO)} and {len(art)} image/font files')


def main(argv) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--from-orb', type=Path, metavar='FILE', help='make the default theme the theme in this .orb')
    ap.add_argument('--check', action='store_true', help='fail if theme.yaml does not list every option as the firmware reads it')
    args = ap.parse_args(argv[1:])
    if args.from_orb and args.check:
        ap.error('--from-orb and --check do not go together')
    try:
        if args.from_orb:
            import_orb(args.from_orb.expanduser())
            return 0
        text = generate()
        target = THEME_DIR / 'theme.yaml'
        if args.check:
            if not target.exists() or target.read_text(encoding='utf-8') != text:
                print(f'{target.relative_to(REPO)} is not in the form the firmware reads it: '
                      'run python3 tools/gen_default_theme.py', file=sys.stderr)
                return 1
            print('default theme is up to date')
            return 0
        target.write_text(text, encoding='utf-8')
        print(f'wrote {target.relative_to(REPO)}')
        return 0
    except GenError as e:
        print(f'error: {e}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
