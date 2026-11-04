# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.77 CUTFIX2: the cut2 report (cutscenes = immersive, still the screen).

CONFIRMED there: every screen frame was refused "camera_still" while the window probe showed the game's camera
moving each frame, because the camera was only sampled inside placeEyes(), which runs only while our eyes render.
The sampler now runs each frame from renderOffscreen; a refused scene names its reason on the headset's screen.
"""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
NEW = ('include/spidy/cutscene077_note.hpp', 'tests/hero077_cutfix2_tests.py')


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Cutfix2(unittest.TestCase):
    def test_sampler_every_frame(self):
        p = src('src/stereo_probe.cpp')
        at = p.index('uint8_t renderOffscreen() {')
        body = p[at:p.index('return originalRenderOffscreen();', at)]
        self.assertIn('sampleGameCamera(); // HERO VR 0.0.77 CUTFIX2', body)
        self.assertLess(body.index('sampleGameCamera();'), body.index('placeEyes();'))
        # Not inside placeEyes() any more (it only runs while our eyes render).
        at = p.index('void placeEyes() {')
        body = p[at:p.index('native_eyes::Command desired{};', at)]
        self.assertNotIn('cameraMoves', body)
        self.assertIn('std::atomic<uint64_t> cameraSamples{};', p)

    def test_note_drawn_and_caught(self):
        g = src('src/game_xr.cpp')
        self.assertIn('#include "spidy/cutscene077_note.hpp"', g)
        at = g.index('else if (cutNoteShow && cutNoteText[0] && !overlayFault && cutNoteNow.load() == 1) {')  # CUT7: live.txt cut_note
        body = g[at:g.index('copyMs =', at)]
        self.assertIn('try {', body)
        self.assertIn('catch (...)', body)
        self.assertIn('cutscene077_note::append(note, backing, cutNoteText, aspect);', body)
        self.assertIn('cutscene077::noteLine(cutMode, refused, cutNoteText, sizeof(cutNoteText));', g)

    def test_hero_grace_only_for_shown(self):
        g = src('src/game_xr.cpp')
        self.assertIn('cutHero.present(player.hero != 0, cutWasShown, cutNowMs)', g)
        self.assertIn('cutWasShown = immersiveScene;', g)

    def test_files(self):
        self.assertIn('one amber line along the top of the screen names why', src('README.md'))
        for p in NEW:
            self.assertTrue((ROOT / p).exists(), p)


if __name__ == '__main__':
    unittest.main()
