"""HERO VR 0.0.76 mpworld: Spider-Man 1 paused. The launcher hides the SPIDER-MAN 1 choice (the code stays) and the
test zip does not ship the SM1 add-on. Made by Agent Mulder."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = ROOT / 'tools' / 'launcher' / 'src'


def text(p):
    return p.read_text(encoding='utf-8')


class Sm1Paused(unittest.TestCase):
    def test_flag(self):
        self.assertIn('public static readonly bool Paused = true;', text(SRC / 'Sm1.cs'))

    def test_picker_hidden_and_pick_ignored(self):
        m = text(SRC / 'MainForm.Sm1.cs')
        self.assertIn('bool Sm1Selected { get { return !Sm1.Paused && settings != null', m)
        body = m[m.index('void BuildGamePicker()'):]
        self.assertTrue(body.split('\n')[2].strip().startswith('if (Sm1.Paused) return;'))
        self.assertIn('if (!Sm1.Paused && settings.GetBool(Sm1.KeyNews, true))', m)

    def test_code_kept(self):
        m = text(SRC / 'MainForm.Sm1.cs')
        for kept in ('class GamePicker', 'void Sm1Play()', 'void BuildSm1()', 'Sm1.RemoveLayers()'):
            self.assertIn(kept, m)
        self.assertTrue((SRC / 'Sm1Session.cs').is_file())
        self.assertTrue((ROOT / 'src' / 'sm1' / 'CMakeLists.txt').is_file())

    def test_zip_without_addon(self):
        pkg = text(ROOT / 'tools' / 'package.ps1')
        self.assertRegex(pkg, re.compile(r'^\$shipSm1 = \$false$', re.M))
        i = pkg.index('if ($shipSm1) {')
        self.assertLess(i, pkg.index("'herovr_sm1_addon.dll'"))
        self.assertLess(i, pkg.index("@('herovr_sm1_layer.dll', 'herovr_sm1_layer.json')"))
        wf = text(ROOT / '.github' / 'workflows' / 'build-prebuilt.yml')
        self.assertIn("but SM1 is paused", wf)
        self.assertIn("-Pattern '^\\$shipSm1 = \\$true'", wf)

    def test_names(self):
        for p in (SRC / 'Sm1.cs', SRC / 'MainForm.Sm1.cs', ROOT / 'tools' / 'package.ps1', pathlib.Path(__file__)):
            low = text(p).lower()
            self.assertNotIn('jim' + 'jam', low)


if __name__ == '__main__':
    unittest.main()
