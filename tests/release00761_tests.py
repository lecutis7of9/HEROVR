"""HERO VR 0.0.76 in the launcher (tag v0.0.76.1, EXPERIMENTAL): the v0.0.76 files with only the version stamps moved,
VERSION 0.0.76 (newer than 0.0.75 in every shipped launcher, so the update is offered), PORT-VERSION Update 39.9
(the update replaces the mod files), launcher 0.0.76.1 build 18. version.json carries no "launcher_build" (a new
VERSION offers it; once installed the versions are equal, so it is not offered again). Made by Agent Mulder."""
import os, re, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def src(p):
    return open(os.path.join(ROOT, p), encoding='utf-8').read()


class Release00761(unittest.TestCase):
    def test_versions(self):
        # v0.0.77 moved VERSION to 0.0.77, PORT-VERSION to Update 39.10 and the launcher to build 19 (release0077_tests pins them)
        self.assertGreaterEqual(tuple(int(x) for x in src('VERSION').strip().split('.')), (0, 0, 76))
        c = src('CMakeLists.txt')
        self.assertRegex(c, r'project\(Spidy VERSION 0\.0\.7[6-9] ')
        self.assertRegex(c, r'option\(HERO_TEST_BUILD "[^"]*" OFF\)')
        self.assertRegex(src('include/spidy/version.hpp'), r'#define SPIDY_PUBLIC_VERSION "0\.0\.7[6-9]"')
        self.assertTrue(os.path.exists(os.path.join(ROOT, 'RELEASE-NOTES-v0.0.76.1.md')))

    def test_launcher_build_18(self):
        m = src('tools/launcher/src/ModVersion.cs')
        self.assertGreaterEqual(int(re.search(r'public const int Build = (\d+);', m).group(1)), 18)
        self.assertIn('PreviousTags = { "v0.0.75", "v0.0.74", "v0.0.73", "v0.0.63", "v0.0.62", "v0.0.6", "v0.0.5" };', m)

    def test_same_version_update_rule_kept(self):
        self.assertIn('m.LauncherBuild > ', src('tools/launcher/src/MainForm.Updates.cs'))


if __name__ == '__main__':
    unittest.main()
