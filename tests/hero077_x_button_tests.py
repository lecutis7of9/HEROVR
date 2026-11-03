# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.77 X IS THE GAME'S X: VR X tap = the game's X, the menu button held 0.5 s = the View X sent.

live.txt x_button (SpidyXButton 5569): 0 game (default), 1 view (05e70a2's X = View).
include/spidy/story077e.hpp (tests/story077e_tests.cpp).
"""
import pathlib
import re
import subprocess
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402

NEW = ('include/spidy/story077e.hpp', 'tests/story077e_tests.cpp', 'tests/hero077_x_button_tests.py')


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Settings(unittest.TestCase):
    def test_setting(self):
        rng, exp, text = L.SETTINGS['x_button']
        self.assertEqual((rng, exp), ((0, 1), 'SpidyXButton'))
        self.assertEqual(L.CHOICE_NAMES['x_button'], ('game', 'view'))
        self.assertEqual(L.CHOICES['x_button']['game'], 0)
        self.assertEqual(L.CHOICES['x_button']['view'], 1)
        self.assertIn('x_button', L.START_ONLY)
        self.assertIn('live, no restart needed', text)
        self.assertEqual(L.parse('x_button = view\n'), ({'x_button': 'view'}, []))
        self.assertEqual(L.parse('x_button = game\n'), ({'x_button': 'game'}, []))
        values, notes = L.parse('x_button = banana\n')
        self.assertEqual(values, {})
        self.assertEqual(len(notes), 1)
        self.assertNotIn('View again', L.SETTINGS['story_abilities'][2])


class Wiring(unittest.TestCase):
    def setUp(self):
        self.game = src('src/game_xr.cpp')

    def test_export(self):
        g = self.game
        self.assertIn('SpidyXButton(void* v) { return setLive(v, 0, 1, 5569, xButtonNow, asU32); }', g)
        self.assertEqual(len(re.findall(r'\b5569\b', g)), 1)
        self.assertIn('std::atomic<uint32_t> xButtonNow{story077e::xGame};', g)

    def test_x_tap_is_game_x(self):
        g = self.game
        at = g.index('if (ab.menuTap) { // X tapped alone')
        body = g[at:at + 400]
        self.assertIn('xPulse.tap();', body)
        self.assertIn('menuTap = .1f, ++xTapsAsView;', body)
        self.assertNotIn('menuTap = .1f; // X tapped alone', g)
        # the press goes out after the ability button strips X, before the hero strip and the switch macro
        ab = g.index('const auto ab = storyAbility.update(ai);')
        put = g.index('if (xPulseDown)\n                            pad.buttons |= game_pad::x;')
        self.assertLess(ab, put)
        self.assertLess(put, g.index('HERO VR 0.0.77 SWITCH HERO (story077::SwapMacro)'))
        self.assertIn('walk.xIsGameX = xGameMode && heroNow() == hero_modes::spiderMan;', g)
        self.assertIn('s.buttons |= walk.xIsGameX ? x : back;', src('include/spidy/game_pad.hpp'))

    def test_menu_hold(self):
        g = self.game
        at = g.index('HERO VR 0.0.77 MENU HOLD: in VR play')
        self.assertLess(g.index('auto pad = game_pad::fromControllers(buttons, mapping, walk);'), at)
        self.assertLess(at, g.index('if ((vanDriving || seatedLock) && mapping == game_pad::Mapping::gameplay)'))
        self.assertLess(at, g.index('if (pausePress.update(frame.seconds'))
        self.assertLess(at, g.index('if (storySwap.running() && (pad.buttons & game_pad::start))'))
        body = g[at:at + 1500]
        self.assertIn('const bool holdOn = xGameMode && mapping == game_pad::Mapping::gameplay;', body)
        self.assertIn('pad.buttons & ~game_pad::start', body)
        self.assertIn('runtime.haptic(0, .25f);', body)
        self.assertIn('MENU HOLD: menu button held 0.5 s -> View #%llu (no pause)', body)
        self.assertIn('HERO VR 0.0.77 X BUTTON (%s): X taps -> game X %llu', g)

    def test_texts(self):
        self.assertIn('{"X TAP / MENU HOLD", "GAME X / VIEW"},', src('src/phone.cpp'))
        self.assertNotIn('VIEW AS BEFORE', src('src/phone.cpp'))
        r = src('README.md')
        self.assertIn('Hold the **Menu** button about half a second', r)
        self.assertNotIn('X opens the game menu', r)

    def test_no_forbidden_words(self):
        for p in NEW:
            text = src(p).lower()
            self.assertNotIn('jim' + 'jam', text, p)
            self.assertNotRegex(text, r'\bmil' + r'es\w*', p)
        added = subprocess.run(['git', 'diff', '05e70a2'], cwd=ROOT, capture_output=True, text=True).stdout
        plus = '\n'.join(l for l in added.splitlines() if l.startswith('+')).lower()
        self.assertNotIn('jim' + 'jam', plus)
        self.assertNotRegex(plus, r'\bmil' + r'es\w*\b')


if __name__ == '__main__':
    unittest.main()
