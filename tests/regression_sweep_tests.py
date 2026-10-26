#!/usr/bin/env python3
"""Version 0.0.62 REGRESSION SWEEP: every issue players or the user have hit has a test that would catch it again.

One class per past issue. Where an older test already covers the issue, COVERAGE pins that test (file and name)
so deleting or renaming it fails here. The rest are checked here, by running the real code where the box can:
  - old launchers (builds 3, 4, 6, 7, 8 from the shipped zips) are compiled with mono and asked whether 0.0.62
    is newer, and the update rule (launcher_build) they ship is checked;
  - PowerShell's real CLIXML output is fed to the launcher's own Clixml decoder;
  - the launch script's Python check is run (pwsh) on a fake Microsoft Store stub, a broken and an old Python;
  - the Nexus zip and release asset checkers are run on good and bad examples.
Environment: HEROVR_LAUNCHER (launcher source, default /workspace/herovr-launcher-0062, else tools/launcher), REGRESSION_STRICT=1 turns
"resource missing" skips into failures (the gate sets it), PWSH (default ~/pwsh/pwsh)."""
import hashlib, io, json, os, pathlib, re, shutil, subprocess, sys, tempfile, unittest, zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
TESTS = ROOT / 'tests'
# The launcher sources: HEROVR_LAUNCHER, the developer copy, or (the published repo, CI) tools/launcher.
L = pathlib.Path(os.environ.get('HEROVR_LAUNCHER') or next(
    (p for p in ('/workspace/herovr-launcher-0062',) if os.path.isfile(os.path.join(p, 'src', 'ModVersion.cs'))),
    str(ROOT / 'tools' / 'launcher')))
STRICT = os.environ.get('REGRESSION_STRICT') == '1'
PWSH = os.environ.get('PWSH', os.path.expanduser('~/pwsh/pwsh'))
CHECKS = ROOT / 'tools' / 'release_checks'
# The launcher zips players have installed: (build, zip, tag of the release they came from).
SHIPPED = [(2, '/workspace/live-before/HERO-VR-Launcher-0.0.5.zip'),   # launcher 0.0.6.1: the update-loop report
           (3, '/workspace/publish-final/HERO-VR-Launcher-0.0.5.zip'),
           (4, '/workspace/publish-hf3/HERO-VR-Launcher-0.0.5.zip'),
           (6, '/tmp/rel/live-b6/HERO-VR-Launcher-0.0.6.zip'),
           (7, '/tmp/rel/live-b7/HERO-VR-Launcher-0.0.6.zip'),
           (8, '/tmp/rel/live-b8/HERO-VR-Launcher-0.0.6.zip')]
# Version 0.0.63: launcher build 9 (the 0.0.62 release) is shipped too. It has the build-9 update rule (Math.Max with the
# installed launcher), so update_loop_tests checks it on its own.
SHIPPED_B9 = (9, '/workspace/herovr-release/github/release-assets-0.0.62/HERO-VR-Launcher-0.0.62.zip')
# This tree's mod version (VERSION, PORT-VERSION): 0.0.62 (build 39.2) for the 0.0.62 release, 0.0.63 (39.3) after.
CUR = (ROOT / 'VERSION').read_text(encoding='utf-8').strip()
CUR_PORT = (ROOT / 'PORT-VERSION').read_text(encoding='utf-8').strip()


def need(cond, what):
    if not cond:
        if STRICT:
            raise AssertionError('missing for the regression sweep: ' + what)
        raise unittest.SkipTest(what)


def src(name):
    need((L / 'src' / name).is_file(), 'launcher source ' + str(L / 'src' / name))
    return (L / 'src' / name).read_text(encoding='utf-8')


