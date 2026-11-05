"""HERO VR 0.0.78 cut18 QUIET LOG: no control characters (BEL is the Windows alert sound) reach the session console,
its console log, the launcher's log box or the start-failure window, and the log box swallows typed keys."""
import io
import pathlib
import re
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))


def src(p):
    return (ROOT / p).read_text(encoding='utf-8-sig')


def session():
    """run_game_vr's console pieces, taken from its source (the module itself needs Windows to import)."""
    import ast
    tree = ast.parse(src('tools/run_game_vr.py'))
    keep = [n for n in tree.body if (isinstance(n, ast.Assign) and any(getattr(t, 'id', '') == '_LOUD' for t in n.targets))
            or getattr(n, 'name', '') in ('quiet_text', '_Echo', 'keep_console')]
    assert len(keep) == 4, len(keep)
    space = {'sys': sys}
    exec(compile(ast.Module(body=keep, type_ignores=[]), 'run_game_vr.py', 'exec'), space)
    return type('Session', (), {k: staticmethod(v) if callable(v) and not isinstance(v, type) else v
                                for k, v in space.items() if not k.startswith('__')})


class QuietLog(unittest.TestCase):
    def test_quiet_text(self):
        run_game_vr = session()
        self.assertEqual(run_game_vr.quiet_text('a\x07b\x00c\x1bd\x7fe\tf\r\n'), 'a?b?c?d?e\tf\r\n')
        self.assertEqual(run_game_vr.quiet_text('plain line'), 'plain line')
        self.assertEqual(run_game_vr.quiet_text('\x07' * 40), '?' * 40)

    def test_console_and_log_are_quiet(self):
        run_game_vr = session()
        with tempfile.TemporaryDirectory() as folder:
            report = pathlib.Path(folder) / 'reports' / 'game-vr-3.json'
            stdout, stderr = sys.stdout, sys.stderr
            shown, errors = sys.stdout, sys.stderr = io.StringIO(), io.StringIO()
            try:
                path = run_game_vr.keep_console(report)
                print('ding\x07\x07 done')
                print('err\x08', file=sys.stderr)
                sys.stdout.log.close()
            finally:
                sys.stdout, sys.stderr = stdout, stderr
            self.assertEqual((shown.getvalue(), errors.getvalue()), ('ding?? done\n', 'err?\n'))
            self.assertNotIn('\x07', path.read_text(encoding='utf-8'))

    def test_quiet_without_a_log(self):
        run_game_vr = session()
        shown = io.StringIO()
        echo = run_game_vr._Echo(shown, None)
        self.assertEqual(echo.write('x\x07'), 2)
        self.assertEqual(shown.getvalue(), 'x?')

    def test_launcher_log_box(self):
        c = src('tools/launcher/src/Controls.cs')
        box = c[c.index('class LogBox'):]
        self.assertIn('public static string Quiet(string text)', box)
        self.assertIn("c == '\\x7f'", box)
        self.assertLess(box.index('text = Quiet(text);'), box.index('box.AppendText('))
        self.assertIn('e.SuppressKeyPress = true', box)
        self.assertIn('box.KeyPress += (s, e) => { e.Handled = true; };', box)

    def test_start_failure_window_is_silent(self):
        ps = src('tools/launch-game-vr.ps1')
        self.assertNotIn("'OK', 'Warning'", ps)
        self.assertIn("MessageBox]::Show($text, 'HERO VR', 'OK', 'None'", ps)
        self.assertIn("$text = $text -replace '[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F\\x7F]', '?'", ps)

    def test_no_beep_calls(self):
        for p in list((ROOT / 'tools').glob('*.py')) + list((ROOT / 'tools').glob('*.ps1')) + \
                list((ROOT / 'tools/launcher/src').glob('*.cs')):
            t = p.read_text(encoding='utf-8-sig', errors='replace')
            self.assertIsNone(re.search(r'\[console\]::Beep|MessageBeep|SystemSounds\.|winsound\.', t, re.I), p.name)
            if p.suffix == '.ps1':
                self.assertIsNone(re.search(r'`a(?!\w)', t), p.name)  # PowerShell's BEL escape


if __name__ == '__main__':
    unittest.main()
