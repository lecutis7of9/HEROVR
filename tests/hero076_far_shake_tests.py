# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 FAR SHAKE: the far background shaking while the head turns (run 001842's video).

live.txt, both live (no restart):
  vr_game_jitter = off (default) | keep   0.0.76 JITTER FIX: the game's own head views (never shown) give the lens
                                          0, not the stock camera's stored jitter, which the lens doubled and
                                          flipped; keep: c71a59c's doubled values (hero076_jitter_fix_tests.py)
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
                          L.export_value('vr_game_jitter', values['vr_game_jitter'])), (1, 0))
        values, _ = L.parse('vr_jitter = on\nvr_game_jitter = off\n')
        self.assertEqual((L.export_value('vr_jitter', values['vr_jitter']),
                          L.export_value('vr_game_jitter', values['vr_game_jitter'])), (0, 1))
        self.assertEqual(L.CHOICE_NAMES['vr_jitter'][0], 'on')
        # 0.0.76 JITTER FIX: off (1) is the default (the DLL's atomic; the line is never persisted).
        self.assertEqual(L.CHOICE_NAMES['vr_game_jitter'], ('keep', 'off'))
        self.assertIn('1 off, the default', L.SETTINGS['vr_game_jitter'][2])

    def test_native(self):
        s = src('src/stereo_probe.cpp')
        for export in ('SpidyVrJitter', 'SpidyVrGameJitter'):
            self.assertIn(f'DWORD WINAPI {export}(void* value)', s)
        # The stock jitter reaches the head views only through gameViewJitter (activeView, placeEyes onHead).
        self.assertEqual(len(re.findall(r'gameViewJitter\(noGameJitter, (?:stock|main)'
                                        r'\.values\[0x42[8c] / 4\]\)', s)), 4)
        self.assertNotIn('stock.values[0x428 / 4], stock.values[0x42c / 4]', s)
        # (The game-screen 3D mode's eyes still take the stock jitter; that mode is not the headset's.)
        self.assertNotIn('main.values[0x428 / 4], main.values[0x42c / 4], false', s)
        self.assertEqual(s.count('eyeJitterFor(eyeJitterOff.load'), 2)
        self.assertIn('std::atomic<uint32_t> eyeJitterOff{0}, gameViewJitterOff{1};', s)
        h = src('include/spidy/sm1style.hpp')
        self.assertIn('inline constexpr float steadyJitter = 1e-6f;', h)
        self.assertIn('return off ? 0.f : stock;', h)

    def test_eye_history_check(self):
        """The outside advice (each eye its own TAA history) is measured each run and reaches the report."""
        s = src('src/stereo_probe.cpp')
        self.assertIn('mt::ownHistory(eyeSet, previous, mainSet, otherSet)', s)
        self.assertIn('++dd.sharedHistory[eye];', s)
        r = src('tools/run_game_vr.py')
        self.assertIn("'SpidySm1StyleData'", r)
        self.assertIn("sm1style_eyes=sm1style_snapshot(game, xr['SpidySm1StyleData']) or sm1style_last", r)
        self.assertIn('from sm1style_record import sm1style_snapshot, sm1style_line', r)
        import sm1style_record as P

        class Fake:
            def __init__(self, raw):
                self.raw = raw

            def read(self, address, size):
                return self.raw[:size]
        import struct
        raw = bytearray(256)
        struct.pack_into('<4I', raw, 0, 0x53314d53, 2, 256, 0)
        struct.pack_into('<2Q', raw, 32, 10, 11)    # calls
        struct.pack_into('<2Q', raw, 48, 9, 10)     # ran
        struct.pack_into('<2Q', raw, 64, 8, 9)      # history
        struct.pack_into('<6Q', raw, 200, 9, 10, 0, 0, 0, 1)
        rec = P.sm1style_snapshot(Fake(bytes(raw)), 0)
        self.assertEqual(rec['left'], dict(calls=10, ran=9, history=8, own_history=9, shared_history=0, job_changed=0))
        self.assertEqual(rec['right']['job_changed'], 1)
        self.assertIn('the eyes never read each other', P.sm1style_line(rec))
        struct.pack_into('<Q', raw, 216, 3)
        self.assertIn('SHARED', P.sm1style_line(P.sm1style_snapshot(Fake(bytes(raw)), 0)))
        self.assertIsNone(P.sm1style_snapshot(Fake(bytes(255)), 0))

    def test_docs(self):
        for doc in ('CHANGELOG.md', 'README-HERO-VR.md', 'BUILD-SM2.md', 'docs/VISUAL-0.0.76.md'):
            text = src(doc)
            self.assertIn('vr_game_jitter', text, doc)


if __name__ == '__main__':
    unittest.main()
