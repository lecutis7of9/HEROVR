"""HERO VR 0.0.78 cut14: immersive cutscenes for everyone.

- The default (no live.txt cutscenes line) is immersive, in the DLL and in run_game_vr.py's session line.
- A one-time move (live_settings.migrate_078_cutscenes, the launcher's LiveFile.MigrateCutscenes078): an existing
  live.txt's flat or 3d from an older build is set to immersive once, with a marker line; a Flat or 3D picked later
  in the launcher's CUTSCENES setting stays.
- The launcher's CUTSCENES setting (Flat / 3D / Immersive, default Immersive) writes live.txt's `cutscenes` key.
- The menu proof is tests/cut14_tests.cpp (ctest cut14).
"""
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
import live_settings as L  # noqa: E402

LAUNCHER = ROOT / 'tools' / 'launcher' / 'src'


def src(rel):
    return (ROOT / rel).read_text(encoding='utf-8')


class DefaultTests(unittest.TestCase):
    def test_default_immersive_everywhere(self):
        self.assertEqual(L.CUTSCENES_DEFAULT, 'immersive')
        self.assertIn('inline constexpr uint32_t defaultMode = immersive;', src('include/spidy/cutscene077.hpp'))
        self.assertIn('std::atomic<uint32_t> cutscenesNow{cutscene077::defaultMode};', src('src/game_xr.cpp'))

    def test_fresh_live_txt_line(self):
        value, line = L.cutscenes_start({})
        self.assertEqual(value, 'immersive')
        self.assertTrue(line.startswith('HERO VR 0.0.78 CUTSCENES IMMERSIVE'), line)

    def test_kept_value_wins(self):
        for word in ('flat', '3d', 'immersive'):
            value, line = L.cutscenes_start({'cutscenes': word})
            self.assertIsNone(value)
            self.assertIn('CUTSCENES ' + word.upper(), line)

    def test_run_game_vr_sends_default_and_migrates_before_vr(self):
        r = src('tools/run_game_vr.py')
        self.assertIn('live_settings.cutscenes_start(', r)
        self.assertIn('live_settings.migrate_078_cutscenes(', r)
        self.assertLess(r.index('live_settings.migrate_078_cutscenes('), r.index('vr_start_settings('))

    def test_nothing_writes_flat_by_default(self):
        for rel in ('tools/run_game_vr.py', 'tools/live_settings.py'):
            self.assertNotRegex(src(rel), r"['\"]cutscenes['\"]\s*:\s*['\"]flat['\"]", rel)
        for f in LAUNCHER.glob('*.cs'):
            self.assertNotRegex(f.read_text(encoding='utf-8'), r'"cutscenes"\s*,\s*"flat"', f.name)
        self.assertIsNone(L.parse(L.HEADER)[0].get('cutscenes'))  # a new live.txt has no cutscenes line


class MigrationPythonTests(unittest.TestCase):
    def setUp(self):
        self.dir = pathlib.Path(tempfile.mkdtemp())
        self.live = self.dir / 'live.txt'

    def tearDown(self):
        shutil.rmtree(self.dir, ignore_errors=True)

    def value(self):
        return L.parse(self.live.read_text(encoding='utf-8'))[0].get('cutscenes')

    def test_fresh_install(self):
        self.assertIsNone(L.migrate_078_cutscenes(self.live))
        self.assertFalse(self.live.exists())
        self.assertIn(L.MIGRATED_078_CUTSCENES, L.HEADER)  # a live.txt written new is already migrated
        self.assertEqual(L.cutscenes_start({})[0], 'immersive')

    def test_old_flat(self):
        for old in ('flat', '0', '3d', '1'):
            self.live.write_text('# old build\nmirror_view = on\ncutscenes = %s\n' % old, encoding='utf-8')
            line = L.migrate_078_cutscenes(self.live)
            self.assertIn('set once to immersive', line)
            self.assertEqual(self.value(), 'immersive')
            self.assertIn(L.MIGRATED_078_CUTSCENES, self.live.read_text(encoding='utf-8'))
            self.assertIn('mirror_view = on', self.live.read_text(encoding='utf-8'))
            self.assertIsNone(L.migrate_078_cutscenes(self.live))  # once

    def test_old_without_cutscenes_or_immersive(self):
        for text in ('mirror_view = on\n', 'cutscenes = immersive\n'):
            self.live.write_text(text, encoding='utf-8')
            self.assertIsNone(L.migrate_078_cutscenes(self.live))
            self.assertIn(L.MIGRATED_078_CUTSCENES, self.live.read_text(encoding='utf-8'))

    def test_launcher_flat_after_migration_stays(self):
        self.live.write_text('cutscenes = flat\n', encoding='utf-8')
        L.migrate_078_cutscenes(self.live)
        text = L.write_back(self.live.read_text(encoding='utf-8'), {'cutscenes': 'flat'})
        self.live.write_text(text, encoding='utf-8')
        for _ in range(3):
            self.assertIsNone(L.migrate_078_cutscenes(self.live))
        self.assertEqual(self.value(), 'flat')
        self.assertIsNone(L.cutscenes_start({'cutscenes': 'flat'})[0])


def mono_tools():
    mcs, mono = shutil.which('mcs'), shutil.which('mono')
    return (mcs, mono) if mcs and mono else None


