"""0.0.70.1 hotfix gate: "[WinError 6] The handle is invalid" at stage "finding the headset" (0.0.7 report, Oct 27).

The launcher starts PowerShell (and through it the Python session) with UseShellExecute=false, redirecting only
stdout and stderr, so the session gets the launcher's own standard input handle. After a Stop VR (PsRunner.RequestStop:
FreeConsole, AttachConsole(session), Ctrl+C, FreeConsole) that handle names a console that is gone. Python's subprocess
duplicates the standard input of every program it starts with stdin not given; with a dead handle that raises
WinError 6. The first such call of a session is xr_runtime.probe, right after "Looking for the headset in ...".

Checked here, on any OS:
  - every subprocess call of the tools a session imports passes stdin= (no call depends on the inherited handle);
  - session() gets past "finding the headset" with a dead standard input, emulated as Windows' subprocess does it
    (and the emulation does raise WinError 6 for a call without stdin=, so the test is not empty);
  - std_handles.repair() replaces a dead standard handle with NUL and leaves good and missing ones;
  - the launcher (build 12) keeps its own standard handles valid: saved and put back around RequestStop's console
    calls, and dead ones cleared before PowerShell starts and when the launcher starts (also after a self-update).
On Windows only (the CI release build, windows-2022): a real child Python whose standard input is a closed handle (evidence:
handle reuse can hide it) reproduces WinError 6 without the fix and starts programs with it; and tests/windows/std_handles_repro.cs runs the
launcher's own PsRunner after a real Stop (see windows_launcher_repro)."""
import ast
import io
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOLS = ROOT/'tools'
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT/'tests'))
import std_handles  # noqa: E402

CALLS = {'run', 'Popen', 'call', 'check_call', 'check_output'}


def session_tools():
    """The tools a VR session imports (tools/package_manifest.py, as tools/package.ps1 packages them)."""
    import package_manifest
    out = io.StringIO()
    with patch('sys.stdout', out), patch.object(sys, 'argv', ['package_manifest.py', 'list', '--tools', str(TOOLS)]):
        try:
            package_manifest.main()
        except SystemExit:
            pass
    import json
    return json.loads(out.getvalue())['tools']


def calls_without_stdin(path):
    tree = ast.parse(path.read_text(encoding='utf-8'), str(path))
    bad = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and node.func.attr in CALLS \
                and isinstance(node.func.value, ast.Name) and node.func.value.id == 'subprocess':
            keys = {k.arg for k in node.keywords}
            if not keys & {'stdin', 'input'}:
                bad.append(f'{path.name}:{node.lineno} subprocess.{node.func.attr}')
    return bad


class DeadStdin:
    """Windows' subprocess with a dead standard input: a program started without stdin= (or stdout=/stderr=
    when those are dead too) fails in DuplicateHandle, as CPython's _get_handles does."""

    def __init__(self, dead=('stdin',)):
        self.dead = dead; self.real = subprocess.Popen._get_handles; self.started = []

    def __enter__(self):
        me = self

        def get_handles(popen, stdin, stdout, stderr):
            for name, value in (('stdin', stdin), ('stdout', stdout), ('stderr', stderr)):
                if value is None and name in me.dead:
                    raise OSError(9, '[WinError 6] The handle is invalid')
            me.started.append((stdin, stdout, stderr))
            return me.real(popen, stdin, stdout, stderr)
        self.patch = patch.object(subprocess.Popen, '_get_handles', get_handles)
        self.patch.start()
        return self

    def __exit__(self, *exc):
        self.patch.stop()


class LaunchPathCalls(unittest.TestCase):
    def test_every_session_tool_call_gives_stdin(self):
        tools = session_tools()
        self.assertIn('xr_runtime', tools)
        self.assertIn('run_game_vr', tools)
        bad = [b for name in tools for b in calls_without_stdin(TOOLS/f'{name}.py')]
        # also the launcher's own Python check and the other programs a session or the launcher runs
        for extra in ('check_bundled_python.py', 'close_game.py', 'probe_link.py'):
            bad += calls_without_stdin(TOOLS/extra)
        self.assertEqual(bad, [], 'subprocess calls that hand on the inherited standard input')

    def test_scan_finds_a_call_without_stdin(self):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d)/'x.py'
            p.write_text("import subprocess\nsubprocess.run(['a'], capture_output=True)\n"
                         "subprocess.run(['b'], stdin=subprocess.DEVNULL)\n", encoding='utf-8')
            self.assertEqual(calls_without_stdin(p), ['x.py:2 subprocess.run'])

    def test_session_and_launcher_scripts_repair_their_handles(self):
        for name in ('run_game_vr.py', 'xr_runtime.py'):
            text = (TOOLS/name).read_text(encoding='utf-8')
            main = text[text.index("if __name__ == '__main__':"):]
            self.assertIn('std_handles.repair()', main.split('\n', 4)[0] + '\n'.join(main.split('\n')[:5]), name)


