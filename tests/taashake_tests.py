# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.73 TAA SHAKE: TAA JITTER FULL/HALF/OFF (default HALF, live) and the per-eye TAA record."""
import pathlib
import re
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings  # noqa: E402


class FakeGame:
    def __init__(self, data):
        self.data = bytes(data)

    def read(self, address, size):
        return self.data[:size]


def launcher():
    text = (ROOT / 'tools/run_game_vr.py').read_text(encoding='utf-8')
    start = text.index('HISTORY_BYTES = ')
    end = text.index('# Update 34.1: SpidyXrStart')
    scope = {'struct': struct}
    exec(text[start:end], scope)  # noqa: S102 - our own source
    return scope, text


class TaaShake(unittest.TestCase):
    def setUp(self):
        self.scope, self.text = launcher()
        self.header = (ROOT / 'include/spidy/engine_taa.hpp').read_text(encoding='utf-8')
        self.probe = (ROOT / 'src/stereo_probe.cpp').read_text(encoding='utf-8')

    def test_default_is_half_and_full_reverts(self):
        self.assertIn('defaultJitterScale = 50', self.header)
        self.assertIn('fullJitterScale = 100, halfJitterScale = 50, offJitterScale = 0', self.header)
        self.assertIn('taa_jitter_scale', live_settings.SETTINGS)
        self.assertNotIn('taa_jitter_scale', live_settings.UPDATE37_DEFAULTS)  # the native default rules

    def test_eyes_layout_matches_native(self):
        self.assertEqual(self.scope['ENGINE_TAA_EYES_BYTES'], 128)
        self.assertIn('static_assert(sizeof(Eyes) == 128)', self.header)
        self.assertIn('eyesMagic = 0x45544e45', self.header)
        self.assertIn("'SpidyEngineTaaEyes'", self.text)
        self.assertIn('__declspec(dllexport) spidy::engine_taa::Eyes SpidyEngineTaaEyes', self.probe)

    def test_offsets_are_the_handlers(self):
        # Read sites in TemporalAa 0x2c2dc60 / 0x2d018d2 / 0x2c9bf40 (BUILD-SM2 section 58).
        self.assertIn('viewSet = 0x18e0, setTaaWidth = 0x3800, viewCut = 0x19ab, viewCutFrame = 0x19ac', self.header)
        self.assertIn('viewStamp = 0x1de8, frameRva = 0xc2c6c30', self.header)

    def test_snapshot_and_line(self):
        raw = struct.pack(self.scope['ENGINE_TAA_EYES_FORMAT'], 0x45544e45, 1, 128, 5,
                          .0002, -.0001, .0003, .0004, 3496, 3744, 3496, 3744, 0, 1, 10, 20, 30, 40,
                          .1, 1.0, 3496, 3744, .35, .19, .52, -.75, 0, 50, 0, 3, 9)
        e = self.scope['engine_taa_eyes_snapshot'](FakeGame(raw), 1)
        self.assertEqual(e['jitter'], 'HALF')
        self.assertEqual(e['render'], [3496, 3744])
        self.assertEqual(e['left']['taa_size'], [3496, 3744])
        self.assertEqual(e['left']['pixels'], [0.35, 0.19])
        self.assertEqual(e['right']['cut'], 1)
        self.assertEqual(e['right']['weight'], 1.0)
        self.assertEqual(e['right']['history_reset_samples'], 3)
        self.assertEqual((e['left']['cut_age'], e['left']['stamp_age']), (10, 30))
        self.assertEqual(e['sequence'], 9)
        line = self.scope['engine_taa_eyes_line'](e)
        self.assertIn('TAA JITTER HALF', line)
        self.assertIn('3496x3744', line)
        self.assertIsNone(self.scope['engine_taa_eyes_snapshot'](FakeGame(raw), 0))
        self.assertIsNone(self.scope['engine_taa_eyes_snapshot'](FakeGame(b'\0' * 128), 1))

    def test_levels(self):
        level = self.scope['jitter_level']
        self.assertEqual([level('OURS', s) for s in (100, 50, 0, 70)], ['FULL', 'HALF', 'OFF', '70%'])
        self.assertEqual(level('NONE', 50), 'NONE')

    def test_help_is_one_short_sentence_with_live_note(self):
        help_text = re.search(r'jitterHelp =\s*"([^"]*)"', self.header).group(1)
        self.assertLessEqual(len(help_text), 80)
        self.assertEqual(help_text.count('.'), 1)
        self.assertIn('live', help_text)
        self.assertIn('only while it is on', help_text)


if __name__ == '__main__':
    unittest.main()
