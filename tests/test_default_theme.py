"""The default theme is a Studio design; these tests keep theme.yaml in the form the firmware reads.

theme.yaml lists every option, and every colour as hex, because tools/gen_default_theme.py wrote
it from what theme_style.cpp makes of the folder. They build tools/dump_theme_defaults.cpp
against the real theme_style.cpp, so they need a C++ compiler and the LVGL/ArduinoJson sources
in .pio/libdeps/native (present once `pio run -e native` has been run). Without them the tests
that need it skip and say why.
"""
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
import gen_default_theme as gen  # noqa: E402

THEME = ROOT / 'src' / 'theme_assets' / 'default'
PLATES = ('clock_plate', 'radar_plate', 'menu_plate', 'settings_plate', 'splash')


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()[:24]
    assert data[:8] == b'\x89PNG\r\n\x1a\n', f'{path.name} is not a PNG'
    return struct.unpack('>II', data[16:24])


def pack_orb(path: Path, files: dict[str, bytes], slug: str = 'test'):
    """A .orb as Studio writes it: magic, slug, then each file as name and length-prefixed bytes."""
    out = b'ORBTHM01' + struct.pack('<H', len(slug)) + slug.encode() + struct.pack('<H', len(files))
    for name, data in files.items():
        n = name.encode()
        out += struct.pack('<H', len(n)) + n + struct.pack('<I', len(data)) + data
    path.write_bytes(out)


class DefaultThemeFilesTest(unittest.TestCase):
    """Needs nothing but Python."""

    def test_builds_without_a_single_warning(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = subprocess.run([sys.executable, str(ROOT / 'tools' / 'build_theme.py'), str(THEME), '--out', tmp],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, msg=r.stderr)
            # a warning here is a mistyped option, an image or font the firmware ignores, or a duplicate
            self.assertEqual(r.stderr, '')

    def test_plates_are_full_screen(self):
        for name in PLATES:
            self.assertEqual(png_size(THEME / f'{name}.png'), (466, 466), name)

    def test_the_fonts_are_not_swallowed_by_gitignore(self):
        # .gitignore has `*.bin`; a theme's fonts are the theme, and a clone without them is a
        # different theme that builds without a word
        fonts = sorted(THEME.glob('font_*.bin'))
        self.assertTrue(fonts, 'the default theme has no fonts')
        # with neither -q nor -v, check-ignore prints only the paths that ARE ignored
        r = subprocess.run(['git', 'check-ignore', *map(str, fonts)], cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(r.stdout, '', 'git would ignore these theme fonts')

    def test_orb_files_unpack_and_cannot_escape_the_folder(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            pack_orb(tmp / 'ok.orb', {'clock_style.json': b'{}', 'clock_plate.png': b'\x89PNG'})
            gen.unpack_orb(tmp / 'ok.orb', tmp / 'ok')
            self.assertEqual(sorted(p.name for p in (tmp / 'ok').iterdir()), ['clock_plate.png', 'clock_style.json'])
            for bad in ('../escape.json', 'sub/dir.json', '..'):
                pack_orb(tmp / 'bad.orb', {bad: b'x'})
                with self.assertRaises(gen.GenError, msg=bad):
                    gen.unpack_orb(tmp / 'bad.orb', tmp / 'bad')
            self.assertFalse((tmp / 'escape.json').exists())
            (tmp / 'notorb.orb').write_bytes(b'PK\x03\x04 not an orb')
            with self.assertRaises(gen.GenError):
                gen.unpack_orb(tmp / 'notorb.orb', tmp / 'notorb')


class DefaultThemeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        tmp = Path(cls._tmp.name)
        try:
            cls.dumper = gen.build_dumper(tmp / 'dump_theme_defaults')
        except gen.GenError as e:
            cls._tmp.cleanup()
            raise unittest.SkipTest(str(e).splitlines()[0])
        cls.built = gen.build_folder(THEME, tmp / 'built')
        cls.state = gen.run_dumper(cls.dumper, cls.built)

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_committed_theme_is_in_the_form_the_firmware_reads(self):
        # If this fails: an option was added to the firmware, or theme.yaml was edited by hand.
        # Run python3 tools/gen_default_theme.py
        self.assertEqual((THEME / 'theme.yaml').read_text(encoding='utf-8'), gen.render_yaml(self.state))

    def test_it_is_the_default_theme(self):
        self.assertEqual(json.loads((self.built / 'theme.json').read_text())['slug'], 'default')
        self.assertRegex((THEME / 'theme.yaml').read_text(encoding='utf-8'), r'(?m)^default: true$')

    def test_the_load_check_can_fail(self):
        # Guards the test above against passing because the files were never read.
        with tempfile.TemporaryDirectory() as tmp:
            built = Path(tmp) / 'default'
            subprocess.run(['cp', '-R', str(self.built), str(built)], check=True)
            path = built / 'radar_style.json'
            data = json.loads(path.read_text())
            data['sweepSpeed'] = self.state['radar']['sweepSpeed'] + 1
            path.write_text(json.dumps(data))
            self.assertNotEqual(gen.run_dumper(self.dumper, built), self.state)

    def _selopa_after_setting(self, style_file: str) -> int:
        with tempfile.TemporaryDirectory() as tmp:
            built = Path(tmp) / 'default'
            subprocess.run(['cp', '-R', str(self.built), str(built)], check=True)
            path = built / style_file
            data = json.loads(path.read_text())
            data['selOpa'] = 10
            path.write_text(json.dumps(data))
            return gen.run_dumper(self.dumper, built)['settings']['selOpa']

    def test_radar_style_cannot_change_the_settings_screen(self):
        # was a bug: radar_style.json's selOpa was loaded into the Settings selected-row opacity
        self.assertEqual(self._selopa_after_setting('radar_style.json'), self.state['settings']['selOpa'])

    def test_settings_style_sets_the_settings_selection_opacity(self):
        self.assertEqual(self._selopa_after_setting('settings_style.json'), 10)

    def test_every_hand_pivots_inside_its_image(self):
        for hand in ('hour', 'minute', 'second'):
            w, h = png_size(THEME / f'clock_hand_{hand}.png')
            spec = self.state['clock']['hands'][hand]
            self.assertTrue(0 < spec['pivotX'] < w and 0 < spec['pivotY'] < h,
                            f'{hand} pivot ({spec["pivotX"]}, {spec["pivotY"]}) is outside {w}x{h}')

    def test_every_option_the_firmware_reads_is_in_the_theme(self):
        src = (ROOT / 'src' / 'theme' / 'core' / 'theme_style.cpp').read_text(encoding='utf-8').split('\n')
        start = next(i for i, l in enumerate(src) if 'static void parse_pill' in l)
        end = next(i for i, l in enumerate(src) if l.startswith('const Clock &clock()'))
        region = re.sub(r'//[^\n]*', '', '\n'.join(src[start:end]))
        read = set(re.findall(r'\["([A-Za-z][A-Za-z0-9_]*)"\]', region))
        read |= set(re.findall(r'\{\s*"([A-Za-z][A-Za-z0-9_]*)"\s*,', region))
        # written by build_theme.py from the folder's files, never by hand
        read -= {'assets', 'assetsHash'}
        text = (THEME / 'theme.yaml').read_text(encoding='utf-8')
        missing = sorted(k for k in read if not re.search(rf'\b{k}\b', text))
        self.assertEqual(missing, [], 'options theme_style.cpp reads that the default theme does not mention')


if __name__ == '__main__':
    unittest.main()