class DeadStdinSession(unittest.TestCase):
    """The player's path: session() -> 'finding the headset' -> preflight -> xr_runtime.detect -> probe."""

    def setUp(self):
        import session_launch_tests as s
        self.s = s; self.vr = s.run_game_vr
        import xr_runtime
        self.xr = xr_runtime
        d = pathlib.Path(tempfile.mkdtemp()); self.addCleanup(__import__('shutil').rmtree, d, True)
        self.probe = d/('probe.cmd' if os.name == 'nt' else 'probe.sh')
        said = 'Headset available: Test Headset; Recommended eye 0: 2000x2000'
        if os.name == 'nt':
            self.probe.write_text(f'@echo {said}\r\n@echo Recommended eye 0: 2000x2000\r\n')
        else:
            self.probe.write_text(f'#!/bin/sh\necho "{said}"\necho "Recommended eye 0: 2000x2000"\n'); self.probe.chmod(0o755)
        self.manifest = d/'virtualdesktop.json'; self.manifest.write_text('{}')

    def runtimes(self, *_a, **_k):
        return [dict(manifest=str(self.manifest), name='Virtual Desktop', active=True, tested=True)]

    def test_emulation_raises_winerror_6_without_stdin(self):
        with DeadStdin(), self.assertRaisesRegex(OSError, r'WinError 6'):
            subprocess.run([sys.executable, '-c', 'pass'], capture_output=True)

    def test_probe_finds_the_headset_with_a_dead_stdin(self):
        with DeadStdin() as dead:
            found, said = self.xr.probe(self.manifest, timeout=20, program=self.probe)
        self.assertTrue(found, said); self.assertIn('Headset available', said)
        self.assertTrue(dead.started)

    def test_session_gets_past_finding_the_headset_with_a_dead_stdin(self):
        vr = self.vr
        startup = vr.Startup(pathlib.Path('unused.json'))
        defaults = list(self.xr.probe.__defaults__); defaults[-1] = self.probe
        with DeadStdin(), patch.object(self.xr, 'runtimes', self.runtimes), \
                patch.object(self.xr.probe, '__defaults__', tuple(defaults)), \
                patch.object(vr, 'game_running', return_value=False), \
                patch.object(vr, 'elevated', return_value=False), \
                patch.object(vr, 'wait_for_game', side_effect=self._prepared), \
                patch.object(self.s.vr_display, 'snapshot', return_value={}), \
                patch.object(self.s.vr_display, 'hud_option_probe', return_value=None), \
                patch.object(self.s.vr_display, 'prepare_launch', return_value=None), \
                patch.object(self.s.vr_display, 'session_changes', return_value={}), \
                patch.object(self.s.vr_display, 'pending', return_value=False), \
                patch('capture_game_state.running_programs', return_value={'virtualdesktop.server.exe'}, create=True), \
                patch('sys.stdout', io.StringIO()) as out:
            with self.assertRaises(self.s.Prepared):
                vr.session(self.s._args(), startup)
        self.assertEqual(startup.stage, 'starting the game', out.getvalue())
        self.assertEqual(startup.fields['xr_runtime']['headset'], 'Test Headset')
        self.assertIn('Looking for the headset in Virtual Desktop', out.getvalue())

    def _prepared(self, **callbacks):
        callbacks['prepare']()
        raise self.s.Prepared()

    def test_network_links_with_dead_stdin_does_not_fail(self):
        with DeadStdin() as dead:
            try:
                self.vr.command_text([sys.executable, '-c', 'print(1)'])
            except LookupError:                 # the 'oem' code page is Windows' only
                self.assertNotEqual(os.name, 'nt')
        self.assertEqual(len(dead.started), 1)


class FakeKernel32:
    """GetStdHandle/SetStdHandle/DuplicateHandle over a table: 0 = none, 'dead' fails to duplicate."""

    def __init__(self, handles):
        self.handles = dict(handles); self.set = {}; self.opened = 0

    def GetStdHandle(self, n): return self.handles[n]
    def GetCurrentProcess(self): return -1

    def DuplicateHandle(self, a, h, b, out, *rest):
        return h not in (777,)          # 777 = a closed handle

    def CloseHandle(self, h): return True
    def GetFileType(self, h): return 0 if h == 778 else 2   # 778 = a dead number reused by an event (not a stream)
    def last_error(self): return 6

    def CreateFileW(self, name, *rest):
        assert name == 'NUL'; self.opened += 1; return 4242

    def SetStdHandle(self, n, h):
        self.handles[n] = h; self.set[n] = h; return True


