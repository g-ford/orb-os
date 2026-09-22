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

    def test_the_built_in_palette_is_the_sky_blue_set_every_screen_now_shows(self):
        # These nine values were app_theme.cpp's APP_THEME_DEFAULT before spec step 4 retired that compiled array;
        # the built-in look then went from night-vision green to sky blue on navy (too close to the Fallout theme's
        # own green). They are written out here so the constants in theme_roles.h cannot drift from what the
        # screens actually show.
        b = built_in_from_header()
        want = {'bg': 0x0D1220, 'panel': 0x141A2B, 'highlight': 0x26314A, 'text': 0xEDF1FA, 'secondary': 0xF0B4C8,
                'dim': 0x5F7BA6, 'primary': 0x8CB8FF, 'hairline': 0x1F2A42, 'onPrimary': 0x0A1020}
        self.assertEqual({k: b[k] for k in want}, want)


class DefaultFileAsATemplateTest(DumperCase):
    """default/theme.yaml states all eleven roles so that it equals the built-in constants. A theme copied from it that
    changes only the four colours at the top would keep the other seven at the built-in green, so the file says where to
    cut, and these tests pin both halves of that."""

    CUT = '\n  # ---- delete from here'      # the divider inside the palette block, not the sentence about it in the header

    def four_colours_only(self, slug='paldemo'):
        text = DEFAULT_YAML.read_text(encoding='utf-8').replace('slug: default', f'slug: {slug}', 1)
        self.assertIn(self.CUT, text, 'the template must mark where its seven derived roles start')
        return text[:text.index(self.CUT)] + '\n'

    def test_the_four_colours_alone_are_a_valid_theme_that_derives_the_other_seven(self):
        derived = self.dump(self.four_colours_only())
        built_in = self.dump()
        self.assertEqual(derived['palette']['mode'], 'theme')
        for role in ('bg', 'primary', 'secondary', 'text'):
            self.assertEqual(derived['palette'][role], built_in['palette'][role])
        # derived, not the built-in's hand-tuned constants: this is what "delete the last seven lines" buys a theme author
        self.assertNotEqual(derived['palette']['dim'], built_in['palette']['dim'])
        self.assertNotEqual(derived['palette']['panel'], built_in['palette']['panel'])

    def test_a_theme_built_from_the_whole_default_file_resolves_like_the_built_in_look(self):
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
