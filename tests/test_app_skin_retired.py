"""Spec step 4: the compiled Default/Office skins and the API that chose between them are gone. Rule 4: a second
palette system must not come back by accident, so this reads the whole source tree rather than sitting beside one call."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'src'

RETIRED = (
    r'APP_THEME_\w+',
    r'\bAppThemeId\b',
    r'app_theme::(get|set|name|init|setRestartHook)\b',
    r'\bofficeMode\b',
    r'MODE_THEME_(SELECT|NOTICE)',
    r'"appTheme"',        # the NVS key: left in place on old Orbs and never read
    r'orb_sim_theme"',    # the simulator's old skin state file (not orb_sim_theme_slug, which is the slug)
)


def strip_comments(text: str) -> str:
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def offences(text: str) -> list:
    code = strip_comments(text)
    return [p for p in RETIRED if re.search(p, code)]


class AppSkinRetiredTest(unittest.TestCase):
    def test_the_check_can_fail(self):
        self.assertEqual(offences('int x = app_theme::get();'), [r'app_theme::(get|set|name|init|setRestartHook)\b'])
        self.assertEqual(offences('// app_theme::get() is gone'), [])
        self.assertEqual(offences('p.getInt("appTheme", 0);'), [r'"appTheme"'])
        self.assertEqual(offences('static inline bool officeMode() { return false; }'), [r'\bofficeMode\b'])

    def test_no_source_file_refers_to_a_retired_skin(self):
        bad = {}
        for p in list(SRC.rglob('*.cpp')) + list(SRC.rglob('*.h')):
            found = offences(p.read_text(encoding='utf-8', errors='replace'))
            if found:
                bad[str(p.relative_to(ROOT))] = found
        self.assertEqual(bad, {})

    def test_the_header_offers_only_the_palette(self):
        code = strip_comments((SRC / 'app' / 'common' / 'app_theme.h').read_text(encoding='utf-8'))
        self.assertIn('const AppPalette &palette();', code)
        self.assertEqual(re.findall(r'^\s*(?:int|void|const char \*)\s+\w+\(', code, re.M), [])


if __name__ == '__main__':
    unittest.main()
