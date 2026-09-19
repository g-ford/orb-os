import json
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'tools' / 'build_theme.py'

def make_png(color_type: int = 6, depth: int = 8, size=466) -> bytes:
    """A valid PNG, a full-screen 466x466 by default. The build reads the header to refuse a pixel
    type the firmware would blank and a plate of the wrong size."""
    width, height = (size, size) if isinstance(size, int) else size

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
    channels = {0: 1, 2: 3, 4: 2, 6: 4}[color_type]
    rows = (b'\x00' + b'\x80' * (channels * width)) * height
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, depth, color_type, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


PNG = make_png()

SAMPLE = """
slug: sample
name: Sample
author: Test Author
version: 1
apps:
  clock: true
  weather: false
names:
  clock: Clock
clock:
  bg: 0x000102
  plateFollow: 2
  text1:
    show: true
    x: 120
    color: #F2F5F9   # a comment after a colour
    fmt: "%H:%M"
  hands:
    order: [3, 4, 0, 1, 2]
    shadow:
      on: true
radar:
  sweepEnabled: true
  rangeKm: 35.5
  zones:
    - r: 120
    - rect: true
      w: 100
      h: 80
menu:
  current:
    fmt: "{name}"
"""


def run(theme_dir: Path, out: Path):
    return subprocess.run([sys.executable, str(SCRIPT), str(theme_dir), '--out', str(out)],
                          capture_output=True, text=True, check=False)


class BuildThemeTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = Path(self._tmp.name)
        self.src = self.tmp / 'sample'
        self.src.mkdir()
        self.out = self.tmp / 'out'

    def tearDown(self):
        self._tmp.cleanup()

    def write(self, yaml_text=SAMPLE, **files):
        (self.src / 'theme.yaml').write_text(yaml_text, encoding='utf-8')
        for name, data in files.items():
            (self.src / name.replace('__', '.')).write_bytes(data)

    def built(self, name):
        return json.loads((self.out / 'sample' / name).read_text(encoding='utf-8'))

    def test_sections_become_the_style_files_verbatim(self):
        self.write()
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        clock = self.built('clock_style.json')
        self.assertEqual(clock['bg'], 0x000102)                  # 0x form is a number
        self.assertEqual(clock['plateFollow'], 2)
        self.assertEqual(clock['text1']['color'], 0xF2F5F9)      # #RRGGBB form is too
        self.assertEqual(clock['text1']['fmt'], '%H:%M')
        self.assertEqual(clock['hands']['order'], [3, 4, 0, 1, 2])
        self.assertIs(clock['hands']['shadow']['on'], True)

        radar = self.built('radar_style.json')
        self.assertEqual(radar['rangeKm'], 35.5)
        self.assertEqual(radar['zones'], [{'r': 120}, {'rect': True, 'w': 100, 'h': 80}])

        self.assertEqual(self.built('menu_style.json')['current']['fmt'], '{name}')

        theme = self.built('theme.json')
        self.assertEqual((theme['slug'], theme['name'], theme['author']), ('sample', 'Sample', 'Test Author'))
        self.assertEqual(theme['apps'], {'clock': True, 'weather': False})
        self.assertEqual(theme['names'], {'clock': 'Clock'})

    def test_sections_left_out_write_no_file(self):
        self.write()
        run(self.src, self.out)
        self.assertFalse((self.out / 'sample' / 'ticker_style.json').exists())

    def test_assets_are_derived_from_the_folder_and_old_names_renamed(self):
        self.write(clock_plate__png=PNG, dial_avi__png=b'ignored-loses-to-canonical',
                   splash_png_default__png=PNG, font_clock1__bin=b'font')
        (self.src / 'notes.txt').write_text('not an asset')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        theme = self.built('theme.json')
        self.assertEqual(theme['assets'], ['clock_plate.png', 'font_clock1.bin', 'splash.png'])
        built = sorted(p.name for p in (self.out / 'sample').iterdir())
        self.assertIn('splash.png', built)
        self.assertNotIn('splash_png_default.png', built)
        self.assertEqual((self.out / 'sample' / 'clock_plate.png').read_bytes(), PNG)
        self.assertIn('both clock_plate.png', result.stderr)

    def test_a_png_the_firmware_would_draw_black_is_refused(self):
        # the firmware's decoders accept only 8-bit RGBA and fill anything else with zeros
        for kind, color_type in (('RGB', 2), ('greyscale', 0), ('greyscale+alpha', 4)):
            with self.subTest(kind):
                self.write(clock_plate__png=make_png(color_type))
                result = run(self.src, self.out)
                self.assertEqual(result.returncode, 1)
                self.assertIn(f'clock_plate.png is 8-bit {kind}', result.stderr)
                self.assertFalse((self.out / 'sample').exists())

    def test_a_plate_of_the_wrong_size_is_refused(self):
        # a plate is drawn at its own pixel size on a 466x466 screen
        for size in (500, 1254, (466, 400)):
            with self.subTest(size=size):
                self.write(clock_plate__png=make_png(size=size))
                result = run(self.src, self.out)
                self.assertEqual(result.returncode, 1)
                self.assertIn('must be exactly that', result.stderr)
                self.assertIn('fit_theme_art.py', result.stderr)

    def test_sprites_may_be_any_size(self):
        self.write(clock_plate__png=PNG, clock_hand_hour__png=make_png(size=(26, 156)))
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

    def test_a_file_that_is_not_a_png_is_refused(self):
        self.write(clock_plate__png=b'not a png at all, just some text bytes')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 1)
        self.assertIn('is not a PNG', result.stderr)

    def test_a_png_that_loses_to_a_canonical_name_is_not_checked(self):
        # only the file that will actually be copied matters
        self.write(clock_plate__png=PNG, dial_avi__png=make_png(2))
        self.assertEqual(run(self.src, self.out).returncode, 0)

    def test_hash_covers_contents_not_just_names(self):
        self.write(clock_plate__png=PNG)
        run(self.src, self.out)
        before = self.built('theme.json')['assetsHash']
        (self.src / 'clock_plate.png').write_bytes(PNG + b'changed')
        run(self.src, self.out)
        after = self.built('theme.json')['assetsHash']
        self.assertNotEqual(before, after)
        self.assertTrue(0 < after <= 0xFFFFFFFF)

    def test_installed_marker_is_written(self):
        self.write()
        run(self.src, self.out)
        self.assertTrue((self.out / 'sample' / '_installed').exists())

    def test_a_mistyped_key_is_reported(self):
        self.write('slug: sample\nradar:\n  sweepEnabeld: true\n  maxAircraft: 5\n')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn('radar.sweepEnabeld', result.stderr)
        self.assertNotIn('radar.maxAircraft', result.stderr)

    def test_real_firmware_keys_are_not_flagged(self):
        # Keys the previous converter's hand-written lists were missing.
        self.write('slug: sample\nclock:\n  secondSweep: true\n  windTitleOpa: 200\n'
                   '  hands:\n    shadow: {on: true, dx: 2, dy: 3}\n')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertNotIn('never reads', result.stderr)

    def test_duplicate_key_is_an_error(self):
        self.write('slug: sample\nclock:\n  bg: 1\n  bg: 2\n')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 1)
        self.assertIn("duplicate key 'bg'", result.stderr)

    def test_unknown_top_level_key_is_an_error(self):
        self.write('slug: sample\nrada:\n  sweepEnabled: true\n')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 1)
        self.assertIn("'rada'", result.stderr)

    def test_oversized_style_file_is_refused(self):
        self.write('slug: sample\nmenu:\n  current:\n    fmt: "' + 'x' * 9000 + '"\n')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 1)
        self.assertIn('menu_style.json', result.stderr)
        self.assertFalse((self.out / 'sample').exists())

    def test_a_failed_build_leaves_the_previous_build_alone(self):
        self.write()
        run(self.src, self.out)
        self.write('slug: sample\nrada: {}\n')
        self.assertEqual(run(self.src, self.out).returncode, 1)
        self.assertTrue((self.out / 'sample' / 'clock_style.json').exists())

    def test_out_overlapping_the_source_is_refused(self):
        self.write()
        result = run(self.src, self.tmp)          # would build to <tmp>/sample == the source
        self.assertEqual(result.returncode, 1)
        self.assertIn('overlaps', result.stderr)
        self.assertTrue((self.src / 'theme.yaml').exists())

    def test_bad_slug_is_an_error(self):
        self.write('slug: Has Spaces\n')
        self.assertEqual(run(self.src, self.out).returncode, 1)

    def test_valueless_key_is_dropped_with_a_warning(self):
        self.write('slug: sample\nradar:\n  rangeKm:\n  maxAircraft: 5\n')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertEqual(self.built('radar_style.json'), {'maxAircraft': 5})
        self.assertIn('radar.rangeKm', result.stderr)


if __name__ == '__main__':
    unittest.main()