# Issue -> the existing tests that already catch it (file, test name). They run in the standard gate (G1 runs every
# tests/*.py, G5* the named ones); this only pins that they still exist.
COVERAGE = {
    'windows.h min/max without NOMINMAX': [('windows_header_tests.py', 'NOMINMAX')],
    'VR lost after switching to the VD desktop / flat launch': [
        ('vr_lost_hotfix_tests.py', 'test_the_always_running_vd_service_is_not_a_connected_headset'),
        ('vr_lost_hotfix_tests.py', 'test_launcher_offers_to_close_the_game_and_never_starts_flat'),
        ('vr_lost_hotfix_tests.py', 'test_preflight_waits_for_the_headset_before_the_game_starts'),
        ('vr_recovery_tests.py', 'def test_')],
    'swapchain XrResult=-2 flat fallback': [
        ('swapchain_retry_hotfix_tests.py', 'test_every_eye_swapchain_goes_through_the_ladder'),
        ('swapchain_retry_hotfix_tests.py', 'test_any_openxr_refusal_at_the_start_is_tried_again')],
    'update deleting the bundled Python': [
        ('launcher_build8_tests.py', 'test_same_version_update_reinstalls_when_python_is_missing'),
        ('launcher_build8_tests.py', 'test_play_vr_and_status_repair_a_folder_without_python'),
        ('bundled_python_zip_tests.py', 'test_package_ships_python')],
    'stale modules after a same-version update': [
        ('launcher_build8_tests.py', 'test_same_version_update_downloads_a_newer_builds_mod_files'),
        ('launcher_build8_tests.py', 'test_start_up_replaces_stale_mod_files_by_itself')],
    'update loop: launcher started from outside the install re-offers the installed version': [
        ('update_loop_tests.py', 'test_no_build_offers_0_0_62_again_after_installing_it'),
        ('update_loop_tests.py', 'test_the_release_manifest_gate_rejects_launcher_build'),
        ('update_loop_tests.py', 'test_build_9_hands_over_to_the_installed_launcher')],
    'PLAY VR goes grey then red with no window (0.0.6.1 launcher, 0.0.62 mod)': [
        ('launch_path_tests.py', 'test_build_2_options_reach_python'),
        ('launch_path_tests.py', 'test_every_failure_is_said_for_an_older_launcher'),
        ('launch_path_tests.py', 'test_a_throw_before_python_is_said'),
        ('launch_path_tests.py', 'test_every_nonzero_exit_shows_a_window')],
    'FLY MODE: punches never land (the thrust triggers read as a held trigger)': [
        ('core_tests.cpp', 'Version 0.0.62 FLY MODE punches: a held thrust trigger holds nothing back'),
        ('regression_sweep_tests.py', 'test_game_xr_marks_fly_mode_triggers')],
    'punches barely land (a far lock takes the punch at the thug in front)': [
        ('core_tests.cpp', 'Version 0.0.62: a thug within punch reach takes the punch from a lock 5 m away')],
    'ranged punches miss the intended target (lock and look only, the fist ignored)': [
        ('core_tests.cpp', 'Version 0.0.62 AIM PICK: the fist and the look pick the nearest enemy in the cone'),
        ('regression_sweep_tests.py', 'test_both_strike_paths_use_the_aim_pick')],
    'enemies trail behind while flying fast (ENEMY TRACKING LIVE fell back over 40 m/s)': [
        ('core_tests.cpp', 'Version 0.0.62 ENEMY TRACKING LIVE: the world list stays usable while flying fast'),
        ('regression_sweep_tests.py', 'test_game_xr_marks_fly_mode_triggers')],
    'Revert to 0.0.6 offered 0.0.62 again at once (skip key 0.0.62#9 vs 0.0.62#0)': [
        ('update_loop_tests.py', 'test_revert_skips_the_key_older_launchers_compute')],
    'placeholder or blank repo setting': [('launcher_build8_tests.py', 'test_blank_or_placeholder_repository_falls_back_to_herovr')],
    'bracketed folder paths / running inside the zip': [('launcher_build9_tests.py', 'test_build_ps1_literal_paths_and_log'),
                                                         ('launcher_build9_tests.py', 'test_cmd')],
    'revert must offer 0.0.6 and 0.0.5, numeric versions': [('launcher_build9_tests.py', 'test_revert_offers_both_earlier_releases'),
                                                            ('launcher_build9_tests.py', 'test_semver_is_numeric')],
    'build 10 new design: fallback to classic, Shift/--classic, every past issue, DPI/screens/locales': [
        ('launcher_hero_tests.py', 'test_injected_failures_fall_back'), ('launcher_hero_tests.py', 'test_hang_before_first_frame'),
        ('launcher_hero_tests.py', 'class PastIssues'), ('launcher_hero_tests.py', 'test_self_test')],
    'release without the prebuilt mod': [('v006_tests.py', 'test_build_ps1_routes_launcher_runs_to_prebuilt'),
                                         ('v006_tests.py', 'test_install_prebuilt_checks_sha')],
}