class Repair(unittest.TestCase):
    def test_dead_stdin_becomes_nul(self):
        k = FakeKernel32({-10: 777, -11: 100, -12: 0})
        self.assertEqual(std_handles.repair(k), ['stdin'])
        self.assertEqual(k.set, {-10: 4242})

    def test_reused_number_is_not_a_stream(self):
        k = FakeKernel32({-10: 778, -11: 100, -12: 0})
        self.assertEqual(std_handles.repair(k), ['stdin'])

    def test_all_dead_and_invalid_value(self):
        k = FakeKernel32({-10: 777, -11: 777, -12: -1})
        self.assertEqual(std_handles.repair(k), ['stdin', 'stdout', 'stderr'])

    def test_good_and_missing_are_left(self):
        k = FakeKernel32({-10: 0, -11: None, -12: 100})
        self.assertEqual(std_handles.repair(k), []); self.assertEqual(k.set, {})

    def test_off_windows_and_real_windows(self):
        if os.name != 'nt':
            self.assertEqual(std_handles.repair(), [])
        else:
            std_handles.repair()   # never raises; handles given by the test runner are valid or repaired


class Launcher(unittest.TestCase):
    SRC = TOOLS/'launcher'/'src'

    def body(self, file, start):
        text = (self.SRC/file).read_text(encoding='utf-8')
        i = text.index(start); depth = 0
        for j in range(text.index('{', i), len(text)):
            depth += {'{': 1, '}': -1}.get(text[j], 0)
            if depth == 0:
                return text[i:j+1]
        raise AssertionError(start)

    def test_start_clears_dead_handles_before_powershell(self):
        b = self.body('PsRunner.cs', 'public bool Start(')
        self.assertLess(b.index('StdHandles.ClearDead()'), b.index('proc.Start()'))
        # PowerShell gets a standard input of its own, never the launcher's: a pipe closed at once
        self.assertLess(b.index('psi.RedirectStandardInput = true;'), b.index('proc.Start()'))
        self.assertLess(b.index('proc.Start()'), b.index('proc.StandardInput.Close()'))
        text = (self.SRC/'PsRunner.cs').read_text(encoding='utf-8')
        self.assertIn('GetFileType(h) != 0', text)

    def test_request_stop_puts_the_handles_back(self):
        b = self.body('PsRunner.cs', 'public bool RequestStop(')
        self.assertLess(b.index('StdHandles.Save()'), b.index('FreeConsole()'))
        fin = b[b.index('finally'):]
        self.assertIn('StdHandles.Restore(saved)', fin)
        self.assertEqual(b.count('StdHandles.Restore('), 1)

    def test_restart_and_main_clear_dead_handles(self):
        # The self-update backend (MainForm.Updates.cs) stays build 9's (launcher_hero_tests pins it): the launcher a
        # self-update restarts - including one restarted by an old build 10/11 after Stop VR - clears them in Main.
        b = self.body('Program.cs', 'static int Main(')
        self.assertLess(b.index('StdHandles.ClearDead()'), b.index('new Mutex('))

    def test_std_handles_class(self):
        text = (self.SRC/'PsRunner.cs').read_text(encoding='utf-8')
        for need in ('static class StdHandles', 'GetStdHandle', 'SetStdHandle', 'DuplicateHandle',
                     'public static int ClearDead()', 'Mono.Runtime'):
            self.assertIn(need, text)


