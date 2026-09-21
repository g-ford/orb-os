#!/usr/bin/env python3
"""Build a theme folder that is ready to copy onto the Orb's SD card.

    python3 tools/build_theme.py src/theme_assets/elegant [--out build/themes]

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
import struct
import sys
import tempfile
from pathlib import Path

try:
    import yaml
except ImportError:
    sys.exit('build_theme.py needs PyYAML:  pip3 install pyyaml')

REPO = Path(__file__).resolve().parents[1]
CORE = REPO / 'src' / 'theme' / 'core'

sys.path.insert(0, str(Path(__file__).resolve().parent))
import font_bake  # noqa: E402  (tools/font_bake.py)

SECTIONS = ('clock', 'radar', 'weather', 'ticker', 'settings', 'menu', 'splash', 'intel')
MANIFEST_KEYS = ('slug', 'name', 'author', 'version', 'default', 'apps', 'names', 'roleDefaults')
DERIVED_KEYS = ('assets', 'assetsHash')      # written by this script, never by hand
MAX_SLUG_LEN = 31                            # theme_select.h MAX_SLUG_LEN 32, less the NUL
FONTS_KEY = 'fonts'
FACE_NAME = re.compile(r'[a-z][a-z0-9_]*')
MAX_FACE_NAME = 14                # 'font_' + name + '.bin' must fit theme_art's 23-character asset name
MAX_FONTS_BEFORE_WARNING = 10     # each distinct font is a face in flash and a copy in PSRAM
MIN_FONT_PX, MAX_FONT_PX = 6, 128
FACE_KEYS = {'src', 'size', 'ranges'}
PALETTE_KEY = 'palette'
BASE_ROLES = ('bg', 'primary', 'secondary', 'text')     # a theme picks these; the rest derive (theme_roles.h)
ROLE_REF = re.compile(r'^\$([A-Za-z_][A-Za-z0-9_]*)$')  # "$primary": a reference the firmware resolves


class BuildError(Exception):
    pass


# ---- facts read from the firmware, so this script has no second copy to drift ----------

# Names the theme art used before the flat, per-screen names, mapped to the name the firmware
# opens. The device knows nothing of these: a built folder only ever holds the new names.
LEGACY_NAMES = {
    'custom_hour_png.png':     'clock_hand_hour.png',
    'custom_minute_png.png':   'clock_hand_minute.png',
    'custom_second_png.png':   'clock_hand_second.png',
    'hand_hour_img_map.png':   'clock_hand_hour.png',
    'hand_min_img_map.png':    'clock_hand_minute.png',
    'dial_img.png':            'clock_plate.png',
    'dial_avi.png':            'clock_plate.png',
    'splash_png_default.png':  'splash.png',
}

# The image names the firmware opens, read out of the source rather than listed here, so a
# new layer added to a screen is picked up without touching this script. Matched by the screen
# prefix every theme asset carries (clock_, radar_, wind_, ...) so unrelated PNG literals in
# src/ are ignored.
_IMAGE_NAME = re.compile(r'"((?:clock|radar|weather|ticker|intel|menu|settings|splash|wind)_[a-z0-9_]+|splash)\.png"')


def firmware_facts() -> dict:
    """Asset names, font slots and limits, taken from the sources that use them."""
    def read(path: Path) -> str:
        if not path.exists():
            raise BuildError(f'cannot find {path.relative_to(REPO)}; run this from a full checkout')
        return path.read_text(encoding='utf-8')

    font, style = read(CORE / 'theme_font.cpp'), read(CORE / 'theme_style.cpp')
    roles_h = read(CORE / 'theme_roles.h')

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
    slots = {n[len('font_'):-len('.bin')] for n in fonts}
    block = re.search(r'#define THEME_ROLE_LIST\(X\)(.*?)\n\n', roles_h, re.S)
    roles = re.findall(r'X\((\w+)\)', block.group(1)) if block else []
    if tuple(roles[:len(BASE_ROLES)]) != BASE_ROLES:
        raise BuildError('could not read THEME_ROLE_LIST out of src/theme/core/theme_roles.h; '
                         'the firmware source has changed shape and this script needs updating')
    return {'images': images, 'fonts': fonts, 'slots': slots, 'roles': roles, 'aliases': aliases,
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



SCREEN = 466                                 # the round AMOLED is 466x466


def name_needs_screen_size(name: str) -> bool:
    """Full-screen art: every plate, the splash, and the wind screen's background."""
    return name.endswith('_plate.png') or name in ('splash.png', 'wind_bg.png')


