# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 HITCH HOLD: a Virtual Desktop frame hold of 0.1-0.5 s no longer drops VR play (run 060032).

live.txt hitch_hold (0 on, the default; 1 off = c71a59c), live, SpidyHitchHold id 5558;
include/spidy/hitch_hold.hpp plays such a frame as a 0.1 s step (core test HITCH HOLD).
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


class HitchHold(unittest.TestCase):
    def test_setting(self):
        rng, export, text = L.SETTINGS['hitch_hold']
        self.assertEqual(rng, (0, 1))
        self.assertEqual(export, 'SpidyHitchHold')
        self.assertIn('no restart needed', text)
        self.assertIn("c71a59c's rule", text)
        self.assertEqual(L.CHOICES['hitch_hold']['on'], 0)
        self.assertEqual(L.CHOICES['hitch_hold']['off'], 1)
        self.assertEqual(L.CHOICE_NAMES['hitch_hold'], ('on', 'off'))
        self.assertIn('hitch_hold', L.START_ONLY)

    def test_wiring(self):
        game = src('src/game_xr.cpp')
        self.assertIn('std::atomic<uint32_t> hitchHoldNow{0};', game)
        self.assertRegex(game, r'SpidyHitchHold\(void\* v\) \{ return setLive\(v, 0, 1, 5558, hitchHoldNow, asU32\); \}')
        self.assertEqual(len(re.findall(r'\b5558\b', game)), 1)
        # Held before anything reads the frame's time: the rig, the swing and the body all see the 0.1 s step.
        body = game[game.index('XrFrame frame = trackedFrame;'):]
        held = body.index('hitch_hold::step(frame.seconds')
        self.assertLess(held, body.index('XrFrame controller = frame;'))
        self.assertLess(held, body.index('rig.update('))
        self.assertIn('VERSION 0.0.76 HITCH HOLD', game)

    def test_header(self):
        h = src('include/spidy/hitch_hold.hpp')
        self.assertIn('longestStep = .1f', h)
        self.assertIn('longestHitch = .5f', h)
        # the rig's own rule it rides under is unchanged
        self.assertIn('f.seconds <= .1f ? 0u : badFrameTime', src('src/game_tracking.cpp'))
        self.assertIn('HITCH HOLD', src('tests/core_tests.cpp'))

    def test_docs(self):
        self.assertIn('HITCH HOLD', src('CHANGELOG.md'))
        self.assertIn('HITCH HOLD', src('BUILD-SM2.md'))
        self.assertIn('hitch_hold', src('docs/VISUAL-0.0.76.md'))


if __name__ == '__main__':
    unittest.main()
