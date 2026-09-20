import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'tools' / 'build_all_themes.py'


def run(*args) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, str(SCRIPT), *map(str, args)], capture_output=True, text=True)


class BuildAllThemesTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.src = Path(self._tmp.name) / 'theme_assets'
        self.out = Path(self._tmp.name) / 'themes'
        self.src.mkdir()

    def tearDown(self):
        self._tmp.cleanup()

    def theme(self, folder: str, yaml: str | None = None) -> Path:
        d = self.src / folder
        d.mkdir()
        (d / 'theme.yaml').write_text(yaml if yaml is not None else f'slug: {folder}\nname: {folder.title()}\n')
        return d

    def installed(self) -> list[str]:
        return sorted(p.parent.name for p in self.out.glob('*/_installed'))

    def test_builds_every_theme_it_finds_however_many_there_are(self):
        for name in ('alpha', 'bravo', 'charlie', 'delta'):
            self.theme(name)
        r = run('--src', self.src, '--out', self.out)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        self.assertEqual(self.installed(), ['alpha', 'bravo', 'charlie', 'delta'])

    def test_a_theme_added_later_is_picked_up_with_no_change_to_the_tool(self):
        self.theme('alpha')
        self.assertEqual(run('--src', self.src, '--out', self.out).returncode, 0)
        self.theme('newcomer')
        self.assertEqual(run('--src', self.src, '--out', self.out).returncode, 0)
        self.assertEqual(self.installed(), ['alpha', 'newcomer'])

    def test_a_folder_without_theme_yaml_is_skipped_not_an_error(self):
        self.theme('alpha')
        (self.src / 'notes').mkdir()
        (self.src / 'README.md').write_text('not a theme')
        r = run('--src', self.src, '--out', self.out)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        self.assertEqual(self.installed(), ['alpha'])
        self.assertIn('notes', r.stdout)            # said out loud, so a typo'd filename is noticed

    def test_one_broken_theme_does_not_stop_the_others_but_the_exit_code_says_so(self):
        self.theme('alpha')
        self.theme('broken', 'slug: broken\nname: One\nname: Two\n')     # a duplicate key is a build error
        self.theme('charlie')
        r = run('--src', self.src, '--out', self.out)
        self.assertEqual(r.returncode, 1)
        self.assertEqual(self.installed(), ['alpha', 'charlie'])
        self.assertIn('broken', r.stdout + r.stderr)

    def test_two_folders_claiming_one_slug_are_refused_rather_than_overwriting(self):
        self.theme('alpha')
        self.theme('copy-of-alpha', 'slug: alpha\nname: Copy\n')
        r = run('--src', self.src, '--out', self.out)
        self.assertEqual(r.returncode, 1)
        self.assertIn('alpha', r.stdout + r.stderr)
        self.assertEqual(self.installed(), ['alpha'])
        self.assertIn('Alpha', (self.out / 'alpha' / 'theme.json').read_text())   # the first one stands

    def test_names_on_the_command_line_build_only_those(self):
        for name in ('alpha', 'bravo', 'charlie'):
            self.theme(name)
        r = run('--src', self.src, '--out', self.out, 'bravo')
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        self.assertEqual(self.installed(), ['bravo'])

    def test_an_unknown_name_is_a_usage_error_and_builds_nothing(self):
        self.theme('alpha')
        r = run('--src', self.src, '--out', self.out, 'alpha', 'nope')
        self.assertEqual(r.returncode, 2)
        self.assertIn('nope', r.stderr)
        self.assertEqual(self.installed(), [])

    def test_no_themes_at_all_is_an_error(self):
        r = run('--src', self.src, '--out', self.out)
        self.assertEqual(r.returncode, 2)

    def test_it_builds_the_themes_this_repo_actually_has(self):
        r = run('--out', self.out)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        on_disk = sorted(p.parent.name for p in (ROOT / 'src' / 'theme_assets').glob('*/theme.yaml'))
        self.assertTrue(on_disk, 'the repo should have at least one theme')
        self.assertEqual(self.installed(), on_disk)


if __name__ == '__main__':
    unittest.main()
