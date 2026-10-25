# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 34.2: the EYE AA TRAIL, ENGINE TAA jitter and SHADOW DISTANCE records against the native layouts."""
import pathlib
import re
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings  # noqa: E402
import shadow_distance as sd  # noqa: E402


class FakeGame:
    def __init__(self, data):
        self.data = bytes(data)

    def read(self, address, size):
        return self.data[:size]


def launcher():
    """run_game_vr's pure helpers, without importing the Windows-only launcher."""
    text = (ROOT / 'tools/run_game_vr.py').read_text(encoding='utf-8')
    start = text.index('HISTORY_BYTES = ')
    end = text.index('# Update 34.1: SpidyXrStart')
    scope = {'struct': struct}
    exec(text[start:end], scope)  # noqa: S102 - our own source, a slice of plain functions
    return scope, text


class Hotfix342(unittest.TestCase):
    def test_live_settings(self):
        self.assertEqual(live_settings.SETTINGS['trail'][1], 'SpidyEyeTrail')
        self.assertEqual(live_settings.SETTINGS['trail'][0], (0, 100))
        self.assertEqual(live_settings.SETTINGS['taa_jitter'][1], 'SpidyEngineTaaJitter')
        self.assertEqual(live_settings.SETTINGS['taa_jitter_scale'][0], (0, 200))
        self.assertIn('trail', live_settings.START_ONLY)
        self.assertNotIn('taa_jitter', live_settings.START_ONLY + live_settings.PERSISTED)
        self.assertEqual(live_settings.ALIASES['motion_blur'], 'trail')

    def test_exports_listed_and_made(self):
        _, text = launcher()
        native = (ROOT / 'src/stereo_probe.cpp').read_text() + (ROOT / 'src/native_eye_gpu.cpp').read_text()
        for name in ('SpidyEyeTrail', 'SpidyEngineTaaJitter', 'SpidyEngineTaaJitterScale', 'SpidyEngineTaaData'):
            self.assertIn(f"'{name}'", text)
            self.assertIn(name, native)

    def test_history_v2(self):
        scope, _ = launcher()
        b = bytearray(96)
        struct.pack_into('<6I', b, 0, 0x53454844, 2, 96, 40, 0, 10)
        struct.pack_into('<3Q', b, 24, 100, 5, 0)
        struct.pack_into('<dQ', b, 48, 20.0, 10)
        struct.pack_into('<2Qd2f', b, 64, 30, 4, 21.0, 1.5, .05)
        h = scope['history_snapshot'](FakeGame(b), 0)
        self.assertEqual((h['trail'], h['moving_pairs'], h['motion_cuts']), (10, 30, 4))
        self.assertAlmostEqual(h['travel_mean_m'], .2, 3)
        self.assertIn('trail 10%', scope['history_line'](h))
        struct.pack_into('<I', b, 4, 1)
        self.assertIsNone(scope['history_snapshot'](FakeGame(b), 0))

    def test_engine_taa_record(self):
        scope, _ = launcher()
        header = (ROOT / 'include/spidy/engine_taa.hpp').read_text()
        self.assertIn('static_assert(sizeof(Data) == %d)' % scope['ENGINE_TAA_BYTES'], header)
        v = [0x41544e45, 1, 144, 1, 2, 100, 2, 2, 1, 3, 1, 2, 4, 6, 2000, 1800, 500, 400,
             .0003, -.0002, .5, -.25, .5, .5, .001, .001, 1, 1, 2, 0, 0, 0, 7]
        t = scope['engine_taa_snapshot'](FakeGame(struct.pack(scope['ENGINE_TAA_FORMAT'], *v)), 0)
        self.assertEqual((t['state'], t['jitter_mode'], t['turned_on'], t['refused']), ('on', 'PIXELS', 3, 1))
        self.assertEqual(t['game_jitter_max'], [.5, .5])
        self.assertIn('jitter PIXELS at 100%', scope['engine_taa_line'](t))

    def test_shadow_distance_asked(self):
        self.assertEqual(sd.EVENT_KINDS[6], 'asked')
        header = (ROOT / 'include/spidy/shadow_distance.hpp').read_text()
        self.assertTrue(re.search(r'maxCars = 200;', header))
        self.assertIn('sliderMoves', (ROOT / 'src/wrist_hud.cpp').read_text())
        self.assertIn('sliderMoves', (ROOT / 'src/game_xr.cpp').read_text())


if __name__ == '__main__':
    unittest.main()
