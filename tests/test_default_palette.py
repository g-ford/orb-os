import re
import sys
import tempfile
import unittest
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests'))
import build_theme  # noqa: E402
import gen_elegant_theme as gen  # noqa: E402
from test_theme_palette import DumperCase, KEEP  # noqa: E402

DEFAULT_YAML = ROOT / 'src' / 'theme_assets' / 'default' / 'theme.yaml'


def built_in_from_header() -> dict:
    """The eleven values of theme_roles::BUILT_IN, in role order, read out of the header so this cannot drift."""
    text = (ROOT / 'src' / 'theme' / 'core' / 'theme_roles.h').read_text(encoding='utf-8')
    block = re.search(r'inline constexpr Palette BUILT_IN = \{\{(.*?)\}\};', text, re.S).group(1)
    values = [int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]{6}),', block)]
    return dict(zip(build_theme.firmware_facts()['roles'], values))


class DefaultFolderTest(unittest.TestCase):
    def test_the_default_folder_lists_exactly_the_built_in_palette(self):
        palette = yaml.safe_load(DEFAULT_YAML.read_text(encoding='utf-8'))['palette']
        self.assertEqual(palette, built_in_from_header())

    def test_the_folder_says_it_is_the_built_in(self):
        data = yaml.safe_load(DEFAULT_YAML.read_text(encoding='utf-8'))
        self.assertEqual((data['slug'], data['name'], data['default']), ('default', 'Default', True))

    def test_the_built_in_palette_equals_todays_default_app_palette(self):
        # legacy themes keep the colours every screen has always had: APP_THEME_DEFAULT in app_theme.cpp
        text = (ROOT / 'src' / 'app' / 'common' / 'app_theme.cpp').read_text(encoding='utf-8')
        block = text[text.index('APP_THEME_DEFAULT'):text.index('APP_THEME_OFFICE')]
        got = dict(re.findall(r'/\*(\w+)\*/\s+lv_color_hex\(0x([0-9A-Fa-f]{6})\)', block))
        b = built_in_from_header()
        want = {'bg': b['bg'], 'panel': b['panel'], 'highlight': b['highlight'], 'ink': b['text'], 'soft': b['secondary'],
                'dim': b['dim'], 'accent': b['primary'], 'hairline': b['hairline'], 'onAccent': b['onPrimary']}
        self.assertEqual({k: int(v, 16) for k, v in got.items()}, want)


class FourColoursEqualTheBuiltInTest(DumperCase):
    def test_a_theme_built_from_the_default_file_resolves_like_the_built_in_look(self):
        # the whole path (builder, theme.json, palette mode, role defaults) against the compiled built-in mode
        text = DEFAULT_YAML.read_text(encoding='utf-8').replace('slug: default', 'slug: paldemo', 1)
        from_file = self.dump(text)
        built_in = self.dump()
        self.assertEqual(from_file['palette']['mode'], 'theme')
        self.assertEqual(built_in['palette']['mode'], 'builtin')
        self.assertEqual({k: from_file['palette'][k] for k in built_in['palette'] if k not in ('mode',)},
                         {k: v for k, v in built_in['palette'].items() if k != 'mode'})
        self.assertEqual({k: from_file[k] for k in KEEP}, {k: built_in[k] for k in KEEP})


if __name__ == '__main__':
    unittest.main()
