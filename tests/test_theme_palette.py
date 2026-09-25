import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import gen_elegant_theme as gen  # noqa: E402

BUILT_IN = {'bg': '0x0D1220', 'primary': '0x8CB8FF', 'secondary': '0xF0B4C8', 'text': '0xEDF1FA',
            'muted': '0x8891A6', 'dim': '0x5F7BA6', 'hairline': '0x1F2A42', 'panel': '0x141A2B',
            'highlight': '0x26314A', 'onPrimary': '0x0A1020', 'alert': '0xE5484D'}
PORTAL_DERIVED = {'bg': '0x0B0E11', 'primary': '0xFF9A1F', 'secondary': '0x82CEFF', 'text': '0xFFFFFF',
                  'muted': '0x919394', 'dim': '0x794D17', 'hairline': '0x3C2A14', 'panel': '0x1A1C1F',
                  'highlight': '0x483115', 'onPrimary': '0x0B0E11', 'alert': '0xE5484D'}
PORTAL = """slug: sample
palette:
  bg: 0x0B0E11
  primary: 0xFF9A1F
  secondary: 0x82CEFF
  text: 0xFFFFFF
"""


def roles(state):
    return {k: v for k, v in state['palette'].items() if k not in ('mode', 'roleDefaults')}


class DumperCase(unittest.TestCase):
    """Builds tools/dump_theme_defaults.cpp once (it links the real theme_style.cpp) and runs it on throwaway themes,
    so what is asserted is what the firmware computes."""

    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        try:
            cls.dumper = gen.build_dumper(Path(cls._tmp.name) / 'dump_theme_defaults')
        except gen.GenError as e:
            cls._tmp.cleanup()
            raise unittest.SkipTest(str(e).splitlines()[0])

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def dump(self, yaml_text=None):
        """What the firmware resolves `yaml_text` to. None means no theme at all: the built-in look."""
        if yaml_text is None:
            return gen.run_dumper(self.dumper)
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / 'sample'
            src.mkdir()
            (src / 'theme.yaml').write_text(yaml_text, encoding='utf-8')
            return gen.run_dumper(self.dumper, gen.build_folder(src, Path(tmp) / 'built'))


class PaletteModeTest(DumperCase):
    def test_no_theme_is_the_built_in_palette(self):
        state = self.dump()
        self.assertEqual(state['palette']['mode'], 'builtin')
        self.assertEqual(roles(state), BUILT_IN)

    def test_a_theme_with_no_palette_is_legacy_and_keeps_the_built_in_roles(self):
        state = self.dump('slug: sample\nradar:\n  rangeKm: 30\n')
        self.assertEqual(state['palette']['mode'], 'legacy')
        self.assertEqual(roles(state), BUILT_IN)

    def test_a_palette_puts_a_theme_in_palette_mode_and_derives_the_rest(self):
        state = self.dump(PORTAL)
        self.assertEqual(state['palette']['mode'], 'theme')
        self.assertEqual(roles(state), PORTAL_DERIVED)
        self.assertIs(state['palette']['roleDefaults'], True)

    def test_role_defaults_false_is_carried(self):
        self.assertIs(self.dump(PORTAL + 'roleDefaults: false\n')['palette']['roleDefaults'], False)


class RoleReferenceTest(DumperCase):
    def test_a_role_reference_resolves_to_the_role_colour(self):
        state = self.dump(PORTAL + 'radar:\n  sweepColor: $secondary\n  sweepLeadColor: $muted\nticker:\n  upColor: $alert\n')
        self.assertEqual(state['radar']['sweepColor'], '0x82CEFF')
        self.assertEqual(state['radar']['sweepLeadColor'], '0x919394')      # a derived role
        self.assertEqual(state['ticker']['upColor'], '0xE5484D')

    def test_explicit_hex_still_wins_and_nested_references_resolve(self):
        state = self.dump(PORTAL + 'radar:\n  sweepColor: 0x123456\n  rtext:\n    - {color: $text}\n    - {color: $primary}\n')
        self.assertEqual(state['radar']['sweepColor'], '0x123456')
        self.assertEqual(state['radar']['rtext'][0]['color'], '0xFFFFFF')
        self.assertEqual(state['radar']['rtext'][1]['color'], '0xFF9A1F')

    def test_text_that_starts_with_a_dollar_is_not_touched(self):
        state = self.dump(PORTAL + 'clock:\n  text1: {fmt: "$5 {name}"}\n')
        self.assertEqual(state['clock']['text1']['fmt'], '$5 {name}')

    def test_a_reference_in_a_theme_with_no_palette_never_reaches_the_firmware(self):
        # the builder refuses it (Task 3); this is what a hand-edited card would do: the read ignores a wrong-typed value
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / 'sample'
            src.mkdir()
            (src / 'theme.yaml').write_text('slug: sample\nradar:\n  rangeKm: 30\n', encoding='utf-8')
            built = gen.build_folder(src, Path(tmp) / 'built')
            (built / 'radar_style.json').write_text('{"sweepColor":"$primary","rangeKm":30}', encoding='utf-8')
            state = gen.run_dumper(self.dumper, built)
        self.assertEqual(state['palette']['mode'], 'legacy')
        self.assertNotEqual(state['radar']['sweepColor'], '0x8CB8FF')      # legacy: the string is left alone and ignored


