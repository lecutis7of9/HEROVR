# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.74 test 2b TAA DIAGNOSTICS: the per-frame per-eye TAA record (default OFF) and its report line."""
import pathlib
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings  # noqa: E402
from taashake_tests import FakeGame, launcher  # noqa: E402


class TaaDiagnostics(unittest.TestCase):
    def setUp(self):
        self.scope, self.text = launcher()
        self.header = (ROOT / 'include/spidy/taa_diag074.hpp').read_text(encoding='utf-8')
        self.probe = (ROOT / 'src/stereo_probe.cpp').read_text(encoding='utf-8')

    def test_layout_matches_native(self):
        self.assertEqual(self.scope['TAA_DIAG_BYTES'], 912)
        self.assertIn('static_assert(sizeof(Diag) == 912)', self.header)
        self.assertIn('static_assert(sizeof(Frame) == 52)', self.header)
        self.assertIn('__declspec(dllexport) spidy::taa_diag::Diag SpidyTaaDiag', self.probe)
        self.assertIn("'SpidyTaaDiag'", self.text)

    def test_default_off_and_live(self):
        self.assertEqual(live_settings.UPDATE38_DEFAULTS.get('taa_diagnostics'), 'off')
        self.assertEqual(live_settings.UPDATE38_DEFAULTS.get('crime_gps'), 'on')
        self.assertIn('taa_diagnostics', live_settings.SETTINGS)

    def test_snapshot_and_line(self):
        ring = []
        for k in range(16):
            ring += [100 + k, .0002, -.0001, .0004, -.0002, .0004, -.0002, 0, 0, 0, 0, 1.0, 1.0]
        raw = struct.pack(self.scope['TAA_DIAG_FORMAT'], 0x44415454, 1, 912, 1, 16, 16, 16, 15, 15, 16, 0, 16, 0,
                          2.0, 2.0, 3, 0, *ring, 7)
        d = self.scope['taa_diag_snapshot'](FakeGame(raw), 1)
        self.assertEqual(d['frames_recorded'], 16)
        self.assertEqual(d['stored_over_sent'], [2.0, 2.0])
        self.assertEqual(len(d['last_frames']), 16)
        self.assertEqual(d['last_frames'][0]['frame'], 103)  # oldest first, from head
        self.assertEqual(d['last_frames'][0]['right']['weight'], 1.0)
        line = self.scope['taa_diag_line'](d)
        self.assertIn('history thrown away L 100% R 100%', line)
        self.assertIn('per-eye history buffers', line)
        self.assertIn('2.0x/2.0x', line)
        off = bytearray(raw)
        struct.pack_into('<I', off, 12, 0)
        self.assertIsNone(self.scope['taa_diag_snapshot'](FakeGame(off), 1))
        self.assertIsNone(self.scope['taa_diag_snapshot'](FakeGame(raw), 0))


if __name__ == '__main__':
    unittest.main()
