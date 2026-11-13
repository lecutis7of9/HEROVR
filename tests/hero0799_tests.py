"""79.9 CAMERA GESTURE and STEALTH."""
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402


def src(path):
    return (ROOT / path).read_text(encoding='utf-8', errors='replace')


class Camera(unittest.TestCase):
    def test_setting(self):
        self.assertEqual(L.CHOICE_NAMES['camera_hero'], ('auto', 'camera', 'scanner', 'off'))
        self.assertEqual(L.SETTINGS['camera_hero'][1], 'SpidyCameraHero')
        self.assertEqual(L.parse('camera_hero = scanner\n')[0]['camera_hero'], 'scanner')
        self.assertEqual(L.export_value('camera_hero', 'off'), 3)
        self.assertIn('SpidyCameraHero(void* v) { return setLive(v, 0, 3, 5691, cameraHeroNow', src('src/game_xr.cpp'))
        self.assertIn('std::atomic<uint32_t> cameraHeroNow{0};', src('src/game_xr.cpp'))

    def test_right_temple_scan_untouched(self):
        g = src('include/spidy/game_pad.hpp')
        a = g.index('class TemplePress')
        b = g.index('// 79.9 CAMERA GESTURE: the LEFT hand')
        t = g[a:b]
        self.assertIn('for (const float x : {-side, side})', t)  # both sides, both hands: as 79.8
        self.assertIn('scans_ += !held();', t)
        x = src('src/game_xr.cpp')
        self.assertIn('templePress.update(frame.seconds, gesturesPlay, frame.head, grips, gripsTracked,\n                                           triggers);', x)
        self.assertIn('walk.scan = templePress.held() && steering;', x)
        # only the LEFT trigger is taken from the scan, and only by the camera
        self.assertIn('triggers[0] = 0;', x)
        self.assertNotIn('triggers[1] = 0;\n                        if (cameraPress', x)

    def test_sends(self):
        x = src('src/game_xr.cpp')
        self.assertIn('game_prompts::demand().leftTrigger', x)
        self.assertIn('walk.aim = walk.aim || sendL2;', x)  # L2 is ORed: the web line's L2 + Y is untouched
        self.assertIn('walk.camera = sendUp;', x)
        g = src('include/spidy/game_pad.hpp')
        self.assertIn('if (walk.camera)\n            s.buttons |= dpadUp;', g)
        self.assertIn('cameraPress.update(frame.seconds, gesturesPlay && cameraHeroNow.load() != 3', x)

    def test_stealth(self):
        x = src('src/game_xr.cpp')
        self.assertIn('const bool yEnemyAround79 = world_trace::nearestEnemyMetres() < 30.f;', x)
        self.assertIn('!yEnemyAround79', x)
        self.assertIn('pad.buttons |= namedPad.buttons;', x)  # a prompt's named button goes out

    def test_tag(self):
        c = src('CMakeLists.txt')
        self.assertIn('set(HERO_TEST_VERSION "0.0.82.1"', c)
        self.assertIn('set(HERO_TEST_NUMBER "1"', c)
        self.assertIn('HERO VR 82.1 HUD TEST (commit', src('.github/workflows/dynamic-body-0791.yml'))
        self.assertIn('HERO VR 79.9 TEST (includes 79.8)', src('src/game_xr.cpp'))
        self.assertIn('camera_press_tests', src('.github/workflows/dynamic-body-0791.yml'))


if __name__ == '__main__':
    unittest.main()
