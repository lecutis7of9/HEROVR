"""0.0.73 test 2 DESKTOP SHORTCUT (fix 20): the test build installer asks once, in a popup with HERO VR's own
icon, to put a HERO VR shortcut on the desktop. It is skipped by No (or -Yes, unattended), never stops the
install, and the icon ships in the payload."""
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = os.path.join(ROOT, 'tools', 'test073')


def read(*p):
    with open(os.path.join(ROOT, *p), encoding='utf-8') as f:
        return f.read()


class Shortcut(unittest.TestCase):
    def test_icon(self):
        with open(os.path.join(T, 'HERO-VR.ico'), 'rb') as f:
            b = f.read()
        self.assertEqual(b[:4], b'\x00\x00\x01\x00')  # an icon file
        self.assertGreaterEqual(int.from_bytes(b[4:6], 'little'), 5)  # 16 to 256 pixels
        self.assertIn("HERO-VR.ico", read('tools', 'test073', 'make_test.py'))

    def test_installer(self):
        s = read('tools', 'test073', 'INSTALL-TEST-BUILD.ps1')
        s.encode('ascii')  # Windows PowerShell 5.1 reads a file without a BOM as ANSI
        step, launch = s.index('# 3b. 0.0.73 test 2 (fix 20)'), s.index("# 4. That folder's own launcher")
        self.assertLess(s.index('INSTALLED into'), step)
        self.assertLess(step, launch)
        body = s[step:launch]
        for want in ('AskShortcut', "'HERO VR.lnk'", 'IconLocation', "the install is fine", 'delete it to undo', '$NoShortcut',
                     'Read-Host'):
            self.assertIn(want, body)
        self.assertNotIn('Fail ', body)  # the shortcut never fails the install
        self.assertNotIn('exit 1', body)

    @unittest.skipUnless(shutil.which(os.environ.get('PWSH', 'pwsh')), 'no PowerShell')
    def test_runs_without_a_desktop(self):
        with tempfile.TemporaryDirectory() as d:
            stage, mod, home = os.path.join(d, 'stage'), os.path.join(d, 'mod'), os.path.join(d, 'home')
            os.makedirs(os.path.join(stage, 'payload')), os.makedirs(os.path.join(mod, 'tools', 'launcher'))
            os.makedirs(os.path.join(home, 'Desktop'))
            open(os.path.join(mod, 'tools', 'launch-game-vr.ps1'), 'w').write('x')
            open(os.path.join(mod, 'tools', 'launcher', 'SM2VRLauncher.exe'), 'w').write('x')
            shutil.copy(os.path.join(T, 'INSTALL-TEST-BUILD.ps1'), stage)
            shutil.copy(os.path.join(T, 'HERO-VR.ico'), os.path.join(stage, 'payload'))
            open(os.path.join(stage, 'payload', 'HERO-VR-TEST-BUILD.txt'), 'w').write('HERO VR 0.0.73 test abc1234 - private test build 2 (not a release)\n')
            import hashlib
            with open(os.path.join(stage, 'payload', 'SHA256SUMS.txt'), 'w') as f:
                for n in ('HERO-VR.ico', 'HERO-VR-TEST-BUILD.txt'):
                    f.write(hashlib.sha256(open(os.path.join(stage, 'payload', n), 'rb').read()).hexdigest() + '  ' + n + '\n')
            for extra, stdin in ((['-Yes'], ''), (['-Yes', '-Shortcut'], ''), ([], '\nn\n'), ([], '\ny\n')):
                r = subprocess.run([os.environ.get('PWSH', 'pwsh'), '-NoProfile', '-File', os.path.join(stage, 'INSTALL-TEST-BUILD.ps1'),
                                    '-Target', mod, '-NoLaunch'] + extra, input=stdin, capture_output=True, text=True,
                                   env=dict(os.environ, HOME=home), timeout=120)
                self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
                self.assertIn('INSTALLED into', r.stdout)
                self.assertIn('Desktop shortcut', r.stdout)
            self.assertTrue(os.path.exists(os.path.join(mod, 'HERO-VR.ico')))


if __name__ == '__main__':
    unittest.main()