class HandEditedThemeTest(DumperCase):
    """Review focus 3: a palette that is nonsense must not crash the firmware."""

    def dump_theme_json(self, theme_json):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / 'sample'
            src.mkdir()
            (src / 'theme.yaml').write_text('slug: sample\n', encoding='utf-8')
            built = gen.build_folder(src, Path(tmp) / 'built')
            (built / 'theme.json').write_text(theme_json, encoding='utf-8')
            return gen.run_dumper(self.dumper, built)

    def test_a_palette_that_is_not_an_object_is_ignored(self):
        self.assertEqual(self.dump_theme_json('{"slug":"sample","palette":7}')['palette']['mode'], 'legacy')

    def test_wrong_typed_roles_fall_back_and_do_not_crash(self):
        state = self.dump_theme_json('{"slug":"sample","palette":{"bg":"red","primary":-1,"text":1.5e40}}')
        self.assertEqual(state['palette']['mode'], 'theme')
        self.assertEqual(state['palette']['bg'], BUILT_IN['bg'])          # not a number: the built-in one stands
        self.assertEqual(state['palette']['text'], BUILT_IN['text'])


KEEP = ('apps', 'names', 'clock', 'radar', 'weather', 'ticker', 'splash', 'intel')


def sections(state):
    return {k: state[k] for k in KEEP}


