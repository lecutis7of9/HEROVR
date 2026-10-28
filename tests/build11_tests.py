"""HERO VR 0.0.72 test build 11 (source gates): the wrist names TEST <n>, FAR WINDOWS' CANDIDATE write is off,
the eyes' fov margin is applied where the views are located, and the installer names the wrist line."""
import os, re, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()

class Build11(unittest.TestCase):
    def test_wrist_and_start_line_name_the_build(self):
        self.assertIn('"TEST " SPIDY_TEST_NUMBER " " SPIDY_TEST_COMMIT', src('src/wrist_hud.cpp'))
        self.assertIn('"HERO VR 0.0.72 TEST " SPIDY_TEST_NUMBER " " SPIDY_TEST_COMMIT', src('src/game_xr.cpp'))
        self.assertRegex(src('CMakeLists.txt'), r'set\(HERO_TEST_NUMBER "11"')
    def test_far_windows_candidate_write_gated(self):
        s = src('src/stereo_probe.cpp')
        self.assertIn('constexpr bool farWindowsCandidateWrite = false;', s)
        self.assertIn('farWindowsCandidateWrite && result && farWindowsReady', s)
    def test_eye_margin_where_views_are_located(self):
        s = src('src/xr_session.cpp')
        i = s.index('xrLocateViews(session_')
        j = s.index('tracked.eyes[i] = {pose(views[i].pose)', i)
        self.assertEqual(s[i:j].count('eye_margin::widen('), 4)
    def test_installer_names_wrist_line(self):
        s = src('tools/release072/INSTALL-TEST-BUILD.ps1')
        self.assertIn('On the wrist (bottom line) you must see: TEST', s)
        self.assertIn('NOT updated (do not start HERO VR from here)', s)
        # the popup's wording is untouched
        self.assertIn('TEST NOTICE', src('src/game_xr.cpp'))

if __name__ == '__main__':
    unittest.main()
