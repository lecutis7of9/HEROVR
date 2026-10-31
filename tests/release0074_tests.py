"""HERO VR 0.0.74 release (EXPERIMENTAL, launcher 0.0.74.1 build 15): no test labels, HERO VR 0.0.74 on the wrist and
the console, multiplayer (the lobby server list) kept, Revert offers 0.0.73 first, SM1STYLE and the window-edge fix on.
Made by Agent Mulder."""
import os, re, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def src(p):
    return open(os.path.join(ROOT, p), encoding='utf-8').read()


class ReleaseLabels0074(unittest.TestCase):
    def test_version(self):
        self.assertEqual(src('VERSION').strip(), '0.0.74')
        self.assertEqual(src('PORT-VERSION').strip(), 'Update 39.7')
        c = src('CMakeLists.txt')
        self.assertRegex(c, r'project\(Spidy VERSION 0\.0\.74 ')
        self.assertRegex(c, r'option\(HERO_TEST_BUILD "[^"]*" OFF\)')
        self.assertIn('#define SPIDY_PUBLIC_VERSION "0.0.74"', src('include/spidy/version.hpp'))
        self.assertIn('build = "39.7";', src('include/spidy/version.hpp'))
        self.assertTrue(os.path.exists(os.path.join(ROOT, 'RELEASE-NOTES-v0.0.74.md')))
        self.assertTrue(os.path.exists(os.path.join(ROOT, 'docs/HERO-0.0.74.md')))

    def test_console_and_wrist(self):
        g = src('src/game_xr.cpp')
        self.assertIn('message(1, 0, "HERO VR " SPIDY_PUBLIC_VERSION " - Made by Agent Mulder: starting OpenXR");', g)
        self.assertIn('f.text = "HERO VR " SPIDY_PUBLIC_VERSION " - MADE BY AGENT MULDER";', src('src/wrist_hud.cpp'))
        self.assertNotIn('note("Test build', g)

    def test_no_test_labels_in_help(self):
        for fn in ('include/spidy/combat38.hpp', 'src/holo_roster.cpp', 'src/roster_page.cpp'):
            for line in src(fn).splitlines():
                code = line.split('//')[0]
                self.assertIsNone(re.search(r'"[^"]*\b(?:TEST|test) [0-9][^"]*"', code), (fn, line))
                self.assertNotIn('this test build', code, fn)

    def test_launcher(self):
        m = src('tools/launcher/src/ModVersion.cs')
        self.assertIn('public const string Version = "0.0.74.1";', m)
        self.assertIn('public const int Build = 15;', m)
        self.assertIn('PreviousTag = "v0.0.73";', m)
        self.assertIn('PreviousTags = { "v0.0.73", "v0.0.63", "v0.0.62", "v0.0.6", "v0.0.5" };', m)
        self.assertIn('[assembly: AssemblyInformationalVersion("0.0.74.1 (launcher build 15)")]',
                      src('tools/launcher/src/AssemblyInfo.cs'))

    def test_multiplayer_kept(self):
        self.assertIn('DefaultUrl = "https://herovr-lobby.herovr.workers.dev"', src('tools/launcher/src/InternetLobby.cs'))
        self.assertTrue(os.path.exists(os.path.join(ROOT, 'src/mp_session.cpp')))

    def test_sm1style_and_window_edges_on(self):
        self.assertTrue(os.path.exists(os.path.join(ROOT, 'include/spidy/eye_cover.hpp')))
        self.assertIn('eye_cover', src('src/stereo_probe.cpp'))


if __name__ == '__main__':
    unittest.main()
