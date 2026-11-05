"""HERO VR 0.0.77 launcher-only hotfix (tag v0.0.77.1): launcher 0.0.77.2 build 20, mod VERSION still 0.0.77, so
version.json's "launcher_build": 20 is what offers it to 0.0.77 players (once: after the update the installed build is
20). The main screen gets a small SPECIAL THANKS TO: block under the subtitle; the Credits sheet line is gone so the
credit is not doubled. Made by Agent Mulder."""
import os, re, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def src(p):
    return open(os.path.join(ROOT, p), encoding='utf-8').read()


class Release00771(unittest.TestCase):
    def test_versions(self):
        # v0.0.78 moved VERSION to 0.0.78, PORT-VERSION to Update 39.11 and the launcher to 0.0.78.1 build 21
        # (release0078_tests pins them); the 0.0.77.2 build 20 stamps only need to be at or past it.
        self.assertGreaterEqual(tuple(int(x) for x in src('VERSION').strip().split('.')), (0, 0, 77))
        m = src('tools/launcher/src/ModVersion.cs')
        self.assertRegex(m, r'public const string Version = "0\.0\.7[7-9]\.\d+";')
        self.assertGreaterEqual(int(re.search(r'public const int Build = (\d+);', m).group(1)), 20)

    def test_thanks_block(self):
        c = src('tools/launcher/src/Cinematic.cs')
        self.assertIn('public string ThanksHeading = "SPECIAL THANKS TO:";', c)
        self.assertIn('"Jim" + "jam094 for immersive cutscenes"', c)
        self.assertIn('"Grundy_Chumbo for voice acting and testing"', c)
        self.assertIn('foreach (var t in Thanks)', c)
        # the wordmark control is tall enough for the three extra lines
        self.assertIn('new Wordmark(), 444, 80, 700, 260)', src('tools/launcher/src/MainForm.cs'))

    def test_credit_not_doubled(self):
        self.assertNotIn('jam094', src('tools/launcher/src/MainForm.Credits.cs'))

    def test_same_version_update_rule_kept(self):
        u = src('tools/launcher/src/MainForm.Updates.cs')
        self.assertIn('m.LauncherBuild > Math.Max(LauncherInfo.Build, SystemInfo.InstalledLauncherBuild(root))', u)


if __name__ == '__main__':
    unittest.main()
