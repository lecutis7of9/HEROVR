"""Update 34.1: every option bit the launcher sends is one the VR module knows.

Update 34 added -EngineTaa (0x800000) to run_game_vr.py, but SpidyXrStart still refused
options > 0x7fffff, so a -EngineTaa launch started no VR (Game XR start: 1001).
"""
import pathlib
import re
import struct
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class LauncherOptionsTests(unittest.TestCase):
    def setUp(self):
        self.launcher = (ROOT / 'tools/run_game_vr.py').read_text(encoding='utf-8')
        self.xr = (ROOT / 'src/game_xr.cpp').read_text(encoding='utf-8')

    def known(self):
        m = re.search(r'constexpr uint32_t knownOptions = (0x[0-9a-fA-F]+);', self.xr)
        self.assertIsNotNone(m, 'knownOptions missing from game_xr.cpp')
        return int(m.group(1), 16)

    def test_every_sent_bit_is_known(self):
        start = self.launcher.index("config = struct.pack('<4I7Q2IfI'")
        end = self.launcher.index('runtime_path(manifest)', start)
        bits = [int(b, 16) for b in re.findall(r'\((0x[0-9a-fA-F]+) if ', self.launcher[start:end])]
        bits.append(0x800000)  # ENGINE_TAA_OPTION
        self.assertIn('ENGINE_TAA_OPTION if a.engine_taa', self.launcher[start:end])
        bits.append(0x1000000)  # SM1STYLE_OPTION (0.0.74 test 10)
        self.assertIn("SM1STYLE_OPTION if getattr(a, 'sm1style', False)", self.launcher[start:end])
        for bit in bits:
            self.assertEqual(bit & ~self.known(), 0, hex(bit))

    def test_unknown_bits_are_skipped_not_refused(self):
        self.assertNotRegex(self.xr, r'config\.options\s*>\s*0x')
        self.assertIn('config.options &= knownOptions;', self.xr)

    def test_options_offset(self):
        self.assertEqual(struct.calcsize('<4I7Q2If'), 84)
        self.assertIn("OPTIONS_OFFSET = struct.calcsize('<4I7Q2If')", self.launcher)
        self.assertIn('ENGINE_TAA_OPTION = 0x800000', self.launcher)

    def test_engine_taa_retry(self):
        self.assertIn('if code == 1001 and a.engine_taa:', self.launcher)


if __name__ == '__main__':
    unittest.main()
