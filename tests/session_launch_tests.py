"""0.0.7 regression: every real launch died with NameError: name 'session_off' is not defined at stage
'starting the game'. session()'s prepare_display() read main()'s local `session_off`; it is now passed in
(session(a, startup, session_off=...)). These tests drive main() -> session() -> prepare_display() with the
game launch mocked, so a name session() cannot see fails here, on any OS (Windows-only modules are stubbed
off Windows), instead of on the player's PC."""
import argparse
import io
import pathlib
import sys
import tempfile
import types
import unittest
from unittest.mock import MagicMock, patch

TOOLS = pathlib.Path(__file__).resolve().parents[1]/'tools'
sys.path.insert(0, str(TOOLS))


class _Stub(types.ModuleType):
    """A Windows-only module off Windows: every name is a MagicMock (the tests patch what session() calls)."""

    def __getattr__(self, name):
        if name.startswith('__'):
            raise AttributeError(name)
        value = MagicMock(name=f'{self.__name__}.{name}')
        setattr(self, name, value)
        return value


# The modules that cannot load off Windows (winreg, msvcrt, ctypes.WinDLL, the live process readers).
WINDOWS_ONLY = ('winreg', 'msvcrt', 'capture_game_state', 'observe_game', 'vr_launcher')


def _load():
    try:
        import run_game_vr
        return run_game_vr, False
    except (ImportError, SystemExit, AttributeError, TypeError):
        pass
    for name in [n for n, m in list(sys.modules.items())
                 if str(getattr(m, '__file__', '') or '').startswith(str(TOOLS))]:
        del sys.modules[name]  # half-imported tool modules
    for name in WINDOWS_ONLY:
        sys.modules.setdefault(name, _Stub(name))
    import run_game_vr
    return run_game_vr, True


run_game_vr, STUBBED = _load()
import vr_display  # noqa: E402  (the instance run_game_vr uses)
import live_settings  # noqa: E402  (0.0.73 lights: the migration marker)


class Prepared(Exception):
    """Stops the session right after prepare_display(), before anything real is launched."""


def _args(**extra):
    values = dict(stop_event=None, close_game=False, xr_runtime='auto', render_scale=100, size=0, auto_launch=True,
                  full_desktop_view=False, smaller_desktop_view=False, game_depth_of_field=False,
                  game_sharpening=False, keep_game_settings=False, mip_bias='0.5', anisotropy=16)
    values.update(extra)
    return argparse.Namespace(**values)


class SessionLaunchTests(unittest.TestCase):
    def _run(self, call):
        """Runs call() with the game, headset and registry mocked; returns prepare_launch's mock."""

        def wait_for_game(**callbacks):
            callbacks['prepare']()  # the real prepare_display(), where the NameError was
            raise Prepared()

        with patch.object(run_game_vr, 'game_running', return_value=False), \
             patch.object(run_game_vr, 'elevated', return_value=False), \
             patch.object(run_game_vr, 'network_links', return_value={}), \
             patch.object(run_game_vr, 'preflight', return_value=('runtime.json', {'name': 'test'})), \
             patch.object(run_game_vr, 'wait_for_game', side_effect=wait_for_game), \
             patch.object(vr_display, 'snapshot', return_value={}), \
             patch.object(vr_display, 'hud_option_probe', return_value=None), \
             patch.object(vr_display, 'prepare_launch', return_value=None) as prepare, \
             patch.object(vr_display, 'session_changes', return_value={}), \
             patch.object(vr_display, 'pending', return_value=False), \
             patch('sys.stdout', io.StringIO()):
            with self.assertRaises(Prepared):
                call()
        return prepare

    def test_session_reaches_prepare_display_without_session_off(self):
        startup = run_game_vr.Startup(pathlib.Path('unused.json'))
        prepare = self._run(lambda: run_game_vr.session(_args(), startup))
        prepare.assert_called_once()
        self.assertEqual(prepare.call_args.kwargs['off'], ())
        self.assertEqual(startup.stage, 'starting the game')

    def test_session_passes_session_off_to_prepare_launch(self):
        for off in ((), ('lens_flares',)):
            with self.subTest(off=off):
                startup = run_game_vr.Startup(pathlib.Path('unused.json'))
                prepare = self._run(lambda: run_game_vr.session(_args(), startup, session_off=off))
                self.assertEqual(prepare.call_args.kwargs['off'], off)
                self.assertTrue(set(off) <= set(vr_display.SESSION_OFF))

    def test_main_hands_live_txt_lens_flares_to_the_launch(self):
        # The path a player takes: main() reads live.txt, then session() starts the game.
        # 0.0.73 lights: OFF IN VR is the default (no line, or test 4's 'game' at the old default before the
        # migration's marker); GAME chosen after the marker stays.
        for live, off in (('lens_flares = off_in_vr\n', ('lens_flares',)),
                          ('lens_flares = game\n', ('lens_flares',)),
                          (live_settings.MIGRATED_073T5 + '\nlens_flares = game\n', ('lens_flares',)),  # 0.0.74 test 2 FLARE BLOCK
                          (live_settings.MIGRATED_073T5 + '\nflare_block = off\nlens_flares = game\n', ()),
                          ('', ('lens_flares',))):
            with self.subTest(live=live), tempfile.TemporaryDirectory() as folder:
                root = pathlib.Path(folder)
                (root/'live.txt').write_text(live, encoding='utf-8')
                report = root/'reports'/'game-vr.json'
                with patch.object(run_game_vr, 'ROOT', root), \
                     patch.object(sys, 'argv', ['run_game_vr.py', '--auto-launch', '--output', str(report)]), \
                     patch.object(run_game_vr.player_check, 'before_session', return_value=None), \
                     patch.object(run_game_vr, 'keep_console', return_value=None), \
                     patch.object(run_game_vr, 'free_commit_mb', return_value=None):
                    prepare = self._run(run_game_vr.main)
                self.assertEqual(prepare.call_args.kwargs['off'], off)
                self.assertNotIn('NameError', report.read_text(encoding='utf-8') if report.exists() else '')


if __name__ == '__main__':
    unittest.main()
