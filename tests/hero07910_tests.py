"""79.10 Prowler stash L1+R1 pull card and WEB LINE MARKER toggle."""
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402


def src(path):
    return (ROOT / path).read_text(encoding='utf-8', errors='replace')


class StashPull(unittest.TestCase):
    def test_cine_approach_is_lbrb_pull(self):
        d = src('include/spidy/story077d.hpp')
        self.assertIn(
            '{".?AVCineApproachPrompt@@", "LB+RB", "HOLD RIGHT STICK CLICK / TWO-HAND YANK", object, 0, 10}',
            d)
        self.assertNotIn(
            '{".?AVCineApproachPrompt@@", "Y", "PRESS Y (THE SCENE ASKS)", cine, 0, 4}',
            d)
        self.assertIn(
            '{".?AVCineQTEBeginAction@@", "Y", "PRESS Y (THE SCENE ASKS)", cine, 0, 4}',
            d)
        self.assertIn('".?AVCineApproachPrompt@@"', d.split('interactKind')[1].split('catchAllKind')[0])


class WeblineMarker(unittest.TestCase):
    def test_setting(self):
        self.assertEqual(L.CHOICE_NAMES['webline_marker'], ('on', 'off'))
        self.assertEqual(L.SETTINGS['webline_marker'][0], (0, 1))
        self.assertEqual(L.SETTINGS['webline_marker'][1], 'SpidyWeblineMarkerMode')
        self.assertIn('webline_marker', L.PERSISTED)
        self.assertEqual(L.CONTROL_KEYS[24], 'webline_marker')
        self.assertEqual(L.SELECTOR['webline_marker'], 10)
        self.assertEqual(L.parse('webline_marker = off\n')[0]['webline_marker'], 'off')
        self.assertEqual(L.export_value('webline_marker', 'on'), 0)
        self.assertEqual(L.export_value('webline_marker', 'off'), 1)
        self.assertEqual(L.decode_hud_settings(10, L.MARKER)['webline_marker'], 'on')
        self.assertEqual(L.decode_hud_settings(10, L.MARKER | 1 << 6)['webline_marker'], 'off')

    def test_wiring(self):
        h = src('include/spidy/wrist_hud.hpp')
        self.assertIn('weblineMarker, // 79.10 WEB LINE MARKER', h)
        self.assertIn('Control::weblineGesture) == controls - 1', h)
        self.assertIn('controls = 253', h)
        w = src('src/wrist_hud.cpp')
        self.assertIn('C::gameWallAnims, C::weblineMarker, C::weblineGesture, C::combatCalibLook', w)
        self.assertIn('r.weblineMarker = true;', w)
        self.assertIn('return "WEB LINE MARKER";', w)
        g = src('src/game_xr.cpp')
        self.assertIn('std::atomic<uint32_t> weblineMarkerNow{0};', g)
        self.assertIn(
            'WINAPI SpidyWeblineMarkerMode(void* v) { return setLive(v, 0, 1, 5662, weblineMarkerNow, asU32); }',
            g)
        self.assertIn('(weblineMarkerNow.load() ? 1u : 0u) << 6', g)
        self.assertIn('if (weblineMarkerNow.load() != 0)', g)
        self.assertIn('continue; // 79.10: live.txt webline_marker', g)

    def test_weblines_toggle(self):  # 0.0.81 hotfix
        self.assertEqual(L.CONTROL_KEYS[25], 'webline_gesture')
        self.assertIn('webline_gesture', L.PERSISTED)
        self.assertEqual(L.SELECTOR['webline_gesture'], 10)
        self.assertEqual(L.decode_hud_settings(10, L.MARKER)['webline_gesture'], 'on')
        self.assertEqual(L.decode_hud_settings(10, L.MARKER | 1 << 7)['webline_gesture'], 'off')
        self.assertEqual(L.decode_hud_settings(3, L.MARKER | 1 << 25), {'webline_gesture': True})
        self.assertEqual(L.parse('webline_gesture = off\n')[0]['webline_gesture'], 'off')
        w = src('src/wrist_hud.cpp')
        self.assertIn('return "WEBLINES";', w)
        self.assertIn('r.weblineGesture = true;', w)
        g = src('src/game_xr.cpp')
        self.assertIn('if (result.weblineGesture)', g)
        self.assertIn('1u << 25', g)
        self.assertIn('(weblineGestureNow.load() == webline_native::modeOff ? 1u : 0u) << 7', g)

    def test_tag(self):
        c = src('CMakeLists.txt')
        self.assertIn('set(HERO_TEST_VERSION "0.0.82.1"', c)
        self.assertIn('set(HERO_TEST_NUMBER "1"', c)
        self.assertIn('HERO VR 82.1 HUD TEST (commit', src('.github/workflows/dynamic-body-0791.yml'))
        self.assertIn('HERO VR 79.10 TEST (includes 79.9)', src('src/game_xr.cpp'))
        self.assertIn('0.0.79.10 TEST 10', src('docs/0791-WEB-MECHANICS-TEST.md'))


if __name__ == '__main__':
    unittest.main()