class Coverage(unittest.TestCase):
    def test_every_pinned_test_still_exists(self):
        missing = []
        for issue, refs in COVERAGE.items():
            for f, name in refs:
                p = TESTS / f
                if not p.is_file() or name not in p.read_text(encoding='utf-8'):
                    missing.append(f'{issue}: {f} {name}')
        self.assertEqual(missing, [])


class Combat062(unittest.TestCase):
    """0.0.62 combat steering (7:12): FLY MODE punches, far-lock punches, ranged aim, fast-flight tracking."""
    def test_game_xr_marks_fly_mode_triggers(self):
        g = (ROOT / 'src' / 'game_xr.cpp').read_text(encoding='utf-8')
        self.assertIn('cf.flightTriggers = flyModeNow.load() != 0 && flyNow.flying', g)
        self.assertIn('combat38::foeWorldUsable(', g)
        self.assertNotIn('<= 3.f + 40.f * std::clamp(age', g)

    def test_both_strike_paths_use_the_aim_pick(self):
        ci = (ROOT / 'include' / 'spidy' / 'combat_input.hpp').read_text(encoding='utf-8')
        cs = (ROOT / 'include' / 'spidy' / 'combat_spidy.hpp').read_text(encoding='utf-8')
        ws = (ROOT / 'src' / 'web_strike.cpp').read_text(encoding='utf-8')
        for text in (ci, cs):
            self.assertIn('wish.gazePick = true;', text)
        self.assertIn('aimRetarget(wish.target', ws)


class NoMinMax(unittest.TestCase):
    """0.0.2 hotfix: MSVC C2589 on std::min/max because windows.h came in without NOMINMAX."""
    def test_every_windows_h_in_the_tree_is_guarded(self):
        cm = (ROOT / 'CMakeLists.txt').read_text()
        self.assertIn('NOMINMAX', cm)
        bad = []
        for p in list((ROOT / 'src').rglob('*.[ch]pp')) + list((ROOT / 'include').rglob('*.hpp')) + list((ROOT / 'tests').rglob('*.cpp')):
            s = p.read_text(encoding='utf-8', errors='replace')
            m = re.search(r'^\s*#\s*include\s*<windows\.h>', s, re.M | re.I)
            if m and p.parts[-2] == 'tests' and 'NOMINMAX' not in s[:m.start()]:
                bad.append(str(p.relative_to(ROOT)))
        self.assertEqual(bad, [], 'test sources include windows.h without NOMINMAX first')
        r = subprocess.run([sys.executable, str(TESTS / 'windows_header_tests.py')], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)


class NetFx48(unittest.TestCase):
    """Launcher build 2 hotfix: Trim(char) and other APIs missing from .NET Framework 4.8 crashed the launcher."""
    def test_checker_runs_in_the_launcher_build_and_on_the_sources(self):
        need((L / 'tools' / 'check-netfx.py').is_file(), 'launcher check-netfx.py')
        self.assertIn('check-netfx.py', (L / 'build.sh').read_text())
        r = subprocess.run([sys.executable, str(L / 'tools' / 'check-netfx.py'), str(L / 'src')], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_checker_catches_the_original_bug(self):
        need((L / 'tools' / 'check-netfx.py').is_file(), 'launcher check-netfx.py')
        with tempfile.TemporaryDirectory() as d:
            for bad in ("var s = x.Trim('/');", "var p = v.Split('.');", "if (s.StartsWith('#')) {}", "int n = Math.Clamp(a, 0, 1);"):
                pathlib.Path(d, 'Bad.cs').write_text('class C { void M() { ' + bad + ' } }')
                r = subprocess.run([sys.executable, str(L / 'tools' / 'check-netfx.py'), d], capture_output=True, text=True)
                self.assertNotEqual(r.returncode, 0, 'not caught: ' + bad)


def zipof(entries):
    b = io.BytesIO()
    with zipfile.ZipFile(b, 'w') as z:
        for n, data in entries.items():
            z.writestr(n, data)
    return b.getvalue()


class NexusZip(unittest.TestCase):
    """Nexus quarantined an upload with an exe; no archive may hide inside the upload either."""
    def run_check(self, entries):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d, 'n.zip'); p.write_bytes(zipof(entries))
            r = subprocess.run([sys.executable, str(CHECKS / 'check_nexus_zip.py'), str(p)], capture_output=True, text=True)
            return r.returncode, r.stdout

    def test_clean_zip_passes(self):
        self.assertEqual(self.run_check({'HERO-VR/README.md': 'hi', 'HERO-VR/x.cmd': '@echo off', 'HERO-VR/a.png': b'\x89PNG\r\n'})[0], 0)

    def test_exe_by_name_or_content_fails(self):
        self.assertEqual(self.run_check({'HERO-VR/SM2VRLauncher.exe': b'MZ\x90\x00'})[0], 1)
        self.assertEqual(self.run_check({'HERO-VR/innocent.txt': b'MZ\x90\x00\x03'})[0], 1)
        self.assertEqual(self.run_check({'HERO-VR/x.dll': b'x'})[0], 1)

    def test_nested_archives_fail(self):
        self.assertEqual(self.run_check({'HERO-VR/mod.zip': zipof({'a': 'b'})})[0], 1)
        self.assertEqual(self.run_check({'HERO-VR/data.bin': zipof({'a': 'b'})})[0], 1)
        self.assertEqual(self.run_check({'HERO-VR/x.7z': b"7z\xbc\xaf'\x1c"})[0], 1)

    def test_one_top_folder(self):
        self.assertEqual(self.run_check({'a.txt': 'x', 'b/c.txt': 'y'})[0], 1)

    def test_the_nexus_script_runs_the_check(self):
        s = pathlib.Path('/workspace/herovr-release/make_nexus_0062.sh')
        need(s.is_file(), 'make_nexus_0062.sh')
        self.assertIn('check_nexus_zip.py', s.read_text())


