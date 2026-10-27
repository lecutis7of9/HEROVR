"""HERO VR 0.0.72 test build 5: the build 4 report's root causes stay fixed (source gates).

Build 4 (the user, 8:33/8:35): no body, no test notice, flat guns, a crash on THOR, DAREDEVIL's charged
jump a plain jump, and a console that said 0.0.70 (run from a backup copy of the mod)."""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools' / 'release072'))
import required_assets as ra  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Body(unittest.TestCase):
    def test_start_at_does_not_shadow_the_detours(self):
        # build 4: startAt's parameters were named writer/conversion, so MH_CreateHook got the game's own
        # address as the detour and MinHook refused it (no body)
        s = src('src/native_body.cpp')
        m = re.search(r'startAt\(([^)]*)\)', s)
        self.assertIsNotNone(m)
        self.assertNotRegex(m.group(1), r'\bwriter\b')
        self.assertIn('reinterpret_cast<void*>(&writer)', s)


class Notice(unittest.TestCase):
    def test_the_notice_runs_on_a_clock_not_on_dt(self):
        s = src('src/game_xr.cpp')
        self.assertIn('testNoticeClock', s)
        self.assertIn('testNoticeClock < 45', s)
        self.assertIn('consoleLine("TEST NOTICE: shown (HERO VR 0.0.72 test " SPIDY_TEST_COMMIT ")")', s)

    def test_the_commit_is_on_the_console_and_the_wrist(self):
        self.assertIn('"HERO VR 0.0.72 test " SPIDY_TEST_COMMIT " - Made by Agent Mulder', src('src/game_xr.cpp'))
        self.assertIn('"HERO VR 0.0.72 test " SPIDY_TEST_COMMIT', src('src/wrist_hud.cpp'))
        self.assertIn('SPIDY_TEST_COMMIT=', src('CMakeLists.txt'))


class Models(unittest.TestCase):
    def test_textures_fit_the_free_memory(self):
        s = src('src/game_xr.cpp')
        self.assertIn('ullAvailPageFile', s)
        self.assertRegex(s, r'512u?\s*:\s*256u?|\?\s*512u?\s*:\s*256u?')
        self.assertIn('MODELS: TEXTURED', s)
        self.assertIn('MODELS: UNTEXTURED', s)

    def test_a_failed_vertex_buffer_is_not_mapped_next_frame(self):
        s = src('src/d3d12_renderer.cpp')
        i = s.index('capacity_ = wanted;')
        self.assertLess(s.rindex('CreateCommittedResource', 0, i), i)


class Thor(unittest.TestCase):
    def test_the_look_never_takes_play_down(self):
        s = src('src/game_xr.cpp')
        self.assertIn('THOR / 0.0.72 look: not drawn this frame', s)


class DaredevilJump(unittest.TestCase):
    def test_no_baton_cast_while_jumping(self):
        h = src('include/spidy/hero072.hpp')
        self.assertIn('since[i] = jumpBusy ? 0.f', h)
        self.assertIn('cast.step(dt, head, p, v, tracked, in.jumpBusy)', src('include/spidy/hero072_play.hpp'))
        x = src('src/game_xr.cpp')
        self.assertIn('in72.jumpBusy = jumpBusy72', x)
        self.assertIn('chargeJump.charging() || leap.leap > 0', x)

    def test_no_grapple_over_a_takeoff(self):
        s = src('src/game_swing.cpp')
        self.assertIn('const bool takingOff = takeoffTransition.phase() != SwingTakeoff::Idle && !grappleLift;', s)
        self.assertIn('&& !takingOff) {', s)


class Install(unittest.TestCase):
    def test_the_installer_ships(self):
        need = ra.required_zip_files()
        for f in ('INSTALL-TEST-BUILD.bat', 'INSTALL-TEST-BUILD.ps1', 'payload/HERO-VR-TEST-BUILD.txt'):
            self.assertIn(f, need)
        bat = (ROOT / 'tools/release072/INSTALL-TEST-BUILD.bat').read_text()
        self.assertIn('INSTALL-TEST-BUILD.ps1', bat)
        ps = (ROOT / 'tools/release072/INSTALL-TEST-BUILD.ps1').read_text()
        for word in ('launch-game-vr.ps1', 'SHA256SUMS.txt', 'Get-FileHash', 'BACKUP', '_before-0.0.72-test',
                     'SM2VRLauncher.exe'):
            self.assertIn(word, ps)

    def test_the_console_names_the_build_and_the_folder(self):
        r = src('tools/run_game_vr.py')
        self.assertIn("TEST_MARKER = 'HERO-VR-TEST-BUILD.txt'", r)
        self.assertIn('MIXED INSTALL', r)
        self.assertLess(r.index('for line in test_build_lines():'), r.index("print(f'SM2 VR {version_line()} - {MADE_BY}'"))


if __name__ == '__main__':
    unittest.main()