def check_png(path: Path, name: str):
    """The firmware's PNG decoders (custom_sprite.cpp, plate_sprite.cpp, ...) accept only 8-bit
    RGBA and fill any other pixel type with zeros, so an RGB, palette or greyscale PNG draws as a
    black screen while the log still says it decoded. Nothing else would tell you."""
    head = path.read_bytes()[:26]
    if len(head) < 26 or head[:8] != b'\x89PNG\r\n\x1a\n':
        raise BuildError(f'{name} is not a PNG file')
    width, height = struct.unpack('>II', head[16:24])
    if name_needs_screen_size(name) and (width, height) != (SCREEN, SCREEN):
        raise BuildError(f'{name} is {width}x{height}; a plate is drawn at its own pixel size on the '
                         f'{SCREEN}x{SCREEN} screen, so it must be exactly that. '
                         f'Fit it with: python3 tools/fit_theme_art.py <master> {name}')
    depth, color_type = head[24], head[25]
    if (depth, color_type) != (8, 6):
        kind = {0: 'greyscale', 2: 'RGB', 3: 'palette', 4: 'greyscale+alpha', 6: 'RGBA'}.get(color_type, f'type {color_type}')
        raise BuildError(f'{name} is {depth}-bit {kind}; the firmware only draws 8-bit RGBA PNGs and shows '
                         f'anything else as black. Re-save it with an alpha channel.')

def assets_hash(assets: dict, font_map: dict | None = None) -> int:
    h = 2166136261
    for name in sorted(assets):
        h = fnv1a(name.encode() + b'\n', h)
        h = fnv1a(assets[name].read_bytes(), h)
    # The slot-to-face map too: swapping which slot uses which of two shipped faces changes no file, yet the
    # flash copy of the map (fonts.map) is now stale, and only a changed hash makes the device re-bake.
    # Left out when there is no map, so a theme without a fonts block keeps the hash it always had.
    for slot, file in sorted((font_map or {}).items()):
        h = fnv1a(f'{slot} {file}\n'.encode(), h)
    return h or 1        # zero is the firmware's "no fingerprint"


