"""0.0.62 PLAY VR gate (6:50 report: "i hit play it turns grey then back to red nothing happens").

The 0.0.6.1 dev launcher (build 2) resets PLAY VR without a window when a session stops with an error: the reason
went only to its session log. These tests run tools/launch-game-vr.ps1 in a staged 0.0.62 package with a fake
python.exe (pwsh on Linux) and check:
  - every option build 2 can pass still binds, and the session reaches Python;
  - a session that stops with exit 1, 2, 5, 6 or a throw before Python says why in a window for a launcher older
    than build 9 (SM2VR_LAUNCHER_BUILD unset), and stays quiet for build 9 (which says it itself);
  - run_game_vr.py writes reports/last-vr-error.txt on every failure, and clears it at every start;
  - launcher build 9 shows a window for every nonzero exit that is not a stop the player asked for.
Env: PWSH (default ~/pwsh/pwsh), HEROVR_LAUNCHER (default /workspace/herovr-launcher-0062), STRICT=1 turns skips
into failures."""
import os, pathlib, re, shutil, subprocess, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
PWSH = os.environ.get('PWSH', os.path.expanduser('~/pwsh/pwsh'))
LAUNCHER = pathlib.Path(os.environ.get('HEROVR_LAUNCHER', '/workspace/herovr-launcher-0062'))
STRICT = os.environ.get('STRICT') == '1'
# Build 2 (0.0.6.1) LaunchOptions: every switch it can pass (its options file, filtered by the script's params).
BUILD2_ARGS = ['-RenderScale', '100', '-SmallerDesktopView', '-GameMonitorView', '-GameWebs']
NATIVE = ['spidy_headset_probe.exe', 'spidy_bridge.dll', 'spidy_render_probe.dll']


def need(cond, why):
    if not cond:
        if STRICT: raise AssertionError('missing: ' + why)
        raise unittest.SkipTest(why)


class LaunchScript(unittest.TestCase):
    def stage(self, exit_code=0, reason=None):
        need(os.path.exists(PWSH) and os.name != 'nt', 'PowerShell 7 at ' + PWSH)
        d = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, d, True)
        shutil.copytree(ROOT / 'tools', d / 'tools', ignore=shutil.ignore_patterns('__pycache__', 'launcher'))
        nat = d / 'build' / 'windows-ninja'; nat.mkdir(parents=True)
        src = re.search(r"foreach\(\$name in @\(([^)]*)\)", (ROOT / 'tools/launch-game-vr.ps1').read_text())
        for n in re.findall(r"'([^']+)'", src.group(1)) if src else NATIVE:
            (nat / n).write_bytes(b'x')
        (d / 'python').mkdir()
        py = d / 'python' / 'python.exe'
        body = '#!/bin/sh\n'
        body += 'case "$1" in -c|-V|--version) exec python3 "$@";; esac\n'  # the script's own version probe
        body += 'echo "ARGS $*"\n'
        if reason: body += f'mkdir -p "{d}/reports"; echo "{reason}" > "{d}/reports/last-vr-error.txt"\n'
        body += f'exit {exit_code}\n'
        py.write_text(body); py.chmod(0o755)
        return d

    def run_script(self, d, build=None, args=BUILD2_ARGS):
        env = dict(os.environ, SM2VR_LAUNCHER='1')
        env.pop('SM2VR_LAUNCHER_BUILD', None)
        if build is not None: env['SM2VR_LAUNCHER_BUILD'] = str(build)
        # The command both launchers run (PsRunner.Start).
        cmd = ("try { & '" + str(d / 'tools/launch-game-vr.ps1') + "' " + ' '.join(args) +
               "; if ($LASTEXITCODE) { exit $LASTEXITCODE } else { exit 0 } } "
               "catch { [Console]::Error.WriteLine('ERROR: ' + $_.Exception.Message); exit 1 }")
        r = subprocess.run([PWSH, '-NoProfile', '-NonInteractive', '-Command', cmd], cwd=d, env=env,
                           capture_output=True, text=True, timeout=120)
        return r.returncode, r.stdout + r.stderr

    def test_build_2_options_reach_python(self):
        code, out = self.run_script(self.stage(0))
        self.assertEqual(code, 0, out[-2000:])
        self.assertIn('ARGS ', out)
        self.assertIn('run_game_vr.py', out)
        self.assertNotIn('older than the mod', out)

    def test_every_failure_is_said_for_an_older_launcher(self):
        for code, reason, words in [(6, None, 'already running'), (5, None, 'administrator'),
                                    (2, 'Spidy VR unavailable: no headset runtime', 'no headset runtime'),
                                    (1, 'Spidy VR stopped on an internal error: ImportError: x', 'internal error')]:
            with self.subTest(code=code):
                rc, out = self.run_script(self.stage(code, reason))
                self.assertEqual(rc, code, out[-2000:])
                self.assertIn(words, out)
                self.assertIn('older than the mod', out)
                rc9, out9 = self.run_script(self.stage(code, reason), build=9)
                self.assertEqual(rc9, code)
                self.assertNotIn('older than the mod', out9)

    def test_a_throw_before_python_is_said(self):
        d = self.stage(0)
        (d / 'python' / 'python.exe').unlink()
        rc, out = self.run_script(d)
        self.assertEqual(rc, 1)
        self.assertIn("own Python is missing", out)
        self.assertIn('older than the mod', out)
        self.assertIn('ERROR: ', out)  # still the launcher's own error line too

    def test_a_player_stop_is_quiet(self):
        rc, out = self.run_script(self.stage(130))
        self.assertEqual(rc, 130)
        self.assertNotIn('older than the mod', out)


class RunGameVr(unittest.TestCase):
    def test_failures_leave_a_reason(self):
        s = (ROOT / 'tools/run_game_vr.py').read_text()
        main = s[s.index("if __name__ == '__main__':"):]
        self.assertIn("START_ERROR.unlink()", main)
        self.assertEqual(main.count('note_start_error('), 2, 'both failure branches write the reason')
        self.assertIn('except Exception as error', main)


class Build9(unittest.TestCase):
    def setUp(self):
        need((LAUNCHER / 'src/MainForm.cs').exists(), 'launcher build 9 sources at ' + str(LAUNCHER))
        self.main = (LAUNCHER / 'src/MainForm.cs').read_text()

    def test_every_nonzero_exit_shows_a_window(self):
        h = self.main[self.main.index('vr.Exited += code => Ui('):]
        h = h[:h.index('job = Job.Vr;')]
        self.assertIn('bool userStopped = stopAsked != DateTime.MinValue;', h)
        self.assertRegex(h, r'else if \(!userStopped && code != 130\)\s*\{[^}]*MessageBox\.Show\(this, StartFailureText\(code, last\)')
        self.assertIn('internal static string StartFailureText', self.main)

    def test_it_tells_the_script_its_build(self):
        ps = (LAUNCHER / 'src/PsRunner.cs').read_text()
        self.assertIn('psi.EnvironmentVariables["SM2VR_LAUNCHER_BUILD"] = LauncherInfo.Build.ToString();', ps)

    def test_powershell_that_cannot_start_is_said(self):
        ps = (LAUNCHER / 'src/PsRunner.cs').read_text()
        self.assertRegex(ps, r'catch \(Exception ex\)\s*\{\s*if \(Output != null\) Output\("Could not start PowerShell: "')
        self.assertIn('if (Exited != null) Exited(-1);', ps)
        self.assertIn('code == -1 ?', self.main)


if __name__ == '__main__':
    unittest.main()