@unittest.skipUnless(os.name == 'nt', 'real Windows handles (the CI release build)')
class RealWindows(unittest.TestCase):
    """A child Python started like the launcher starts the session: STARTF_USESTDHANDLES, its stdin a closed handle."""

    CHILD = ("import sys,subprocess; sys.path.insert(0, sys.argv[1]); fix = sys.argv[2] == '1'\n"
             "import std_handles\n"
             "r = std_handles.repair() if fix else []\n"
             "try:\n"
             "    subprocess.run(['cmd', '/c', 'echo', 'hi'], capture_output=True, timeout=30); print('STARTED', r)\n"
             "except OSError as e:\n"
             "    print('FAILED', e)\n")

    def child(self, fix):
        import _winapi, msvcrt
        dead = _winapi.DuplicateHandle(_winapi.GetCurrentProcess(), msvcrt.get_osfhandle(os.open(os.devnull, os.O_RDONLY)),
                                       _winapi.GetCurrentProcess(), 0, True, _winapi.DUPLICATE_SAME_ACCESS)
        _winapi.CloseHandle(dead)                         # a handle value that is gone, like the freed console's
        with tempfile.TemporaryFile() as out:
            si = subprocess.STARTUPINFO(); si.dwFlags = subprocess.STARTF_USESTDHANDLES
            si.hStdInput = dead
            si.hStdOutput = _winapi.DuplicateHandle(_winapi.GetCurrentProcess(), msvcrt.get_osfhandle(out.fileno()),
                                                    _winapi.GetCurrentProcess(), 0, True, _winapi.DUPLICATE_SAME_ACCESS)
            si.hStdError = si.hStdOutput
            cmd = subprocess.list2cmdline([sys.executable, '-c', self.CHILD, str(TOOLS), '1' if fix else '0'])
            hp, ht, pid, tid = _winapi.CreateProcess(None, cmd, None, None, True, subprocess.CREATE_NO_WINDOW, None, None, si)
            _winapi.WaitForSingleObject(hp, 60000); _winapi.CloseHandle(hp); _winapi.CloseHandle(ht)
            _winapi.CloseHandle(si.hStdOutput)
            out.seek(0); return out.read().decode('utf-8', 'replace')

    def test_dead_stdin_reproduces_winerror_6(self):
        said = self.child(False)
        print('\n  without the fix:', said.strip())
        # Evidence only: a handle closed by the parent may be reused in the child, so this does not always fail.
        # The faithful reproduction is WindowsLauncherRepro (the 0.0.70 launcher's real Stop VR sequence).
        if 'FAILED' not in said:
            self.skipTest('a closed handle did not reproduce here: ' + said.strip())
        self.assertIn('WinError 6', said)

    def test_repair_starts_programs(self):
        said = self.child(True)
        print('\n  with std_handles.repair():', said.strip())
        self.assertIn("STARTED ['stdin']", said)


@unittest.skipUnless(os.name == 'nt', 'real Windows, the launcher compiled with csc (the CI release build)')
class WindowsLauncherRepro(unittest.TestCase):
    """tests/windows/std_handles_repro.cs: the launcher's PsRunner after a real Stop, started like from Explorer."""

    @classmethod
    def setUpClass(cls):
        import time
        csc = [c for c in (os.path.expandvars(r'%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe'),
                           os.path.expandvars(r'%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe')) if os.path.exists(c)]
        if not csc:
            raise unittest.SkipTest('no csc.exe')
        cls.dir = pathlib.Path(tempfile.mkdtemp())
        exe = cls.dir/'repro.exe'
        src = sorted(str(p) for p in (TOOLS/'launcher'/'src').glob('*.cs')) + [str(ROOT/'tests'/'windows'/'std_handles_repro.cs')]
        refs = ['/r:' + r for r in ('System.dll', 'System.Core.dll', 'System.Drawing.dll', 'System.Windows.Forms.dll',
                                     'System.IO.Compression.dll', 'System.IO.Compression.FileSystem.dll',
                                     'System.Web.Extensions.dll', 'System.Management.dll')]
        built = subprocess.run([csc[0], '/nologo', '/target:winexe', '/langversion:5', '/main:Sm2VrLauncher.StdHandlesRepro',
                                '/out:' + str(exe)] + refs + src, stdin=subprocess.DEVNULL, capture_output=True, text=True)
        if built.returncode:
            raise AssertionError('csc: ' + built.stdout[-3000:])
        out = cls.dir/'result.txt'
        args = subprocess.list2cmdline([str(out), str(TOOLS), sys.executable, str(ROOT/'tests'/'windows'/'std_handles_child.py')])
        os.startfile(str(exe), 'open', args)          # ShellExecute, as Explorer starts the launcher
        deadline = time.monotonic() + 240
        while time.monotonic() < deadline and not out.exists():
            time.sleep(1)
        time.sleep(1)
        cls.said = dict(line.split(': ', 1) for line in out.read_text().splitlines() if ': ' in line) if out.exists() else {}
        print('\n' + '\n'.join(f'  {k}: {v}' for k, v in cls.said.items()))

    def test_harness_ran(self):
        self.assertIn('stop-fix', self.said, self.said)
        self.assertNotIn('harness-error', self.said)

    def test_legacy_stop_leaves_dead_handles_and_winerror_6(self):
        # Evidence only (the root cause on real Windows); printed above, recorded in the CI log.
        if 'WinError 6' not in self.said.get('legacy-python', ''):
            self.skipTest('0.0.70 path not reproduced here: ' + self.said.get('legacy-python', '?'))

    def test_python_fix(self):
        self.assertIn('STARTED', self.said.get('python-fix', ''))

    def test_launcher_fix(self):
        self.assertIn('STARTED', self.said.get('launcher-fix', ''))

    def test_stop_fix_keeps_handles(self):
        self.assertNotIn('(dead)', self.said.get('stop-handles', '(dead)'))
        self.assertIn('STARTED', self.said.get('stop-fix', ''))


if __name__ == '__main__':
    unittest.main()
