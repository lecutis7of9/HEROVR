"""HERO VR 0.0.77 (tag v0.0.77, EXPERIMENTAL): the cut8 build with the release stamps, VERSION 0.0.77 (newer than
0.0.76 in every shipped launcher, so the update is offered once), PORT-VERSION Update 39.10, launcher 0.0.77.1 build 19.
version.json carries no "launcher_build". The launcher's Credits page carries the one immersive cutscenes credit line.
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

    def test_launcher_build_19(self):
        m = src('tools/launcher/src/ModVersion.cs')
        self.assertIn('public const string Version = "0.0.77.1";', m)
        self.assertIn('public const int Build = 19;', m)
        self.assertIn('PreviousTags = { "v0.0.75", "v0.0.74", "v0.0.73", "v0.0.63", "v0.0.62", "v0.0.6", "v0.0.5" };', m)
        a = src('tools/launcher/src/AssemblyInfo.cs')
        self.assertIn('[assembly: AssemblyVersion("0.0.77.1")]', a)
        self.assertIn('[assembly: AssemblyInformationalVersion("0.0.77.1 (launcher build 19)")]', a)

    def test_credit_line(self):
        self.assertIn('line().Add("immersive cutscenes by " + "jim" + "jam094", Theme.Ui(11f), Theme.Dim);',
                      src('tools/launcher/src/MainForm.Credits.cs'))

    def test_same_version_update_rule_kept(self):
        self.assertIn('m.LauncherBuild > ', src('tools/launcher/src/MainForm.Updates.cs'))


if __name__ == '__main__':
    unittest.main()