class ReleaseHasPrebuilt(unittest.TestCase):
    """0.0.5's first release asked players to build the mod: every release must list the ready-made mod."""
    def run_check(self, d, *a):
        return subprocess.run([sys.executable, str(CHECKS / 'check_release_manifest.py'), d, *a], capture_output=True, text=True)

    def make(self, d, prebuilt=True, wrong_hash=False):
        mod = b'mod bytes'; pathlib.Path(d, 'HERO-VR-9.9.9-mod-win64.zip').write_bytes(mod)
        pathlib.Path(d, 'HERO-VR-Launcher-9.9.9.zip').write_bytes(b'l'); pathlib.Path(d, 'SHA256SUMS.txt').write_text('x')
        files = [{'kind': 'source', 'name': 'HERO-VR-Launcher-9.9.9.zip', 'url': 'https://x/download/v9.9.9/HERO-VR-Launcher-9.9.9.zip'}]
        if prebuilt:
            files.append({'kind': 'prebuilt', 'name': 'HERO-VR-9.9.9-mod-win64.zip', 'size': len(mod),
                          'url': 'https://x/download/v9.9.9/HERO-VR-9.9.9-mod-win64.zip',
                          'sha256': '0' * 64 if wrong_hash else hashlib.sha256(mod).hexdigest()})
        pathlib.Path(d, 'version.json').write_text(json.dumps({'tag': 'v9.9.9', 'version_number': '9.9.9', 'launcher_build_in_zip': 9, 'files': files}))

    def test_good_passes_missing_or_wrong_fails(self):
        for kw, rc in (({}, 0), ({'prebuilt': False}, 1), ({'wrong_hash': True}, 1)):
            with tempfile.TemporaryDirectory() as d:
                self.make(d, **kw)
                self.assertEqual(self.run_check(d, '--tag', 'v9.9.9').returncode, rc, kw)

    def test_the_shipped_releases_pass(self):
        # Both were launcher-only re-releases of a version already out (their launcher_build is the hotfix case).
        for d in ('/workspace/publish-hf3', '/tmp/rel/live-b8'):
            need(os.path.isdir(d), d)
            r = self.run_check(d, '--launcher-only-hotfix')
            self.assertEqual(r.returncode, 0, r.stdout)

    def test_finalize_runs_the_check(self):
        s = pathlib.Path('/tmp/rel/finalize_00062.sh')
        need(s.is_file(), 'finalize_00062.sh')
        self.assertIn('check_release_manifest.py', s.read_text())


