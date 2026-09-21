"""The drawn face is only drawn where there is no image, and only in palette mode. That is a property of how
compose_custom is wired, and the drawing needs LVGL, so it is pinned here on the source."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'src' / 'app' / 'clock' / 'clock_view.cpp'


def code() -> str:
    return re.sub(r'//[^\n]*', '', SRC.read_text(encoding='utf-8'))


def compose_custom(text: str) -> str:
    start = text.index('static void compose_custom(')
    return text[start:text.index('static void draw_custom(', start)]


class DrawnFaceWiringTest(unittest.TestCase):
    def test_the_dial_is_drawn_only_when_there_is_no_plate_and_the_theme_has_a_palette(self):
        body = compose_custom(code())
        self.assertRegex(body, r'else\s*\{\s*lv_canvas_fill_bg\([^;]*\);\s*if \(theme_style::paletteOn\(\)\) draw_palette_dial\(ti\);')

    def test_a_hand_is_drawn_only_when_it_has_no_sprite(self):
        body = compose_custom(code())
        self.assertRegex(body, r'if \(spr\.data\) blend_custom_hand\([^;]*\);\s*else if \(k < 3 && theme_style::paletteOn\(\)\) \{')

    def test_the_drawn_pieces_are_defined(self):
        text = code()
        for name in ('draw_palette_dial', 'draw_palette_hand', 'draw_palette_hub'):
            self.assertIn(f'static void {name}(', text)

    def test_a_drawn_second_hand_never_sweeps(self):
        text = code()
        start = text.index('static bool sweep_possible()')
        body = text[start:text.index('static lv_area_t second_box(', start)]
        self.assertIn('custom_hand(2).data', body)


class BuiltInLookHasNoCompiledArtTest(unittest.TestCase):
    """With no theme selected the clock is drawn, not a compiled photograph. Every clock image (plate, overlay, hands)
    comes through decode_sd_first, so that is where the compiled flash fallback is refused: one guard on the shared
    path rather than a check at each caller."""

    def test_decode_sd_first_drops_the_flash_fallback_in_the_built_in_mode(self):
        text = re.sub(r'//[^\n]*', '', (ROOT / 'src' / 'app' / 'common' / 'custom_sprite.cpp').read_text(encoding='utf-8'))
        start = text.index('bool decode_sd_first(')
        body = text[start:text.index('uint16_t *s_plate', start)]
        self.assertRegex(body, r'if \(theme_style::paletteMode\(\) == theme_style::PaletteMode::BuiltIn\) flashPng = nullptr;')


if __name__ == '__main__':
    unittest.main()
