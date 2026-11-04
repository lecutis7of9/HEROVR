# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.77 CUTFIX: in-engine scenes go 3d / immersive on SM2, which has no game-time hook.

The cut1 report: cutscenes = immersive, the opening scene stayed on the flat screen because SceneClock only ran
from game_time's clock, never installed on Spider-Man 2. include/spidy/cutscene077_gate.hpp
(tests/cutscene077_gate_tests.cpp): the game camera's motion is that clock there; refusal reasons per gate.
"""
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402

NEW = ('include/spidy/cutscene077_gate.hpp', 'tests/cutscene077_gate_tests.cpp', 'tests/hero077_cutfix_tests.py')


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Cutfix(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.game = src('src/game_xr.cpp')

    def test_sm2_has_no_time_hook(self):
        # The root cause: game_time is not ported, so its clock alone never runs on SM2.
        self.assertIn('return spidy::sm2::timeNotPorted;', src('src/game_time.cpp'))

    def test_clock_falls_back_to_camera_motion(self):
        g = self.game
        self.assertIn('#include "spidy/cutscene077_gate.hpp"', g)
        self.assertIn('extern std::atomic<uint64_t> cameraMoves;', g)
        self.assertIn('cutMotion.advancing(clock.installed != 0, clock.gameFrame > 0, cameraMovesNow, screen,', g)
        self.assertNotIn('sceneClock.running(clock.installed && clock.gameFrame > 0', g)
        h = src('include/spidy/cutscene077_gate.hpp')
        self.assertIn('if (timeHooked)\n            return timeAdvances;', h)
        self.assertIn('nowMs - upMs_ >= motionSettleMs', h)

    def test_probe_counts_moves_read_only(self):
        p = src('src/stereo_probe.cpp')
        self.assertIn('std::atomic<uint64_t> cameraMoves{};', p)
        # CUTFIX2: sampled each frame in renderOffscreen, not only while our eyes render (placeEyes).
        at = p.index('void sampleGameCamera() {')
        body = p[at:p.index('uint8_t renderOffscreen() {', at)]
        self.assertIn('cutscene077::cameraMoved(lastPose, lastLens, pose.data(), lens)', body)
        self.assertNotIn('SetPose', body)
        self.assertNotIn('originalSubmit', body)

    def test_gates_kept(self):
        # Every safety gate of cut1 stays: the screen, a player, the bridge, not gameplay, not VIEW: GAME SCREEN, a
        # tracked head, eyes not lost, no fault; a refused or failed command keeps the screen.
        g = self.game
        self.assertIn('cutMode == cutscene077::immersive && !cutImmersiveFault && screen && cutHeroOk &&', g)
        self.assertIn('bridgeRunning && !gameplay && !flatScreen && validTrackedPose(frame.head) &&', g)
        self.assertIn('return screen || gameScreen.holding();', g)
        self.assertIn('cutImmersiveFault = true;', g)

    def test_refusals_logged(self):
        g = self.game
        self.assertIn('HERO VR 0.0.77 CUTFIX: scene clock %s, camera moves %llu (+%llu) of %llu frames ', g)
        self.assertIn('HERO VR 0.0.77 CUTFIX: a %.1f s screen stretch, %s %s; frames %llu refused ', g)
        self.assertIn('cutscene077::videoNote', g)
        h = src('include/spidy/cutscene077_gate.hpp')
        for name in ('no_player', 'no_bridge', 'gameplay', 'view_game_screen', 'no_head_pose', 'eyes_lost',
                     'clock_stopped', 'camera_still', 'menu', 'depth_off', 'no_panel', 'fault'):
            self.assertIn('"%s"' % name, h)

    def test_video_stays_flat_in_docs(self):
        self.assertIn('pre-rendered video has no 3D camera and stays on the flat screen', L.SETTINGS['cutscenes'][2])
        self.assertIn('A pre-rendered video (a movie, not the game\'s world) has no 3D camera', src('README.md'))
        self.assertIn('it can only be shown on the flat screen', src('include/spidy/cutscene077_gate.hpp'))

    def test_cmake(self):
        self.assertIn('spidy_cutscene077_gate_tests', src('CMakeLists.txt'))
        for p in NEW:
            self.assertTrue((ROOT / p).exists(), p)


if __name__ == '__main__':
    unittest.main()
