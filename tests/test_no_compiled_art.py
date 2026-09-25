"""Spec step 5: no compiled theme art or bitmap font remains in the firmware image. Rule 4: this is the shared check that
makes the deletion stick, rather than a note beside each file. It counts numeric table entries, so a renamed dial or a
re-pasted sprite is caught by what it is, not by what it is called."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'src'
# A numeric table entry: 0x1F, or 217. Ordinary code has a few dozen of these; a compiled picture has tens of thousands.
DATA_TOKEN = re.compile(r'(?<![\w.])(?:0x[0-9A-Fa-f]{1,4}|\d{1,3})\s*,')
LIMIT = 1000
KEPT = ('src/theme/fonts/',)   # the Inter size ladder that the text-size sliders index into


def data_tokens(text: str) -> int:
    return len(DATA_TOKEN.findall(text))


def strip_comments(text: str) -> str:
    return re.sub(r'//[^\n]*', '', re.sub(r'/\*.*?\*/', '', text, flags=re.S))



def missing_filter_entries(line: str) -> list:
    """Files a build_src_filter line names (either sign) that are not under src/. Wildcards are not files."""
    return [p for _sign, p in re.findall(r'([+-])<([^>]+)>', line) if '*' not in p and not (SRC / p).exists()]


class NoCompiledArtTest(unittest.TestCase):
    def test_the_check_can_fail(self):
        self.assertGreater(data_tokens('static const uint8_t X[] = {' + ','.join(['1'] * 2000) + ',};'), LIMIT)
        self.assertLess(data_tokens('int a[] = {1, 2, 3};'), LIMIT)

    def test_no_file_under_app_or_theme_carries_a_data_table(self):
        big = {}
        for base in ('src/app', 'src/theme'):
            for p in (ROOT / base).rglob('*'):
                rel = p.relative_to(ROOT).as_posix()
                if p.suffix in ('.h', '.c', '.cpp') and not rel.startswith(KEPT):
                    n = data_tokens(p.read_text(encoding='utf-8', errors='replace'))
                    if n > LIMIT:
                        big[rel] = n
        self.assertEqual(big, {})

    def test_the_compiled_bitmap_fonts_are_gone(self):
        self.assertEqual(sorted(p.name for p in (SRC / 'theme' / 'custom').glob('custom_*font*.c')), [])
        for p in list(SRC.rglob('*.h')) + list(SRC.rglob('*.cpp')):
            self.assertNotRegex(strip_comments(p.read_text(encoding='utf-8', errors='replace')), r'extern const lv_font_t custom_',
                                f'{p.relative_to(ROOT)} declares a compiled bitmap font')

    def test_the_three_slots_that_had_a_bitmap_face_fall_back_to_montserrat(self):
        fonts = strip_comments((SRC / 'theme' / 'core' / 'theme_font.cpp').read_text(encoding='utf-8'))
        radar = strip_comments((SRC / 'theme' / 'custom' / 'custom_radar.h').read_text(encoding='utf-8'))
        self.assertIn('case S_WHEEL_SEL:  return &lv_font_montserrat_44;', fonts)
        self.assertIn('#define CUSTOM_RTEXT1_FONT (&lv_font_montserrat_26)', radar)
        self.assertIn('#define CUSTOM_RTEXT2_FONT (&lv_font_montserrat_20)', radar)

    def test_the_simulator_source_filter_names_only_files_that_exist(self):
        """PlatformIO ignores a <file> that is not there, so a stale entry never fails a build: an exclusion that
        names a deleted file silently stops excluding anything. Globs are skipped; every named file must exist."""
        ini = (ROOT / 'platformio.ini').read_text(encoding='utf-8')
        line = next(l for l in ini[ini.index('[env:native]'):].splitlines() if l.startswith('build_src_filter'))
        self.assertEqual(missing_filter_entries(line), [])

    def test_the_filter_check_can_fail(self):
        self.assertEqual(missing_filter_entries('+<**/*.cpp> -<gone/away.cpp> -<main.cpp>'), ['gone/away.cpp'])
        self.assertEqual(missing_filter_entries('+<**/*.cpp> +<**/*.c>'), [])


if __name__ == '__main__':
    unittest.main()
