"""HERO VR 0.0.78 cut21 QUIET TRIM: the launcher's log box trims its oldest third without the edit a read-only
rich edit refuses with the Windows alert sound (SelectedText = "" is WM_CLEAR), so a long launcher run no longer
sounds an alert for every line once the box passes MaxChars."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
LAUNCHER = ROOT / 'tools' / 'launcher' / 'src'


def src(name):
    return (LAUNCHER / name).read_text(encoding='utf-8-sig')


class QuietTrim(unittest.TestCase):
    def test_no_empty_selected_text(self):
        for f in LAUNCHER.glob('*.cs'):
            code = '\n'.join(l.split('//')[0] for l in f.read_text(encoding='utf-8-sig').splitlines())
            self.assertNotRegex(code, r'SelectedText\s*=\s*""', f.name)
            self.assertNotRegex(code, r'SelectedText\s*=\s*(string\.Empty|null)\b', f.name)

    def test_trim_is_a_writable_replace(self):
        c = src('Controls.cs')
        self.assertIn('if (box.TextLength > MaxChars) Trim();', c)
        body = c[c.index('        void Trim()'):]
        body = body[:body.index('\n        }\n')]
        self.assertIn('box.Select(0, box.TextLength / 3);', body)
        self.assertLess(body.index('box.ReadOnly = false;'), body.index('EM_REPLACESEL'))
        self.assertIn('finally { box.ReadOnly = true; }', body)
        self.assertIn('if (box.TextLength > MaxChars) box.Clear();', body)
        self.assertRegex(c, r'const int EM_REPLACESEL = 0xC2;')
        self.assertIn('const int MaxChars = 400000;', c)


if __name__ == '__main__':
    unittest.main()