class RoleDefaultsTest(DumperCase):
    """A theme that is only a palette gets a designed look: every colour option it leaves out takes its role."""

    def test_the_options_a_palette_theme_leaves_out_take_their_role(self):
        s = self.dump(PORTAL)
        self.assertEqual(s['radar']['sweepColor'], '0xFF9A1F')              # primary
        self.assertEqual(s['radar']['sweepLeadColor'], '0xFFFFFF')          # text
        self.assertEqual(s['radar']['blipAltGround'], '0x919394')           # muted
        self.assertEqual(s['radar']['blipAltLow'], '0x82CEFF')              # secondary
        self.assertEqual(s['radar']['blipAltMid'], '0xC1B48F')              # halfway from secondary to primary
        self.assertEqual(s['radar']['blipAltHigh'], '0xFF9A1F')
        self.assertEqual(s['radar']['blipAltCruise'], '0xFFC279')           # primary 40% toward text
        self.assertEqual(s['radar']['blipAltJet'], '0xFFFFFF')
        self.assertEqual(s['radar']['card']['color'], '0x1A1C1F')           # panel
        self.assertEqual([t['color'] for t in s['radar']['rtext']], ['0xFFFFFF', '0xFF9A1F', '0x82CEFF', '0x919394'])
        self.assertEqual(s['ticker']['upColor'], '0xFF9A1F')
        self.assertEqual(s['ticker']['downColor'], '0xE5484D')              # alert
        self.assertEqual(s['ticker']['flatColor'], '0x919394')
        self.assertEqual(s['ticker']['priceColor'], '0xFFFFFF')
        self.assertEqual(s['intel']['staleColor'], '0xE5484D')
        self.assertEqual(s['intel']['selBarColor'], '0x483115')
        self.assertEqual(s['weather']['ringColor'], '0x3C2A14')             # hairline
        self.assertEqual(s['weather']['credit']['color'], '0x919394')
        self.assertEqual(s['clock']['bg'], '0x0B0E11')
        self.assertEqual(s['clock']['windRingFill'], '0xFF9A1F')
        self.assertEqual(s['splash']['theme']['color'], '0xFF9A1F')
        self.assertEqual(s['splash']['network']['color'], '0x919394')

    def test_the_optional_overlay_tint_follows_the_background(self):
        # found by the Task 5 review: the one colour option left at a compiled hue. Off unless a theme enables it, but a
        # light palette that does would otherwise get a black tint.
        self.assertEqual(self.dump(PORTAL)['radar']['overlayColor'], '0x0B0E11')
        self.assertEqual(self.dump(PORTAL.replace('0x0B0E11', '0xF4F5F7'))['radar']['overlayColor'], '0xF4F5F7')

    def test_a_drawn_face_gets_the_three_hands_ticking(self):
        s = self.dump(PORTAL)
        hands = s['clock']['hands']
        self.assertEqual([hands[h]['show'] for h in ('hour', 'minute', 'second')], [True, True, True])
        self.assertEqual([hands[h]['show'] for h in ('static1', 'static2')], [False, False])
        self.assertEqual(hands['order'], [0, 1, 2])
        self.assertIs(s['clock']['secondSweep'], False)
        self.assertIs(s['splash']['theme']['show'], True)                   # the theme's name is on the splash

    def test_a_theme_can_hide_a_hand_and_the_role_default_does_not_bring_it_back(self):
        # roleDefaults turns the three hands on so a drawn face has them; a theme's own show:false wins, and a hand that is
        # not shown is never drawn (compose_custom skips it before it looks for a sprite: tests/test_clock_wiring.py)
        hands = self.dump(PORTAL + 'clock:\n  hands:\n    second: {show: false}\n')['clock']['hands']
        self.assertIs(hands['second']['show'], False)
        self.assertEqual([hands[h]['show'] for h in ('hour', 'minute')], [True, True])
        hands = self.dump(PORTAL)['clock']['hands']                     # said nothing: all three on, as a drawn face needs
        self.assertEqual([hands[h]['show'] for h in ('hour', 'minute', 'second')], [True, True, True])

    def test_what_a_theme_states_beats_the_role_default(self):
        s = self.dump(PORTAL + 'radar:\n  sweepColor: 0x123456\nticker:\n  upColor: $alert\n')
        self.assertEqual(s['radar']['sweepColor'], '0x123456')
        self.assertEqual(s['ticker']['upColor'], '0xE5484D')
        self.assertEqual(s['radar']['sweepLeadColor'], '0xFFFFFF')          # the others still default

    def test_role_defaults_false_keeps_every_compiled_default(self):
        # Review focus 1: a theme that only wants the palette's $role references must not be restyled.
        legacy = sections(self.dump('slug: sample\n'))
        kept = sections(self.dump(PORTAL + 'roleDefaults: false\n'))
        self.assertEqual(kept, legacy)

    def test_role_references_work_with_role_defaults_off(self):
        s = self.dump(PORTAL + 'roleDefaults: false\nradar:\n  sweepColor: $secondary\n')
        self.assertEqual(s['radar']['sweepColor'], '0x82CEFF')
        self.assertEqual(s['radar']['sweepLeadColor'], sections(self.dump('slug: sample\n'))['radar']['sweepLeadColor'])

    def test_a_legacy_theme_is_untouched_by_role_defaults(self):
        legacy = self.dump('slug: sample\nradar:\n  sweepColor: 0x0A0B0C\n')
        self.assertEqual(legacy['palette']['mode'], 'legacy')
        self.assertEqual(legacy['radar']['sweepColor'], '0x0A0B0C')
        self.assertNotEqual(legacy['radar']['sweepLeadColor'], '0xEDF1FA')   # not the built-in text colour: still compiled


class BuiltInLookTest(DumperCase):
    def test_no_theme_draws_the_built_in_palette_everywhere(self):
        s = self.dump()
        self.assertEqual(s['palette']['mode'], 'builtin')
        self.assertEqual(s['radar']['sweepColor'], '0x8CB8FF')
        self.assertEqual(s['radar']['blipAltGround'], '0x8891A6')
        self.assertEqual(s['ticker']['downColor'], '0xE5484D')
        self.assertEqual(s['clock']['bg'], '0x0D1220')
        self.assertEqual([s['clock']['hands'][h]['show'] for h in ('hour', 'minute', 'second')], [True, True, True])
        self.assertIs(s['splash']['theme']['show'], True)


if __name__ == '__main__':
    unittest.main()
