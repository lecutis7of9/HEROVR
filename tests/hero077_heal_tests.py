# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.77 HEAL ANYWHERE: the right stick held down heals in VR play and, hurt, on the game screen.

live.txt screen_heal (SpidyScreenHeal 5570): 0 on (default), 1 off.
include/spidy/story077f.hpp (tests/story077f_tests.cpp).
"""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402

NEW = ('include/spidy/story077f.hpp', 'tests/story077f_tests.cpp', 'tests/hero077_heal_tests.py')


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Settings(unittest.TestCase):
    def test_setting(self):
        rng, exp, text = L.SETTINGS['screen_heal']
        self.assertEqual((rng, exp), ((0, 1), 'SpidyScreenHeal'))
        self.assertEqual(L.CHOICE_NAMES['screen_heal'], ('on', 'off'))
        self.assertEqual(L.CHOICES['screen_heal']['on'], 0)
        self.assertEqual(L.CHOICES['screen_heal']['off'], 1)
        self.assertIn('screen_heal', L.START_ONLY)
        self.assertIn('live, no restart needed', text)
        self.assertEqual(L.parse('screen_heal = off\n'), ({'screen_heal': 'off'}, []))
        values, notes = L.parse('screen_heal = banana\n')
        self.assertEqual(values, {})
        self.assertEqual(len(notes), 1)


class Wiring(unittest.TestCase):
    def setUp(self):
        self.game = src('src/game_xr.cpp')

    def test_export(self):
        g = self.game
        self.assertIn('SpidyScreenHeal(void* v) { return setLive(v, 0, 1, 5570, screenHealNow, asU32); }', g)
        self.assertEqual(len(re.findall(r'\b5570\b', g)), 1)
        self.assertIn('std::atomic<uint32_t> screenHealNow{story077f::screenHealOn};', g)

    def test_screen_heal_after_pad(self):
        g = self.game
        pad = g.index('auto pad = game_pad::fromControllers(buttons, mapping, walk);')
        body = g[pad:pad + 2500]
        self.assertIn('mapping == game_pad::Mapping::menus && healOn.load()', body)
        self.assertIn('pad.buttons |= game_pad::dpadDown;', body)
        self.assertIn('HERO VR 0.0.77 HEAL: D-pad down out #%u', body)
        self.assertIn('HERO VR 0.0.77 CRAWL FEED', g[pad:pad + 5000])

    def test_play_heal_second_way(self):
        g = self.game
        at = g.index('walk.heal = healOn.load() && push == game_pad::StickPush::Way::down;')
        body = g[at:at + 1500]
        self.assertIn('healStick.update(frame.seconds, healLive && healOn.load()', body)
        self.assertIn('!abilityX', body)
        self.assertIn('walk.heal = true;', body)

    def test_crawl_feed(self):
        g = self.game
        at = g.index('if (crawlsFloor && steering && !playing')
        self.assertIn('walk.forward = motion.walkForward;', g[at:at + 500])
        self.assertLess(at, g.index('auto pad = game_pad::fromControllers(buttons, mapping, walk);'))

    def test_story7_kept(self):
        g = self.game
        self.assertIn('SpidyXButton(void* v) { return setLive(v, 0, 1, 5569, xButtonNow, asU32); }', g)
        self.assertIn('xPulse.tap();', g)

    def test_docs(self):
        self.assertIn('screen_heal = off', src('README.md'))
        self.assertIn('HEAL, CARDS TOO', src('src/phone.cpp'))
        self.assertIn('screen_heal', src('apps/launcher/ui.cpp'))
        self.assertIn('HEAL ANYWHERE', src('CHANGELOG.md'))
        self.assertNotIn('X: game "\n                                  "menu (hold: gadget wheel)', self.game)

    def test_words(self):
        for p in NEW:
            t = src(p)
            self.assertIsNone(re.search(r'\bmiles\w*', t, re.I), p)
            self.assertNotIn('jim' + 'jam', t.lower(), p)


if __name__ == '__main__':
    unittest.main()
