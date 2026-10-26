"""0.0.5 hotfix 3 (B3ARVR, Virtual Desktop): VR lost or headset not connected never leaves the game silently flat."""
import ast
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
RUN = (ROOT/'tools/run_game_vr.py').read_text(encoding='utf-8')
# The public repo carries the launcher in tools/launcher; the dev tree keeps it beside (herovr-launcher-006).
LAUNCHER = next((p for p in (ROOT/'tools/launcher/src', ROOT.parent/'herovr-launcher-006/src') if p.is_dir()),
                ROOT/'tools/launcher/src')


def pick(*names):
    tree = ast.parse(RUN)
    nodes = [n for n in tree.body if (isinstance(n, ast.FunctionDef) and n.name in names) or
             (isinstance(n, ast.Assign) and any(getattr(t, 'id', None) in names for t in n.targets))]
    space = {'time': __import__('time')}
    exec(compile(ast.Module(nodes, []), 'run_game_vr.py', 'exec'), space)
    return space


class HeadsetWait(unittest.TestCase):
    def setUp(self):
        self.space = pick('HEADSET_WAIT', 'HEADSET_RETRY', 'HEADSET_MISSING', 'wait_for_headset')

    def run_wait(self, answers, wait=300):
        now = [0.0]
        said = []
        calls = []

        def ask():
            calls.append(now[0])
            answer = answers.pop(0) if answers else None
            if answer is None:
                raise RuntimeError('No headset found (Virtual Desktop: XR_ERROR_FORM_FACTOR_UNAVAILABLE).')
            return answer

        def sleep(seconds):
            now[0] += seconds
        result = self.space['wait_for_headset'](ask, wait=wait, retry=3, clock=lambda: now[0], sleep=sleep,
                                                say=said.append)
        return result, said, calls

    def test_found_at_once_says_nothing(self):
        result, said, calls = self.run_wait([('vd.json', 'ok')])
        self.assertEqual(result, ('vd.json', 'ok'))
        self.assertEqual(said, [])
        self.assertEqual(len(calls), 1)

    def test_waits_with_one_plain_message_until_the_headset_answers(self):
        result, said, calls = self.run_wait([None, None, None, ('vd.json', 'ok')])
        self.assertEqual(result, ('vd.json', 'ok'))
        self.assertEqual(len(said), 1)
        self.assertTrue(said[0].startswith('Headset not connected: open Virtual Desktop in your headset first'))
        self.assertIn('the game is not started without a headset', said[0])
        self.assertEqual(calls, [0, 3, 6, 9])

    def test_gives_up_without_starting_the_game(self):
        with self.assertRaises(RuntimeError) as caught:
            self.run_wait([], wait=30)
        self.assertIn('the game was not started', str(caught.exception))
        self.assertIn('No headset found', str(caught.exception))

    def test_the_default_wait_is_five_minutes(self):
        self.assertEqual(self.space['HEADSET_WAIT'], 300)


class NeverSilentlyFlat(unittest.TestCase):
    def test_preflight_waits_for_the_headset_before_the_game_starts(self):
        body = RUN[RUN.index('def preflight('):]
        body = body[:body.index('\ndef ', 1)]
        self.assertEqual(body.count('wait_for_headset('), 2)
        session = RUN[RUN.index('def session('):]
        self.assertLess(session.index('preflight(a.xr_runtime)'), session.index("startup.stage = 'starting the game'"))

    def test_vr_lost_says_what_to_do_and_asks_the_launcher_to_close_the_game(self):
        self.assertIn('VR_LOST_HELP', RUN)
        self.assertIn('vr_lost = True', RUN)
        self.assertRegex(RUN, r'if vr_lost and alive\(game\):\s*\n\s*#[^\n]*\n\s*return CLOSE_GAME_EXIT')
        help_text = pick('VR_LOST_HELP')['VR_LOST_HELP']
        self.assertIn('Close Spider-Man 2', help_text)
        self.assertIn('open Virtual Desktop in your headset', help_text)

    def test_close_game_closes_any_running_game_the_launcher_asked_about(self):
        self.assertIn('        if a.close_game:\n', RUN)
        ps1 = (ROOT/'tools/launch-game-vr.ps1').read_text(encoding='utf-8')
        self.assertIn('[switch]$CloseGame', ps1)
        self.assertIn("if($CloseGame) { $captureArgs+=@('--close-game') }", ps1)

    @unittest.skipUnless(LAUNCHER.is_dir(), 'launcher sources not beside this tree')
    def test_launcher_offers_to_close_the_game_and_never_starts_flat(self):
        form = (LAUNCHER/'MainForm.cs').read_text(encoding='utf-8')
        self.assertIn('extra.Add("-CloseGame")', form)
        self.assertIn('code == CloseGameExit', form)
        self.assertIn('const int CloseGameExit = 6;', form)
        self.assertIn('Headset not connected: open Virtual Desktop in your headset first', form)
        self.assertNotIn('start anyway', form)
        self.assertNotIn('Close it first, then press PLAY VR', form)
        vr = (ROOT/'tools/vr_launcher.py').read_text(encoding='utf-8')
        self.assertIn('CLOSE_GAME_EXIT=6', vr)

    @unittest.skipUnless(LAUNCHER.is_dir(), 'launcher sources not beside this tree')
    def test_the_always_running_vd_service_is_not_a_connected_headset(self):
        info = (LAUNCHER/'SystemInfo.cs').read_text(encoding='utf-8')
        self.assertNotIn('"VirtualDesktop.Service"', info)
        self.assertIn('Running("VirtualDesktop.Streamer")', info)

    @unittest.skipUnless(LAUNCHER.is_dir(), 'launcher sources not beside this tree')
    def test_launcher_build_is_bumped_for_self_update(self):
        version = (LAUNCHER/'ModVersion.cs').read_text(encoding='utf-8')
        build = int(re.search(r'public const int Build = (\d+);', version).group(1))
        self.assertGreaterEqual(build, 4)


if __name__ == '__main__':
    unittest.main()
