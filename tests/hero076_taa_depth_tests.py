# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 A-depth: SPIDY TAA's history moved with the eyes' depth (include/spidy/taa_depth.hpp).

live.txt, live (no restart):
  vr_taa_depth = off (default) | on | auto   auto: on once the TAA WATCH depth log has CONFIRMED the eye depth;
                                             off: 509bdc9's history (the head's turn only)
The blend shares and the colour clip are 509bdc9's, unchanged.
"""
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class TaaDepth(unittest.TestCase):
    def test_setting_live(self):
        self.assertEqual(L.SETTINGS['vr_taa_depth'][:2], ((0, 1, 2), 'SpidyVrTaaDepth'))
        self.assertNotIn('vr_taa_depth', L.START_ONLY + L.PERSISTED)
        self.assertNotIn('vr_taa_depth', L.RESTART)
        self.assertIn('no restart', L.SETTINGS['vr_taa_depth'][2])

    def test_words(self):
        values, errors = L.parse('vr_taa_depth = on\n')
        self.assertEqual(errors, [])
        self.assertEqual(L.export_value('vr_taa_depth', values['vr_taa_depth']), 1)
        values, _ = L.parse('vr_taa_depth = off\n')
        self.assertEqual(L.export_value('vr_taa_depth', values['vr_taa_depth']), 0)
        values, _ = L.parse('vr_taa_depth = auto\n')
        self.assertEqual(L.export_value('vr_taa_depth', values['vr_taa_depth']), 2)

    def test_native(self):
        n = src('src/native_eye_gpu.cpp')
        self.assertIn('DWORD WINAPI SpidyVrTaaDepth(void* value)', n)
        self.assertIn('return 4607;', n)
        self.assertIn('std::atomic<uint32_t> taaDepthOn{taa_depth::off};', n)  # off by default (CANDIDATE)
        self.assertIn('taaDepthConfirmed = log.verdict == taa_watch::confirmed;', n)
        self.assertIn('#ifdef TAA_DEPTH', n)
        self.assertIn('abs(had - want) <= 0.1 * max(had, want) + 1e-7', n)
        self.assertIn('taaDepthStop(3);', n)
        h = src('include/spidy/taa_depth.hpp')
        self.assertIn('inline constexpr float tolerance = .1f;', h)
        self.assertIn('inline constexpr uint32_t automatic = 2;', h)

    def test_core_unchanged(self):
        a = src('include/spidy/vr_aa.hpp')
        self.assertIn('inline constexpr float keepStill = .9f, keepMoving = .6f;', a)
        n = src('src/native_eye_gpu.cpp')
        # The clip and the blend as 509bdc9's.
        self.assertIn('const float3 boxLo = max(lo, mean - extra.w * sigma);', n)
        self.assertIn('const float a = (1 - keep) / (1 + middle.x), b = keep / (1 + before.x);', n)
        self.assertIn('if (most > 1)\n        before = centre + offset / most;', n)

    def test_docs(self):
        for doc in ('CHANGELOG.md', 'README-HERO-VR.md', 'BUILD-SM2.md', 'docs/VISUAL-0.0.76.md'):
            self.assertIn('vr_taa_depth', src(doc), doc)


if __name__ == '__main__':
    unittest.main()