def build_fonts(theme_dir: Path, block, facts: dict, bake_dir: Path, warnings: list):
    """Turn the `fonts:` block into face files in bake_dir and a {slot: file} map.

    Returns ({slot: 'font_<face>.bin'}, {'font_<face>.bin': Path}). A face is baked once however
    many slots use it, and a face no slot uses is not baked at all."""
    if not isinstance(block, dict) or set(block) - {'faces', 'slots'}:
        raise BuildError('fonts: holds `faces` and `slots` and nothing else')
    faces, slots = block.get('faces') or {}, block.get('slots') or {}
    if not isinstance(faces, dict) or not isinstance(slots, dict):
        raise BuildError('fonts.faces and fonts.slots must each be a mapping')

    root = theme_dir.resolve()
    for name, spec in faces.items():
        where = f'fonts.faces.{name}'
        if not FACE_NAME.fullmatch(name) or len(name) > MAX_FACE_NAME:
            raise BuildError(f'{where}: a face name is lowercase letters, digits and _, starting with a letter, at most '
                             f'{MAX_FACE_NAME} characters (its file, font_{name}.bin, must fit the flash '
                             f"index's 23-character asset names)")
        if name in facts['slots']:
            raise BuildError(f'{where}: {name!r} is also a slot name, so an unmapped {name} slot would load this '
                             f'face by accident. Pick another name')
        if not isinstance(spec, dict) or 'src' not in spec or set(spec) - FACE_KEYS:
            raise BuildError(f'{where}: needs `src`, and may also have `size` and `ranges` (got {spec!r})')
        src = (root / str(spec['src'])).resolve()
        if root not in src.parents:
            raise BuildError(f'{where}.src: {spec["src"]!r} is outside the theme folder')
        if not src.is_file():
            raise BuildError(f'{where}.src: {spec["src"]!r} does not exist')
        suffix = src.suffix.lower()
        if suffix == '.bin':
            if 'size' in spec or 'ranges' in spec:
                raise BuildError(f'{where}: a .bin is already baked at one size, so it takes no size or ranges')
        elif suffix in ('.ttf', '.otf'):
            size = spec.get('size')
            if not isinstance(size, int) or isinstance(size, bool) or not MIN_FONT_PX <= size <= MAX_FONT_PX:
                raise BuildError(f'{where}.size: a pixel size from {MIN_FONT_PX} to {MAX_FONT_PX} is needed')
            if 'ranges' in spec and not isinstance(spec['ranges'], str):
                raise BuildError(f'{where}.ranges: must be text, in lv_font_conv --range form')
        else:
            raise BuildError(f'{where}.src: must be a .ttf, .otf or an already-baked .bin, '
                             f'not {suffix or "a file with no extension"}')

    used = []
    for slot, face in slots.items():
        if slot not in facts['slots']:
            raise BuildError(f'fonts.slots.{slot}: not a slot theme_font.cpp loads '
                             f'(known: {", ".join(sorted(facts["slots"]))})')
        if not isinstance(face, str) or face not in faces:
            raise BuildError(f'fonts.slots.{slot}: {face!r} is not a face defined in fonts.faces')
        if face not in used:
            used.append(face)
    for name in faces:
        if name not in used:
            warnings.append(f'fonts.faces.{name}: no slot uses it, so it is not baked')

    files = {}
    for name in sorted(used):
        spec = faces[name]
        src = (root / str(spec['src'])).resolve()
        out = bake_dir / f'font_{name}.bin'
        if src.suffix.lower() == '.bin':
            shutil.copyfile(src, out)
        else:
            try:
                font_bake.bake_face(src, spec['size'], out, spec.get('ranges', font_bake.DEFAULT_RANGES))
            except font_bake.FontBakeError as e:
                raise BuildError(f'fonts.faces.{name}: {e}')
        files[out.name] = out
    return {slot: f'font_{face}.bin' for slot, face in sorted(slots.items())}, files


def build_palette(block, roles: list) -> dict:
    """Validate the `palette:` block and return {role: int}. A theme picks the four base roles and may state any
    other; nothing else is a role. Values are colours, never references: a palette that pointed at itself would
    have no answer."""
    if not isinstance(block, dict):
        raise BuildError('palette: must be a mapping of role names to colours')
    out = {}
    for name, value in block.items():
        if name not in roles:
            raise BuildError(f'palette.{name}: not a role (roles: {", ".join(roles)})')
        if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= 0xFFFFFF:
            raise BuildError(f'palette.{name}: must be a colour written 0xRRGGBB or #RRGGBB, not {value!r} '
                             f'(a palette states colours; only the sections use $role)')
        out[name] = value
    missing = [r for r in BASE_ROLES if r not in out]
    if missing:
        raise BuildError(f'palette: needs {", ".join(missing)} (a theme picks bg, primary, secondary and text; '
                         f'the other roles are derived)')
    return out