HARNESS = r'''
using System; using System.IO;
namespace Sm2VrLauncher { static class T { static int Main(string[] a) {
    string root = a[0], step = a[1];
    if (step == "migrate") Console.WriteLine(LiveFile.MigrateCutscenes078(root) ?? "null");
    else if (step == "choose") Console.WriteLine(LiveFile.SetChosen(root, "cutscenes", a[2]));
    Console.WriteLine("VALUE " + (LiveFile.Choice(root, "cutscenes", new[]{"flat","3d","immersive"}, "immersive")));
    Console.WriteLine("MARKER " + LiveFile.MigratedCutscenes078);
    return 0; } } }
'''


@unittest.skipUnless(mono_tools(), 'mcs / mono not here (the launcher build checks the C# side)')
class MigrationLauncherTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = pathlib.Path(tempfile.mkdtemp())
        (cls.tmp / 'T.cs').write_text(HARNESS, encoding='utf-8')
        cls.exe = cls.tmp / 'T.exe'
        mcs, _ = mono_tools()
        subprocess.run([mcs, '-nologo', '-out:' + str(cls.exe), str(LAUNCHER / 'LiveFile.cs'), str(cls.tmp / 'T.cs')],
                       check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def run_step(self, root, *args):
        out = subprocess.run([mono_tools()[1], str(self.exe), str(root), *args], check=True, capture_output=True,
                             text=True).stdout.splitlines()
        return out

    def setUp(self):
        self.root = pathlib.Path(tempfile.mkdtemp())
        self.live = self.root / 'live.txt'

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def test_marker_matches_python(self):
        out = self.run_step(self.root, 'show')
        self.assertIn('MARKER ' + L.MIGRATED_078_CUTSCENES, out)

    def test_fresh_install(self):
        out = self.run_step(self.root, 'migrate')
        self.assertEqual(out[0], 'null')
        self.assertFalse(self.live.exists())
        self.assertIn('VALUE immersive', out)

    def test_old_flat(self):
        self.live.write_text('mirror_view = on\r\ncutscenes = flat\r\n', encoding='utf-8')
        out = self.run_step(self.root, 'migrate')
        self.assertIn('set once to immersive', out[0])
        self.assertIn('VALUE immersive', out)
        self.assertIsNone(L.migrate_078_cutscenes(self.live))  # the game's side sees it done
        self.assertEqual(L.parse(self.live.read_text(encoding='utf-8'))[0].get('cutscenes'), 'immersive')

    def test_launcher_flat_after_migration_stays(self):
        self.live.write_text('cutscenes = 3d\r\n', encoding='utf-8')
        self.run_step(self.root, 'migrate')
        out = self.run_step(self.root, 'choose', 'flat')
        self.assertIn('True', out)
        self.assertEqual(self.run_step(self.root, 'migrate')[0], 'null')
        self.assertIsNone(L.migrate_078_cutscenes(self.live))  # run_game_vr.py keeps it too
        self.assertEqual(L.parse(self.live.read_text(encoding='utf-8'))[0].get('cutscenes'), 'flat')

    def test_launcher_flat_on_fresh_install_stays(self):
        out = self.run_step(self.root, 'choose', 'flat')
        self.assertIn('VALUE flat', out)
        self.assertIsNone(L.migrate_078_cutscenes(self.live))
        self.assertEqual(L.parse(self.live.read_text(encoding='utf-8'))[0].get('cutscenes'), 'flat')


class LauncherSettingTests(unittest.TestCase):
    def setUp(self):
        self.opts = (LAUNCHER / 'LaunchOptions.cs').read_text(encoding='utf-8')

    def test_cutscenes_option(self):
        m = re.search(r'Opt\.LivePick\("COMFORT", "cutscenes", "CUTSCENES", "([^"]+)",\s*new\[\] ?\{ ?"flat", ?"3d", ?'
                      r'"immersive" ?\},\s*new\[\] ?\{ ?"Flat", ?"3D", ?"Immersive" ?\}, "immersive"\)', self.opts)
        self.assertIsNotNone(m, 'the CUTSCENES option')
        sub = m.group(1)
        self.assertNotIn('. ', sub)  # one sentence
        self.assertTrue(sub.endswith('.') and len(sub) <= 110, sub)

    def test_live_choice_never_an_argument(self):
        self.assertRegex(self.opts, r'case OptKind\.LiveChoice:\s*break;')
        form = (LAUNCHER / 'MainForm.Options.cs').read_text(encoding='utf-8')
        self.assertIn('LiveFile.SetChosen(', form)
        self.assertIn('LiveFile.Choice(', form)
        main = (LAUNCHER / 'MainForm.cs').read_text(encoding='utf-8')
        self.assertLess(main.index('LiveFile.MigrateCutscenes078(root)'), main.index('BuildOptions('))


class MenuProofTests(unittest.TestCase):
    def test_cut14_replay_registered(self):
        c = src('CMakeLists.txt')
        self.assertIn('add_test(NAME cut14 COMMAND spidy_cut14_tests)', c)
        g = src('src/game_xr.cpp')
        self.assertIn('cutscene077::menuScreenFrame(cutMenuScreen, cutFrame)', g)
        self.assertIn('cutscene077::immersiveFrame(immersiveHold, menuScreen, cutFrame)', g)
        self.assertIn('cutFrame.mode = cutMode;', g)
        h = src('include/spidy/cutscene077.hpp')
        # cut20: the menu press includes the both-thumbsticks pause; the resume inputs follow (tests/cut20_tests.cpp)
        self.assertIn('f.screen && f.mode == immersive, f.record, f.menuPressed || f.stickPause, f.nowMs,', h)
        self.assertIn('f.viewing && f.vrPlay && f.gameplay, f.timeRuns, f.shownBefore, f.closePressed', h)
        self.assertIn('bool frontEnd_{true}', h)


if __name__ == '__main__':
    unittest.main()
