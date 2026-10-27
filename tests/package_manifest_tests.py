# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
# Update 22: the shareable package's contents come from the code (tools/package_manifest.py), and the
# starters never start a broken Python.
import io, contextlib, pathlib, re, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import package_manifest as pm

SESSION_MODULES = {'spidy_headset_probe.exe', 'spidy_bridge.dll', 'spidy_render_probe.dll', 'spidy_ray_bridge.dll',
                   'spidy_movement_bridge.dll', 'spidy_stereo_probe.dll', 'spidy_render_memory.dll',
                   'bhaptics_library.dll'}


def write(folder, name, text):
    (pathlib.Path(folder) / name).write_text(text, encoding='utf-8')


class RealTools(unittest.TestCase):
    def setUp(self):
        self.m = pm.manifest(ROOT / 'tools')

    def test_closure_has_the_session_tools(self):
        for tool in ('run_game_vr', 'xr_runtime', 'vr_launcher', 'live_settings', 'inspect_game', 'session_comfort',
                     'probe_game_grab', 'probe_stereo'):
            self.assertIn(tool, self.m['tools'])
        self.assertEqual(self.m['missing_tools'], [])

    def test_every_listed_tool_is_imported_somewhere(self):
        # Nothing extra: each tool but the entries is imported by another listed tool.
        imported = set()
        for tool in self.m['tools']:
            imported |= {name.split('.')[0] for name, _ in pm.imports_of(ROOT / 'tools' / f'{tool}.py')}
        for tool in self.m['tools']:
            if tool not in pm.ENTRIES:
                self.assertIn(tool, imported)

    def test_session_modules_are_named(self):
        self.assertTrue(SESSION_MODULES <= set(self.m['binaries']), SESSION_MODULES - set(self.m['binaries']))

    def test_only_stdlib_beyond_the_tools(self):
        # The embeddable Python has no pip packages: every other import must be the standard library.
        stdlib = set(getattr(sys, 'stdlib_module_names', ())) | {'msvcrt', 'winreg', '_thread'}
        if not stdlib - {'msvcrt', 'winreg', '_thread'}:
            self.skipTest('needs Python 3.10+')
        for module in self.m['stdlib'] + self.m['optional']:
            self.assertIn(module.split('.')[0], stdlib, module)

    def test_no_personal_or_build_files_are_tools(self):
        self.assertFalse({'live', 'next-launch', 'package_manifest'} & set(self.m['tools']))


