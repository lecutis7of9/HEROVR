# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.77 CUT6: the swing start gate, the swing's retarget, the menu screen without the stick."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Cut6(unittest.TestCase):
    def test_swing_start_gate(self):
        g = src('src/game_xr.cpp')
        self.assertIn('swing077::StartGate swingGate;', g)
        self.assertIn('swingGate.update(swingClock, raysStarted && motion.active && !flatScreen && controlsVisible,', g)
        self.assertIn('const bool off = swingGate.result(swingClock, code);', g)
        self.assertIn('swingFailed = swingGate.stopped(code);', g)
        self.assertIn('HERO VR 0.0.77 CUT6 SWING START', g)
        h = src('include/spidy/swing077.hpp')
        self.assertIn('return code == 1000 || code == 1002 || code == 1004;', h)

    def test_retarget_keeps_the_player(self):
        g = src('src/game_xr.cpp')
        at = g.index('const auto follow = [&](const game_player::Player& p) {')
        body = g[at:at + 2500]
        self.assertNotIn('return !retargetSwing(&swing);', body)
        self.assertIn('swingGate.restart();', body)

    def test_menu_screen_no_stick(self):
        g = src('src/game_xr.cpp')
        self.assertNotIn('cutSteering', g)
        self.assertIn('player.record, (frame.buttons & buttonMenu) != 0);', g)
        self.assertIn('bool update(bool screen, uint64_t record, bool menuPressed)', src('include/spidy/cutscene077.hpp'))

    def test_standing_is_076(self):
        h = src('include/spidy/swing077.hpp')
        self.assertIn('return f.steering && f.padInstalled && f.swingStatus && f.grounded && !f.owned', h)
        self.assertIn('swing077::standing(', src('src/game_xr.cpp'))

    def test_replay_wired(self):
        c = src('CMakeLists.txt')
        self.assertIn('add_test(NAME cut6_replay COMMAND spidy_cut6_replay_tests', c)
        lines = [l for l in src('tests/data/cut6_234713_samples.txt').splitlines() if not l.startswith('#')]
        self.assertEqual(len(lines), 2095)

    def test_no_forbidden_words(self):
        for p in ('include/spidy/swing077.hpp', 'tests/cut6_replay_tests.cpp', 'tests/hero077_cut6_tests.py'):
            t = src(p).lower()
            self.assertNotIn('jim' + 'jam', t)
            self.assertNotIn('mil' + 'es', t)


if __name__ == '__main__':
    unittest.main()