class PowerShellOutput(unittest.TestCase):
    """The launcher log filled with '#< CLIXML <Objs ...>' instead of the script's words."""
    def test_every_powershell_start_asks_for_text(self):
        s = src('PsRunner.cs')
        starts = re.findall(r'"-NoProfile[^"]*"', s)
        self.assertTrue(starts)
        for a in starts:
            self.assertIn('-OutputFormat Text', a)
        self.assertIn('new Clixml()', s)

    def test_the_decoder_reads_real_clixml(self):
        need(os.path.exists(PWSH) and shutil.which('mcs') and shutil.which('mono'), 'pwsh + mono')
        s = src('PsRunner.cs')
        i = s.index('sealed class Clixml'); i = s.rfind('///', 0, i) if s.rfind('///', 0, i) > s.rfind('}', 0, i) else i
        depth, j = 0, s.index('{', s.index('sealed class Clixml'))
        for k in range(j, len(s)):
            depth += {'{': 1, '}': -1}.get(s[k], 0)
            if depth == 0:
                break
        cls = s[s.index('sealed class Clixml'):k + 1]
        r = subprocess.run([PWSH, '-NoProfile', '-OutputFormat', 'XML', '-Command',
                            "Write-Host 'HERO hello'; Write-Warning 'HERO careful'; Write-Error 'HERO boom'"],
                           capture_output=True, text=True)
        raw = r.stdout + r.stderr
        self.assertIn('CLIXML', raw)
        with tempfile.TemporaryDirectory() as d:
            pathlib.Path(d, 'in.txt').write_text(raw)
            pathlib.Path(d, 'c.cs').write_text(
                'using System; using System.Collections.Generic; using System.Text; using System.Text.RegularExpressions;\n'
                'namespace Sm2VrLauncher {\n' + cls + '\nstatic class H { static void Main(string[] a) { var c = new Clixml();\n'
                ' foreach (var l in System.IO.File.ReadAllLines(a[0])) foreach (var o in c.Feed(l)) Console.WriteLine("OUT:" + o); } } }')
            b = subprocess.run(['mcs', '-nologo', '-r:System.dll', '-out:' + d + '/c.exe', d + '/c.cs'], capture_output=True, text=True)
            self.assertEqual(b.returncode, 0, b.stdout + b.stderr)
            o = subprocess.run(['mono', d + '/c.exe', d + '/in.txt'], capture_output=True, text=True).stdout
        for w in ('HERO hello', 'HERO careful', 'HERO boom'):
            self.assertIn('OUT:', o)
            self.assertTrue(any(w in l for l in o.splitlines()), w + ' not decoded: ' + o[:400])
        self.assertNotIn('<Objs', o)

    def test_install_script_writes_plain_text(self):
        self.assertIn('[Console]', (ROOT / 'tools' / 'install-prebuilt.ps1').read_text(encoding='utf-8'))


class LockedOldFiles(unittest.TestCase):
    """A self-update or rollback stopped on a locked SM2VRLauncher.exe.old / files still open from a rollback."""
    def test_self_update_never_needs_to_delete_a_locked_old(self):
        s = src('Backups.cs')
        m = s[s.index('public static void MoveSelfAside'):]
        m = m[:m.index('\n        }') + 10]
        self.assertRegex(m, r'try \{ if \(File\.Exists\(old\)\) File\.Delete\(old\); \} catch \{ old = path \+ "\." \+ DateTime\.Now\.Ticks \+ "\.old"; \}')
        self.assertIn('File.Move(path, old)', m)
        self.assertIn('foreach (var f in Directory.GetFiles(dir, "*.old")) try { File.Delete(f); } catch { }', s)

    def test_old_exes_are_never_restored_or_backed_up(self):
        s = src('Backups.cs')
        self.assertIn('if (IsLauncherLeftover(rel)) return true;', s)

    def test_a_locked_rollback_changes_nothing_and_says_why(self):
        s = src('Installer.cs')
        self.assertIn('TransactionLocked', s)
        self.assertIn('Nothing was changed', s)
        self.assertIn('FinishRollbackLocked', s)


