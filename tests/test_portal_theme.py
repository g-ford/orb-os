import hashlib
import json
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import gen_elegant_theme as gen  # noqa: E402
sys.path.insert(0, str(ROOT / 'tests'))
from font_facts import converter_available, font_facts, font_table_bytes  # noqa: E402

THEME = ROOT / 'src' / 'theme_assets' / 'portal'
PLATES = ('clock_plate', 'radar_plate', 'weather_plate', 'menu_plate', 'settings_plate',
          'splash', 'intel_plate', 'ticker_plate')


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()[:24]
    assert data[:8] == b'\x89PNG\r\n\x1a\n', f'{path.name} is not a PNG'
    return struct.unpack('>II', data[16:24])


def build(out: Path) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, str(ROOT / 'tools' / 'build_theme.py'), str(THEME), '--out', str(out)],
                          capture_output=True, text=True)


class PortalThemeTest(unittest.TestCase):
    def test_builds_without_a_single_warning(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = build(Path(tmp))
            self.assertEqual(r.returncode, 0, msg=r.stderr)
            # a warning here is a mistyped option, an image the firmware ignores, or a duplicate
            self.assertEqual(r.stderr, '')

    def test_plates_are_full_screen(self):
        for name in PLATES:
            self.assertEqual(png_size(THEME / f'{name}.png'), (466, 466), name)

    def test_every_hand_pivots_inside_its_image(self):
        import re
        text = (THEME / 'theme.yaml').read_text(encoding='utf-8')
        for hand, name in (('hour', 'clock_hand_hour'), ('minute', 'clock_hand_minute'),
                           ('second', 'clock_hand_second')):
            m = re.search(rf'^\s*{hand}:.*pivotX: (\d+),\s*pivotY: (\d+)', text, re.M)
            self.assertTrue(m, hand)
            w, h = png_size(THEME / f'{name}.png')
            px, py = int(m.group(1)), int(m.group(2))
            self.assertTrue(0 < px < w and 0 < py < h, f'{hand} pivot ({px}, {py}) is outside {w}x{h}')
            self.assertEqual(px, w // 2, f'{hand} pivot should be on the hand\'s centre line')

    def test_the_sentry_droid_turns_about_its_centre(self):
        import re
        text = (THEME / 'theme.yaml').read_text(encoding='utf-8')
        w, h = png_size(THEME / 'radar_blip.png')
        px = int(re.search(r'^\s*blipPivotX: (\d+)', text, re.M).group(1))
        py = int(re.search(r'^\s*blipPivotY: (\d+)', text, re.M).group(1))
        self.assertEqual((px, py), (w // 2, h // 2), 'the firmware rotates the sprite about this point')
        self.assertIn('blipTypeImage: true', text)
        # tint would flatten the droid to one colour and lose the red eye
        self.assertIn('blipImageTint: false', text)

    def test_the_art_script_reproduces_the_committed_images(self):
        # the images are drawn by tools/portal_art.py; a hand-edited PNG would be lost on the next run
        try:
            import numpy  # noqa: F401
            from PIL import Image, ImageChops
        except ImportError:
            self.skipTest('Pillow and numpy are needed to redraw the art')
        with tempfile.TemporaryDirectory() as tmp:
            sys.path.insert(0, str(ROOT / 'tools'))
            import portal_art
            original = portal_art.OUT
            portal_art.OUT = Path(tmp)
            try:
                portal_art.main([])
            finally:
                portal_art.OUT = original
            for png in sorted(Path(tmp).glob('*.png')):
                # pixels, not bytes: the PNG encoder differs between Pillow/zlib versions
                a, b = Image.open(png).convert('RGBA'), Image.open(THEME / png.name).convert('RGBA')
                self.assertEqual(a.size, b.size, png.name)
                self.assertIsNone(ImageChops.difference(a, b).getbbox(), f'{png.name} differs from the script')


BARLOW_SHA256 = '95aa02c7c43096e0dd44d787ba6216864a67157e402adab59b35572e0c1577ea'   # Barlow-Regular.ttf, google/fonts ofl/barlow
# Every slot Fallout themes, plus the menu's prev/next hints: one typeface for all the text Portal draws
PORTAL_SLOTS = {'menu_current', 'menu_prev', 'menu_next', 'settings', 'radar1', 'radar2', 'radar3', 'radar4',
                'weather1', 'weather2', 'weather3', 'weather4', 'intel_title', 'intel_text', 'intel_source',
                'intel_age', 'intel_brief', 'ticker_name', 'ticker_price', 'ticker_change', 'ticker_strip',
                'wind_title', 'wind_ask', 'wind_turns'}
PORTAL_PX = {14, 20, 28, 40, 46}
GLYPHS = set(range(0x20, 0x7F)) | {0xB0, 0xB1, 0xB7, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026}


class PortalFontsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        r = build(Path(cls._tmp.name))
        if r.returncode != 0:
            cls._tmp.cleanup()
            if 'lv_font_conv' in r.stderr and not converter_available():
                raise unittest.SkipTest("lv_font_conv (or npx) is needed to bake Portal's faces")
            raise AssertionError(r.stderr)
        cls.stderr = r.stderr
        cls.built = Path(cls._tmp.name) / 'portal'
        cls.map = json.loads((cls.built / 'theme.json').read_text())['fonts']

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_barlow_ships_with_its_licence(self):
        ttf = THEME / 'source' / 'Barlow-Regular.ttf'
        self.assertEqual(hashlib.sha256(ttf.read_bytes()).hexdigest(), BARLOW_SHA256)
        licence = (ttf.parent / 'OFL.txt').read_text(encoding='utf-8')
        self.assertIn('SIL OPEN FONT LICENSE', licence.upper())
        self.assertIn('Barlow Project Authors', licence)

    def test_every_slot_portal_draws_has_a_face(self):
        self.assertEqual(set(self.map), PORTAL_SLOTS)

    def test_sizes_are_kept_to_five(self):
        files = set(self.map.values())
        self.assertLessEqual(len(files), 5)
        self.assertEqual({font_facts(self.built / f)['size'] for f in files}, PORTAL_PX)
        self.assertEqual(sorted(p.name for p in self.built.glob('font_*.bin')), sorted(files))

    def test_all_four_radar_text_slots_share_one_face(self):
        self.assertEqual(len({self.map[f'radar{i}'] for i in range(1, 5)}), 1)

    def test_every_face_is_the_format_the_firmware_reads_and_has_the_glyphs_the_screens_draw(self):
        for name in set(self.map.values()):
            f = font_facts(self.built / name)
            self.assertEqual((f['tag'], f['bpp'], f['compression']), (b'head', 4, 0), name)
            self.assertEqual(font_table_bytes(self.built / name), (self.built / name).stat().st_size, name)
            missing = GLYPHS - f['codepoints']
            self.assertFalse(missing, f'{name} lacks {sorted(hex(c) for c in missing)}: it would draw blank')

    def test_it_builds_without_a_warning(self):
        self.assertEqual(self.stderr, '')


class PortalLoadTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        try:
            dumper = gen.build_dumper(Path(cls._tmp.name) / 'dump_theme_defaults')
        except gen.GenError as e:
            cls._tmp.cleanup()
            raise unittest.SkipTest(str(e).splitlines()[0])
        out = Path(cls._tmp.name) / 'out'
        r = build(out)
        assert r.returncode == 0, r.stderr
        cls.state = gen.run_dumper(dumper, out / 'portal')

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_firmware_reads_the_theme(self):
        s = self.state
        self.assertEqual(s['names']['flight'], 'Sentry Eye')
        self.assertTrue(s['apps']['weather'])
        self.assertFalse(s['apps']['surveillance'])
        self.assertEqual(s['clock']['hands']['order'], [0, 1, 2])
        self.assertEqual(s['radar']['sweepColor'], '0xFF9A1F')
        self.assertEqual(s['ticker']['upColor'], '0x1FA2FF')
        self.assertEqual(s['intel']['title'], 'ANNOUNCEMENT')

    def test_app_names_fit_the_menu(self):
        # the knob menu draws the current name large; 11 characters is the widest seen to clear the portals
        for key, name in self.state['names'].items():
            self.assertLessEqual(len(name), 11, f'names.{key}: {name!r}')

    def test_aircraft_are_kept_inside_the_bezel(self):
        zones = self.state['radar']['zones']
        self.assertEqual(len(zones), 1)
        self.assertTrue(zones[0]['invert'])
        self.assertLessEqual(zones[0]['r'], 217, 'the bezel starts at r=217 in radar_plate.png')

    def test_weather_text_lines_are_shown(self):
        self.assertEqual([t['show'] for t in self.state['weather']['wtext']], [True, True, True, False])


if __name__ == '__main__':
    unittest.main()
