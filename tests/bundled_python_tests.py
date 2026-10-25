"""Update 26: the shareable package always runs its own Python 3.12.10, never the PC's, and says so plainly."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = (ROOT / 'tools' / 'launch-game-vr.ps1').read_text(encoding='utf-8')


def python_block():
    start = SCRIPT.index('function Test-SpidyPython')
    return SCRIPT[start:SCRIPT.index("# Update 24: Spidy's modules have Microsoft's C++ runtime built in")]


class BundledPython(unittest.TestCase):
    def test_package_never_searches_the_pc(self):
        block = python_block()
        packaged = block[block.index('if($packaged) {'):block.index('} elseif($Python) {')]
        for search in ('Get-Command', 'py.exe', 'LOCALAPPDATA', 'candidates'):
            self.assertNotIn(search, packaged)
        self.assertIn('$Python=$bundledPython', packaged)
        self.assertIn("This package's own Python is missing", packaged)
        self.assertIn('installing or updating Python will not help', packaged)
        # The bundled one is no longer a candidate of the development search (no silent fall-through).
        search = block[block.index('} else {', block.index('} elseif($Python) {')):]
        self.assertNotIn("'python\\python.exe'", search)

    def test_package_ps1_pins_and_checks(self):
        text = (ROOT / 'tools' / 'package.ps1').read_text(encoding='utf-8')
        self.assertIn("$pythonVersion = '3.12.10'", text)
        self.assertIn('$stagedVersion -ne $pythonVersion', text)
        self.assertIn("This package's own Python is missing", text)

    def test_launcher_uses_only_the_bundled_one_in_a_package(self):
        system = (ROOT / 'apps' / 'launcher' / 'system.cpp').read_text(encoding='utf-8')
        self.assertIn('findPython(scan.root, scan.packaged)', system)
        ui = (ROOT / 'apps' / 'launcher' / 'ui.cpp').read_text(encoding='utf-8')
        self.assertIn('scan_.packaged ? spidy::launcher::kBundledPythonHelp', ui)

    @unittest.skipUnless(shutil.which('pwsh'), 'PowerShell 7 is not installed')
    def test_runs(self):
        cases = {'good': '#!/bin/sh\nexec python3 "$@"\n', 'missing': None,
                 'broken': '#!/bin/sh\necho "Fatal Python error" >&2; exit 1\n', 'old': '#!/bin/sh\necho "(3, 9)"\n'}
        for name, body in cases.items():
            with tempfile.TemporaryDirectory() as d:
                root = pathlib.Path(d) / 'pkg dir'
                (root / 'python').mkdir(parents=True)
                exe = root / 'python' / 'python.exe'
                if body:
                    exe.write_text(body)
                    exe.chmod(0o755)
                script = root / 't.ps1'
                script.write_text(f"$ErrorActionPreference='Stop'\n$projectRoot='{root}'\n$Python='/usr/bin/python3'\n"
                                  + python_block() + '\n"CHOSEN=$Python"\n')
                run = subprocess.run(['pwsh', '-NoProfile', '-File', str(script)], capture_output=True, text=True)
                said = ' '.join((run.stdout + run.stderr).split())
                if name == 'good':
                    self.assertEqual(run.returncode, 0, said)
                    self.assertIn(f'CHOSEN={exe}', said)
                    self.assertIn('-Python /usr/bin/python3 is ignored', said)
                else:
                    self.assertNotEqual(run.returncode, 0, name)
                    self.assertIn("This package's own Python", said, name)


if __name__ == '__main__':
    unittest.main()
