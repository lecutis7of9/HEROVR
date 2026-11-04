# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.77 CUTFIX3: stuck after an immersive scene, and the menu panel over the immersive world."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Cutfix3(unittest.TestCase):
    def test_scene_end_releases(self):
        g = src('src/game_xr.cpp')
        at = g.index('if (cutWasShown && !immersiveScene) {')
        body = g[at:g.index('cutWasShown = immersiveScene;', at)]
        for s in ('immersiveHold.release();', 'cutMenu.release();', 'stripClock.reset();',
                  'cutStuck.sceneEnded(cutNowMs);', 'CUTFIX3 SCENE END'):
            self.assertIn(s, body)

    def test_stuck_watch(self):
        g = src('src/game_xr.cpp')
        at = g.index('if (cutStuck.update(')
        body = g[at:g.index('game_pad::submit(pad, 200);', at)]
        for s in ('gameScreen.reset();', 'immersiveHold.release();', 'pad = {};', 'CUTFIX3 STUCK WATCH'):
            self.assertIn(s, body)

    def test_menu_panel_faithful(self):
        g = src('src/game_xr.cpp')
        self.assertIn('inset.keyed = !menuMode;', g)
        self.assertIn('inset.backdrop = menuMode ? 0.f : shape.backdrop;', g)
        self.assertIn('cutscene077::menuPanel(vr_settings::screenWidth(values.screenSize)', g)
        self.assertIn('CUTFIX3 PANEL', g)

    def test_no_forbidden_words(self):
        for p in ('src/game_xr.cpp', 'include/spidy/cutscene077.hpp', 'tests/cutscene077_tests.cpp'):
            t = src(p).lower()
            self.assertNotIn('jim' + 'jam', t)


if __name__ == '__main__':
    unittest.main()