class Closure(unittest.TestCase):
    def setUp(self):
        # check() imports the entry scripts: keep the real tools' modules and sys.path as they were.
        saved_path, saved = list(sys.path), {n: sys.modules.get(n) for n in pm.ENTRIES}
        for name in pm.ENTRIES:
            sys.modules.pop(name, None)

        def restore():
            sys.path[:] = saved_path
            for name, module in saved.items():
                if module is None:
                    sys.modules.pop(name, None)
                else:
                    sys.modules[name] = module
        self.addCleanup(restore)

    def folder(self):
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        return pathlib.Path(d.name)

    def test_transitive_and_inside_functions(self):
        d = self.folder()
        write(d, 'run_game_vr.py', 'import os\nimport a\ndef f():\n    from b import x\n')
        write(d, 'xr_runtime.py', 'import json\n')
        write(d, 'a.py', 'import c\n')
        write(d, 'b.py', 'x = 1\n')
        write(d, 'c.py', 'import zlib\n')
        write(d, 'unused.py', 'import a\n')
        m = pm.manifest(d)
        self.assertEqual(m['tools'], ['a', 'b', 'c', 'run_game_vr', 'xr_runtime'])
        self.assertEqual(m['stdlib'], ['json', 'os', 'zlib'])

    def test_missing_tool_fails_the_check(self):
        d = self.folder()
        write(d, 'run_game_vr.py', 'import a\n')
        write(d, 'xr_runtime.py', 'import a\n')
        write(d, 'a.py', 'import gone_helper\n')
        problems, _ = pm.check(d, import_entries=False)
        self.assertTrue(any('gone_helper' in p for p in problems), problems)
        with contextlib.redirect_stdout(io.StringIO()) as out:
            self.assertEqual(pm.main(['check', '--tools', str(d), '--no-import']), 1)
        self.assertIn('MISSING', out.getvalue())

    def test_missing_entry_is_reported(self):
        d = self.folder()
        write(d, 'run_game_vr.py', 'import os\n')
        self.assertEqual(pm.manifest(d)['missing_tools'], ['xr_runtime'])
        problems, _ = pm.check(d, import_entries=False)
        self.assertTrue(any('xr_runtime' in p for p in problems))

    def test_guarded_import_is_optional(self):
        d = self.folder()
        write(d, 'run_game_vr.py', 'try:\n    import not_a_real_module_xyz\nexcept ImportError:\n    pass\n')
        write(d, 'xr_runtime.py', '')
        m = pm.manifest(d)
        self.assertEqual(m['optional'], ['not_a_real_module_xyz'])
        problems, notes = pm.check(d)
        self.assertEqual(problems, [])
        self.assertTrue(notes)

    def test_entries_are_imported(self):
        d = self.folder()
        write(d, 'run_game_vr.py', 'raise RuntimeError("boom")\n')
        write(d, 'xr_runtime.py', '')
        problems, _ = pm.check(d)
        self.assertTrue(any('boom' in p for p in problems), problems)

    def test_binaries_from_tools_and_launcher_script(self):
        d = self.folder()
        write(d, 'run_game_vr.py', 'MODULES = ("spidy_bridge.dll", "Spidy_Stereo_Probe.DLL")\n')
        write(d, 'xr_runtime.py', '')
        write(d, 'unused.py', 'X = "spidy_unused.dll"\n')
        write(d, 'launch-game-vr.ps1', "'spidy_headset_probe.exe','bhaptics_library.dll'\n")
        self.assertEqual(pm.manifest(d)['binaries'],
                         ['bhaptics_library.dll', 'spidy_bridge.dll', 'spidy_headset_probe.exe', 'spidy_stereo_probe.dll'])


