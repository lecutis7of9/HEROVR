"""HERO VR 0.0.72 test build 6: THOR's crash stays fixed, its bisect keys, and the test notice's A / laser.

Build 5 (the user, 9:37): THOR still crashed the game when selected. The minidump: an access violation in
spidy_stereo_probe.dll's strlen (RVA 0x2a6ea0) called from wrist_hud's bannerRow (RVA 0x197107), RAX
"ACTIVE H": bannerRow's names[9] had nine heroes, and THOR is hero 9 (GREEN LANTERN 10), so it read the
"ACTIVE HERO: " literal's bytes as a pointer."""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as ls  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class ThorCrash(unittest.TestCase):
    def test_no_short_hero_name_table(self):
        s = src('src/wrist_hud.cpp')
        self.assertNotRegex(s, r'const char\*\s+names\[9\]')
        self.assertIn('activeHeroName(heroOf(view))', s)
        self.assertIn('hero_modes::heroNames', s)

    def test_step_lines(self):
        s = src('src/game_xr.cpp')
        for n in (1, 2, 3):
            self.assertIn('THOR: step %d' % n, s)


class BisectKeys(unittest.TestCase):
    KEYS = ('thor_lightning', 'thor_storm', 'thor_chain', 'thor_hammer_model', 'thor_flight')

    def test_keys(self):
        for k in self.KEYS:
            self.assertIn(k, ls.UPDATE38_KEYS, k)
            self.assertEqual(ls.UPDATE38_DEFAULTS[k], 'on', k)
        self.assertIn("'SpidyThorHammerModel'", src('tools/live_settings.py'))
        g = src('src/game_xr.cpp')
        self.assertIn('SpidyThorHammerModel', g)
        self.assertIn('SpidyThorFlight', g)
        self.assertIn('combat38::thorHammerModel', g)
        self.assertIn('combat38::thorFlight', g)

    def test_readme(self):
        r = src('tools/release072/READ-ME-FIRST.txt')
        for k in self.KEYS:
            self.assertIn(k + '=off', r)


class Notice(unittest.TestCase):
    def test_exact_text_kept(self):
        h = src('include/spidy/test_notice.hpp')
        self.assertIn('"You are on the latest test version, please be aware that some features do not work or are '
                      'unfinished, revert to 0.0.63 if unplayable and report issues in my discord server linked on '
                      'the launcher"', h.replace('"\n    "', '').replace('" "', ''))
        self.assertIn('closeHint = "Press A to close"', h)

    def test_a_closes_and_never_jumps(self):
        g = src('src/game_xr.cpp')
        self.assertIn('testNotice.holdsA()', g)
        self.assertIn('in.hands[i].a = noticeA[i]', g)
        # the mask comes before anything reads A (the phone takes its controls first)
        self.assertLess(g.index('testNotice.holdsA()'), g.index('phone.take(frame'))
        c = src('src/test_notice.cpp')
        self.assertIn('Ended::pressedA', c)
        self.assertIn('aLatch_', c)

    def test_laser_and_outline(self):
        c = src('src/test_notice.cpp')
        self.assertIn('laserOn_[i] = true', c)
        self.assertIn('v.laser[k]', c)
        self.assertRegex(c, r'if \(v\.hover\) \{\s*const Vec3 white')


if __name__ == '__main__':
    unittest.main()
