"""HERO VR 0.0.78 WHAT'S NEW (the owner, t1879u): the launcher lists what changed in each update - version.json's
whats_new_history (make_version_json.py, from every RELEASE-NOTES-v*.md), the install's notes and GitHub's."""
import os, sys, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'launcher', 'tools'))
import make_version_json as mvj


def src(p):
    return open(os.path.join(ROOT, p), encoding='utf-8').read()


class WhatsNew(unittest.TestCase):
    def test_history_every_version(self):
        h = mvj.whats_new_history(ROOT)
        versions = [x['version'] for x in h]
        self.assertIn('0.0.77', versions)
        self.assertIn('0.0.76.1', versions)
        self.assertTrue(all(x['changes'] for x in h))
        keys = [tuple(int(p) for p in v.split('.')) for v in versions]
        self.assertEqual(keys, sorted(keys, reverse=True))

    def test_changes_parse(self):
        c = mvj.notes_changes("# T\n\n- **One.** first\n  more\n- `two`\n\ntext\n  not a bullet\n")
        self.assertEqual(c, ['One. first more', 'two'])

    def test_launcher(self):
        w = src('tools/launcher/src/WhatsNew.cs')
        self.assertIn('RELEASE-NOTES-v*.md', w)
        self.assertIn('WhatsNewHistory', w)
        self.assertIn('GitHub.All(repo)', src('tools/launcher/src/MainForm.Updates.cs'))
        self.assertIn('"whats_new_history"', src('tools/launcher/src/Manifest.cs'))
        self.assertIn('new AccentButton("What\'s New"', src('tools/launcher/src/MainForm.Updates.cs'))


if __name__ == '__main__':
    unittest.main()
