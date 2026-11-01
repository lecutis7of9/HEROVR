"""HERO VR 0.0.75 hotfix 1 (v0.0.75.1, EXPERIMENTAL): VERSION stays 0.0.75 (semver: a four-part 0.0.75.1 would read as
older than 0.0.75 in every shipped launcher), PORT-VERSION Update 39.8.1 (so the update replaces the mod files),
launcher 0.0.75.2 build 17 (version.json "launcher_build": 17 offers it to 0.0.75 players), Revert offers 0.0.75 first,
the see-through depth lock freeze fix in. Made by Agent Mulder."""
import os, re, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def src(p):
    return open(os.path.join(ROOT, p), encoding='utf-8').read()


class Release00751(unittest.TestCase):
    def test_versions(self):
        self.assertEqual(src('VERSION').strip(), '0.0.75')
        self.assertRegex(src('PORT-VERSION').strip(), r'^Update 39\.8\.[12]$')  # 39.8.2: hotfix 2 (release00752_tests)
        self.assertRegex(src('CMakeLists.txt'), r'option\(HERO_TEST_BUILD "[^"]*" OFF\)')
        self.assertRegex(src('include/spidy/version.hpp'), r'build = "39\.8\.[12]";')
        self.assertTrue(os.path.exists(os.path.join(ROOT, 'RELEASE-NOTES-v0.0.75.1.md')))

    def test_launcher_build_17(self):
        m = src('tools/launcher/src/ModVersion.cs')
        self.assertRegex(m, r'public const string Version = "0\.0\.75\.[23]";')
        self.assertRegex(m, r'public const int Build = 1[78];')  # 18: hotfix 2
        self.assertIn('public const string PreviousTag = "v0.0.75";', m)
        self.assertIn('public const string HotfixOf = "v0.0.75";', m)
        self.assertRegex(m, r'public const string HotfixBuild = "Update 39\.8\.[12]";')
        self.assertIn('PreviousTags = { "v0.0.75", "v0.0.74", "v0.0.73", "v0.0.63", "v0.0.62", "v0.0.6", "v0.0.5" };', m)
        self.assertRegex(src('tools/launcher/src/AssemblyInfo.cs'),
                         r'AssemblyInformationalVersion\("0\.0\.75\.[23] \(launcher build 1[78]\)"\)')

    def test_revert_offers_the_release_it_fixes(self):
        rd = src('tools/launcher/src/RevertDialog.cs')
        self.assertIn('string.Equals(tag, LauncherInfo.HotfixOf, StringComparison.OrdinalIgnoreCase)', rd)
        self.assertIn('string.Equals(InstalledBuild(root), LauncherInfo.HotfixBuild, StringComparison.OrdinalIgnoreCase)', rd)

    def test_same_version_update_rule_kept(self):
        u = src('tools/launcher/src/MainForm.Updates.cs')
        self.assertIn('m.LauncherBuild > ', u)

    def test_freeze_fix_in(self):
        g = src('src/native_eye_gpu.cpp')
        self.assertIn('// no depthTagLock held here', g)


if __name__ == '__main__':
    unittest.main()
