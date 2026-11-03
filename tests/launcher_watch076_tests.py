"""0.0.76 LAUNCHER WATCH: VR no longer ends (and the game no longer goes back to its flat screen) when Windows holds
the launcher's keep-alive up while the game loads at the menu (2026-11-03 01:55: 5.2 s, VR ended by itself, exit 1,
and the stop window showed the ENGINE TAA note because it said "refused"). A stale -EngineTaa is ignored with a note."""
import argparse
import pathlib
import re
import types
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
XR = (ROOT / 'src/game_xr.cpp').read_text(encoding='utf-8')
RUNNER = (ROOT / 'tools/run_game_vr.py').read_text(encoding='utf-8')


def runner_helpers():
    """run_game_vr's pure helpers (the launcher itself is Windows-only): the option bits, the LAUNCHER WATCH block,
    vr_aa_options and apply_vr_aa."""
    scope = {}
    for constant in re.findall(r'^((?:VR_AA_[A-Z]+_OPTION|ENGINE_TAA_OPTION) = .+)$', RUNNER, re.M):
        exec(constant, scope)  # noqa: S102 - our own source
    start = RUNNER.index('# 0.0.76 LAUNCHER WATCH (include/spidy/launcher_watch.hpp)')
    end = RUNNER.index('\n\n\n', RUNNER.index('def apply_vr_aa('))
    exec(RUNNER[start:end], scope)  # noqa: S102 - our own source, a slice of plain functions
    return types.SimpleNamespace(**scope)


r = runner_helpers()
PS1 = (ROOT / 'tools/launch-game-vr.ps1').read_text(encoding='utf-8')
OPTIONS = (ROOT / 'tools/launcher/src/LaunchOptions.cs').read_text(encoding='utf-8')
FORM = (ROOT / 'tools/launcher/src/MainForm.cs').read_text(encoding='utf-8')


def session_loop():
    start = XR.index('while (!stopRequested && (!nativeDeadline || GetTickCount64() < nativeDeadline)) {')
    return XR[start:start + 1200]


class Dll(unittest.TestCase):
    def test_a_late_keep_alive_alone_does_not_end_vr(self):
        loop = session_loop()
        self.assertNotIn('if (!config.durationMs && GetTickCount64() >= keepAliveDeadline)\n                break;', loop)
        self.assertIn('launcherCheck(); verdict != launcher_watch::keepRunning', loop)
        self.assertIn('launcherCheck() == launcher_watch::launcherClosed', loop)

    def test_the_export_and_its_handle(self):
        self.assertIn('extern "C" __declspec(dllexport) DWORD WINAPI SpidyXrLauncher(void* pid)', XR)
        self.assertIn('OpenProcess(SYNCHRONIZE, FALSE, id)', XR)
        self.assertIn('WaitForSingleObject(process, 0) == WAIT_OBJECT_0', XR)
        # Each start forgets the last launcher (an older one that never names itself keeps the minute rule).
        self.assertIn('launcherProcess.exchange(nullptr)', XR)

    def test_the_last_line_says_why(self):
        self.assertIn('"VR session finished: the launcher closed"', XR)
        self.assertIn('"VR session finished: the launcher did not answer for a minute"', XR)
        for said in ('the launcher closed', 'did not answer for a minute'):
            self.assertNotRegex(said, r'(?i)failed|refus|error')


class Runner(unittest.TestCase):
    def test_the_export_is_optional_and_named_after_the_start(self):
        self.assertEqual(r.LAUNCHER_EXPORT, 'SpidyXrLauncher')
        self.assertIn('LAUNCHER_EXPORT))  # 0.0.76 LAUNCHER WATCH', RUNNER)
        start = RUNNER.index("code = call_with_payload(process, xr['SpidyXrStart'], config)")
        named = RUNNER.index('print(name_launcher(process, xr, os.getpid()), flush=True)')
        self.assertLess(start, named)

    def test_name_launcher(self):
        calls = []
        r.name_launcher.__globals__['call_remote'] = lambda process, address, value=0: calls.append((address, value)) or 0
        self.assertIn('VR ends when the launcher closes', r.name_launcher('p', {'SpidyXrLauncher': 7}, 4242))
        self.assertEqual(calls, [(7, 4242)])
        r.name_launcher.__globals__['call_remote'] = lambda process, address, value=0: 3990
        self.assertIn('(3990)', r.name_launcher('p', {'SpidyXrLauncher': 7}, 1))
        self.assertIn('older', r.name_launcher('p', {}, 1))
        for line in (r.name_launcher('p', {}, 1),):
            self.assertNotRegex(line, r'(?i)failed|refus|^error')

    def test_late_keep_alive_note(self):
        self.assertIsNone(r.keep_alive_late(0, 99))
        self.assertIsNone(r.keep_alive_late(10, 11.2))
        line = r.keep_alive_late(14.81, 20.05)
        self.assertIn('5.2 s apart', line)
        self.assertIn('VR kept running', line)
        self.assertNotRegex(line, r'(?i)failed|refus|error')


def picture_args(**extra):
    a = argparse.Namespace(render_scale=100, supersample=100, upscale=100, mip_bias='0.5', anisotropy=16, size=0,
                           game_sharpening=False, engine_taa=True, no_eye_antialiasing=False, vr_aa='ours')
    vars(a).update(extra)
    return a


class StaleEngineTaa(unittest.TestCase):
    def test_ignored_with_a_note_under_ours(self):
        a = picture_args()
        line = r.apply_vr_aa(a)
        self.assertFalse(a.engine_taa)
        self.assertIn('ENGINE TAA -> off (-EngineTaa, a saved launcher option, ignored)', line)
        self.assertEqual(r.vr_aa_options(a), 0)

    def test_ignored_under_smaa_and_keep(self):
        for choice in ('smaa', 'keep'):
            a = picture_args(vr_aa=choice)
            r.apply_vr_aa(a)
            self.assertFalse(a.engine_taa, choice)

    def test_the_notes_do_not_read_as_a_failure(self):
        note = re.search(r'Write-Host "(ENGINE TAA: -EngineTaa[^"]*)"', PS1).group(1)
        self.assertNotRegex(note, r'(?i)failed|refus|^error')
        self.assertIn('VR starts either way', note)
        start = RUNNER.index('ENGINE TAA (-EngineTaa): the eyes get')
        self.assertNotRegex(RUNNER[start:start + 300], r'(?i)refus')

    def test_launcher_default_off(self):
        self.assertRegex(OPTIONS, r'Opt\.Sw\("PICTURE", "EngineTaa", "ENGINE TAA", "[^"]*", false\)')

    def test_stop_window_skips_status_notes(self):
        self.assertIn('lock (lines) last = StopReason(lines);', FORM)
        self.assertIn('"ENGINE TAA", "LAUNCHER WATCH"', FORM)


if __name__ == '__main__':
    unittest.main()
