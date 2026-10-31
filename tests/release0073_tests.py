"""HERO VR 0.0.73 release: the MP SOLO TEST is not in the public build. (0.0.73's GAME TAA / image_style and its
GAME AA TAA are gone in 0.0.74 test 10, 0.0.72's picture: GameTaaGone.)"""
import os, pathlib, sys, tempfile, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import live_settings as L  # noqa: E402


def src(p):
    return open(os.path.join(ROOT, p), encoding='utf-8').read()


# 0.0.74 test 1 is a private test build on top of the 0.0.73 release: HERO_TEST_BUILD ON and MP SOLO TEST back
# (OFF by default). The public-release-only gates below are skipped there; Hero074Test1 checks the test build.
import re as _re  # noqa: E402
TEST_BUILD = bool(_re.search(r'option\(HERO_TEST_BUILD "[^"]*" ON\)', src('CMakeLists.txt')))


class GameTaaGone(unittest.TestCase):
    """0.0.74 test 10 (0.0.72's picture): GAME TAA (image_style), GAME AA TAA and KEEP MY GAME AA are gone, not
    only off; an old image_style line is skipped quietly and taken out once (migrate_074t10_72)."""
    def test_gone(self):
        for name in ('image_style', 'image'):
            self.assertNotIn(name, L.SETTINGS)
            self.assertNotIn(name, L.UPDATE38_DEFAULTS)
            self.assertIn(name, L.RETIRED_073)
        for gone in ('image_style_of', 'image_style_in_file', 'game_aa_taa', 'migrate_0073', 'MIGRATED_0073'):
            self.assertFalse(hasattr(L, gone), gone)
        self.assertFalse(os.path.exists(os.path.join(ROOT, 'include/spidy/image_style.hpp')))
        self.assertNotIn('imageStyle', src('src/stereo_probe.cpp'))
        self.assertNotIn('combat38::imageStyle', src('src/game_xr.cpp'))
        r = src('tools/run_game_vr.py')
        self.assertNotIn('image_style', r)
        self.assertNotIn('keep_game_aa', r)

    def test_old_line_skipped_quietly(self):
        values, warnings = L.parse('image_style = spidyvr\nhistory = 80\n')
        self.assertEqual(warnings, [])
        self.assertNotIn('image_style', values)
        self.assertEqual(values['history'], 80)


@unittest.skip("0.0.73 only: 0.0.74 ships multiplayer (the user's choice); Hero074Test1 checks it is there, OFF by default")
class NoMultiplayerTest(unittest.TestCase):
    def test_mp_solo_test_left_out(self):
        self.assertNotIn('mp_solo_test', L.SETTINGS)
        self.assertNotIn('mp_solo_test', L.UPDATE38_DEFAULTS)
        self.assertNotIn('mp_solo_test', L.UPDATE38_KEYS)
        for f in ('include/spidy/combat38.hpp', 'include/spidy/wrist_hud.hpp', 'src/wrist_hud.cpp', 'src/game_xr.cpp',
                  'CMakeLists.txt', 'tools/update37_records.py'):
            s = src(f)
            for bad in ('mpSoloTest', 'MpSoloTest', 'mp_solo_test', 'mp_session', 'mp_ghost'):
                self.assertNotIn(bad, s, (f, bad))
        for f in ('src/mp_session.cpp', 'src/mp_net.cpp', 'include/spidy/mp_session.hpp', 'tools/mp_test_buddy.py',
                  'docs/MP-SOLO-TEST.md'):
            self.assertFalse(os.path.exists(os.path.join(ROOT, f)), f)


@unittest.skip("0.0.73 only: release0074_tests.ReleaseLabels0074 checks the 0.0.74 release")
class ReleaseLabels(unittest.TestCase):
    """The public 0.0.73 release: no test labels, HERO VR 0.0.73 on the wrist and the console."""
    def test_version_and_labels(self):
        import re
        self.assertEqual(src('VERSION').strip(), '0.0.73')
        self.assertRegex(src('CMakeLists.txt'), r'project\(Spidy VERSION 0\.0\.73 ')
        self.assertRegex(src('CMakeLists.txt'), r'option\(HERO_TEST_BUILD "[^"]*" OFF\)')
        self.assertIn('message(1, 0, "HERO VR " SPIDY_PUBLIC_VERSION " - Made by Agent Mulder: starting OpenXR");', src('src/game_xr.cpp'))
        self.assertNotIn('"SM2 VR " SPIDY_PUBLIC_VERSION', src('src/game_xr.cpp'))
        self.assertIn('f.text = "HERO VR " SPIDY_PUBLIC_VERSION " - MADE BY AGENT MULDER";', src('src/wrist_hud.cpp'))
        for fn in ('include/spidy/combat38.hpp', 'src/holo_roster.cpp', 'src/roster_page.cpp'):
            for line in src(fn).splitlines():
                code = line.split('//')[0]
                self.assertIsNone(re.search(r'"[^"]*\b(?:TEST|test) [0-9][^"]*"', code), (fn, line))
                self.assertNotIn('this test build', code, fn)
        self.assertTrue(os.path.exists(os.path.join(ROOT, 'RELEASE-NOTES-v0.0.73.md')))



class Hero074Test1(unittest.TestCase):
    """0.0.74 test 1: MP SOLO TEST back, OFF by default (the 0.0.74 release keeps it); the test number stays set."""
    def test_mp_solo_test_off_by_default(self):
        self.assertEqual(L.UPDATE38_DEFAULTS['mp_solo_test'], 'off')
        self.assertEqual(L.export_value('mp_solo_test', 'off'), 0)
        self.assertEqual(L.export_value('mp_solo_test', 'on'), 1)
        self.assertIn('2, {"OFF", "ON", nullptr, nullptr}}', src('include/spidy/combat38.hpp'))
        self.assertIn('c38(combat38::mpSoloTest) == 1', src('src/game_xr.cpp'))

    def test_labels(self):
        c = src('CMakeLists.txt')
        self.assertIn('set(HERO_TEST_NUMBER "10" CACHE', c)  # 0.0.74 test 10: the notice says TEST 10
        self.assertIn('set(HERO_TEST_VERSION "0.0.74" CACHE', c)


if __name__ == '__main__':
    unittest.main()
