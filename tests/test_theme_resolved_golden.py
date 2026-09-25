import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import gen_elegant_theme as gen  # noqa: E402
import resolved_golden  # noqa: E402
import sys as _sys
from pathlib import Path as _Path
_sys.path.insert(0, str(_Path(__file__).resolve().parent))   # `import skips` works however the tests are run
import skips  # noqa: E402

THEMES = ROOT / 'src' / 'theme_assets'
GOLDEN = ROOT / 'tests' / 'golden'


class DiffPathsTest(unittest.TestCase):
    def test_it_names_the_path_and_both_values(self):
        a = {'radar': {'sweepColor': '0x111111', 'zones': [{'r': 1}]}, 'apps': {'clock': True}}
        b = {'radar': {'sweepColor': '0x222222', 'zones': [{'r': 2}]}, 'apps': {'clock': True}}
        self.assertEqual(resolved_golden.diff_paths(a, b),
                         ["radar.sweepColor: '0x111111' -> '0x222222'", 'radar.zones[0].r: 1 -> 2'])

    def test_equal_values_have_no_differences(self):
        self.assertEqual(resolved_golden.diff_paths({'a': [1, {'b': 2}]}, {'a': [1, {'b': 2}]}), [])

    def test_a_missing_key_is_a_difference(self):
        self.assertEqual(resolved_golden.diff_paths({'a': 1}, {}), ['a: 1 -> <missing>'])


class ResolvedGoldenTest(unittest.TestCase):
    """A shipped theme must resolve to exactly the values it resolved to when the golden was captured. Compiled
    once per class: building the dumper is the slow part."""

    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        try:
            cls.dumper = gen.build_dumper(Path(cls._tmp.name) / 'dump_theme_defaults')
        except gen.GenError as e:
            cls._tmp.cleanup()
            raise skips.unmet(str(e).splitlines()[0])

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def check(self, slug):
        got = resolved_golden.resolve_theme(THEMES / slug, self.dumper)
        want = json.loads((GOLDEN / f'resolved_{slug}.json').read_text(encoding='utf-8'))
        self.assertEqual(resolved_golden.diff_paths(want, got), [],
                         f'{slug} resolves differently from the golden captured before palettes existed')

    def test_elegant(self):
        self.check('elegant')

    def test_fallout(self):
        self.check('fallout')

    def test_portal(self):
        self.check('portal')


if __name__ == '__main__':
    unittest.main()
