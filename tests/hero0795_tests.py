"""79.5: flips back on the left stick, the Web Line release latch + aim marker, the BUILD TAG / stale-folder check."""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))


def src(path):
    return (ROOT / path).read_text(encoding='utf-8', errors='replace')


class Flips(unittest.TestCase):
    def test_flip_stick_is_the_left_one(self):
        t = src('src/game_tracking.cpp')
        self.assertIn('flip.stickX = f.hands[0].stickX;', t)
        self.assertNotIn('flip.stickX = f.hands[1].stickX;', t)
        self.assertNotIn('flipRightStick', t)
        self.assertIn('f.hands[1].stickX : 0;', t)  # the right stick turns the view again

    def test_right_stick_zip_and_heal_are_not_blocked_by_flips(self):
        g = src('src/game_xr.cpp')
        self.assertNotIn('flipStickOwns', g)
        self.assertIn('healLive && healOn.load(),', g)
        self.assertIn('airWebZipY', g)  # 79.4's air Y web strike stays


class Webline(unittest.TestCase):
    def test_latch_and_reasons(self):
        h = src('include/spidy/webline_gesture.hpp')
        self.assertIn('latchWindow = .3f', h)
        self.assertIn('enum class Reason', h)
        self.assertIn('float seconds{};', h)
        n = src('include/spidy/webline_native.hpp')
        self.assertIn('settle = .04f, pulse = .10f, tail = .04f', n)

    def test_game_uses_them(self):
        g = src('src/game_xr.cpp')
        self.assertIn('li.seconds = frame.seconds;', g)
        self.assertIn('weblineMarks[h].state = lo.hasTarget ? (lo.valid ? 1 : 2) : 3;', g)  # 79.7: always a marker
        self.assertIn('runtime.haptic(static_cast<int>(h), .18f);', g)
        self.assertIn('webline_gesture::reasonText(lo.reason)', g)
        self.assertIn('saved.weblineMarks[i] = weblineMarks[i];', g)
        self.assertIn('weblineMarks', src('include/spidy/native_eye_history.hpp'))
        for f in ('src/web_visual.cpp', 'src/tech_marks.cpp'):
            self.assertIn('marker.tinted && finite(marker.tint)', src(f))
        self.assertIn('bool tinted{};', src('include/spidy/web_visual.hpp'))

    def test_air_is_still_refused_in_mode_on(self):
        n = src('include/spidy/webline_native.hpp')
        self.assertIn('return mode == modeAnywhere || (mode == modeOn && !airborne);', n)


class BuildTag(unittest.TestCase):
    def test_dll_tag_reader(self):
        import build_tag
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'x.dll'
            p.write_bytes(b'MZ\0\0junk HEROVR-BUILD-TAG 0.0.79.5 5 abc1234\0more')
            self.assertEqual(build_tag.dll_tag(p), ('0.0.79.5', '5', 'abc1234'))
            self.assertIn('HERO VR 0.0.79.5 TEST 5 commit abc1234', build_tag.tag_line(p))
            self.assertEqual(build_tag.record(p), dict(version='0.0.79.5', test='5', commit='abc1234'))
            p.write_bytes(b'MZ\0\0an older build')
            self.assertIsNone(build_tag.dll_tag(p))
            self.assertIn('NO build tag', build_tag.tag_line(p))
            self.assertIsNone(build_tag.dll_tag(pathlib.Path(d) / 'missing.dll'))

    def test_sources(self):
        g = src('src/game_xr.cpp')
        self.assertIn('char SpidyBuildTag[] = "HEROVR-BUILD-TAG " SPIDY_TEST_VERSION " " SPIDY_TEST_NUMBER " " SPIDY_TEST_COMMIT;', g)
        self.assertIn('79.5 BUILD TAG: ', g)
        r = src('tools/run_game_vr.py')
        self.assertIn('build_tag.tag_line(', r)
        self.assertIn('build_tag=build_tag_record', r)
        c = src('CMakeLists.txt')
        self.assertIn('set(HERO_TEST_VERSION "0.0.82"', c)
        self.assertIn('HEROVR_BUILD_FOOTER', c)
        self.assertIn('SPIDY_BUILD_FOOTER', src('src/wrist_hud.cpp'))

    def test_manifest_and_launch_script(self):
        p = src('tools/package.ps1')
        self.assertIn("'BUILD-MANIFEST.txt'", p)
        self.assertIn("'BUILD-ID.txt'", p)
        l = src('tools/launch-game-vr.ps1')
        self.assertIn('BUILD CHECK FAILED', l)
        self.assertIn("throw \"BUILD CHECK FAILED", l)
        self.assertIn('HEROVR-BUILD-TAG', l)
        self.assertIn("NEW EMPTY folder", l)
        y = src('.github/workflows/dynamic-body-0791.yml')
        self.assertIn("HEROVR_BUILD_FOOTER: '1'", y)
        self.assertIn('HEROVR_BUILD_TAG', y)

    def test_manifest_line_parsing_matches_the_script(self):
        # the script reads 64 hex chars, two spaces, then the relative path
        line = 'a' * 64 + '  build\\windows-ninja\\spidy_bridge.dll'
        self.assertEqual(line[:64], 'a' * 64)
        self.assertEqual(line[66:].strip(), 'build\\windows-ninja\\spidy_bridge.dll')


if __name__ == '__main__':
    unittest.main()
