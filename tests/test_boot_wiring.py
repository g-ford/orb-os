"""main.cpp's boot order is what makes the flash cache exist at all. It cannot be run on the desktop, so this
checks the one thing that went missing once and nothing else noticed: the call to the boot bake."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / 'src' / 'main.cpp'


def code() -> str:
    """main.cpp without // comments, so a call that is only mentioned in a comment does not count."""
    return re.sub(r'//[^\n]*', '', MAIN.read_text(encoding='utf-8'))


class BootBakeWiringTest(unittest.TestCase):
    """8b63b1d ("Refactor theme assets into core and graphics folders") removed the boot bake's only caller,
    theme_manager::ensureDefaultBaked(), without a replacement. Nothing wrote the flash cache from then on, so
    every theme drew from the SD card and the shared fonts / fonts.map path could never run on a device."""

    def test_the_boot_bake_is_called_once_the_progress_hook_is_set_and_before_the_splash(self):
        src = code()
        progress = src.index('theme_art::set_progress(')
        bake = src.find('theme_art::bake_active_theme(')
        self.assertNotEqual(bake, -1, 'nothing in main.cpp calls theme_art::bake_active_theme(): the flash cache is never written')
        splash = src.index('ui_splash_show();', progress)
        self.assertTrue(progress < bake < splash,
                        'the bake must run after set_progress() (so its progress shows) and before the splash')

    def test_the_bake_runs_after_the_art_partition_is_mapped(self):
        src = code()
        self.assertLess(src.index('theme_art::begin('), src.index('theme_art::bake_active_theme('),
                        'bake_active_theme() needs the partition mapped by theme_art::begin() first')

    def test_fonts_load_after_the_bake(self):
        src = code()
        self.assertLess(src.index('theme_art::bake_active_theme('), src.index('theme_font::begin('),
                        'theme_font::begin() reads only what the bake wrote, so it must come after it')


if __name__ == '__main__':
    unittest.main()
