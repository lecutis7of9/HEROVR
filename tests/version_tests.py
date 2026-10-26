"""Update 38.5: the public version (semver) - VERSION, CMakeLists.txt's project line, version.hpp's
fallback and the release scripts agree; PORT-VERSION keeps the internal build. docs/VERSIONING.md."""
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))


def hpp_text():
    return (ROOT / 'include/spidy/version.hpp').read_text(encoding='utf-8')


class PublicVersion(unittest.TestCase):
    def test_one_version(self):
        public = (ROOT / 'VERSION').read_text(encoding='utf-8')
        self.assertRegex(public, r'^\d+\.\d+\.\d+\n$')
        cmake = re.search(r'project\(Spidy VERSION (\d+\.\d+\.\d+)', (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8'))
        self.assertEqual(cmake.group(1), public.strip())
        hpp = (ROOT / 'include/spidy/version.hpp').read_text(encoding='utf-8')
        # Real builds take the number from CMake (the fallback only serves the strict compile checks).
        self.assertIn('add_compile_definitions(SPIDY_PUBLIC_VERSION="${PROJECT_VERSION}")',
                      (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8'))
        self.assertIn('number = SPIDY_PUBLIC_VERSION', hpp)
        build = re.search(r'build = "([\d.]+)"', hpp).group(1)
        self.assertEqual((ROOT / 'PORT-VERSION').read_text(encoding='utf-8').strip(), 'Update ' + build)

    def test_first_public_release(self):
        self.assertEqual((ROOT / 'VERSION').read_text(encoding='utf-8').strip(), '0.0.6')
        self.assertEqual((ROOT / 'PORT-VERSION').read_text(encoding='utf-8').strip(), 'Update 39.1')

    def test_release_scripts_bump_version_too(self):
        ps = (ROOT / 'tools/release.ps1').read_text(encoding='utf-8')
        yml = (ROOT / '.github/workflows/release.yml').read_text(encoding='utf-8')
        self.assertIn("-- CMakeLists.txt VERSION", ps)
        self.assertIn('"$PWD/VERSION"', yml)
        self.assertIn('-- CMakeLists.txt VERSION', yml)

    def test_shown_everywhere(self):
        # run_game_vr needs Windows to import: its two functions are run on their own here.
        src = (ROOT / 'tools/run_game_vr.py').read_text(encoding='utf-8')
        scope = {'ROOT': ROOT}
        for name in ('public_version', 'version_line', 'port_version'):
            m = re.search(r'^def %s\(\):\n(?:    .*\n|\n)+' % name, src, re.M)
            exec(m.group(0), scope)
        self.assertEqual(scope['public_version'](), '0.0.6')
        self.assertEqual(scope['version_line'](), 'Version 0.0.6 (build Update 39.1)')
        self.assertIn("version=public_version(), port_version=port_version()", src)
        finder = (ROOT / 'tools/address_finder.py').read_text(encoding='utf-8')
        self.assertIn("'version': public_version()", finder)
        self.assertIn("SM2 VR {version_line()}", (ROOT / 'tools/run_game_vr.py').read_text(encoding='utf-8'))
        self.assertIn('p.text(spidy::version::statusLine,', (ROOT / 'src/wrist_hud.cpp').read_text(encoding='utf-8'))
        self.assertIn('statusLine = "VERSION " SPIDY_PUBLIC_VERSION', hpp_text())
        for p in ('tools/launch-game-vr.ps1', 'tools/build.ps1', 'tools/collect-reports.ps1'):
            self.assertIn('SM2 VR Version', (ROOT / p).read_text(encoding='utf-8'), p)

    def test_semver_order(self):
        key = lambda v: tuple(int(x) for x in v.split('.'))
        self.assertTrue(key('0.0.2') > key('0.0.1') and key('0.1.0') > key('0.0.9') and key('0.0.10') > key('0.0.9'))


if __name__ == '__main__':
    unittest.main()