class StorePythonStub(unittest.TestCase):
    """The Microsoft Store 'python.exe' alias (WindowsApps) runs but is not Python; it must never be picked."""
    def fn(self):
        s = (ROOT / 'tools' / 'launch-game-vr.ps1').read_text(encoding='utf-8')
        i = s.index('function Test-SpidyPython')
        j = s.index('\n}\n', i) + 3
        return s, s[i:j]

    def test_the_package_only_ever_uses_its_own_python(self):
        s, _ = self.fn()
        self.assertIn("$bundledPython=Join-Path $projectRoot 'python\\python.exe'", s)
        p = s[s.index('if($packaged) {'):s.index('} elseif($Python) {')]
        self.assertIn('$Python=$bundledPython', p)
        self.assertNotRegex(p, r'Get-Command|where(\.exe)? python|WindowsApps')

    def test_a_store_stub_broken_or_old_python_is_refused(self):
        need(os.path.exists(PWSH), 'pwsh')
        _, f = self.fn()
        with tempfile.TemporaryDirectory() as d:
            fakes = {'stub': 'echo "Python was not found; run without arguments to install from the Microsoft Store" >&2\nexit 9009',
                     'broken': 'echo "Fatal Python error" >&2\nexit 1', 'old': 'echo "(3, 10)"', 'good': 'echo "(3, 12)"',
                     'garbage': 'echo "hello"'}
            for n, body in fakes.items():
                p = pathlib.Path(d, n); p.write_text('#!/bin/sh\n' + body + '\n'); p.chmod(0o755)
            ps = f + '\n' + '\n'.join(f"'{n}=' + [string](Test-SpidyPython '{d}/{n}')" for n in fakes) + f"\n'missing=' + [string](Test-SpidyPython '{d}/none')"
            pathlib.Path(d, 't.ps1').write_text(ps)
            r = subprocess.run([PWSH, '-NoProfile', '-File', d + '/t.ps1'], capture_output=True, text=True)
        got = dict(l.split('=', 1) for l in r.stdout.split() if '=' in l)
        self.assertEqual(got, {'stub': '', 'broken': '', 'old': '', 'good': '3.12', 'garbage': '', 'missing': ''}, r.stdout + r.stderr)