def check_refs(node, where: str, roles: list, has_palette: bool):
    """Check every role reference in a section and return what to write. `$primary` is passed through as it is, for
    the firmware to resolve; `$$x` is the text `$x`. Anything else that starts with `$` (a price, say) is plain text."""
    if isinstance(node, dict):
        return {k: check_refs(v, f'{where}.{k}', roles, has_palette) for k, v in node.items()}
    if isinstance(node, list):
        return [check_refs(v, f'{where}[{i}]', roles, has_palette) for i, v in enumerate(node)]
    if isinstance(node, str) and node.startswith('$'):
        if node.startswith('$$'):
            text = node[1:]
            m = ROLE_REF.match(text)
            if m and m.group(1) in roles:
                raise BuildError(f'{where}: {node!r} would be read as the role reference {text}; change the text')
            return text
        m = ROLE_REF.match(node)
        if m:
            if m.group(1) not in roles:
                raise BuildError(f'{where}: {node} is not a role (roles: {", ".join(roles)})')
            if not has_palette:
                raise BuildError(f'{where}: {node} needs a palette: block')
    return node


# ---- build -----------------------------------------------------------------------------

def build(theme_dir: Path, out_root: Path, warnings: list) -> Path:
    with tempfile.TemporaryDirectory(prefix='orb-theme-faces-') as bake_dir:
        return _build(theme_dir, out_root, warnings, Path(bake_dir))


def _build(theme_dir: Path, out_root: Path, warnings: list, bake_dir: Path) -> Path:
    facts = firmware_facts()
    yaml_path = theme_dir / 'theme.yaml'
    if not yaml_path.exists():
        raise BuildError(f'no theme.yaml in {theme_dir}')

    data = clean(load_yaml(yaml_path), 'theme.yaml', warnings)
    font_block = data.pop(FONTS_KEY, None)
    palette_block = data.pop(PALETTE_KEY, None)

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
    roles = facts['roles']
    palette = build_palette(palette_block, roles) if palette_block is not None else None
    for section in SECTIONS:
        if section in data:
            data[section] = check_refs(data[section], section, roles, palette is not None)
    if 'roleDefaults' in data:
        if not isinstance(data['roleDefaults'], bool):
            raise BuildError('roleDefaults: must be true or false')
        if palette is None:
            warnings.append('roleDefaults: has no effect without a palette:')

    typos = []
    for key in SECTIONS:
        if key in data:
            unknown_keys(data[key], facts['keys'], key, typos)
    for path in typos:
        warnings.append(f'{path}: theme_style.cpp never reads this key, so it does nothing (typo?)')

    assets = gather_assets(theme_dir, facts, warnings)
    for asset_name, source in assets.items():
        if asset_name.endswith('.png'):
            check_png(source, asset_name)     # by its canonical name: that decides what it is
    font_map = {}
    if font_block is not None:
        font_map, face_files = build_fonts(theme_dir, font_block, facts, bake_dir, warnings)
        for slot in font_map:                     # a mapped slot no longer loads its own file
            legacy = f'font_{slot}.bin'
            if legacy in assets:
                warnings.append(f'{legacy}: shadowed by fonts.slots.{slot}, so it is not copied')
                del assets[legacy]
        assets.update(face_files)
    fonts_shipped = sorted(a for a in assets if a.endswith('.bin'))
    if font_block is not None and len(fonts_shipped) > MAX_FONTS_BEFORE_WARNING:   # a theme that defines faces
        warnings.append(f'{len(fonts_shipped)} distinct fonts; each is a separate face in flash and in PSRAM. '
                        f'Reuse a size where the layout allows (the budget is {MAX_FONTS_BEFORE_WARNING}).')
    theme = {k: data[k] for k in MANIFEST_KEYS if k in data}
    theme['slug'] = slug
    theme.setdefault('name', slug)
    theme['assets'] = sorted(assets)
    theme['assetsHash'] = assets_hash(assets, font_map)
    if font_map:
        theme['fonts'] = font_map
    if palette:
        theme['palette'] = palette

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
    if fonts_shipped:
        total_kb = sum(assets[a].stat().st_size for a in fonts_shipped) / 1024
        print(f'  fonts: {len(fonts_shipped)} file(s), {total_kb:.0f} KB')
    print(f'  {len([a for a in assets if a.endswith(".png")])} image(s), '
          f'{len(fonts_shipped)} font(s), '
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
