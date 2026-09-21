import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests'))
import palettize  # noqa: E402
from font_facts import converter_available  # noqa: E402

THEMES = ROOT / 'src' / 'theme_assets'
BASE = ('bg', 'primary', 'secondary', 'text')


def build(slug: str):
    tmp = tempfile.TemporaryDirectory()
    r = subprocess.run([sys.executable, str(ROOT / 'tools' / 'build_theme.py'), str(THEMES / slug), '--out', tmp.name],
                       capture_output=True, text=True)
    if r.returncode != 0:
        tmp.cleanup()
        if 'lv_font_conv' in r.stderr and not converter_available():
            raise unittest.SkipTest("lv_font_conv (or npx) is needed to bake this theme's faces")
        raise AssertionError(r.stderr)
    return tmp, Path(tmp.name) / slug


def refs_in(built: Path) -> int:
    """How many "$role" strings the built style files carry."""
    n = 0

    def walk(node):
        nonlocal n
        if isinstance(node, dict):
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)
        elif isinstance(node, str) and re.fullmatch(r'\$[A-Za-z_]\w*', node):
            n += 1

    for f in built.glob('*_style.json'):
        walk(json.loads(f.read_text(encoding='utf-8')))
    return n


class ShippedPalettesTest(unittest.TestCase):
    def check_palette(self, slug):
        tmp, built = build(slug)
        self.addCleanup(tmp.cleanup)
        theme = json.loads((built / 'theme.json').read_text(encoding='utf-8'))
        self.assertTrue(all(r in theme['palette'] for r in BASE), f'{slug} does not state bg, primary, secondary and text')
        self.assertIs(theme['roleDefaults'], False, f'{slug} states only what differs from the compiled values')
        return built, theme

    def test_fallout_states_its_palette_and_uses_it(self):
        built, theme = self.check_palette('fallout')
        self.assertEqual({k: theme['palette'][k] for k in BASE},
                         {'bg': 0x021A0C, 'primary': 0x1BFF80, 'secondary': 0x11B25A, 'text': 0xB6FFD2})
        self.assertGreaterEqual(refs_in(built), 30)

    def test_portal_states_its_palette_and_uses_it(self):
        built, theme = self.check_palette('portal')
        self.assertEqual({k: theme['palette'][k] for k in BASE},
                         {'bg': 0x0B0E11, 'primary': 0xFF9A1F, 'secondary': 0x82CEFF, 'text': 0xFFFFFF})
        self.assertGreaterEqual(refs_in(built), 25)

    def test_elegant_states_a_palette_but_keeps_its_colours_explicit(self):
        built, theme = self.check_palette('elegant')
        self.assertEqual(refs_in(built), 0)          # it is the exhaustive reference, regenerated from resolved values

    def test_no_colour_is_written_twice(self):
        # "a theme states each colour once": a hex value equal to a palette colour should be its $role
        for slug in ('fallout', 'portal'):
            text = (THEMES / slug / 'theme.yaml').read_text(encoding='utf-8')
            palette = set(palettize.palette_colours(text).values())
            outside = set(palettize.count_colours(text))
            self.assertEqual(sorted(hex(c) for c in outside & palette), [],
                             f'{slug}: these colours are in the palette but still written as hex')


if __name__ == '__main__':
    unittest.main()
