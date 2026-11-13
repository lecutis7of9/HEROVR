"""82.1 HUD ANCHOR: the settings, the wiring and the test build's tag."""
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402


def src(path):
    return (ROOT / path).read_text(encoding='utf-8', errors='replace')


class HudAnchor(unittest.TestCase):
    def test_settings(self):
        self.assertEqual(L.CHOICE_NAMES['hud_anchor'], ('auto', 'world', 'head'))
        self.assertEqual(L.SETTINGS['hud_anchor'][1], 'SpidyHudAnchor')
        self.assertEqual(L.SETTINGS['hud_depth'][0], (5, 300))
        self.assertEqual(L.SETTINGS['hud_depth'][1], 'SpidyHudDepth')
        self.assertEqual(L.parse('hud_anchor = head\nhud_depth = 60\n')[0], {'hud_anchor': 'head', 'hud_depth': 60})
        self.assertEqual(L.export_value('hud_anchor', 'world'), 1)
        self.assertTrue(L.parse('hud_depth = 9999\n')[1])
        g = src('src/game_xr.cpp')
        self.assertIn('SpidyHudAnchor(void* v) { return setLive(v, 0, 2, 5782, hudAnchorNow', g)
        self.assertIn('setLive(v, hud_anchor::depthLow, hud_anchor::depthHigh, 5783, hudDepthNow', g)
        self.assertIn('hudAnchorNow{hud_anchor::anchorAuto}', g)
        self.assertEqual(g.count('5782'), 1)
        self.assertEqual(g.count('5783'), 1)

    def test_wiring(self):
        g = src('src/game_xr.cpp')
        self.assertIn('hudCameraStore(camera);', g)
        self.assertIn('hud_anchor::anchoredRect(', g)
        self.assertIn('overlayViews[eye].viewProjection, at, slot.shape.tw', g)
        self.assertIn('hud_anchor::panelPoint(head.position, facing, x, y)', g)
        # head mode keeps cut21's cards exactly
        self.assertIn('anchorMode == hud_anchor::anchorHead', g)
        self.assertIn('prompt_layer::cardRect(', g)
        # a read is a fresh one only when the layer's grid changed
        self.assertIn('layerGrid.stamp != anchorStamp', g)
        self.assertIn('grid.stamp = layerGridMs;', src('src/native_eye_gpu.cpp'))
        self.assertIn('std::atomic<float> cameraLens[4]{};', src('src/stereo_probe.cpp'))

    def test_never_the_whole_layer(self):
        h = src('include/spidy/hud_anchor.hpp')
        self.assertIn('prompt_layer::maxCardTan', h)
        self.assertIn('prompt_layer::shrink', h)
        g = src('src/game_xr.cpp')
        self.assertIn('prompt_layer::regions(layerGrid, pieces)', g)  # the pieces are still the layer's small lit regions

    def test_tag(self):
        c = src('CMakeLists.txt')
        self.assertIn('set(HERO_TEST_VERSION "0.0.82.1"', c)
        self.assertIn('set(HERO_TEST_NUMBER "1"', c)
        w = src('.github/workflows/dynamic-body-0791.yml')
        self.assertIn('HERO VR 82.1 HUD TEST (commit', w)
        self.assertIn('- experiment/082-hud-fix', w)
        self.assertIn('hud_anchor_tests', w)
        self.assertIn('0.0.82.1 TEST 1', src('docs/082-HUD-ANCHOR-TEST.md'))


if __name__ == '__main__':
    unittest.main()
