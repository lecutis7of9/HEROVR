# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 FAR SHAKE: the far background shaking while the head turns (run 001842's video).

live.txt, both live (no restart):
  vr_game_jitter = off (default) | keep   the game's own head view (never shown) with no TAA jitter; keep: the flat
                                          game's 1280x720 jitter on it, up to ~3.5 eye pixels a frame (as before)
  vr_jitter = on (default) | off          the eyes' Halton walk, or one fixed tiny offset (TemporalAa still runs)
"""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class FarShake(unittest.TestCase):
    def test_settings_live(self):
        self.assertEqual(L.SETTINGS['vr_jitter'][:2], ((0, 1), 'SpidyVrJitter'))
        self.assertEqual(L.SETTINGS['vr_game_jitter'][:2], ((0, 1), 'SpidyVrGameJitter'))
        for name in ('vr_jitter', 'vr_game_jitter'):
            self.assertNotIn(name, L.START_ONLY + L.PERSISTED)
            self.assertNotIn(name, L.RESTART)
            self.assertIn('no restart', L.SETTINGS[name][2])

    def test_words(self):
        values, errors = L.parse('vr_jitter = off\nvr_game_jitter = keep\n')
        self.assertEqual(errors, [])
        self.assertEqual((L.export_value('vr_jitter', values['vr_jitter']),
                          L.export_value('vr_game_jitter', values['vr_game_jitter'])), (1, 1))
        values, _ = L.parse('vr_jitter = on\nvr_game_jitter = off\n')
        self.assertEqual((L.export_value('vr_jitter', values['vr_jitter']),
                          L.export_value('vr_game_jitter', values['vr_game_jitter'])), (0, 0))
        self.assertEqual(L.CHOICE_NAMES['vr_jitter'][0], 'on')
        self.assertEqual(L.CHOICE_NAMES['vr_game_jitter'][0], 'off')

    def test_native(self):
        s = src('src/stereo_probe.cpp')
        for export in ('SpidyVrJitter', 'SpidyVrGameJitter'):
            self.assertIn(f'DWORD WINAPI {export}(void* value)', s)
        # The stock jitter reaches the head view only through gameViewJitter (activeView twice, placeEyes once).
        self.assertEqual(len(re.findall(r'gameViewJitter\(gameViewJitterKeep\.load\(\) != 0, (?:stock|main)'
                                        r'\.values\[0x42[8c] / 4\]\)', s)), 6)
        self.assertNotIn('stock.values[0x428 / 4], stock.values[0x42c / 4]', s)
        # (The game-screen 3D mode's eyes still take the stock jitter; that mode is not the headset's.)
        self.assertNotIn('main.values[0x428 / 4], main.values[0x42c / 4], false', s)
        self.assertEqual(s.count('eyeJitterFor(eyeJitterOff.load'), 2)
        h = src('include/spidy/sm1style.hpp')
        self.assertIn('inline constexpr float steadyJitter = 1e-6f;', h)

    def test_docs(self):
        for doc in ('CHANGELOG.md', 'README-HERO-VR.md', 'BUILD-SM2.md', 'docs/VISUAL-0.0.76.md'):
            text = src(doc)
            self.assertIn('vr_game_jitter', text, doc)


if __name__ == '__main__':
    unittest.main()
