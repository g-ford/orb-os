"""The compiled splash PNG must not be drawn in palette mode: it is the brown card the built-in look replaces."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def code(path: str) -> str:
    return re.sub(r'//[^\n]*', '', (ROOT / path).read_text(encoding='utf-8'))


class SplashWiringTest(unittest.TestCase):
    def test_there_is_no_compiled_splash(self):
        text = code('src/theme/graphics/splash_art.cpp')
        for word in ('SPLASH_PNG', 'CUSTOM_SPLASH', 'splash_png_', 'custom_splash.h'):
            self.assertNotIn(word, text)

    def test_the_decoder_takes_no_skin_argument(self):
        self.assertIn('bool splash_art_decode(lv_img_dsc_t *out);', code('src/theme/graphics/splash_art.h'))
        self.assertIn('bool splash_art_decode(lv_img_dsc_t *out) {', code('src/theme/graphics/splash_art.cpp'))

    def test_nothing_is_allocated_when_nothing_will_be_decoded(self):
        """ensure() takes the 466x466 RGB565 decode buffer (about 424 KB of PSRAM) and it is never freed. A theme with no
        splash of its own decodes nothing in any mode, so the function must return before that allocation, and after the
        pre-baked flash check, which needs no buffer either."""
        text = code('src/theme/graphics/splash_art.cpp')
        body = text[text.index('bool splash_art_decode('):]
        early = re.search(r'if \(!\(slug\[0\] && theme_style::hasAsset\("splash\.png"\)\)\) return false;', body)
        self.assertIsNotNone(early, 'no early return for a theme with no splash image of its own')
        self.assertLess(body.index('find_active("splash.png"'), early.start(), 'the pre-baked flash splash must still win')
        self.assertLess(early.start(), body.index('if (!ensure())'), 'the return must come before the allocation')

    def test_the_splash_background_is_the_palette_background(self):
        text = code('src/app/ui/ui.cpp')
        self.assertIn('theme_style::paletteOn() ? app_theme::palette().bg : lv_color_black()', text)


if __name__ == '__main__':
    unittest.main()
