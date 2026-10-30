# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.74 test 2b TAA HOLD: each eye's TAA stamps set back (0.0.74 test 3: default OFF - it never fired), live."""
import pathlib
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings  # noqa: E402
import update37_records  # noqa: E402
from taashake_tests import FakeGame, launcher  # noqa: E402


class TaaHold(unittest.TestCase):
    def setUp(self):
        self.scope, self.text = launcher()
        self.header = (ROOT / 'include/spidy/taa_hold074.hpp').read_text(encoding='utf-8')
        self.probe = (ROOT / 'src/stereo_probe.cpp').read_text(encoding='utf-8')
        self.combat = (ROOT / 'include/spidy/combat38.hpp').read_text(encoding='utf-8')
        self.xr = (ROOT / 'src/game_xr.cpp').read_text(encoding='utf-8')

    def test_off_by_default_live_with_subtext(self):
        self.assertEqual(live_settings.UPDATE38_DEFAULTS.get('taa_hold'), 'off')
        self.assertEqual(live_settings.export_value('taa_hold', 'off'), 0)
        self.assertEqual(live_settings.export_value('taa_hold', 'on'), 1)
        self.assertEqual(live_settings.SETTINGS['taa_hold'][1], 'SpidyTaaHold')
        self.assertIn('live)', live_settings.SETTINGS['taa_hold'][2])
        self.assertIn(('taa_hold', ('OFF', 'ON')), update37_records.V0072_OPTIONS)
        self.assertIn('"taa_hold", "TAA HOLD", "TAAH", "ON sets back the TAA stamps; it never fired, so OFF by '
                      'default; live."', self.combat)
        self.assertIn('setHero007(v, combat38::taaHold, 5504)', self.xr)
        self.assertIn('SpidyTaaHoldSet(combat38::taaHoldOn(c38(combat38::taaHold))', self.xr)

    def test_only_the_job_is_written(self):
        self.assertIn('std::atomic<bool> taaHoldOn{false}', self.probe)
        self.assertIn('taaHoldJob(i, view, static_cast<uint8_t*>(settings), reinterpret_cast<uintptr_t>(job))', self.probe)
        body = self.probe.split('void taaHoldJob(')[1].split('\nvoid copyJob(')[0]
        self.assertNotIn('memcpy(reinterpret_cast<void*>(v', body)  # the view is never written
        self.assertIn('job + th::jobStamp', body)
        self.assertIn('holdAge = 16', self.header)

    def test_snapshot_and_line(self):
        self.assertEqual(self.scope['TAA_HOLD_BYTES'], 120)
        self.assertIn("'SpidyTaaHoldData'", self.text)
        raw = struct.pack(self.scope['TAA_HOLD_FORMAT'], 0x444c4854, 1, 120, 1, 100, 100, 98, 97, 99, 98, 1, 2,
                          5000, 5000, 16, 16, 16, 16, 0, 7)
        h = self.scope['taa_hold_snapshot'](FakeGame(raw), 1)
        self.assertEqual(h['held'], [98, 97])
        self.assertTrue(h['on'])
        line = self.scope['taa_hold_line'](h)
        self.assertIn('TAA HOLD ON', line)
        self.assertIn('L 99% R 98%', line)
        self.assertIsNone(self.scope['taa_hold_snapshot'](FakeGame(raw), 0))


if __name__ == '__main__':
    unittest.main()
