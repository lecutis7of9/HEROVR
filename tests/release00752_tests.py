"""HERO VR 0.0.75 hotfix 2 (v0.0.75.2, EXPERIMENTAL): VERSION stays 0.0.75, PORT-VERSION Update 39.8.2, launcher
0.0.75.3 build 18 (version.json "launcher_build": 18 offers it to 0.0.75 and 0.0.75.1 players), Revert offers 0.0.75
first; the Spider-Man 1 hero suits (additive, launcher-installed) and the wrist holo's suit hint. Made by Agent Mulder."""
import os, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def src(p):
    return open(os.path.join(ROOT, p), encoding='utf-8').read()


class Release00752(unittest.TestCase):
    def test_versions(self):
        self.assertEqual(src('VERSION').strip(), '0.0.75')
        self.assertEqual(src('PORT-VERSION').strip(), 'Update 39.8.2')
        self.assertRegex(src('CMakeLists.txt'), r'option\(HERO_TEST_BUILD "[^"]*" OFF\)')
        self.assertIn('build = "39.8.2";', src('include/spidy/version.hpp'))
        self.assertTrue(os.path.exists(os.path.join(ROOT, 'RELEASE-NOTES-v0.0.75.2.md')))

    def test_launcher_build_18(self):
        m = src('tools/launcher/src/ModVersion.cs')
        self.assertIn('public const string Version = "0.0.75.3";', m)
        self.assertIn('public const int Build = 18;', m)
        self.assertIn('public const string PreviousTag = "v0.0.75";', m)
        self.assertIn('public const string HotfixOf = "v0.0.75";', m)
        self.assertIn('public const string HotfixBuild = "Update 39.8.2";', m)
        self.assertIn('PreviousTags = { "v0.0.75", "v0.0.74", "v0.0.73", "v0.0.63", "v0.0.62", "v0.0.6", "v0.0.5" };', m)
        self.assertIn('[assembly: AssemblyInformationalVersion("0.0.75.3 (launcher build 18)")]',
                      src('tools/launcher/src/AssemblyInfo.cs'))

    def test_still_experimental(self):
        r = src('README-HERO-VR.md')
        self.assertIn('EXPERIMENTAL BUILD', r)
        self.assertIn('0.0.63 is the most recent STABLE build', r)

    def test_notes_say_what_is_not_in(self):
        n = src('RELEASE-NOTES-v0.0.75.2.md').lower()
        for word in ('untested', 'held props', 'grapple', 'vehicles', 'suits menu'):
            self.assertIn(word, n)

    def test_sm1_suits_in(self):
        self.assertIn('def check_additive', src('tools/sm1_suits/sm1suits.py'))
        self.assertIn('suitHint', src('src/sm1/sm1_core.cpp'))
        self.assertIn('pickSuit(config.hero);', src('src/sm1/sm1_layer.cpp'))


if __name__ == '__main__':
    unittest.main()
