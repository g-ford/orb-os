import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import font_golden  # noqa: E402
import sys as _sys
from pathlib import Path as _Path
_sys.path.insert(0, str(_Path(__file__).resolve().parent))   # `import skips` works however the tests are run
import skips  # noqa: E402

BUILD = ROOT / 'tools' / 'build_theme.py'
THEMES = ROOT / 'src' / 'theme_assets'
GOLDEN = ROOT / 'tests' / 'golden'


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class ResolveTest(unittest.TestCase):
    def test_a_slot_resolves_through_the_map_else_its_own_file_else_nothing(self):
        with tempfile.TemporaryDirectory() as tmp:
            built = Path(tmp)
            (built / 'theme.json').write_text(json.dumps({'fonts': {'radar1': 'font_face.bin'}}))
            (built / 'font_face.bin').write_bytes(b'FACE')
            (built / 'font_radar2.bin').write_bytes(b'OWN')
            got = font_golden.resolve(built)
            self.assertEqual(got['radar1'], sha(b'FACE'))
            self.assertEqual(got['radar2'], sha(b'OWN'))
            self.assertNotIn('radar3', got)

    def test_a_theme_json_with_no_map_uses_the_slot_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            built = Path(tmp)
            (built / 'theme.json').write_text('{}')
            (built / 'font_wheel_item.bin').write_bytes(b'S')
            self.assertEqual(font_golden.resolve(built), {'wheel_item': sha(b'S')})

    def test_render_is_sorted_slot_and_digest_lines(self):
        self.assertEqual(font_golden.render({'b': 'y', 'a': 'x'}), 'a x\nb y\n')


def converter_available() -> bool:
    return bool(os.environ.get('LV_FONT_CONV') or shutil.which('lv_font_conv') or shutil.which('npx'))


class ShippedThemesKeepTheirTypefaceTest(unittest.TestCase):
    """Every slot of a shipped theme must resolve to byte-identical glyphs before and after it was moved to
    the fonts: block. The golden files were captured from the themes as they were shipped, one font file per slot."""

    def check(self, slug):
        with tempfile.TemporaryDirectory() as tmp:
            r = subprocess.run([sys.executable, str(BUILD), str(THEMES / slug), '--out', tmp],
                               capture_output=True, text=True)
            if r.returncode != 0 and 'lv_font_conv' in r.stderr and not converter_available():
                raise skips.unmet('lv_font_conv (or npx) is needed to bake this theme\'s faces')
            self.assertEqual(r.returncode, 0, msg=r.stderr)
            got = font_golden.render(font_golden.resolve(Path(tmp) / slug))
        want = (GOLDEN / f'fonts_{slug}.txt').read_text(encoding='utf-8')
        self.assertEqual(got, want, f'{slug}: a slot now draws different glyphs than it did when the golden was captured')

    def test_elegant(self):
        self.check('elegant')

    def test_fallout(self):
        self.check('fallout')


if __name__ == '__main__':
    unittest.main()
