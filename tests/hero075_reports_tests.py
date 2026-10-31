# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.75 REPORTS: the Reports zip takes only a crash dump written after the session started, and the prompts log
(the whole prompt timeline with the fly notes, <report>-prompts.log) goes in it; SM1STYLE and WINDOW EDGES each say
on / off and why in one console line; the launcher tells the player to set the game's Anti-Aliasing to TAA when
SM1STYLE fell back for that."""
import ast
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


def prompts_code():
    """fresh_lines and PromptsLog out of tools/run_game_vr.py, without importing it (Windows-only modules)."""
    tree = ast.parse(src('tools/run_game_vr.py'))
    keep = [n for n in tree.body if getattr(n, 'name', None) in ('fresh_lines', 'PromptsLog')]
    assert len(keep) == 2
    space = {'pathlib': pathlib}
    exec(compile(ast.Module(body=keep, type_ignores=[]), 'run_game_vr.py', 'exec'), space)
    return space


class PromptsLog(unittest.TestCase):
    def test_fresh_lines(self):
        fresh = prompts_code()['fresh_lines']
        self.assertEqual(fresh([], ['a', 'b']), ['a', 'b'])
        self.assertEqual(fresh(['a', 'b'], ['a', 'b']), [])
        self.assertEqual(fresh(['a', 'b', 'c'], ['b', 'c', 'd', 'e']), ['d', 'e'])
        self.assertEqual(fresh(['x'], ['y']), ['y'])
        self.assertEqual(fresh(None, None), [])

    def test_log_appends_only_new_lines_and_never_raises(self):
        log_class = prompts_code()['PromptsLog']
        with tempfile.TemporaryDirectory() as d:
            report = pathlib.Path(d) / 'game-vr-20261031-120000.json'
            log = log_class(report)
            self.assertEqual(log.path.name, 'game-vr-20261031-120000-prompts.log')
            log.add(['1 >> FLY: on'])
            log.add(['1 >> FLY: on', '2 >> SUPERSONIC'])
            log.add(None)
            log.add(['2 >> SUPERSONIC', '3 call'])
            self.assertEqual(log.path.read_text(encoding='utf-8').splitlines(),
                             ['1 >> FLY: on', '2 >> SUPERSONIC', '3 call'])
            broken = log_class(pathlib.Path(d) / 'missing' / 'r.json')
            broken.add(['a'])  # its folder is not there: stops quietly
            self.assertTrue(broken.broken)
            log_class(None).add(['a'])

    def test_hooked_into_the_session(self):
        s = src('tools/run_game_vr.py')
        self.assertIn('prompts_log = PromptsLog(a.output)', s)
        self.assertIn("prompts_log.add((prompt_last or {}).get('timeline'))", s)


class CollectReports(unittest.TestCase):
    def test_dump_only_after_the_session_start(self):
        ps = src('tools/collect-reports.ps1')
        self.assertIn('[switch]$NoCrashDump', ps)
        self.assertIn("$_.CreationTime", ps)
        self.assertIn('$dump.LastWriteTime -le $sessionStart', ps)
        self.assertIn('none newer than this session', ps)
        self.assertIn('no session report to date it against', ps)
        # the report's logs (BaseName*.log) carry its -prompts.log
        self.assertIn("-Filter ($report.BaseName + '*.log')", ps)
        self.assertIn("-prompts.log", ps)


class ConsoleLines(unittest.TestCase):
    def test_one_line_each(self):
        g = src('src/game_xr.cpp')
        self.assertIn('consoleLine(spidy::sm1style::windowEdgesLine(edges));', g)
        self.assertIn('consoleLine(s1::reasonLine(sd->reason));', g)
        self.assertIn('consoleLine(s1::notAskedLine);', g)
        self.assertIn('consoleLine(sd ? s1::neverStartedLine : s1::noRecordLine);', g)
        sp = src('src/stereo_probe.cpp')
        self.assertIn('extern "C" uint32_t SpidyWindowEdgesNow()', sp)
        self.assertIn('eyeCoverState.store(eyeCoverReady ? 1u : 2u);', sp)

    def test_launcher_taa_hint(self):
        mf = src('tools/launcher/src/MainForm.cs')
        self.assertIn('LiveFile.Sm1StyleAaLine(line)', mf)
        self.assertIn('sessionLog.Line(LiveFile.Sm1StyleAaHint, Theme.Warn)', mf)
        self.assertIn('Anti-Aliasing to TAA', src('tools/launcher/src/LiveFile.cs'))

    @unittest.skipUnless(shutil.which('mcs') and shutil.which('mono'), 'mono')
    def test_launcher_matches_the_game_line(self):
        game_line = ("SM1STYLE: off, not used - the game's Anti-Aliasing makes no TAA targets - the 0.0.72 eye picture "
                     "this session; set the game's Anti-Aliasing to TAA (game Settings > Graphics) and restart VR to "
                     "use SM1STYLE")
        self.assertIn('"set the game\'s Anti-Aliasing to TAA"', src('include/spidy/sm1style.hpp'))
        harness = r'''
using System;
namespace Sm2VrLauncher { static class H { static int Main(string[] a) {
  if (!LiveFile.Sm1StyleAaLine(a[0])) return 1;
  if (LiveFile.Sm1StyleAaLine("SM1STYLE: off, not used - this game's TAA code differs from 2.810")) return 2;
  if (LiveFile.Sm1StyleAaLine(null)) return 3;
  return 0; } } }'''
        with tempfile.TemporaryDirectory() as d:
            (pathlib.Path(d) / 'h.cs').write_text(harness)
            lf = ROOT / 'tools' / 'launcher' / 'src' / 'LiveFile.cs'
            r = subprocess.run(['mcs', '-nologo', '-out:' + d + '/h.exe', str(lf), d + '/h.cs'],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            r = subprocess.run(['mono', d + '/h.exe', game_line], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)


if __name__ == '__main__':
    unittest.main()
