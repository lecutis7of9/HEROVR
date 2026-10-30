"""HERO VR 0.0.74 test 3 SAFETY CHECKS: MOD CHECK (other mods' loaders moved aside for the session and put
back), TWO-GPU CHECK (Windows' graphics preference on hybrid PCs, backed up and put back), CLEARER ERRORS (a
crash while loading named with its module, DXGI device-lost fix steps) and the crash dump in Reports.
Each is on by default and can be turned off. Made by Agent Mulder."""
import contextlib, io, json, os, pathlib, sys, tempfile, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import mod_check, gpu_check, start_errors  # noqa: E402
def src(p):
    with open(os.path.join(ROOT, p), encoding='utf-8') as f: return f.read()
quiet = lambda *a, **k: None

# The October 29 player's game log lines (the address is the player's, read from the log, not a game address).
CRASH_LOG = ('19:54:20:042 (00063088) > [Crash] Unhandled exception at address 0x000000006aa9251e (code: 0xc0000005): '
             '"Access violation"\n19:54:20:566 (00063088) > [Crash] Created minidump at "C:\\Users\\p\\Documents\\'
             'Marvel\'s Spider-Man 2\\Marvel\'s Spider-Man 2-v2.810.0.0 2026-10-29-19-54-20.mdmp".\n')


class FakeReg:
    HKEY_CURRENT_USER, KEY_READ, KEY_WRITE, REG_SZ = 1, 1, 2, 1
    def __init__(self, values=None): self.values = dict(values or {})
    def CreateKeyEx(self, *a): return contextlib.nullcontext(self)
    def OpenKey(self, *a): return contextlib.nullcontext(self)
    def QueryValueEx(self, key, name):
        if name not in self.values: raise FileNotFoundError(name)
        return self.values[name], 1
    def SetValueEx(self, key, name, _, kind, value): self.values[name] = value
    def DeleteValue(self, key, name): del self.values[name]


class ModCheck(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); t = pathlib.Path(self.tmp.name)
        self.game, self.reports = t/'game', t/'reports'
        self.game.mkdir(); self.reports.mkdir()
        (self.game/'Spider-Man2.exe').write_bytes(b'exe')
        (self.game/'winmm.dll').write_bytes(b'loader')
        (self.game/'DINPUT8.dll').write_bytes(b'asi')
    def tearDown(self): self.tmp.cleanup()

    def test_finds_and_moves_aside_then_puts_back(self):
        self.assertEqual(sorted(n.lower() for n in mod_check.found(self.game)), ['dinput8.dll', 'winmm.dll'])
        moved = mod_check.move_aside(self.game, self.reports, log=quiet)
        self.assertEqual(len(moved), 2)
        self.assertFalse((self.game/'winmm.dll').exists())
        self.assertTrue((self.game/'_hero-vr-moved'/'winmm.dll').exists())
        self.assertTrue((self.reports/'mod-check-moved.json').exists())
        self.assertEqual(mod_check.restore(self.reports, game_running=True, log=quiet), [])  # never while the game runs
        self.assertEqual(len(mod_check.restore(self.reports, log=quiet)), 2)
        self.assertEqual((self.game/'winmm.dll').read_bytes(), b'loader')
        self.assertFalse((self.game/'_hero-vr-moved').exists())
        self.assertFalse((self.reports/'mod-check-moved.json').exists())

    def test_a_mod_manager_putting_it_back_is_named(self):
        moved = mod_check.move_aside(self.game, self.reports, names=['winmm.dll'], log=quiet)
        (self.game/'winmm.dll').write_bytes(b'overstrike put it back')
        back = mod_check.came_back(self.game, moved)
        self.assertEqual(back, ['winmm.dll'])
        text = mod_check.came_back_text(back)
        self.assertIn('MOD CHECK: winmm.dll came back', text); self.assertIn('Overstrike', text)
        mod_check.restore(self.reports, log=quiet)  # a different file came back: both are kept, nothing lost
        self.assertEqual((self.game/'winmm.dll').read_bytes(), b'overstrike put it back')
        self.assertTrue(any(p.read_bytes() == b'loader' for p in self.game.rglob('winmm.dll')))

    def test_an_old_waiter_does_not_undo_a_new_session(self):
        mod_check.move_aside(self.game, self.reports, names=['winmm.dll'], log=quiet)
        self.assertEqual(mod_check.restore(self.reports, log=quiet, only_token='an older session'), [])
        self.assertFalse((self.game/'winmm.dll').exists())

    def test_launcher_list_is_the_same(self):
        cs = src('tools/launcher/src/SafetyChecks.cs')
        for name in mod_check.LOADERS:
            self.assertIn('"%s"' % name, cs)


