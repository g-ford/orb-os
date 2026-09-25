"""An unmet prerequisite fails the run unless skipping was asked for.

Several tests build a small program against the real firmware sources, or bake a theme's typefaces, and used to
report "skipped" when they could not: a missing tool, but also a compile error in the firmware. unittest prints
`OK (skipped=N)` and exits 0 for that, so a firmware change that broke a dumper-backed test looked green. Now the
default is that it fails, and skipping is a decision made with ORB_ALLOW_SKIPS=1.
"""
import os
import sys
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import skips  # noqa: E402


class UnmetTest(unittest.TestCase):
    def test_an_unmet_prerequisite_fails_by_default(self):
        with mock.patch.dict(os.environ, {}, clear=False):
            os.environ.pop('ORB_ALLOW_SKIPS', None)
            e = skips.unmet('lv_font_conv is needed')
        self.assertIsInstance(e, AssertionError)
        self.assertNotIsInstance(e, unittest.SkipTest)
        self.assertIn('lv_font_conv is needed', str(e))
        self.assertIn('ORB_ALLOW_SKIPS=1', str(e))          # says how to skip on purpose

    def test_it_skips_only_when_asked_to(self):
        with mock.patch.dict(os.environ, {'ORB_ALLOW_SKIPS': '1'}):
            e = skips.unmet('no compiler')
        self.assertIsInstance(e, unittest.SkipTest)

    def test_zero_and_empty_do_not_count_as_asking(self):
        for value in ('', '0'):
            with self.subTest(value=value), mock.patch.dict(os.environ, {'ORB_ALLOW_SKIPS': value}):
                self.assertNotIsInstance(skips.unmet('x'), unittest.SkipTest)

    def test_the_failure_is_a_type_callers_can_catch_to_clean_up(self):
        self.assertTrue(issubclass(skips.Unmet, AssertionError))
        with mock.patch.dict(os.environ, {'ORB_ALLOW_SKIPS': ''}):
            self.assertIsInstance(skips.unmet('x'), skips.Unmet)


class NoBareSkipsTest(unittest.TestCase):
    """The rule only holds if nothing bypasses it: every skip in this directory goes through skips.unmet."""

    def test_no_test_file_skips_directly(self):
        here = Path(__file__).resolve().parent
        offenders = []
        for p in sorted(here.glob('test_*.py')):
            if p.name == Path(__file__).name:
                continue
            text = p.read_text(encoding='utf-8')
            for token in ('raise unittest.SkipTest', 'self.skipTest(', '@unittest.skip'):
                if token in text:
                    offenders.append(f'{p.name}: {token}')
        self.assertEqual(offenders, [], 'route these through skips.unmet(...)')


if __name__ == '__main__':
    unittest.main()
