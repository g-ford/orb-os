import json
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests'))
from font_facts import converter_available, font_facts, font_table_bytes  # noqa: E402
import gen_elegant_theme as gen  # noqa: E402
import sys as _sys
from pathlib import Path as _Path
_sys.path.insert(0, str(_Path(__file__).resolve().parent))   # `import skips` works however the tests are run
import skips  # noqa: E402

THEME = ROOT / 'src' / 'theme_assets' / 'fallout'
PLATES = ('clock_plate', 'radar_plate', 'weather_plate', 'menu_plate', 'settings_plate',
          'splash', 'intel_plate', 'ticker_plate')
SPRITES = ('clock_hand_hour', 'clock_hand_minute', 'clock_hand_second', 'radar_blip')

FACE = THEME / 'source' / 'ShareTechMono-Regular.ttf'
FONT_PX = {     # the size each slot's layout was tuned for: the contract this theme keeps
    'wheel_sel': 46, 'wheel_item': 27, 'radar1': 22, 'radar2': 16, 'radar3': 16, 'radar4': 16,
    'weather1': 22, 'weather2': 22, 'weather3': 16, 'weather4': 16, 'intel_title': 32, 'intel_text': 22,
    'intel_source': 12, 'intel_age': 12, 'intel_brief': 18, 'ticker_name': 16, 'ticker_price': 40,
    'ticker_change': 22, 'ticker_strip': 16, 'wind_title': 28, 'wind_ask': 20, 'wind_turns': 16}
GLYPHS = list(range(0x20, 0x7F)) + [0xB0, 0xB1, 0xB7, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026]



def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()[:24]
    assert data[:8] == b'\x89PNG\r\n\x1a\n', f'{path.name} is not a PNG'
    return struct.unpack('>II', data[16:24])


def build(out: Path) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, str(ROOT / 'tools' / 'build_theme.py'), str(THEME), '--out', str(out)],
                          capture_output=True, text=True)


def build_or_skip(out: Path):
    """Build Fallout into `out`; skip the caller's class when its faces cannot be baked here."""
    r = build(out)
    if r.returncode != 0:
        if 'lv_font_conv' in r.stderr and not converter_available():
            raise skips.unmet("lv_font_conv (or npx) is needed to bake Fallout's faces")
        raise AssertionError(r.stderr)
    return out / 'fallout'


