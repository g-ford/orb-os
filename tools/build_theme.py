#!/usr/bin/env python3
"""Build a theme folder that is ready to copy onto the Orb's SD card.

    python3 tools/build_theme.py src/theme_assets/default [--out build/themes]

The theme is authored as a folder holding theme.yaml plus its images and fonts. The device
never sees the YAML: it reads the JSON files theme_style.cpp already knows, so this script
writes exactly those, into <out>/<slug>/, alongside the images.

The rule that keeps this small: YAML keys ARE the JSON keys. A top-level section named
after a screen becomes that screen's file, verbatim:

    clock:    -> clock_style.json        weather:  -> weather_style.json
    radar:    -> radar_style.json        ticker:   -> ticker_style.json
    settings: -> settings_style.json     menu:     -> menu_style.json
    splash:   -> splash_style.json       intel:    -> intel_style.json

and slug / name / author / version / default / apps / names become theme.json. Nothing is
whitelisted here, so any option theme_style.cpp reads is configurable and stays so as
options are added. What the script DOES check is the things that go wrong silently on the
device: a mistyped key (the firmware ignores it without a word), a duplicate key (YAML
keeps the last), a JSON file over the size the firmware will read, and the asset list.

Baking (PNG -> RGB565 in the themeart flash partition) happens on the device, on the first
boot after the theme is selected. This script cannot do that, but it makes it correct:
theme.json's `assets` is derived from the files actually in the folder, because the device
only bakes what is listed, and `assetsHash` covers their contents, because that is what
makes it re-bake a replaced image that kept its name.
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from pathlib import Path

try:
    import yaml
except ImportError:
    sys.exit('build_theme.py needs PyYAML:  pip3 install pyyaml')

REPO = Path(__file__).resolve().parents[1]
CORE = REPO / 'src' / 'theme' / 'core'

SECTIONS = ('clock', 'radar', 'weather', 'ticker', 'settings', 'menu', 'splash', 'intel')
MANIFEST_KEYS = ('slug', 'name', 'author', 'version', 'default', 'apps', 'names')
DERIVED_KEYS = ('assets', 'assetsHash')      # written by this script, never by hand
MAX_SLUG_LEN = 31                            # theme_select.h MAX_SLUG_LEN 32, less the NUL


class BuildError(Exception):
    pass


# ---- facts read from the firmware, so this script has no second copy to drift ----------

# Names the theme art used before the flat, per-screen names, mapped to the name the firmware
# opens. The device knows nothing of these: a built folder only ever holds the new names.
LEGACY_NAMES = {
    'custom_hour_png.png':     'clock_hand_hour.png',
    'custom_minute_png.png':   'clock_hand_minute.png',
    'custom_second_png.png':   'clock_hand_second.png',
    'office_hour_png.png':     'clock_hand_hour.png',
    'office_minute_png.png':   'clock_hand_minute.png',
    'hand_hour_img_map.png':   'clock_hand_hour.png',
    'hand_min_img_map.png':    'clock_hand_minute.png',
    'dial_img.png':            'clock_plate.png',
    'dial_avi.png':            'clock_plate.png',
    'splash_png_default.png':  'splash.png',
    'splash_png_office.png':   'splash.png',
}

# The image names the firmware opens, read out of the source rather than listed here, so a
# new layer added to a screen is picked up without touching this script. Matched by what an
# asset is called (plate, overlay, hand, ...) so unrelated PNG literals in src/ are ignored.
_IMAGE_NAME = re.compile(r'"((?:[a-z]+_)?(?:plate|overlay|sweep|blip|static[12]|hand_hour|hand_minute|hand_second)|splash)\.png"')


def firmware_facts() -> dict:
    """Asset names, font slots and limits, taken from the sources that use them."""
    def read(path: Path) -> str:
        if not path.exists():
            raise BuildError(f'cannot find {path.relative_to(REPO)}; run this from a full checkout')
        return path.read_text(encoding='utf-8')

    font, style = read(CORE / 'theme_font.cpp'), read(CORE / 'theme_style.cpp')

    images = set()
    for path in (REPO / 'src').rglob('*.cpp'):
        images.update(f'{m}.png' for m in _IMAGE_NAME.findall(path.read_text(encoding='utf-8', errors='ignore')))
    aliases = dict(LEGACY_NAMES)
    fonts = set(re.findall(r'"(font_[a-z0-9_]+\.bin)"', font))
    limit = re.search(r'MAX_STYLE_JSON_BYTES\s*=\s*(\d+)', style)
    keys = set(re.findall(r'"([A-Za-z][A-Za-z0-9_]*)"', style))

    if not images or not fonts or not limit or not keys:
        raise BuildError('could not read the asset names / font slots / JSON limit out of src/theme/core; '
                         'the firmware source has changed shape and this script needs updating')
    return {'images': images, 'fonts': fonts, 'aliases': aliases,
            'max_json': int(limit.group(1)), 'keys': keys}


# ---- YAML ------------------------------------------------------------------------------

class StrictLoader(yaml.SafeLoader):
    """SafeLoader with two changes, both for the same reason: a theme option that is quietly
    not what you wrote.

    1. A duplicate key is an error. PyYAML keeps the last one without a word.
    2. Only true/false are booleans. YAML 1.1 also reads on/off/yes/no as booleans, and
       `on` is a real firmware key (hands.shadow.on), so under the stock rules that option
       could not be written at all: the key itself became the boolean True."""

    def construct_mapping(self, node, deep=False):
        seen = set()
        for key_node, _ in node.value:
            key = self.construct_object(key_node, deep=True)
            if key in seen:
                raise yaml.constructor.ConstructorError(
                    None, None, f'duplicate key {key!r}', key_node.start_mark)
            seen.add(key)
        return super().construct_mapping(node, deep)


_BOOL_TAG = 'tag:yaml.org,2002:bool'
StrictLoader.yaml_implicit_resolvers = {
    first: [(tag, rx) for tag, rx in resolvers if tag != _BOOL_TAG]
    for first, resolvers in yaml.SafeLoader.yaml_implicit_resolvers.items()
}
StrictLoader.add_implicit_resolver(_BOOL_TAG, re.compile(r'^(?:true|True|TRUE|false|False|FALSE)$'), list('tTfF'))


# `color: #F2F5F9` is a comment to YAML, so PyYAML would read it as an empty value and the
# colour would vanish without an error. Rewrite it to the 0xF2F5F9 form YAML reads as a number.
_HASH_COLOR = re.compile(r'^(\s*(?:-\s+)?[^#\s][^:#]*:\s+)#([0-9A-Fa-f]{6})(\s+#.*|\s*)$')


def load_yaml(path: Path):
    lines = []
    for line in path.read_text(encoding='utf-8').splitlines():
        m = _HASH_COLOR.match(line)
        lines.append(f'{m.group(1)}0x{m.group(2)}{m.group(3)}' if m else line)
    try:
        data = yaml.load('\n'.join(lines), Loader=StrictLoader)
    except yaml.YAMLError as e:
        raise BuildError(f'{path.name}: {e}')
    if data is None:
        data = {}
    if not isinstance(data, dict):
        raise BuildError(f'{path.name}: the top level must be a mapping of keys, not a {type(data).__name__}')
    return data


def clean(node, where: str, warnings: list):
    """Strings-only keys, and no nulls: `key:` with nothing after it is an empty value the
    firmware would ignore, which is never what was meant."""
    if isinstance(node, dict):
        out = {}
        for k, v in node.items():
            if not isinstance(k, str):
                raise BuildError(f'{where}: key {k!r} is a {type(k).__name__}, keys must be text (quote it)')
            if v is None:
                warnings.append(f'{where}.{k}: has no value, left out')
                continue
            out[k] = clean(v, f'{where}.{k}', warnings)
        return out
    if isinstance(node, list):
        return [clean(v, f'{where}[{i}]', warnings) for i, v in enumerate(node)]
    if isinstance(node, (str, bool, int, float)):
        return node
    raise BuildError(f'{where}: {node!r} is a {type(node).__name__}, which has no JSON form (quote it if it is text)')


def unknown_keys(node, known: set, where: str, out: list):
    if isinstance(node, dict):
        for k, v in node.items():
            if k not in known:
                out.append(f'{where}.{k}')
            unknown_keys(v, known, f'{where}.{k}', out)
    elif isinstance(node, list):
        for i, v in enumerate(node):
            unknown_keys(v, known, f'{where}[{i}]', out)


# ---- assets ----------------------------------------------------------------------------

def fnv1a(data: bytes, h: int = 2166136261) -> int:
    for b in data:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def gather_assets(theme_dir: Path, facts: dict, warnings: list) -> dict:
    """{canonical name: source path} for every image and font the device will use."""
    found = {}
    for path in sorted(theme_dir.iterdir()):
        if not path.is_file() or path.suffix.lower() not in ('.png', '.bin'):
            continue
        name = path.name
        canon = name if (name in facts['images'] or name in facts['fonts']) else facts['aliases'].get(name)
        if canon is None:
            warnings.append(f'{name}: not an asset the firmware loads, not copied')
            continue
        if canon in found:
            # The canonical name was seen first only if it sorts first; decide explicitly.
            if name == canon:
                warnings.append(f'{found[canon].name} and {name} are both {canon}; using {name}')
                found[canon] = path
            else:
                warnings.append(f'{name} and {found[canon].name} are both {canon}; using {found[canon].name}')
            continue
        if canon != name:
            warnings.append(f'{name}: old name, copied as {canon}')
        found[canon] = path
    return found


def assets_hash(assets: dict) -> int:
    h = 2166136261
    for name in sorted(assets):
        h = fnv1a(name.encode() + b'\n', h)
        h = fnv1a(assets[name].read_bytes(), h)
    return h or 1        # zero is the firmware's "no fingerprint"


# ---- build -----------------------------------------------------------------------------

def build(theme_dir: Path, out_root: Path, warnings: list) -> Path:
    facts = firmware_facts()
    yaml_path = theme_dir / 'theme.yaml'
    if not yaml_path.exists():
        raise BuildError(f'no theme.yaml in {theme_dir}')

    data = clean(load_yaml(yaml_path), 'theme.yaml', warnings)

    slug = str(data.get('slug') or theme_dir.name)
    if not re.fullmatch(r'[a-z0-9][a-z0-9_-]*', slug) or len(slug) > MAX_SLUG_LEN:
        raise BuildError(f'slug {slug!r} must be lowercase letters, digits, - or _, at most {MAX_SLUG_LEN} characters')

    for key in DERIVED_KEYS:
        if key in data:
            warnings.append(f'{key}: ignored, it is worked out from the files in the folder')
            del data[key]
    for key in data:
        if key not in SECTIONS and key not in MANIFEST_KEYS:
            raise BuildError(f'top-level key {key!r} is not a screen section ({", ".join(SECTIONS)}) '
                             f'or a theme setting ({", ".join(MANIFEST_KEYS)})')
    for section in SECTIONS:
        if section in data and not isinstance(data[section], dict):
            raise BuildError(f'{section}: must be a mapping of options')

    typos = []
    for key in SECTIONS:
        if key in data:
            unknown_keys(data[key], facts['keys'], key, typos)
    for path in typos:
        warnings.append(f'{path}: theme_style.cpp never reads this key, so it does nothing (typo?)')

    assets = gather_assets(theme_dir, facts, warnings)
    theme = {k: data[k] for k in MANIFEST_KEYS if k in data}
    theme['slug'] = slug
    theme.setdefault('name', slug)
    theme['assets'] = sorted(assets)
    theme['assetsHash'] = assets_hash(assets)

    files = {'theme.json': theme}
    files.update({f'{s}_style.json': data[s] for s in SECTIONS if data.get(s)})
    payloads = {}
    for name, obj in files.items():
        blob = json.dumps(obj, separators=(',', ':'), ensure_ascii=False).encode('utf-8')
        if len(blob) > facts['max_json']:
            raise BuildError(f'{name} would be {len(blob)} bytes; the firmware will not read a style file '
                             f'over {facts["max_json"]} and would silently ignore it')
        payloads[name] = blob

    target = (out_root / slug).resolve()
    src = theme_dir.resolve()
    if target == src or target in src.parents or src in target.parents:
        raise BuildError(f'--out would put the build at {target}, which overlaps the theme source {src}; '
                         f'the build replaces its target, so pick a folder outside the source')

    # Built beside the target and swapped in, so a failure never leaves a half-written theme
    # where a good one used to be.
    stage = out_root.resolve() / f'.{slug}.building'
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)
    for name, path in assets.items():
        shutil.copyfile(path, stage / name)
    for name, blob in payloads.items():
        (stage / name).write_bytes(blob)
    # _installed LAST, always: the Orb only counts a folder that has it (see theme_select.cpp).
    (stage / '_installed').write_bytes(b'')
    if target.exists():
        shutil.rmtree(target)
    stage.rename(target)

    print(f'built {target}')
    print(f'  {len([a for a in assets if a.endswith(".png")])} image(s), '
          f'{len([a for a in assets if a.endswith(".bin")])} font(s), '
          f'{len(payloads)} JSON file(s), assetsHash 0x{theme["assetsHash"]:08x}')
    return target


def main(argv) -> int:
    ap = argparse.ArgumentParser(description='Build a theme folder ready to copy to the Orb SD card.')
    ap.add_argument('theme_dir', type=Path, help='folder holding theme.yaml and the theme images/fonts')
    ap.add_argument('--out', type=Path, default=REPO / 'build' / 'themes',
                    help='folder to build into; the theme lands in <out>/<slug>/ (default: build/themes)')
    args = ap.parse_args(argv[1:])
    if not args.theme_dir.is_dir():
        print(f'error: {args.theme_dir} is not a folder', file=sys.stderr)
        return 2
    args.out.mkdir(parents=True, exist_ok=True)
    warnings: list = []
    try:
        build(args.theme_dir, args.out, warnings)
    except BuildError as e:
        for w in warnings:
            print(f'warning: {w}', file=sys.stderr)
        print(f'error: {e}', file=sys.stderr)
        return 1
    for w in warnings:
        print(f'warning: {w}', file=sys.stderr)
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
