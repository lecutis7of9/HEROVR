# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.77 CUTSCENES: how the game's scenes show in the headset.

live.txt cutscenes (SpidyCutscenes 5571): 0 flat (default, the screen as before), 1 3d, 2 immersive.
include/spidy/cutscene077.hpp (tests/cutscene077_tests.cpp).
"""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402

NEW = ('include/spidy/cutscene077.hpp', 'tests/cutscene077_tests.cpp', 'tests/hero077_cutscene_tests.py')


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Settings(unittest.TestCase):
    def test_setting(self):
        rng, exp, text = L.SETTINGS['cutscenes']
        self.assertEqual((rng, exp), ((0, 2), 'SpidyCutscenes'))
        self.assertEqual(L.CHOICE_NAMES['cutscenes'], ('flat', '3d', 'immersive'))
        for name, value in (('flat', 0), ('3d', 1), ('immersive', 2), ('0', 0), ('1', 1), ('2', 2)):
            self.assertEqual(L.CHOICES['cutscenes'][name], value)
        self.assertIn('cutscenes', L.START_ONLY)
        self.assertIn('live, no restart needed', text)
        for word in ('flat', 'immersive', '3d'):
            self.assertEqual(L.parse('cutscenes = %s\n' % word), ({'cutscenes': word}, []))
        for junk in ('banana', '3', '9', 'on', ''):
            values, notes = L.parse('cutscenes = %s\n' % junk)
            self.assertNotIn('cutscenes', values)

    def test_default_flat(self):
        g = src('src/game_xr.cpp')
        self.assertIn('std::atomic<uint32_t> cutscenesNow{cutscene077::flat};', g)
        self.assertIn('flat = 0', src('include/spidy/cutscene077.hpp').replace('flat{0}', 'flat = 0'))


class Wiring(unittest.TestCase):
    def setUp(self):
        self.game = src('src/game_xr.cpp')

    def test_export(self):
        self.assertIn('extern "C" __declspec(dllexport) DWORD WINAPI SpidyCutscenes(void* v) { return setLive(v, 0, 2, '
                      '5571, cutscenesNow, asU32); }', self.game)
        self.assertEqual(len(re.findall(r'\b5571\b', self.game)), 1)

    def test_immersive_never_throws(self):
        g = self.game
        at = g.index('if (!vrPlay && immersiveScene) {')
        body = g[at:g.index('if (!vrPlay) {', at)]
        self.assertIn('eyes.enabled = 4;', body)
        self.assertNotIn('check(', body)
        self.assertIn('cutImmersiveFault = true;', body)
        self.assertIn('return !code || screen || gameScreen.holding();', body)

    def test_panels_caught(self):
        g = self.game
        for start in ('if (showScreen && cutImmersive', 'if (strip3d && !overlayFault)'):
            at = g.index(start)
            body = g[at:g.index('catch (...)', at)]
            self.assertIn('try {', body)
            self.assertIn('overlay.renderViews(', body)

    def test_scene_hold_and_play(self):
        g = self.game
        self.assertIn('immersiveHold.update(', g)
        self.assertIn('(frame.buttons & buttonMenu) != 0', g)
        self.assertIn('copied = copied && saved && !saved->cutscene;', g)
        self.assertIn('Presentation::cutscene', g)
        self.assertIn('cutscene = 5', src('include/spidy/presentation_gate.hpp'))
        self.assertIn("5: 'cutscene_immersive'", src('tools/run_game_vr.py'))

    def test_hotfix_kept(self):
        # The floor crawl's raw left stick and SCREEN HEAL's D-pad down are untouched: the pad mapping still
        # comes from the screen, which an immersive scene keeps.
        g = self.game
        self.assertIn('++crawlFeedForced77;', g)
        self.assertIn('const bool screenLive = mapping == game_pad::Mapping::menus && healOn.load() &&', g)
        self.assertIn('pad.buttons |= game_pad::dpadDown;', g)
        self.assertIn('extern "C" __declspec(dllexport) DWORD WINAPI SpidyScreenHeal(void* v) { return setLive(v, 0, 1, '
                      '5570, screenHealNow, asU32); }', g)

    def test_probe(self):
        p = src('src/stereo_probe.cpp')
        self.assertIn('desired.enabled == 2 || desired.enabled == 3', p)
        self.assertIn('eyesPlaced', p)

    def test_cmake(self):
        c = src('CMakeLists.txt')
        self.assertIn('spidy_cutscene077_tests', c)
        for p in NEW:
            self.assertTrue((ROOT / p).exists(), p)


if __name__ == '__main__':
    unittest.main()
