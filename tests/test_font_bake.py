import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import font_bake  # noqa: E402

STUB = ROOT / 'tests' / 'stub_lv_font_conv.py'


class BakeFaceTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = Path(self._tmp.name)
        self.log = self.tmp / 'calls.log'
        env = mock.patch.dict(os.environ, {'LV_FONT_CONV': str(STUB), 'STUB_LOG': str(self.log)})
        env.start()
        self.addCleanup(env.stop)
        self.face = self.tmp / 'Face.ttf'
        self.face.write_bytes(b'TTF')

    def test_the_environment_override_wins(self):
        self.assertEqual(font_bake.converter(), [str(STUB)])

    def test_a_face_is_baked_with_the_arguments_the_firmware_needs(self):
        out = self.tmp / 'out.bin'
        font_bake.bake_face(self.face, 22, out)
        self.assertEqual(out.read_bytes(), b'STUB Face.ttf 22\n')
        args = self.log.read_text().split()
        for want in ('--bpp', '4', '--no-compress', '--format', 'bin', '--size', '22'):
            self.assertIn(want, args)
        self.assertEqual(args[args.index('--range') + 1], font_bake.DEFAULT_RANGES)

    def test_ranges_can_be_replaced(self):
        font_bake.bake_face(self.face, 16, self.tmp / 'o.bin', ranges='0x20-0x7E')
        args = self.log.read_text().split()
        self.assertEqual(args[args.index('--range') + 1], '0x20-0x7E')

    def test_a_failing_converter_raises_with_its_message(self):
        with mock.patch.dict(os.environ, {'STUB_FAIL': '1'}):
            with self.assertRaises(font_bake.FontBakeError) as cm:
                font_bake.bake_face(self.face, 16, self.tmp / 'o.bin')
        self.assertIn('asked to fail', str(cm.exception))

    def test_a_missing_converter_raises_instead_of_a_traceback(self):
        with mock.patch.dict(os.environ, {'LV_FONT_CONV': '/nonexistent/lv_font_conv'}):
            with self.assertRaises(font_bake.FontBakeError) as cm:
                font_bake.bake_face(self.face, 16, self.tmp / 'o.bin')
        self.assertIn('lv_font_conv', str(cm.exception))


if __name__ == '__main__':
    unittest.main()
