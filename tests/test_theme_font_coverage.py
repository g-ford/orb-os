import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests'))
import font_golden  # noqa: E402
from font_facts import converter_available  # noqa: E402

THEMES = ROOT / 'src' / 'theme_assets'
# The only three text slots whose compiled fallback is a bitmap face baked into the firmware
# (custom_menu_font1, custom_radar_font1, custom_radar_font2). Radar text 3, the menu's prev/next
# hints and Settings fall back to LVGL's own Montserrat, which stays. A theme with no face for one of
# these three would change lettering the day the compiled fonts are deleted.
COMPILED_BITMAP_SLOTS = ('menu_current', 'radar1', 'radar2')


def missing_compiled_slots(resolved: dict) -> list:
    return [s for s in COMPILED_BITMAP_SLOTS if s not in resolved]


class FontCoverageTest(unittest.TestCase):
    def test_the_check_can_fail(self):
        self.assertEqual(missing_compiled_slots({}), list(COMPILED_BITMAP_SLOTS))
        self.assertEqual(missing_compiled_slots({'menu_current': 'x', 'radar1': 'x'}), ['radar2'])
        self.assertEqual(missing_compiled_slots({s: 'x' for s in COMPILED_BITMAP_SLOTS}), [])

    def test_every_shipped_theme_supplies_a_face_for_the_three_compiled_bitmap_slots(self):
        themes = sorted(p for p in THEMES.iterdir() if (p / 'theme.yaml').exists())
        self.assertGreaterEqual(len(themes), 3, 'expected at least elegant, fallout and portal')
        for theme in themes:
            with self.subTest(theme=theme.name), tempfile.TemporaryDirectory() as tmp:
                r = subprocess.run([sys.executable, str(ROOT / 'tools' / 'build_theme.py'), str(theme), '--out', tmp],
                                   capture_output=True, text=True)
                if r.returncode != 0 and 'lv_font_conv' in r.stderr and not converter_available():
                    self.skipTest("lv_font_conv (or npx) is needed to bake this theme's faces")
                self.assertEqual(r.returncode, 0, msg=r.stderr)
                built = next(p for p in Path(tmp).iterdir() if not p.name.startswith('.'))
                missing = missing_compiled_slots(font_golden.resolve(built))
                self.assertFalse(missing, f'{theme.name} draws {missing} with a compiled face; give it a face in fonts:')


if __name__ == '__main__':
    unittest.main()