class FalloutThemeTest(unittest.TestCase):
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
        text = (THEME / 'theme.yaml').read_text(encoding='utf-8')
        for hand, name in (('hour', 'clock_hand_hour'), ('minute', 'clock_hand_minute'),
                           ('second', 'clock_hand_second')):
            m = re.search(rf'^\s*{hand}:.*pivotX: (\d+),\s*pivotY: (\d+)', text, re.M)
            self.assertTrue(m, hand)
            w, h = png_size(THEME / f'{name}.png')
            px, py = int(m.group(1)), int(m.group(2))
            self.assertTrue(0 < px < w and 0 < py < h, f'{hand} pivot ({px}, {py}) is outside {w}x{h}')
            self.assertEqual(px, w // 2, f'{hand} pivot should be on the hand\'s centre line')

    def test_the_blip_turns_about_its_centre(self):
        text = (THEME / 'theme.yaml').read_text(encoding='utf-8')
        w, h = png_size(THEME / 'radar_blip.png')
        px = int(re.search(r'^\s*blipPivotX: (\d+)', text, re.M).group(1))
        py = int(re.search(r'^\s*blipPivotY: (\d+)', text, re.M).group(1))
        self.assertEqual((px, py), (w // 2, h // 2), 'the firmware rotates the sprite about this point')
        self.assertIn('blipTypeImage: true', text)

    def test_every_colour_in_the_yaml_is_green(self):
        # Green phosphor, never amber: a red channel above the green one is what amber is made of.
        text = (THEME / 'theme.yaml').read_text(encoding='utf-8')
        colours = re.findall(r'\b0x([0-9A-Fa-f]{6})\b', text)
        self.assertGreater(len(colours), 8, 'the palette should be stated, not left to the defaults')
        for c in colours:
            r, g, b = int(c[0:2], 16), int(c[2:4], 16), int(c[4:6], 16)
            self.assertTrue(g >= r and g >= b, f'0x{c} is not green-dominant')

    def test_the_art_is_green_and_black_outside_the_dial(self):
        try:
            import numpy as np
            from PIL import Image
        except ImportError:
            raise skips.unmet('Pillow and numpy are needed to look at the art')
        for name in PLATES + SPRITES:
            px = np.asarray(Image.open(THEME / f'{name}.png').convert('RGBA')).astype(int)
            opaque = px[..., 3] > 0
            self.assertFalse(np.any(opaque & (px[..., 0] > px[..., 1] + 6)), f'{name} has pixels redder than green')
            self.assertFalse(np.any(opaque & (px[..., 2] > px[..., 1] + 6)), f'{name} has pixels bluer than green')
            if name in PLATES:
                yy, xx = np.mgrid[0:466, 0:466]
                outside = np.hypot(xx - 232.5, yy - 232.5) > 236
                self.assertFalse(np.any(px[..., :3][outside] > 0), f'{name} is not black outside the circle')

    def test_the_art_script_reproduces_the_committed_images(self):
        # the images are drawn by tools/fallout_art.py; a hand-edited PNG would be lost on the next run
        try:
            import numpy  # noqa: F401
            from PIL import Image, ImageChops
        except ImportError:
            raise skips.unmet('Pillow and numpy are needed to redraw the art')
        with tempfile.TemporaryDirectory() as tmp:
            import fallout_art
            original = fallout_art.OUT
            fallout_art.OUT = Path(tmp)
            try:
                fallout_art.main([])
            finally:
                fallout_art.OUT = original
            drawn = sorted(Path(tmp).glob('*.png'))
            self.assertEqual({p.stem for p in drawn}, set(PLATES + SPRITES))
            for png in drawn:
                # pixels, not bytes: the PNG encoder differs between Pillow/zlib versions
                a, b = Image.open(png).convert('RGBA'), Image.open(THEME / png.name).convert('RGBA')
                self.assertEqual(a.size, b.size, png.name)
                self.assertIsNone(ImageChops.difference(a, b).getbbox(), f'{png.name} differs from the script')


class FalloutFirmwareDecodeTest(unittest.TestCase):
    """What the DEVICE draws, not what Pillow says the PNG holds.

    The firmware's PNG decoder mis-reads some valid streams: from one column to the end of a row it
    goes one byte out of step, so black comes out pure red. Two of this theme's plates did exactly that,
    and every other test here passed, because none of them decoded a PNG the way the firmware does.
    Needs PNGdec (`pio run -e native` once) and a C++ compiler."""

    @classmethod
    def setUpClass(cls):
        try:
            import numpy  # noqa: F401
            import PIL  # noqa: F401
        except ImportError:
            raise skips.unmet('Pillow and numpy are needed to compare pixels')
        pngdec = ROOT / '.pio' / 'libdeps' / 'native' / 'PNGdec' / 'src'
        if not pngdec.is_dir():
            raise skips.unmet('PNGdec is not fetched: run `pio run -e native` once')
        cls._tmp = tempfile.TemporaryDirectory()
        tmp = Path(cls._tmp.name)
        try:
            objs = []
            for name in ('adler32', 'crc32', 'infback', 'inffast', 'inflate', 'inftrees', 'zutil'):
                subprocess.run(['cc', '-c', '-O1', f'-I{pngdec}', str(pngdec / f'{name}.c'), '-o', str(tmp / f'{name}.o')],
                               check=True, capture_output=True)
                objs.append(str(tmp / f'{name}.o'))
            cls.dump = tmp / 'png_decode_dump'
            subprocess.run(['c++', '-std=c++17', '-O1', '-DPNG_MAX_BUFFERED_PIXELS=8192',
                            f'-I{ROOT / "src/theme/graphics"}', f'-I{pngdec}', str(ROOT / 'tests/png_decode_dump.cpp'),
                            str(ROOT / 'src/theme/graphics/png_decode.cpp'), str(pngdec / 'PNGdec.cpp'), *objs,
                            '-o', str(cls.dump)], check=True, capture_output=True)
        except (OSError, subprocess.CalledProcessError) as e:
            cls._tmp.cleanup()
            raise skips.unmet(f'cannot build the decoder harness: {e}')

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_the_firmware_decodes_every_image_as_the_png_says(self):
        import numpy as np
        from PIL import Image

        def rgb565(a):
            a = a.astype(np.uint16)
            return ((a[..., 0] & 0xF8) << 8) | ((a[..., 1] & 0xFC) << 3) | (a[..., 2] >> 3)

        out = Path(self._tmp.name) / 'out.raw'
        for name in PLATES + SPRITES:
            png = THEME / f'{name}.png'
            r = subprocess.run([str(self.dump), str(png), str(out)], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, f'{name}: the firmware could not decode it ({r.stderr.strip()})')
            want_img = np.asarray(Image.open(png).convert('RGBA'))
            h, w = want_img.shape[:2]
            got = np.fromfile(out, dtype='<u2').reshape(h, w)
            wrong = got != rgb565(want_img[..., :3])
            if wrong.any():
                rows = np.nonzero(wrong.any(axis=1))[0]
                self.fail(f'{name}: {int(wrong.sum())} pixels decode wrongly on the device (rows {rows.min()}-{rows.max()}): '
                          're-save with another compress_level in fallout_art.save_png')


class FalloutFirmwareFontTest(unittest.TestCase):
    """The device loads each font with LVGL's lv_font_load. If that fails it says nothing and draws the compiled
    face, so a bad file would just look like the font never shipped. Needs the native env's liblvgl.a
    (`pio run -e native` once) and a C++ compiler."""

    @classmethod
    def setUpClass(cls):
        libs = sorted((ROOT / '.pio' / 'build' / 'native').glob('lib*/liblvgl.a'))
        if not libs:
            raise skips.unmet('liblvgl.a is not built: run `pio run -e native` once')
        cls._tmp = tempfile.TemporaryDirectory()
        cls.check = Path(cls._tmp.name) / 'font_load_check'
        lvgl = ROOT / '.pio' / 'libdeps' / 'native' / 'lvgl'
        try:
            subprocess.run(['c++', '-std=c++17', '-O1', '-DLV_CONF_INCLUDE_SIMPLE', f'-I{ROOT / "include"}', f'-I{lvgl}',
                            f'-I{lvgl / "src"}', str(ROOT / 'tests/font_load_check.cpp'), str(libs[0]), '-lm',
                            '-o', str(cls.check)], check=True, capture_output=True)
            cls.built = build_or_skip(Path(cls._tmp.name) / 'themes')
        except (OSError, subprocess.CalledProcessError) as e:
            cls._tmp.cleanup()
            raise skips.unmet(f'cannot build the font loader harness: {e}')
        except (unittest.SkipTest, skips.Unmet):
            cls._tmp.cleanup()
            raise
        cls.faces = sorted(set(json.loads((cls.built / 'theme.json').read_text())['fonts'].values()))

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_lvgl_loads_every_face_with_the_glyphs_the_screens_draw(self):
        want = [f'{c:X}' for c in GLYPHS]
        for name in self.faces:
            r = subprocess.run([str(self.check), str(self.built), name, *want], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, f'{name}: {r.stdout.strip() or r.stderr.strip()}')


class FalloutFontTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        try:
            cls.built = build_or_skip(Path(cls._tmp.name))
        except (unittest.SkipTest, skips.Unmet):
            cls._tmp.cleanup()
            raise
        cls.map = json.loads((cls.built / 'theme.json').read_text())['fonts']

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_it_maps_exactly_the_slots_it_always_did(self):
        self.assertEqual(set(self.map), set(FONT_PX))

    def test_it_ships_ten_faces_not_twenty_two(self):
        self.assertEqual(len(set(self.map.values())), 10)
        self.assertEqual(sorted(p.name for p in self.built.glob('font_*.bin')), sorted(set(self.map.values())))

    def test_each_slot_is_baked_at_the_size_its_layout_was_tuned_for(self):
        for slot, px in FONT_PX.items():
            f = font_facts(self.built / self.map[slot])
            self.assertEqual(f['tag'], b'head', slot)
            # 4 bits per pixel, uncompressed: the curved-text and menu renderers read the bitmaps directly
            self.assertEqual((f['bpp'], f['compression']), (4, 0), slot)
            self.assertEqual(f['size'], px, f'{slot} is baked at the wrong size')

    def test_no_font_is_truncated(self):
        for name in set(self.map.values()):
            size = (self.built / name).stat().st_size
            self.assertEqual(font_table_bytes(self.built / name), size,
                             f'{name}: its tables do not add up to its {size} bytes')

    def test_each_font_has_the_glyphs_the_screens_draw(self):
        want = set(range(0x20, 0x7F)) | {0xB0, 0xB7, 0x2019, 0x201C, 0x2014}     # ASCII, degrees, dot, quotes, dash
        for name in set(self.map.values()):
            missing = want - font_facts(self.built / name)['codepoints']
            self.assertFalse(missing, f'{name} lacks {sorted(hex(c) for c in missing)}: it would draw blank')

    def test_the_licence_ships_with_the_face(self):
        # the OFL requires the copyright notice and licence to travel with the font
        self.assertTrue(FACE.exists())
        licence = (THEME / 'source' / 'OFL.txt').read_text(encoding='utf-8')
        self.assertIn('SIL Open Font License', licence)
        self.assertIn('Carrois', licence)


class FalloutLoadTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        try:
            dumper = gen.build_dumper(Path(cls._tmp.name) / 'dump_theme_defaults')
        except gen.GenError as e:
            cls._tmp.cleanup()
            raise skips.unmet(str(e).splitlines()[0])
        out = Path(cls._tmp.name) / 'out'
        r = build(out)
        assert r.returncode == 0, r.stderr
        cls.state = gen.run_dumper(dumper, out / 'fallout')

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_firmware_reads_the_theme(self):
        s = self.state
        self.assertEqual(s['names']['clock'], 'TIME')
        self.assertEqual(s['names']['flight'], 'MAP')
        self.assertTrue(s['apps']['weather'])
        self.assertFalse(s['apps']['surveillance'])
        self.assertEqual(s['clock']['hands']['order'], [0, 1, 2])
        self.assertEqual(s['radar']['sweepColor'], '0x1BFF80')
        self.assertEqual(s['intel']['title'], 'RADIO')

    def test_app_names_fit_the_menu(self):
        # the knob menu draws the current name large; 11 characters is the widest seen to fit
        for key, name in self.state['names'].items():
            self.assertLessEqual(len(name), 11, f'names.{key}: {name!r}')

    def test_aircraft_are_kept_inside_the_bezel(self):
        zones = self.state['radar']['zones']
        self.assertEqual(len(zones), 1)
        self.assertTrue(zones[0]['invert'])
        self.assertLessEqual(zones[0]['r'], 217, 'the bezel starts at r=217 in radar_plate.png')


if __name__ == '__main__':
    unittest.main()
