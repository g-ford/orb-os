import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path
from unittest import mock

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


STUB_CONVERTER = ROOT / 'tests' / 'stub_lv_font_conv.py'

FONTS_YAML = """slug: sample
fonts:
  faces:
    label: {src: t.ttf, size: 16}
  slots:
    radar2: label
    radar3: label
"""


class FontFacesTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = Path(self._tmp.name)
        self.src = self.tmp / 'sample'
        self.src.mkdir()
        self.out = self.tmp / 'out'
        self.log = self.tmp / 'calls.log'
        env = mock.patch.dict(os.environ, {'LV_FONT_CONV': str(STUB_CONVERTER), 'STUB_LOG': str(self.log)})
        env.start()
        self.addCleanup(env.stop)

    def write(self, yaml_text, **files):
        (self.src / 'theme.yaml').write_text(yaml_text, encoding='utf-8')
        for name, data in files.items():
            (self.src / name.replace('__', '.')).write_bytes(data)

    def built(self, name):
        return json.loads((self.out / 'sample' / name).read_text(encoding='utf-8'))

    def fonts_built(self):
        return sorted(p.name for p in (self.out / 'sample').glob('font_*.bin'))

    def calls(self):
        return len(self.log.read_text().splitlines()) if self.log.exists() else 0

    def test_a_face_is_baked_once_however_many_slots_use_it(self):
        self.write(FONTS_YAML, t__ttf=b'TTF')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertEqual(self.built('theme.json')['fonts'],
                         {'radar2': 'font_label.bin', 'radar3': 'font_label.bin'})
        self.assertEqual(self.fonts_built(), ['font_label.bin'])
        self.assertEqual((self.out / 'sample' / 'font_label.bin').read_bytes(), b'STUB t.ttf 16\n')
        self.assertEqual(self.calls(), 1)
        self.assertIn('font_label.bin', self.built('theme.json')['assets'])

    def test_changing_a_face_changes_the_assets_hash(self):
        self.write(FONTS_YAML, t__ttf=b'TTF')
        run(self.src, self.out)
        first = self.built('theme.json')['assetsHash']
        self.write(FONTS_YAML.replace('size: 16', 'size: 18'), t__ttf=b'TTF')
        run(self.src, self.out)
        self.assertNotEqual(self.built('theme.json')['assetsHash'], first)

    def test_a_bin_face_is_copied_verbatim_and_needs_no_converter(self):
        (self.src / 'fonts').mkdir()
        (self.src / 'fonts' / 'raw.bin').write_bytes(b'\x01\x02RAW')
        self.write('slug: sample\nfonts:\n  faces:\n    raw: {src: fonts/raw.bin}\n  slots:\n    radar1: raw\n')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertEqual((self.out / 'sample' / 'font_raw.bin').read_bytes(), b'\x01\x02RAW')
        self.assertEqual(self.calls(), 0)

    def test_no_fonts_block_builds_as_before(self):
        self.write('slug: sample\nradar:\n  rangeKm: 30\n')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertNotIn('fonts', self.built('theme.json'))

    def assert_refused(self, yaml_text, needle, **files):
        self.write(yaml_text, **files)
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 1, msg=result.stdout)
        self.assertIn(needle, result.stderr)
        self.assertNotIn('Traceback', result.stderr)
        return result

    def test_a_misspelt_slot_is_an_error_that_lists_the_real_ones(self):
        result = self.assert_refused(FONTS_YAML.replace('radar2:', 'radr2:'), 'fonts.slots.radr2', t__ttf=b'TTF')
        self.assertIn('known:', result.stderr)
        self.assertIn('radar2', result.stderr)

    def test_a_slot_naming_an_undefined_face_is_an_error(self):
        self.assert_refused(FONTS_YAML.replace('radar2: label', 'radar2: nope'), "'nope'", t__ttf=b'TTF')

    def test_a_face_named_like_a_slot_is_an_error(self):
        self.assert_refused(FONTS_YAML.replace('label', 'radar2'), 'also a slot name', t__ttf=b'TTF')

    def test_a_face_name_too_long_for_the_flash_index_is_an_error(self):
        self.assert_refused(FONTS_YAML.replace('label', 'a' * 15), '14 characters', t__ttf=b'TTF')

    def test_a_src_outside_the_theme_folder_is_an_error(self):
        (self.tmp / 'evil.ttf').write_bytes(b'TTF')
        self.assert_refused(FONTS_YAML.replace('src: t.ttf', 'src: ../evil.ttf'), 'outside the theme folder')
        self.assertEqual(self.calls(), 0)

    def test_a_missing_src_is_an_error(self):
        self.assert_refused(FONTS_YAML, 'does not exist')

    def test_a_bin_face_with_a_size_is_an_error(self):
        (self.src / 'raw.bin').write_bytes(b'x')
        self.assert_refused('slug: sample\nfonts:\n  faces:\n    raw: {src: raw.bin, size: 16}\n  slots:\n    radar1: raw\n',
                            'already baked')

    def test_a_ttf_face_without_a_size_is_an_error(self):
        self.assert_refused(FONTS_YAML.replace(', size: 16', ''), 'pixel size', t__ttf=b'TTF')

    def test_a_missing_converter_is_one_clean_error(self):
        self.write(FONTS_YAML, t__ttf=b'TTF')
        with mock.patch.dict(os.environ, {'LV_FONT_CONV': '/nonexistent/lv_font_conv'}):
            result = run(self.src, self.out)
        self.assertEqual(result.returncode, 1)
        self.assertIn('error:', result.stderr)
        self.assertIn('lv_font_conv', result.stderr)
        self.assertNotIn('Traceback', result.stderr)

    def test_a_converter_failure_is_reported_with_its_message(self):
        self.write(FONTS_YAML, t__ttf=b'TTF')
        with mock.patch.dict(os.environ, {'STUB_FAIL': '1'}):
            result = run(self.src, self.out)
        self.assertEqual(result.returncode, 1)
        self.assertIn('asked to fail', result.stderr)

    def test_a_stale_slot_file_beside_a_mapping_is_not_shipped(self):
        self.write(FONTS_YAML, t__ttf=b'TTF', font_radar2__bin=b'STALE')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertEqual(self.fonts_built(), ['font_label.bin'])
        self.assertIn('shadowed', result.stderr)
        self.assertNotIn('font_radar2.bin', self.built('theme.json')['assets'])

    def test_an_unmapped_slot_keeps_its_own_file(self):
        self.write(FONTS_YAML, t__ttf=b'TTF', font_radar4__bin=b'OWN')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertEqual(self.fonts_built(), ['font_label.bin', 'font_radar4.bin'])
        self.assertIn('font_radar4.bin', self.built('theme.json')['assets'])

    def test_an_unused_face_warns_and_is_not_baked(self):
        self.write(FONTS_YAML.replace('  slots:', '    spare: {src: t.ttf, size: 20}\n  slots:'), t__ttf=b'TTF')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn('fonts.faces.spare', result.stderr)
        self.assertEqual(self.calls(), 1)

    def faces_and_slots(self, count):
        slots = ['radar1', 'radar2', 'radar3', 'radar4', 'weather1', 'weather2', 'weather3', 'weather4',
                 'intel_title', 'intel_text', 'intel_source', 'intel_age']
        faces = ''.join(f'    f{i}: {{src: t.ttf, size: {10 + i}}}\n' for i in range(count))
        maps = ''.join(f'    {slots[i]}: f{i}\n' for i in range(count))
        return f'slug: sample\nfonts:\n  faces:\n{faces}  slots:\n{maps}'

    def test_more_than_ten_distinct_fonts_warn_and_ten_do_not(self):
        self.write(self.faces_and_slots(11), t__ttf=b'TTF')
        eleven = run(self.src, self.out)
        self.assertEqual(eleven.returncode, 0, msg=eleven.stderr)
        self.assertIn('distinct fonts', eleven.stderr)
        self.write(self.faces_and_slots(10), t__ttf=b'TTF')
        ten = run(self.src, self.out)
        self.assertEqual(ten.returncode, 0, msg=ten.stderr)
        self.assertNotIn('distinct fonts', ten.stderr)

    def test_a_theme_with_no_fonts_block_is_never_warned_about_its_per_slot_files(self):
        # Elegant (11 files) and Fallout (22) shipped one file per slot before faces existed; the budget
        # applies to a theme that defines faces, not to one that has not migrated yet.
        legacy = {f'font_{slot}__bin': b'x' for slot in
                  ('radar1', 'radar2', 'radar3', 'radar4', 'weather1', 'weather2', 'weather3', 'weather4',
                   'intel_title', 'intel_text', 'intel_source')}
        self.write('slug: sample\nradar:\n  rangeKm: 30\n', **legacy)
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertNotIn('distinct fonts', result.stderr)
        self.assertEqual(len(self.fonts_built()), 11)

    def test_remapping_slots_to_the_same_faces_changes_the_assets_hash(self):
        # Review finding: the hash covered only the files, so swapping which slot uses which of two shipped
        # faces left it unchanged, the device skipped the re-bake, and the flash fonts.map stayed stale.
        first = ('slug: sample\nfonts:\n  faces:\n    small: {src: t.ttf, size: 16}\n    big: {src: t.ttf, size: 20}\n'
                 '  slots:\n    radar2: small\n    radar3: big\n')
        swapped = first.replace('radar2: small\n    radar3: big', 'radar2: big\n    radar3: small')
        self.assertNotEqual(first, swapped)
        self.write(first, t__ttf=b'TTF')
        self.assertEqual(run(self.src, self.out).returncode, 0)
        h1 = self.built('theme.json')['assetsHash']
        self.write(swapped, t__ttf=b'TTF')
        self.assertEqual(run(self.src, self.out).returncode, 0)
        h2 = self.built('theme.json')['assetsHash']
        self.assertNotEqual(h1, h2)

    def test_the_font_count_and_size_are_printed(self):
        self.write(FONTS_YAML, t__ttf=b'TTF')
        result = run(self.src, self.out)
        self.assertRegex(result.stdout, r'fonts: 1 file\(s\), \d+ KB')
        self.assertRegex(result.stdout.strip().splitlines()[-1], r'image\(s\), 1 font\(s\)')


if __name__ == '__main__':
    unittest.main()
