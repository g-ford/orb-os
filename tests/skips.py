"""What a test does when something it needs is not there (a compiler, lv_font_conv, Pillow, a built native tree).

The default is to FAIL. Set ORB_ALLOW_SKIPS=1 to skip instead, knowingly. See tests/test_skips.py for why: a skipped
test exits 0, so a compile error in the firmware sources used to look green.

    raise skips.unmet('lv_font_conv (or npx) is needed to bake this theme')      # in place of unittest.SkipTest
"""
import os
import unittest


class Unmet(AssertionError):
    """A prerequisite is missing. A subclass of AssertionError so it is a test failure, and a type of its own so a
    setUpClass that has to clean up first can catch it."""


def allowed() -> bool:
    return os.environ.get('ORB_ALLOW_SKIPS', '') not in ('', '0')


def unmet(reason: str) -> Exception:
    """The exception to raise for an unmet prerequisite: a skip if skipping was asked for, otherwise a failure."""
    if allowed():
        return unittest.SkipTest(reason)
    return Unmet(f'unmet prerequisite: {reason}. Install it, or set ORB_ALLOW_SKIPS=1 to skip these tests knowingly.')
