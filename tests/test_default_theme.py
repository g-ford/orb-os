"""The default theme is generated from the firmware; these tests keep it that way.

They build tools/dump_theme_defaults.cpp against the real theme_style.cpp, so they need a C++
compiler and the LVGL/ArduinoJson sources in .pio/libdeps/native (present once `pio run -e
native` has been run). Without them the tests skip and say why.
"""
import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import gen_default_theme as gen  # noqa: E402

THEME = ROOT / 'src' / 'theme_assets' / 'default'


class DefaultThemeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        try:
            cls.dumper = gen.build_dumper(Path(cls._tmp.name) / 'dump_theme_defaults')
        except gen.GenError as e:
            cls._tmp.cleanup()
            raise unittest.SkipTest(str(e).splitlines()[0])
        cls.defaults = gen.run_dumper(cls.dumper)
        cls.expected = gen.with_aviator(cls.defaults)

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def build(self, out: Path, theme_dir: Path = THEME) -> Path:
        r = subprocess.run([sys.executable, str(ROOT / 'tools' / 'build_theme.py'), str(theme_dir), '--out', str(out)],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, msg=r.stderr)
        return out / theme_dir.name

    def test_committed_theme_matches_the_firmware(self):
        # If this fails a firmware default changed: run python3 tools/gen_default_theme.py
        self.assertEqual((THEME / 'theme.yaml').read_text(encoding='utf-8'), gen.generate())

    def test_built_theme_loads_back_to_the_defaults(self):
        with tempfile.TemporaryDirectory() as tmp:
            built = self.build(Path(tmp))
            self.assertEqual(gen.run_dumper(self.dumper, built), self.expected)

    def test_the_load_check_can_fail(self):
        # Guards the test above against passing because the files were never read.
        with tempfile.TemporaryDirectory() as tmp:
            built = self.build(Path(tmp))
            path = built / 'radar_style.json'
            data = json.loads(path.read_text())
            data['sweepSpeed'] = self.expected['radar']['sweepSpeed'] + 1
            path.write_text(json.dumps(data))
            self.assertNotEqual(gen.run_dumper(self.dumper, built), self.expected)

    def test_clock_uses_the_aviator_dial(self):
        second = self.expected['clock']['hands']['second']
        self.assertEqual((second['centerX'], second['centerY']), gen.aviator_sub_dial())
        for name in ('clock_plate', 'clock_hand_hour', 'clock_hand_minute', 'clock_hand_second'):
            self.assertTrue((THEME / f'{name}.png').exists(), name)

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
