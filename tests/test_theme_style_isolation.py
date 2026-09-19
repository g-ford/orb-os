"""One screen's style file must only ever change that screen.

theme_style.cpp had `s_settings.selOpa = ...` inside the block that reads radar_style.json, so a
radar theme dimmed the Settings screen's selected row and settings_style.json's own selOpa was
never read. A comment cannot stop a copy-paste like that; this can.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STYLE = ROOT / 'src' / 'theme' / 'core' / 'theme_style.cpp'

# the state each file is allowed to write (prefix match, so s_asset covers s_assetN / s_assetsHash)
OWNS = {
    'clock_style.json': ('s_clock',),
    'ticker_style.json': ('s_ticker',),
    'weather_style.json': ('s_weather',),
    'radar_style.json': ('s_radar',),
    'settings_style.json': ('s_settings',),
    'splash_style.json': ('s_splash',),
    'intel_style.json': ('s_intel',),
    'menu_style.json': ('s_menu',),
    'theme.json': ('s_apps', 's_names', 's_asset'),
}


def blocks() -> dict:
    src = STYLE.read_text(encoding='utf-8')
    body = src[src.index('void load() {'):src.index('const Clock &clock()')]
    parts = re.split(r'read_style_json\(slug, "([a-z_.]+)", doc\)', body)
    out = {}
    for i in range(1, len(parts), 2):
        # up to the next read_style_json(...): comments removed, they mention other screens' state
        out[parts[i]] = re.sub(r'//[^\n]*', '', parts[i + 1].split('read_style_json')[0])
    return out


class StyleIsolationTest(unittest.TestCase):
    def test_every_style_file_is_covered(self):
        self.assertEqual(sorted(blocks()), sorted(OWNS),
                         'a style file was added or removed: update OWNS in this test')

    def test_each_style_file_writes_only_its_own_screen(self):
        for name, chunk in blocks().items():
            written = set(re.findall(r'\bs_[A-Za-z]+', chunk))
            stray = sorted(w for w in written if not w.startswith(OWNS[name]))
            self.assertEqual(stray, [], f'{name} writes state that belongs to another screen')


if __name__ == '__main__':
    unittest.main()
