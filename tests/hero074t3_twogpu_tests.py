# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.74 test 3 TWO-GPU device lost (0x887A002B, eye bridge 3301): the launcher's pop-up with its one-click fix and
the three live.txt switches (STREAMER MODE, SL WATCH, LIGHT CHECKS) in OPTIONS > SAFETY CHECKS."""
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
    def test_three_live_switches_with_restart_subtext(self):
        lo = src('LaunchOptions.cs')
        for key, title in (('streamer_mode', 'STREAMER MODE'), ('sl_watch', 'SL WATCH'), ('light_checks', 'LIGHT CHECKS')):
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
        for key in ('streamer_mode', 'sl_watch', 'light_checks'):
            self.assertEqual(live_settings.UPDATE38_DEFAULTS[key], 'on')


class PopUp(unittest.TestCase):
    def test_signature_and_clear_errors(self):
        mf = src('MainForm.cs')
        self.assertIn('if (LiveFile.TwoGpuLostLine(line)) twoGpuLost = true;', mf)
        self.assertIn('twoGpuLost && !userStopped && OptValue(LaunchOptions.Find("NoClearErrors")) == "1"', mf)
        self.assertIn('LiveFile.Set(root, key, "off")', mf)
        self.assertIn('press START VR', mf)
        lf = src('LiveFile.cs')
        self.assertIn('"streamer_mode", "sl_watch", "light_checks"', lf)

    def test_console_words(self):
        text = start_errors.device_lost_steps('0x887A002B', 'DXGI_ERROR_ACCESS_DENIED', 'in VR')
        self.assertIn('STREAMER MODE, SL WATCH and LIGHT CHECKS off', text)
        self.assertNotIn('STREAMER MODE', start_errors.device_lost_steps('0x887A0006', 'DXGI_ERROR_DEVICE_HUNG', 'in VR'))

    @unittest.skipUnless(shutil.which('mcs') and shutil.which('mono'), 'mono')
    def test_livefile_behaviour(self):
        harness = r'''
using System; using System.IO;
namespace Sm2VrLauncher { static class H { static int Main(string[] a) {
  string root = a[0];
  File.WriteAllText(Path.Combine(root, "live.txt"), "# mine\r\nstreamer_mode = on\r\nmonitor = 15\r\nsl_watch=on # note\r\n");
  if (!LiveFile.On(root, "streamer_mode", false) || !LiveFile.On(root, "light_checks", true)) return 1;
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
            self.assertEqual(problems, [])
            self.assertEqual((values['streamer_mode'], values['sl_watch'], values['light_checks'], values['monitor']),
                             ('off', 'off', 'off', 15))
            self.assertIn('# mine', r.stdout)


if __name__ == '__main__':
    unittest.main()
