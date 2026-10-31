# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.74 test 3 TWO-GPU device lost (0x887A002B, eye bridge 3301): the launcher's pop-up with its one-click fix and
the live.txt switch LIGHT CHECKS in OPTIONS > SAFETY CHECKS. 0.0.74 test 10 PICTURE 72: STREAMER MODE and SL WATCH are
gone with the 0.0.73 picture switches; the pop-up turns LIGHT CHECKS off only."""
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings  # noqa: E402
import start_errors  # noqa: E402

SRC = ROOT / 'tools' / 'launcher' / 'src'


def src(name):
    return (SRC / name).read_text(encoding='utf-8')


class Switches(unittest.TestCase):
    def test_live_switch_with_restart_subtext(self):
        lo = src('LaunchOptions.cs')
        for key in ('streamer_mode', 'sl_watch'):  # 0.0.74 test 10 PICTURE 72: gone
            self.assertNotIn('"%s"' % key, lo)
            self.assertNotIn(key, live_settings.SETTINGS)
            self.assertIn(key, live_settings.RETIRED_073)
        for key, title in (('light_checks', 'LIGHT CHECKS'),):
            line = next(l for l in lo.splitlines() if 'Opt.Live(' in l and '"%s"' % key in l)
            self.assertIn('"SAFETY CHECKS"', line)
            self.assertIn('"%s"' % title, line)
            sub = re.findall(r'"([^"]*)"', line)[-1]
            self.assertIn('restart', sub)
            self.assertEqual(sub.count('. '), 0, sub)  # one sentence
            self.assertIn(key, live_settings.SETTINGS)  # a real live.txt setting
            self.assertEqual(live_settings.export_value(key, 'off'), 1)
        self.assertIn('light_checks', live_settings.RESTART_NOTICE if hasattr(live_settings, 'RESTART_NOTICE') else 'light_checks')
        self.assertIn('case OptKind.Live:\n                        break;', lo)  # never a launch argument
        self.assertIn('if (o.Kind == OptKind.Live) return true;', lo)

    def test_defaults_unchanged(self):
        # No default changes without evidence (SL WATCH default off is only proposed, docs/TAA-074-test3.md).
        for key in ('light_checks',):
            self.assertEqual(live_settings.UPDATE38_DEFAULTS[key], 'on')


class PopUp(unittest.TestCase):
    def test_signature_and_clear_errors(self):
        mf = src('MainForm.cs')
        self.assertIn('if (LiveFile.TwoGpuLostLine(line)) twoGpuLost = true;', mf)
        self.assertIn('twoGpuLost && !userStopped && OptValue(LaunchOptions.Find("NoClearErrors")) == "1"', mf)
        self.assertIn('LiveFile.Set(root, key, "off")', mf)
        self.assertIn('press START VR', mf)
        lf = src('LiveFile.cs')
        self.assertIn('TwoGpuKeys = { "light_checks" };', lf)

    def test_console_words(self):
        text = start_errors.device_lost_steps('0x887A002B', 'DXGI_ERROR_ACCESS_DENIED', 'in VR')
        self.assertIn('SAFETY CHECKS: LIGHT CHECKS off', text)
        self.assertNotIn('STREAMER MODE', text)
        self.assertNotIn('LIGHT CHECKS', start_errors.device_lost_steps('0x887A0006', 'DXGI_ERROR_DEVICE_HUNG', 'in VR'))

    @unittest.skipUnless(shutil.which('mcs') and shutil.which('mono'), 'mono')
    def test_livefile_behaviour(self):
        harness = r'''
using System; using System.IO;
namespace Sm2VrLauncher { static class H { static int Main(string[] a) {
  string root = a[0];
  File.WriteAllText(Path.Combine(root, "live.txt"), "# mine\r\nstreamer_mode = on\r\nmonitor = 15\r\nsl_watch=on # note\r\n");
  if (!LiveFile.On(root, "light_checks", true)) return 1;
  foreach (var k in LiveFile.TwoGpuKeys) if (!LiveFile.Set(root, k, "off")) return 2;
  foreach (var k in LiveFile.TwoGpuKeys) if (LiveFile.On(root, k, true)) return 3;
  if (LiveFile.Get(root, "monitor") != "15") return 4;
  if (!LiveFile.TwoGpuLostLine("[D3D] ERROR! Device removed detected (0x887A002B: UNKNOWN)")) return 5;
  if (!LiveFile.TwoGpuLostLine("GPU eye bridge: 3301; then: Native GPU bridge did not stop cleanly")) return 6;
  if (LiveFile.TwoGpuLostLine("GPU eye bridge: 3302")) return 7;
  Console.Write(File.ReadAllText(Path.Combine(root, "live.txt")));
  return 0; } } }'''
        with tempfile.TemporaryDirectory() as d:
            (pathlib.Path(d) / 'h.cs').write_text(harness)
            r = subprocess.run(['mcs', '-nologo', '-out:' + d + '/h.exe', str(SRC / 'LiveFile.cs'), d + '/h.cs'],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            r = subprocess.run(['mono', d + '/h.exe', d], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            values, problems = live_settings.parse(r.stdout)
            self.assertEqual(problems, [])  # the retired streamer_mode / sl_watch lines are skipped quietly
            self.assertNotIn('streamer_mode', values)
            self.assertNotIn('sl_watch', values)
            self.assertEqual((values['light_checks'], values['monitor']), ('off', 15))
            self.assertIn('streamer_mode = on', r.stdout)  # the pop-up leaves them; migrate_074t10_72 takes them out
            self.assertIn('# mine', r.stdout)


if __name__ == '__main__':
    unittest.main()
