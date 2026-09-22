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

    def test_a_hidden_hand_is_never_drawn(self):
        """show:false is checked before the sprite lookup, so it also stops a drawn hand and the hub that follows one."""
        body = compose_custom(code())
        self.assertRegex(body, r'if \(!hd\.show\) continue;\s*CustomSprite spr = custom_hand\(k\);\s*if \(spr\.data\) blend_custom_hand')
        self.assertRegex(body, r'if \(drewHand\) draw_palette_hub\(\);')          # the hub only ever follows a drawn hand

    def test_the_drawn_pieces_are_defined(self):
        text = code()
        for name in ('draw_palette_dial', 'draw_palette_hand', 'draw_palette_hub'):
            self.assertIn(f'static void {name}(', text)

    def test_a_drawn_second_hand_never_sweeps(self):
        text = code()
        start = text.index('static bool sweep_possible()')
        body = text[start:text.index('static lv_area_t second_box(', start)]
        self.assertIn('custom_hand(2).data', body)


class NoCompiledClockArtTest(unittest.TestCase):
    """Every clock image (plate, overlay, hands) comes through one decoder. It used to refuse its compiled flash fallback
    in the built-in mode; there is no fallback left to refuse."""

    def test_the_decoder_has_no_flash_fallback(self):
        text = re.sub(r'//[^\n]*', '', (ROOT / 'src' / 'app' / 'common' / 'custom_sprite.cpp').read_text(encoding='utf-8'))
        self.assertIn('bool decode_from_sd(const char *assetName, bool alpha,', text)
        self.assertNotIn('flashPng', text)
        self.assertNotRegex(text, r'CUSTOM_\w+_PNG')

    def test_the_hand_header_carries_options_not_pixels(self):
        text = (ROOT / 'src' / 'theme' / 'custom' / 'custom_hands.h').read_text(encoding='utf-8')
        self.assertNotRegex(text, r'_PNG(_LEN)?\b')
        # The compiled defaults for each hand's `show` are options and stay (theme_style.cpp reads them).
        self.assertIn('#define CUSTOM_HAS_HOUR 1', text)
        self.assertIn('#define CUSTOM_HAND_ORDER { 2, 0, 1 }', text)


class OneFaceTest(unittest.TestCase):
    """The clock has one face, the theme's. The compiled Aviator, Imperial, Digital and Office faces were unreachable
    once CUSTOM_CLOCK.active became a constant true, and they carried most of the firmware's compiled art."""
    RETIRED = ('FACE_AVIATOR', 'FACE_IMPERIAL', 'FACE_DIGITAL', 'FACE_OFFICE', 'FACE_CUSTOM', 's_face',
               'draw_aviator', 'draw_imperial', 'draw_digital', 'draw_office', 'DIAL_IMG', 'DIAL_AVI',
               'HAND_HOUR_IMG', 'HAND_MIN_IMG', 'office_', 'OFFICE_')

    def test_no_compiled_face_is_left(self):
        text = code()
        for word in self.RETIRED:
            self.assertNotIn(word, text, f'{word} is a compiled face or its art')

    def test_redraw_draws_the_themes_face_and_nothing_else(self):
        text = code()
        body = text[text.index('static void redraw('):]
        body = body[:body.index('\n}\n')]
        self.assertIn('draw_custom(ti);', body)
        self.assertNotIn('switch', body)

    def test_the_sweep_does_not_ask_which_face_is_showing(self):
        text = code()
        start = text.index('static bool sweep_possible()')
        self.assertNotIn('not a custom face', text[start:text.index('static lv_area_t second_box(', start)])


if __name__ == '__main__':
    unittest.main()
