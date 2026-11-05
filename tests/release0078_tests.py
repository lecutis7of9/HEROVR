"""HERO VR 0.0.78 (tag v0.0.78, EXPERIMENTAL): the cut21 build with the release stamps, VERSION 0.0.78 (newer than
0.0.77 in every shipped launcher, so the update is offered once), PORT-VERSION Update 39.11, launcher 0.0.78.1 build 21.
version.json carries no "launcher_build" (a new VERSION offers it; once installed the versions are equal, so no loop).
Immersive cutscenes stay the default and the Punisher's arsenal is the pistol and the pump. Made by Agent Mulder."""
import os, re, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def src(p):
    return open(os.path.join(ROOT, p), encoding='utf-8').read()


class Release0078(unittest.TestCase):
    def test_versions(self):
        self.assertEqual(src('VERSION').strip(), '0.0.78')
        self.assertEqual(src('PORT-VERSION').strip(), 'Update 39.11')
        c = src('CMakeLists.txt')
        self.assertIn('project(Spidy VERSION 0.0.78 ', c)
        self.assertRegex(c, r'option\(HERO_TEST_BUILD "[^"]*" OFF\)')
        v = src('include/spidy/version.hpp')
        self.assertIn('#define SPIDY_PUBLIC_VERSION "0.0.78"', v)
        self.assertIn('build = "39.11";', v)
        self.assertIn('Mod Version 0.0.78 (build Update 39.11)', src('SOURCE.txt'))
        self.assertIn('HERO VR 0.0.78 (v0.0.78): source code', src('SOURCE.txt'))

    def test_launcher_build_21(self):
        m = src('tools/launcher/src/ModVersion.cs')
        self.assertIn('public const string Version = "0.0.78.1";', m)
        self.assertIn('public const int Build = 21;', m)
        self.assertIn('PreviousTags = { "v0.0.75", "v0.0.74", "v0.0.73", "v0.0.63", "v0.0.62", "v0.0.6", "v0.0.5" };', m)
        a = src('tools/launcher/src/AssemblyInfo.cs')
        self.assertIn('[assembly: AssemblyVersion("0.0.78.1")]', a)
        self.assertIn('[assembly: AssemblyFileVersion("0.0.78.1")]', a)
        self.assertIn('[assembly: AssemblyInformationalVersion("0.0.78.1 (launcher build 21)")]', a)

    def test_same_version_update_rule_kept(self):
        self.assertIn('m.LauncherBuild > Math.Max(LauncherInfo.Build, SystemInfo.InstalledLauncherBuild(root))',
                      src('tools/launcher/src/MainForm.Updates.cs'))

    def test_release_notes(self):
        n = src('RELEASE-NOTES-v0.0.78.md')
        self.assertIn('**EXPERIMENTAL BUILD: 0.0.78 may crash or have issues. 0.0.63 is the most recent STABLE build**', n)
        self.assertIn('**0.0.78 is now available in the launcher.**', n)
        for bug in ("Sandman occasionally doesn't progress", "clean-up-the-house objective", "Mary Jane's tunnel chase",
                    'the UI can follow your eyes', 'gliding can be hard'):
            self.assertIn(bug, n)
        self.assertIn('HERO-VR-Launcher-0.0.78.zip', n)


if __name__ == '__main__':
    unittest.main()
