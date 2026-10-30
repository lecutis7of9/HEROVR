"""HERO VR 0.0.72 test build 12 (source gates): TEST 12 on the wrist; WINDOW LIGHTS (LOD FADE off by default,
the game view's lens never narrower than both eyes plus the margin, at both submits); the Batman cape found
on the suit; the pouch draw keeps the trigger for the grapple."""
import os, re, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()

class Build12(unittest.TestCase):
    def test_test_number(self):
        self.assertRegex(src("CMakeLists.txt"), r"set\(HERO_TEST_NUMBER \"[0-9]+\"")  # 0.0.73: its own test numbers
    def test_lod_fade_off_by_default(self):
        self.assertIn('inline constexpr uint32_t defaultFade = fadeOff', src('include/spidy/view_gates.hpp'))
        self.assertIn("'lod_fade': 0}", src('tools/live_settings.py'))
    def test_game_view_lens_widened_at_both_submits(self):
        s = src('src/stereo_probe.cpp')
        i = s.index('native_eyes::Bounds viewLens(')
        self.assertRegex(s[i:i + 600], r'recording_fov::widened\(head[,)]')  # 0.0.73 test 2: + the CULL MARGIN
        self.assertIn('const auto lens = viewLens(head.lens);', s)
        self.assertIn('auto viewed = onHead(viewLens(lens), gameNear, vj::gameHead);', s)  # 0.0.74 test 8: + the view
        self.assertIn('shadowLens = aligned;', s)  # the eyes' shadows keep the eyes' own lens
    def test_cape_found_on_the_suit(self):
        self.assertIn('findCape', src('src/suit_wear.cpp'))
    def test_phone_spider_man_only_and_put_back(self):
        s = src('src/game_xr.cpp')
        self.assertIn('phoneDrawPlay = drawPlay && heroNow() == hero_modes::spiderMan', s)
        self.assertIn('pocketDraw.update(frame.seconds, phoneDrawPlay,', s)
        self.assertIn('phonePutBack.update(phone.open()', s)
    def test_popup_wording_kept(self):
        self.assertIn('TEST NOTICE', src('src/game_xr.cpp'))

if __name__ == '__main__':
    unittest.main()
