"""HERO VR 0.0.77 (tag v0.0.77, EXPERIMENTAL): the cut8 build with the release stamps, VERSION 0.0.77 (newer than
0.0.76 in every shipped launcher, so the update is offered once), PORT-VERSION Update 39.10, launcher 0.0.77.1 build 19.
version.json carries no "launcher_build". (0.0.77.2 moved the immersive cutscenes credit to the main screen's SPECIAL THANKS TO: block.)
Made by Agent Mulder."""
import os, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def src(p):
    return open(os.path.join(ROOT, p), encoding='utf-8').read()


class Release0077(unittest.TestCase):
    def test_versions(self):
        self.assertEqual(src('VERSION').strip(), '0.0.77')
        self.assertEqual(src('PORT-VERSION').strip(), 'Update 39.10')
        c = src('CMakeLists.txt')
        self.assertIn('project(Spidy VERSION 0.0.77 ', c)
        self.assertRegex(c, r'option\(HERO_TEST_BUILD "[^"]*" OFF\)')
        v = src('include/spidy/version.hpp')
        self.assertIn('#define SPIDY_PUBLIC_VERSION "0.0.77"', v)
        self.assertIn('build = "39.10";', v)
        self.assertIn('Mod Version 0.0.77 (build Update 39.10)', src('SOURCE.txt'))
        self.assertTrue(os.path.exists(os.path.join(ROOT, 'RELEASE-NOTES-v0.0.77.md')))

    def test_launcher_build_19_or_later(self):
        # 0.0.77.1 build 19 shipped with v0.0.77; launcher-only hotfixes of 0.0.77 (0.0.77.2 build 20, tag v0.0.77.1) follow.
        import re
        m = src('tools/launcher/src/ModVersion.cs')
        self.assertRegex(m, r'public const string Version = "0\.0\.77\.\d+";')
        self.assertGreaterEqual(int(re.search(r'public const int Build = (\d+);', m).group(1)), 19)
        self.assertIn('PreviousTags = { "v0.0.75", "v0.0.74", "v0.0.73", "v0.0.63", "v0.0.62", "v0.0.6", "v0.0.5" };', m)

    def test_same_version_update_rule_kept(self):
        self.assertIn('m.LauncherBuild > ', src('tools/launcher/src/MainForm.Updates.cs'))


if __name__ == '__main__':
    unittest.main()