class TwoGpuCheck(unittest.TestCase):
    def test_hybrid_and_monitor_on_the_processor_graphics(self):
        amd_igpu = dict(name='AMD Radeon(TM) Graphics', vendor=gpu_check.AMD, dedicated_mb=512, outputs=1)
        nvidia = dict(name='NVIDIA GeForce RTX 5090', vendor=gpu_check.NVIDIA, dedicated_mb=32000, outputs=0)
        c = gpu_check.classify([amd_igpu, nvidia, nvidia, dict(name='Microsoft Basic Render Driver', vendor=gpu_check.MICROSOFT)])
        self.assertTrue(c['hybrid']); self.assertTrue(c['monitor_on_integrated'])
        self.assertEqual(c['discrete'], ['NVIDIA GeForce RTX 5090'])
        self.assertFalse(gpu_check.classify([nvidia])['hybrid'])
        self.assertFalse(gpu_check.classify([amd_igpu, dict(nvidia, outputs=1)])['monitor_on_integrated'])

    def test_preference_is_set_backed_up_and_put_back(self):
        self.assertEqual(gpu_check.with_preference('SwapEffectUpgradeEnable=1;'), 'GpuPreference=2;SwapEffectUpgradeEnable=1;')
        game, vrs, vrc = r'C:\g\Spider-Man2.exe', r'C:\s\bin\win64\vrserver.exe', r'C:\s\bin\win64\vrcompositor.exe'
        reg = FakeReg({game: 'GpuPreference=1;', vrc: 'SwapEffectUpgradeEnable=1;'})
        with tempfile.TemporaryDirectory() as reports:
            self.assertEqual(sorted(gpu_check.apply([game, vrs, vrc], reports, reg=reg, log=quiet)), sorted([game, vrs, vrc]))
            self.assertTrue(all('GpuPreference=2;' in reg.values[p] for p in (game, vrs, vrc)))
            gpu_check.apply([game], reports, reg=reg, log=quiet)  # a second session keeps the first backup
            reg.values[vrc] = 'GpuPreference=1;'                 # the player changed this one since: it stays
            back = gpu_check.restore(reports, reg=reg, log=quiet)
            self.assertEqual(sorted(back), sorted([game, vrs]))
            self.assertEqual(reg.values, {game: 'GpuPreference=1;', vrc: 'GpuPreference=1;'})
            self.assertFalse(os.path.exists(os.path.join(reports, gpu_check.BACKUP)))

    def test_launcher_puts_it_back_when_turned_off_and_before_revert(self):
        self.assertIn('gpu-preference-backup.json', src('tools/launcher/src/SafetyChecks.cs'))
        self.assertIn('opt.Param == "NoGpuCheck" && !t.Checked', src('tools/launcher/src/MainForm.Options.cs'))
        self.assertIn('SafetyChecks.RestoreGpuPreferences(root)', src('tools/launcher/src/MainForm.Updates.cs'))


class ClearerErrors(unittest.TestCase):
    def test_the_players_crash_while_loading(self):
        found = start_errors.crash(CRASH_LOG)
        self.assertEqual((found['address'], found['code']), (0x6aa9251e, '0xC0000005'))
        self.assertTrue(found['minidump'].endswith('.mdmp'))
        modules = [dict(name='winmm.dll', base=0x6aa00000, size=0x100000, path=r'C:\Games\SM2\winmm.dll')]
        text = start_errors.crash_message(found, modules, loaders=['winmm.dll'], game_folder=r'C:\Games\SM2')
        self.assertIn('The game crashed while loading', text)
        self.assertIn('winmm.dll (+0x9251e)', text)
        self.assertIn('press Reports', text)

    def test_windows_own_winmm_is_not_blamed(self):
        found = start_errors.crash(CRASH_LOG)
        modules = [dict(name='winmm.dll', base=0x6aa00000, size=0x100000, path=r'C:\Windows\System32\winmm.dll')]
        text = start_errors.crash_message(found, modules, loaders=[], game_folder=r'C:\Games\SM2')
        self.assertNotIn('Likely cause', text)
        self.assertNotIn("in the game's folder", text)

    def test_device_lost_fix_steps(self):
        text = start_errors.device_lost_steps('0x887A0005', 'DXGI_ERROR_DEVICE_REMOVED', 'while the game was starting')
        for step in ('graphics card', 'High performance', 'driver', 'Normal camera on the monitor: On', '#spidy-bugs'):
            self.assertIn(step, text)
        self.assertIn('0x887A0005', text)
        run = src('tools/run_game_vr.py')
        self.assertIn('start_errors.device_lost_steps(', run)
        self.assertIn('clear=not a.no_clear_errors', run)
        self.assertIn('crash_while_loading(', run)


class SwitchesAndReports(unittest.TestCase):
    def test_options_on_by_default_with_one_line_help(self):
        lo = src('tools/launcher/src/LaunchOptions.cs')
        for param, title in (('NoModCheck', 'MOD CHECK'), ('NoGpuCheck', 'TWO-GPU CHECK'), ('NoClearErrors', 'CLEARER ERRORS')):
            line = next(l for l in lo.splitlines() if '"%s"' % param in l)
            self.assertIn('Opt.Inv("SAFETY CHECKS"', line); self.assertIn('"%s"' % title, line)
            self.assertIn('restart', line)
        ps = src('tools/launch-game-vr.ps1')
        for switch, flag in (('NoModCheck', '--no-mod-check'), ('MoveModLoaders', '--move-mod-loaders'),
                             ('NoGpuCheck', '--no-gpu-check'), ('NoClearErrors', '--no-clear-errors')):
            self.assertIn('[switch]$' + switch, ps); self.assertIn(flag, ps)
            if switch != 'MoveModLoaders':
                self.assertIn("'%s'" % flag, src('tools/run_game_vr.py'))
        mf = src('tools/launcher/src/MainForm.cs')
        self.assertIn('extra.Add("-MoveModLoaders")', mf)
        self.assertIn('MessageBoxButtons.YesNoCancel', mf)

    def test_reports_add_the_newest_crash_dump(self):
        cr = src('tools/collect-reports.ps1')
        self.assertIn('[switch]$NoCrashDump', cr)
        self.assertIn('*.mdmp', cr); self.assertIn("Marvel's Spider-Man 2", cr)
        self.assertIn('"Crash dump in reports"', src('tools/launcher/src/MainForm.Options.cs'))
        self.assertIn('"-NoCrashDump"', src('tools/launcher/src/MainForm.cs'))

    def test_credits(self):
        for f in ('tools/mod_check.py', 'tools/gpu_check.py', 'tools/start_errors.py'):
            self.assertIn('Made by Agent Mulder', src(f))


if __name__ == '__main__':
    unittest.main()