class Scripts(unittest.TestCase):
    """The Windows scripts, read as text (they run on Windows only)."""

    def text(self, name):
        return (ROOT / name).read_text(encoding='utf-8')

    def test_package_uses_the_manifest_and_self_tests(self):
        s = self.text('tools/package.ps1')
        self.assertIn('package_manifest.py', s)
        self.assertIn('check --tools', s)
        self.assertIn("'SM2-VR-'", s)
        self.assertIn('-win64.zip', s)
        for item in ('Launch Spidy VR.cmd', 'PORT-VERSION', 'SHARING.md', 'Music', 'THIRD_PARTY_NOTICES.md'):
            self.assertIn(item, s)
        self.assertNotIn('Compress-Archive -', s)   # drops empty folders, backslashes in Windows PowerShell
        self.assertNotIn('spidy-linux.sh', s)
        self.assertNotIn('PLAYERS.md', s)

    def test_package_name_from_port_version(self):
        version = (ROOT / 'PORT-VERSION').read_text(encoding='utf-8').splitlines()[0].strip()
        name = 'SM2-VR-' + re.sub(r'[^A-Za-z0-9.]+', '-', version).strip('-')
        self.assertRegex(name, r'^SM2-VR-Update-\d+(\.\d+){0,2}$')  # 24.1: a fix release; 38.6.1: a hotfix

    def test_launcher_script_checks_each_python(self):
        s = self.text('tools/launch-game-vr.ps1')
        self.assertIn("-c 'import sys; print(sys.version_info[:2])'", s)
        self.assertIn("[version]'3.11'", s)
        self.assertIn('python.org', s)
        self.assertIn('Add python.exe to PATH', s)
        packaged = s.index("'python\\python.exe'")
        self.assertLess(packaged, s.index('Get-Command python.exe'))   # the package's own Python first
        self.assertNotIn('(Get-Command python.exe -ErrorAction Stop).Source', s)

    def test_launcher_script_runs_without_sources(self):
        s = self.text('tools/launch-game-vr.ps1')
        self.assertIn('Where-Object { Test-Path -LiteralPath $_ }', s)
        self.assertNotIn("Get-ChildItem -LiteralPath (Join-Path $projectRoot 'src')", s)

    def test_scripts_are_ascii(self):
        # Windows PowerShell 5.1 reads a file without a BOM in the ANSI code page.
        for name in ('tools/package.ps1', 'tools/launch-game-vr.ps1'):
            self.assertTrue(self.text(name).isascii(), name)

    def test_sharing_note(self):
        s = self.text('SHARING.md')
        for item in ('2.810.0.0', 'Steam', 'Windows 10', 'Music', 'F8', 'trigger', 'reports',
                     'Unzip, start your VR app, double-click Launch Spidy VR.cmd'):
            self.assertIn(item, s)
        # Update 24: nothing to install, one starter.
        self.assertNotIn('Visual C++ Redistributable', s)
        self.assertNotIn('Launch Spidy Game VR', s)

    def test_v072_game_body_probe_ships(self):
        # Version 0.0.72 GAME BODY: the no-headset probe, what it imports and a starter per check go in tools\.
        probe = pm.manifest(ROOT / 'tools', ('probe_game_body',))
        self.assertIn('probe_game_body', probe['tools'])
        self.assertEqual(probe['missing_tools'], [])
        self.assertTrue(set(probe['binaries']) <= set(pm.manifest(ROOT / 'tools')['binaries']))
        s = self.text('tools/package.ps1')
        for item in ("--entry probe_game_body", "@($probeManifest.tools)", "@($probeManifest.binaries)",
                     "tools\\probe_game_body.py') --help", "'Game Body Test 1 - checks.bat', 'Game Body Test 2 - body.bat'"):
            self.assertIn(item, s)
        for name, phases in (('Game Body Test 1 - checks.bat', '--phases find\r\n'),
                             ('Game Body Test 2 - body.bat', '--phases find,solve\r\n')):
            bat = (ROOT / 'tools' / name).read_bytes().decode('ascii')
            self.assertNotIn('\n', bat.replace('\r\n', ''))
            self.assertIn('"%~dp0..\\python\\python.exe" -B -X utf8 "%~dp0probe_game_body.py" ' + phases, bat)
            self.assertIn('pause', bat)

    def test_update24_unzip_and_play(self):
        s = self.text('tools/package.ps1')
        self.assertIn("foreach ($starter in @('Launch Spidy VR.cmd')) {", s)
        self.assertIn("'tools\\Spidy Launcher.exe'", s)
        for check in ('player_check.py\') runtime', "player_check.py') folder", "$starters.Count -ne 1",
                      'Unzip, start your VR app, double-click Launch Spidy VR.cmd', 'Parser]::ParseFile'):
            self.assertIn(check, s)
        ps1 = self.text('tools/launch-game-vr.ps1')
        self.assertIn('Unblock-File', ps1)
        self.assertIn('[switch]$GameWebs', ps1)
        self.assertIn("else { $captureArgs+=@('--overlay-webs')", ps1)
        self.assertIn('[int]$RenderScale=100', ps1)
        self.assertIn('[int]$Supersample=100', ps1)
        self.assertNotIn('vc_redist', ps1)   # the modules have the runtime built in; player_check checks old builds
        cmake = self.text('CMakeLists.txt')
        self.assertIn('set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")', cmake)
        for target in ('minhook', 'openxr_loader', 'spidy_launcher'):
            self.assertIn(f'set_property(TARGET {target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded', cmake)
        self.assertNotIn('MultiThreadedDLL', cmake)
        run = self.text('tools/run_game_vr.py')
        self.assertLess(run.index('player_check.before_session('), run.index('    keep_console(a.output)'))


if __name__ == '__main__':
    unittest.main()
