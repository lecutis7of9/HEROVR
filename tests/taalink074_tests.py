# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.74 test 3 TAA LINK / JITTER GUARD: the eyes' TAA history link record, and no jitter while it is refused."""
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


class TaaLink(unittest.TestCase):
    def setUp(self):
        self.scope, self.text = launcher()
        self.probe = (ROOT / 'src/stereo_probe.cpp').read_text(encoding='utf-8')
        self.combat = (ROOT / 'include/spidy/combat38.hpp').read_text(encoding='utf-8')
        self.xr = (ROOT / 'src/game_xr.cpp').read_text(encoding='utf-8')
        self.header = (ROOT / 'include/spidy/taa_link074.hpp').read_text(encoding='utf-8')

    def test_guard_on_by_default_and_switchable(self):
        self.assertEqual(live_settings.UPDATE38_DEFAULTS.get('jitter_guard'), 'on')
        self.assertEqual(live_settings.export_value('jitter_guard', 'on'), 0)
        self.assertEqual(live_settings.export_value('jitter_guard', 'off'), 1)
        self.assertEqual(live_settings.SETTINGS['jitter_guard'][1], 'SpidyJitterGuard')
        self.assertIn('live)', live_settings.SETTINGS['jitter_guard'][2])
        self.assertIn(('jitter_guard', ('ON', 'OFF')), update37_records.V0072_OPTIONS)
        self.assertIn('{"jitter_guard", "JITTER GUARD", "JITG",', self.combat)
        self.assertIn('setHero007(v, combat38::jitterGuard, 5506)', self.xr)
        self.assertIn('SpidyTaaGuardSet(spidy::taa_link::guardOn(c38(combat38::jitterGuard))', self.xr)

    def test_read_only_and_guard_keeps_taa_running(self):
        body = self.probe.split('void taaLinkJob(')[1].split('\nvoid copyJob(')[0]
        self.assertNotIn('write(', body)
        self.assertNotIn('memcpy', body)
        self.assertIn('taaLinkJob(i, reinterpret_cast<uintptr_t>(job))', self.probe)
        self.assertIn('j = {spidy::taa_link::guardJitter, spidy::taa_link::guardJitter};', self.probe)
        self.assertIn('guardJitter = 1e-6f', self.header)  # not 0: TemporalAa skips on zero (0x2c2dd09)
        self.assertIn('0x2c98290', self.header)

    def test_snapshot_and_line(self):
        self.assertEqual(self.scope['TAA_LINK_BYTES'], 160)
        self.assertIn("'SpidyTaaLinkData'", self.text)
        raw = struct.pack(self.scope['TAA_LINK_FORMAT'], 0x4b4e4c54, 1, 160, 1, 100, 100, 0, 0, 90, 2,
                          0, 5, 0, 95, 0, 98, 0, 2, 4321, 1, 0, 40, 1, 9)
        t = self.scope['taa_link_snapshot'](FakeGame(raw), 1)
        self.assertEqual(t['refused'], [90, 2])
        self.assertEqual(t['ages'][1], [0, 98, 0, 2])
        line = self.scope['taa_link_line'](t)
        self.assertIn('L refused 90%', line)
        self.assertIn('R refused 2%', line)
        self.assertIn('JITTER GUARD ON, 4321 lenses held still', line)
        self.assertIsNone(self.scope['taa_link_snapshot'](FakeGame(raw), 0))


if __name__ == '__main__':
    unittest.main()
