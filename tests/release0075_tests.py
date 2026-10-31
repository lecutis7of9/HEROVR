"""HERO VR 0.0.75 release (EXPERIMENTAL, launcher 0.0.75.1 build 16): no test labels, HERO VR 0.0.75 on the wrist and
the console, multiplayer (the lobby server list) kept, Revert offers 0.0.74 first, then 0.0.73, SM1STYLE and the window-edge fix on.
Made by Agent Mulder."""
import os, re, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def src(p):
    return open(os.path.join(ROOT, p), encoding='utf-8').read()


class ReleaseLabels0075(unittest.TestCase):
    def test_version(self):
        self.assertEqual(src('VERSION').strip(), '0.0.75')
        self.assertEqual(src('PORT-VERSION').strip(), 'Update 39.8')
        c = src('CMakeLists.txt')
        self.assertRegex(c, r'project\(Spidy VERSION 0\.0\.75 ')
        self.assertRegex(c, r'option\(HERO_TEST_BUILD "[^"]*" OFF\)')
        self.assertIn('#define SPIDY_PUBLIC_VERSION "0.0.75"', src('include/spidy/version.hpp'))
        self.assertIn('build = "39.8";', src('include/spidy/version.hpp'))
        self.assertTrue(os.path.exists(os.path.join(ROOT, 'RELEASE-NOTES-v0.0.75.md')))
        self.assertTrue(os.path.exists(os.path.join(ROOT, 'docs/HERO-0.0.75.md')))

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
        self.assertIn('public const string Version = "0.0.75.1";', m)
        self.assertIn('public const int Build = 16;', m)
        self.assertIn('PreviousTag = "v0.0.74";', m)
        self.assertIn('PreviousTags = { "v0.0.74", "v0.0.73", "v0.0.63", "v0.0.62", "v0.0.6", "v0.0.5" };', m)
        self.assertIn('[assembly: AssemblyInformationalVersion("0.0.75.1 (launcher build 16)")]',
                      src('tools/launcher/src/AssemblyInfo.cs'))

    def test_multiplayer_kept(self):
        self.assertIn('DefaultUrl = "https://herovr-lobby.herovr.workers.dev"', src('tools/launcher/src/InternetLobby.cs'))
        self.assertTrue(os.path.exists(os.path.join(ROOT, 'src/mp_session.cpp')))

    def test_sm1style_and_window_edges_on(self):
        self.assertTrue(os.path.exists(os.path.join(ROOT, 'include/spidy/eye_cover.hpp')))
        self.assertIn('eye_cover', src('src/stereo_probe.cpp'))


    def test_motorcycle_assets_checked(self):
        import sys; sys.path.insert(0, os.path.join(ROOT, 'tools', 'release072'))
        import required_assets as ra
        for f in ('Models/Vehicle/Motorcycle/model.glb', 'Models/Vehicle/Motorcycle/Handlebars/model.glb',
                  'Models/Vehicle/Motorcycle/layout.json', 'Models/Vehicle/Motorcycle/license.txt',
                  'Models/Vehicle/Motorcycle/Handlebars/license.txt'):
            self.assertIn(f, ra.RELEASE075_MODEL_FILES)

    def test_sm1_addon_optional(self):
        self.assertIn("herovr_sm1_addon.dll is missing: Spider-Man 1 then starts iilyn''s mod without the add-on", src('tools/package.ps1'))
        self.assertIn('No starts his mod without the add-on this time.', src('tools/launcher/src/Sm1.cs'))

    def test_ci_builds_the_release_branch(self):
        w = src('.github/workflows/build-prebuilt.yml')
        self.assertEqual(w.count("startsWith(github.ref_name, 'hero-0.0.75')"), w.count("startsWith(github.ref_name, 'hero-0.0.74')"))


if __name__ == '__main__':
    unittest.main()
