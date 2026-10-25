"""Update 32.1: a mid-session graphics setting (run32b: Ambient Occlusion on)
rebuilt the game's eye images; the bridge took that as fault 3501 and VR
stopped. The new image is now taken over, or the eyes pause (3503) with VR
kept up on the game screen and a notice. Static checks on the sources."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def read(rel):
    return (ROOT / rel).read_text(encoding='utf-8')


class EyeSwapTests(unittest.TestCase):
    def test_rebuilt_eye_is_not_fatal(self):
        s = read('src/native_eye_gpu.cpp')
        self.assertNotRegex(s, r'fault\s*=\s*3501')
        self.assertIn('eye_swap::compatible(', s)
        self.assertIn('eyesLostCode = eye_swap::lostCode;', s)
        self.assertIn('retiredEyes.push_back(', s)

    def test_pairs_stop_while_lost(self):
        s = read('src/native_eye_gpu.cpp')
        self.assertGreaterEqual(len(re.findall(r'if \(eyesLostCode\.load\(\) \|\| !known\[0\]', s)), 2)
        self.assertIn('settleSwappedEyes()', s)

    def test_lost_eyes_show_game_screen_with_notice(self):
        s = read('src/game_xr.cpp')
        self.assertIn('if (viewing && gpuStarted && native_gpu::eyesLost())\n                        screen = true;', s)
        self.assertIn('native_gpu::wantScreen(viewing && (!vrPlay || native_gpu::eyesLost()));', s)
        self.assertIn('eye_swap::appendLostNotice(notice, aspect);', s)
        self.assertIn('message(3, 0, eye_swap::lostMessage);', s)
        self.assertIn('message(3, 0, eye_swap::swappedMessage);', s)

    def test_notice_text(self):
        h = read('include/spidy/eye_swap.hpp')
        lines = re.findall(r'"([A-Z0-9 ]+)"', h[h.index('lostLines'):h.index('lostMessage')])
        self.assertEqual(len(lines), 3)
        for line in lines:
            self.assertLessEqual(len(line), 80)


if __name__ == '__main__':
    unittest.main()
