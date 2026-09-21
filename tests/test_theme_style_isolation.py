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
    # theme.json is the theme's own file rather than a screen's: its manifest (apps, names, asset list), its shared font
    # map, and its palette state (s_palette also covers s_paletteMode).
    'theme.json': ('s_apps', 's_names', 's_asset', 's_fontMap', 's_palette', 's_roleDefaults'),
}

# The one sanctioned way for a file to touch every screen: theme_palette::apply_role_defaults gives each colour option its
# role default before the screen files are read, so a theme's own value wins. It is the shared path, and the dumper tests
# (tests/test_theme_palette.py) pin what it writes. Any other write to another screen's state is still refused.
_ROLE_DEFAULTS_CALL = re.compile(r'theme_palette::apply_role_defaults\([^;]*\);')


def blocks() -> dict:
    src = STYLE.read_text(encoding='utf-8')
    body = src[src.index('void load() {'):src.index('const Clock &clock()')]
    parts = re.split(r'read_style_json\(slug, "([a-z_.]+)", doc\)', body)
    out = {}
    for i in range(1, len(parts), 2):
        # up to the next read_style_json(...): comments removed, they mention other screens' state. A file read twice
        # (theme.json: its palette first, then everything else) is ONE block: keying by name alone let the second chunk
        # replace the first, so the palette block was never checked.
        chunk = re.sub(r'//[^\n]*', '', parts[i + 1].split('read_style_json')[0])
        out[parts[i]] = out.get(parts[i], '') + chunk
    return out


def stray_writes(name: str, chunk: str) -> list:
    """The state a style file's block touches that is not its own, apart from the sanctioned role-defaults call, which
    only theme.json (where the palette lives) may make."""
    if name == 'theme.json':
        chunk = _ROLE_DEFAULTS_CALL.sub('', chunk)
    written = set(re.findall(r'\bs_[A-Za-z]+', chunk))
    return sorted(w for w in written if not w.startswith(OWNS[name]))


class StyleIsolationTest(unittest.TestCase):
    def test_every_style_file_is_covered(self):
        self.assertEqual(sorted(blocks()), sorted(OWNS),
                         'a style file was added or removed: update OWNS in this test')

    def test_each_style_file_writes_only_its_own_screen(self):
        for name, chunk in blocks().items():
            self.assertEqual(stray_writes(name, chunk), [], f'{name} writes state that belongs to another screen')

    def test_the_role_defaults_call_is_the_only_exemption(self):
        call = 'theme_palette::apply_role_defaults(s_palette, s_clock, s_radar, s_weather, s_ticker, s_menu, s_settings, s_splash, s_intel);'
        self.assertEqual(stray_writes('theme.json', call), [])
        self.assertEqual(stray_writes('theme.json', call + ' s_settings.selOpa = 10;'), ['s_settings'])
        # and only theme.json may make it: the same call inside another screen's block is a cross-screen write
        self.assertEqual(stray_writes('radar_style.json', call),
                         ['s_clock', 's_intel', 's_menu', 's_palette', 's_settings', 's_splash', 's_ticker', 's_weather'])

    def test_a_file_read_twice_is_one_block(self):
        body = STYLE.read_text(encoding='utf-8')
        self.assertGreaterEqual(body.count('read_style_json(slug, "theme.json", doc)'), 2,
                                'theme.json is read twice in load(); if that changes, this test can go')
        self.assertIn('s_palette', blocks()['theme.json'])      # the first read (the palette) is in the block


if __name__ == '__main__':
    unittest.main()
