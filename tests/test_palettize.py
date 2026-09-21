import sys
import unittest
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import palettize  # noqa: E402


class CountColoursTest(unittest.TestCase):
    def test_it_counts_both_spellings_and_ignores_comments(self):
        text = 'a: 0xFF9A1F\nb: #ff9a1f\nc: {color: 0x0B0E11, bg: 0xFF9A1F}  # 0x123456 is not counted\n# 0xABCDEF\n'
        self.assertEqual(palettize.count_colours(text), Counter({0xFF9A1F: 3, 0x0B0E11: 1}))

    def test_a_palette_block_is_not_counted(self):
        text = 'palette:\n  bg: 0x000000\n  primary: 0x1DFF86\nradar:\n  sweepColor: 0x1DFF86\n'
        self.assertEqual(palettize.count_colours(text), Counter({0x1DFF86: 1}))


class SuggestTest(unittest.TestCase):
    def test_portals_colours_suggest_portals_palette(self):
        counts = Counter({0xFF9A1F: 16, 0x0B0E11: 8, 0xFFFFFF: 7, 0x82CEFF: 7, 0x1FA2FF: 4, 0x8794A3: 3})
        self.assertEqual(palettize.suggest(counts),
                         {'bg': 0x0B0E11, 'primary': 0xFF9A1F, 'secondary': 0x82CEFF, 'text': 0xFFFFFF})

    def test_it_never_suggests_one_colour_for_two_roles(self):
        counts = Counter({0x000000: 5, 0xFFFFFF: 4})
        got = palettize.suggest(counts)
        self.assertEqual(len(set(got.values())), len(got))

    def test_a_theme_with_almost_no_colours_gets_a_partial_suggestion_not_a_crash(self):
        self.assertEqual(palettize.suggest(Counter()), {})
        self.assertEqual(palettize.suggest(Counter({0x808080: 2})), {'primary': 0x808080})     # grey: not a bg, not bright, not colourful


if __name__ == '__main__':
    unittest.main()
