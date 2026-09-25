"""THEME_CAPS is the capability level the firmware reports, and docs/theme-caps.md is the ledger of what each level added
and what an Orb below it does. A capability bumps the constant and adds an entry; this fails when the two disagree, so
the ledger cannot quietly fall behind the way a comment beside the constant could."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / 'src' / 'theme' / 'core' / 'theme_style.h'
LEDGER = ROOT / 'docs' / 'theme-caps.md'

ENTRY = re.compile(r'(?m)^\s{0,3}(\d{1,3})  \S')

# Levels 45 to 48 predate this test and were documented at the fields that added them (search theme_style.h for
# 'THEME_CAPS 47'), not in the ledger. They are an allowance for history, not a pattern: this set must not grow.
KNOWN_GAPS = {45, 46, 47, 48}


def level(header_text: str) -> int:
    return int(re.search(r'constexpr int THEME_CAPS = (\d+);', header_text).group(1))


def newest_entry(ledger_text: str) -> int:
    return max(int(n) for n in ENTRY.findall(ledger_text))


class LedgerTest(unittest.TestCase):
    def test_the_ledger_ends_at_the_current_level(self):
        self.assertEqual(newest_entry(LEDGER.read_text(encoding='utf-8')), level(HEADER.read_text(encoding='utf-8')),
                         'THEME_CAPS was bumped without a ledger entry in docs/theme-caps.md, or the reverse')

    def test_no_new_level_goes_without_an_entry(self):
        text = LEDGER.read_text(encoding='utf-8')
        have = {int(n) for n in ENTRY.findall(text)}
        missing = {n for n in range(1, level(HEADER.read_text(encoding='utf-8')) + 1) if n not in have}
        self.assertEqual(missing, KNOWN_GAPS, 'a level has no ledger entry (the four known gaps are the only allowance)')

    def test_the_check_can_fail(self):
        self.assertEqual(newest_entry('//  53  a\n//  54  b\n'.replace('//', '')), 54)
        self.assertEqual(level('constexpr int THEME_CAPS = 7;'), 7)


if __name__ == '__main__':
    unittest.main()
