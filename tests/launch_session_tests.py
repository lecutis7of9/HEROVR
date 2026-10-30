"""0.0.63 gate (26 Oct player report: 0.0.62 never started VR, "NameError: name 'session_off' is not defined" at stage
'starting the game'). No earlier gate ran the real Python launch path: launch_path_tests.py stops at a fake python.exe,
and run_game_vr.py's session() needs Windows and the game.

This runs the REAL run_game_vr.main() -> session() -> prepare_display() -> vr_display.prepare_launch on any OS. Only
the Windows/game modules (every other tools/*.py except live_settings) are stand-ins; run_game_vr.py's own code runs
as shipped. The game "starts" through wait_for_game(prepare=prepare_display), then the test stops the session.
It fails on any NameError (or other error) before or inside prepare_display, and checks the live.txt lens_flares
choice reaches prepare_launch(off=...). Also: no undefined names in tools/*.py (pyflakes, when installed; STRICT=1
makes a missing pyflakes a failure).
Env: HEROVR_TOOLS = a tools folder to check instead (e.g. an extracted release mod zip's tools/)."""
import importlib.abc, importlib.machinery, os, pathlib, shutil, subprocess, sys, tempfile, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOLS = pathlib.Path(os.environ.get('HEROVR_TOOLS', ROOT / 'tools'))
STRICT = os.environ.get('STRICT') == '1'
REAL = {'live_settings'}


class Started(Exception):
    """The game would start now: prepare_display ran."""


class Stubs(importlib.abc.MetaPathFinder, importlib.abc.Loader):
    def __init__(self, names): self.names = names
    def find_spec(self, name, path, target=None):
        return importlib.machinery.ModuleSpec(name, self) if name in self.names else None
    def create_module(self, spec):
        m = mock.MagicMock(name=spec.name); m.__name__ = spec.name; m.__spec__ = spec; return m
    def exec_module(self, m): pass


def run_session(live_txt=None, extra=()):
    """run_game_vr.main() up to the game start; returns (prepare_launch kwargs, printed text)."""
    names = {p.stem for p in TOOLS.glob('*.py')} - REAL - {'run_game_vr'}
    saved_mods = {k: sys.modules.pop(k) for k in list(sys.modules) if k in names | REAL | {'run_game_vr'}}
    finder = Stubs(names); sys.meta_path.insert(0, finder); sys.path.insert(0, str(TOOLS))
    d = pathlib.Path(tempfile.mkdtemp()); argv = sys.argv
    try:
        import run_game_vr as r
        (d / 'reports').mkdir()
        if live_txt is not None: (d / 'live.txt').write_text(live_txt, encoding='utf-8')
        r.ROOT = d
        calls = []
        vd = r.vr_display
        vd.prepare_launch.side_effect = lambda **kw: calls.append(kw) or {'WindowWidth': 1280, 'WindowHeight': 720}
        vd.snapshot.return_value = {}; vd.hud_option_probe.return_value = ''
        vd.session_changes.return_value = {}; vd.pending.return_value = False
        vd.SMALLER_HEIGHT = 720; vd.HEIGHT = 1080

        def wait_for_game(prepare=None, **kw):
            prepare(); raise Started()
        r.wait_for_game = wait_for_game
        r.elevated = lambda: False
        r.game_running = lambda: False
        r.preflight = lambda req: ({}, {'name': 'SteamVR', 'automatic': True})
        r.network_links = lambda *a, **k: {'wifi': None, 'cable': True}
        r.link_warning = lambda *a, **k: None
        r.keep_console = lambda *a, **k: None
        r.free_commit_mb = lambda: 0   # ctypes.windll (the report's memory field)
        sys.argv = ['run_game_vr.py', '--auto-launch', '--output', str(d / 'reports/game-vr.json'), *extra]
        out = []
        with mock.patch('builtins.print', lambda *a, **k: out.append(' '.join(map(str, a)))):
            try:
                r.main()
            except Started:
                pass
        return calls, '\n'.join(out)
    finally:
        sys.argv = argv; sys.meta_path.remove(finder); sys.path.remove(str(TOOLS))
        for k in list(sys.modules):
            if k in names | REAL | {'run_game_vr'}: sys.modules.pop(k)
        sys.modules.update(saved_mods); shutil.rmtree(d, True)


class RealLaunchPath(unittest.TestCase):
    def test_reaches_prepare_launch_default(self):
        calls, _ = run_session()
        self.assertEqual(len(calls), 1, 'prepare_display never reached vr_display.prepare_launch')
        self.assertEqual(tuple(calls[0]['off']), ())  # 0.0.74 test 3 SPIDYVR STOCK (default on): flares left alone
        calls, _ = run_session('spidyvr_stock = off\n')
        self.assertEqual(tuple(calls[0]['off']), ('lens_flares',))  # STOCK off: 0.0.73 lights, OFF IN VR by default

    def test_lens_flares_off_in_vr_reaches_prepare_launch(self):
        calls, text = run_session('spidyvr_stock = off\nlens_flares = off_in_vr\n')
        self.assertEqual(len(calls), 1)
        self.assertEqual(tuple(calls[0]['off']), ('lens_flares',))
        self.assertIn('LENS FLARES: OFF IN VR this session', text)

    def test_smaller_and_full_desktop_paths(self):
        for extra in (['--smaller-desktop-view'], ['--full-desktop-view'], []):
            try:
                calls, _ = run_session(extra=extra)
            except SystemExit as e:   # an option this tree does not have: argparse, not the launch path
                if e.code == 2: continue
                raise
            self.assertEqual(len(calls), 1, extra)


class NoUndefinedNames(unittest.TestCase):
    def test_pyflakes_undefined_names(self):
        import importlib.util
        if importlib.util.find_spec('pyflakes') is None:
            if STRICT: raise AssertionError('pyflakes is not installed (pip install pyflakes)')
            raise unittest.SkipTest('pyflakes not installed')
        files = sorted(str(p) for p in TOOLS.glob('*.py'))
        res = subprocess.run([sys.executable, '-m', 'pyflakes', *files], capture_output=True, text=True)
        bad = [l for l in res.stdout.splitlines() if 'undefined name' in l or 'referenced before assignment' in l]
        self.assertEqual(bad, [])


if __name__ == '__main__':
    unittest.main(verbosity=2)
