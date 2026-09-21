"""The compiled splash PNG must not be drawn in palette mode: it is the brown card the built-in look replaces."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def code(path: str) -> str:
    return re.sub(r'//[^\n]*', '', (ROOT / path).read_text(encoding='utf-8'))


class SplashWiringTest(unittest.TestCase):
    def test_the_compiled_fallback_is_skipped_in_palette_mode_unless_office(self):
        text = code('src/theme/graphics/splash_art.cpp')
        self.assertRegex(text, r'if \(!ok && !\(theme_style::paletteOn\(\) && !office\)\) \{')

    def test_the_splash_background_is_the_palette_background(self):
        text = code('src/app/ui/ui.cpp')
        self.assertIn('(office || theme_style::paletteOn()) ? app_theme::palette().bg : lv_color_black()', text)


if __name__ == '__main__':
    unittest.main()