class SelfUpdateAndRevertAllBuilds(unittest.TestCase):
    """Every launcher players have (builds 2-4, 6-8; build 5 was never published) must see 0.0.62 as newer, and build 9's
    Revert must offer 0.0.6 and 0.0.5. The live E2E (fresh, self-update b4/b6/b7/b8, revert) runs after upload."""
    def compile_and_ask(self, modversion_cs, expect_build):
        with tempfile.TemporaryDirectory() as d:
            pathlib.Path(d, 'mv.cs').write_text(modversion_cs)
            pathlib.Path(d, 'h.cs').write_text(
                'namespace Sm2VrLauncher { static class H { static int Main() {\n'
                ' var a = ModVersion.Parse("Version %s (build %s)"); var b = ModVersion.Parse("0.0.6"); var c = ModVersion.Parse("0.0.5");\n'
                ' var v = ModVersion.Parse("v%s"); var p = ModVersion.Parse("0.0.62");\n'
                ' System.Console.WriteLine("build=" + LauncherInfo.Build);\n'
                ' return (a > b && b > c && a > c && !(b > a) && a.Patch == %d && v.CompareTo(a) == 0 && p > b && p > c && !(p > a) && (a > p || a.Patch == 62)) ? 0 : 1; } } }'
                % (CUR, CUR_PORT.replace('Update ', ''), CUR, int(CUR.split('.')[2])))
            r = subprocess.run(['mcs', '-nologo', '-out:' + d + '/h.exe', d + '/mv.cs', d + '/h.cs'], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            o = subprocess.run(['mono', d + '/h.exe'], capture_output=True, text=True)
            self.assertEqual(o.returncode, 0, f'build {expect_build}: {CUR} is not seen as newer than 0.0.62/0.0.6/0.0.5')
            self.assertIn(f'build={expect_build}', o.stdout)

    def test_every_shipped_launcher_sees_0_0_62_as_newer(self):
        need(shutil.which('mcs') and shutil.which('mono'), 'mono')
        for build, z in SHIPPED:
            need(os.path.isfile(z), z)
            with zipfile.ZipFile(z) as zz:
                names = zz.namelist()
                mv = [n for n in names if n.endswith('tools/launcher/src/ModVersion.cs')]
                self.assertEqual(len(mv), 1, z)
                self.compile_and_ask(zz.read(mv[0]).decode('utf-8'), build)
                every = ''.join(zz.read(n).decode('utf-8', 'replace') for n in names if n.endswith('.cs'))
                # The update rule they ship: a newer mod version, or the same version with a higher launcher_build,
                # read from releases/latest/download/version.json.
                self.assertIn('m.LauncherBuild > LauncherInfo.Build', every, z)
                self.assertIn('releases/latest/download/version.json', every, z)

    def test_build_9_itself(self):
        need(shutil.which('mcs') and shutil.which('mono'), 'mono')
        s = src('ModVersion.cs')
        # build 9 (Version 0.0.62) or later (build 10: the HERO VR design, branch launcher-redesign): never below 9,
        # so every shipped launcher (builds 3-9) is offered the update when version.json says launcher_build 10
        cur = int(re.search(r'public const int Build = (\d+);', s).group(1))
        self.assertGreaterEqual(cur, 9)
        self.compile_and_ask(s, cur)
        # build 9: 0.0.6 and 0.0.5; build 10 (Version 0.0.63): 0.0.62 first (launcher_build9_tests pins the order)
        self.assertIn('public static readonly string[] PreviousTags = { %s"v0.0.6", "v0.0.5" };' % ('"v0.0.62", ' if cur >= 10 else ''), s)

    def test_the_0_0_62_manifest_outranks_every_build(self):
        mj = src('ModVersion.cs')
        # 9 = the 0.0.62 release; 10 = the HERO VR design build (launcher-redesign), never below 9
        self.assertGreaterEqual(int(re.search(r'public const int Build = (\d+);', mj).group(1)), 9)
        self.assertTrue(all(b < 9 for b, _ in SHIPPED))
        cur = int(re.search(r'public const int Build = (\d+);', mj).group(1))
        # the release script for this tree's version (0.0.62: build 9; 0.0.63: build 10)
        name = 'make_release_%s.sh' % CUR.replace('.', '').zfill(5)
        s = pathlib.Path('/tmp/rel') / name
        need(s.is_file(), name)
        t = s.read_text()
        self.assertIn('--launcher-build %d' % cur, t)
        # ...but the published version.json carries it only as information (update-loop fix)
        self.assertIn("m['launcher_build_in_zip'] = m.pop('launcher_build', %d)" % cur, t)

    def test_the_live_e2e_covers_every_path(self):
        s = pathlib.Path('/workspace/herovr-release/e2e62/run.sh')
        need(s.is_file(), '/workspace/herovr-release/e2e62/run.sh')
        t = s.read_text()
        for mode in ('fresh)', 'selfupdate)', 'revert06|revert05|reupdate)', 'SM2VR_LAUNCHER_TEST_PREVIOUS_TAG'):
            self.assertIn(mode, t)
        c = (s.parent / 'check.py').read_text()
        for what in ('python/python.exe present', 'player file kept', "SkipUpdate", 'launcher exe = the'):
            self.assertIn(what, c)


class PublishIgnoredFiles(unittest.TestCase):
    """The publish script stopped on files .gitignore ignores ('paths are ignored by one of your .gitignore files')."""
    def test_hotfix_publisher_skips_ignored_files(self):
        p = pathlib.Path('/workspace/hotfix-bundle/HERO-VR-hotfix2-publish/publish-hotfix.ps1')
        need(p.is_file(), 'publish-hotfix.ps1')
        s = p.read_text(encoding='utf-8')
        self.assertIn('git check-ignore -q -- $gitPath', s)
        self.assertIn('.gitignore ignores it', s)

    def test_repo_sync_refuses_to_silently_drop_ignored_files(self):
        p = pathlib.Path('/tmp/rel/sync_repo.py')
        need(p.is_file(), '/tmp/rel/sync_repo.py')
        s = p.read_text()
        self.assertIn("'check-ignore'", s)
        self.assertIn('IGNORED', s)

    def test_repo_sync_check_works(self):
        p = pathlib.Path('/tmp/rel/sync_repo.py')
        need(p.is_file() and shutil.which('git'), 'sync_repo.py + git')
        with tempfile.TemporaryDirectory() as d:
            repo, stage = pathlib.Path(d, 'repo'), pathlib.Path(d, 'stage')
            repo.mkdir(); stage.mkdir()
            subprocess.run(['git', 'init', '-q', str(repo)], check=True)
            (repo / '.gitignore').write_text('/reports/\n')
            (stage / 'a.txt').write_text('a'); (stage / 'reports').mkdir(); (stage / 'reports' / 'r.txt').write_text('r')
            r = subprocess.run([sys.executable, str(p)], capture_output=True, text=True,
                               env={**os.environ, 'SYNC_STAGE': str(stage), 'SYNC_REPO': str(repo)})
            self.assertNotEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn('reports/r.txt', r.stdout)


if __name__ == '__main__':
    unittest.main(verbosity=1)
