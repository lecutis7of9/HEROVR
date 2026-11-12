"""79.7: GAME WALL ANIMS stick / yaw fix, wall frame, hand-climb reach, bold Web Line marker, slow-walk jitter."""
import pathlib
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import hero797_record as R  # noqa: E402


def src(path):
    return (ROOT / path).read_text(encoding='utf-8', errors='replace')


class WallAnims(unittest.TestCase):
    def test_level_view_never_turns_the_view_and_the_stick_climbs(self):
        t = src('src/game_tracking.cpp')
        self.assertIn('if (!wallLevel_ && length(level) > .5f && length(tracked) > .5f) {', t)
        self.assertIn('if (wallLevel_ && std::abs(held.y) < .7f && handValid(f.hands[0])) {', t)
        self.assertIn('ahead = wallUp;', t)

    def test_yaw_jump_safety_replaces_the_tilt_detector(self):
        t = src('src/game_tracking.cpp')
        self.assertIn('std::abs(delta) > yawJumpLimit', t)
        self.assertIn('rig_.turn(-delta, f.head.position);', t)
        self.assertIn('yawJumpLimit = 1.0471976f', src('include/spidy/game_tracking.hpp'))  # 60 degrees
        w = src('include/spidy/wall_anims.hpp')
        self.assertNotIn('tiltDegrees', w)
        self.assertIn('yawJumpDegrees', w)

    def test_stand_off_and_wall_frame(self):
        t = src('src/game_tracking.cpp')
        self.assertIn('surface.crawl && wallLevel_', t)
        self.assertIn('gameCrawlStandOff = .3f', src('include/spidy/game_tracking.hpp'))
        self.assertIn('out.crawlNormal = held;', t)
        g = src('src/game_xr.cpp')
        self.assertIn('pose.reserved = azimuth | (headCm << 8);', g)
        self.assertIn('t.crawlHipDistance', src('src/native_body.cpp'))
        self.assertIn('if (gameWallCrawl && w > 0.f && length(t.crawlNormal) > .5f', src('src/body_ik.cpp'))

    def test_hand_climb_reach(self):
        h = src('include/spidy/hand_crawl.hpp')
        self.assertIn('wallAnimsReach = .7f', h)
        self.assertIn('> c.handReach)', h)
        self.assertIn('cc.handReach = wallAnimsOn ? hand_crawl::Config::wallAnimsReach : hand_crawl::Config::reach;', src('src/game_xr.cpp'))


class Marker(unittest.TestCase):
    def test_bold_marker_is_drawn_whatever_reticles_say(self):
        g = src('src/game_xr.cpp')
        self.assertIn('webline_marker::minPixels', g)
        self.assertIn('appendWeblineMarker(translucent, line.point', g)
        self.assertIn('appendWeblinePreview(translucent, line.from, line.point', g)
        self.assertIn('weblineMarks[h].state = lo.hasTarget ? (lo.valid ? 1 : 2) : 3;', g)
        self.assertIn('minPixels = 40.f', src('include/spidy/webline_marker.hpp'))
        self.assertIn('std::max(minPixels, 40.f) * .5f * px', src('src/web_visual.cpp'))

    def test_counters_parse(self):
        raw = struct.pack('<4I11Q', R.WEBLINE_MAGIC, 1, R.WEBLINE_SIZE, 0, 5, 100, 80, 98, 79, 5, 3, 120, 40, 20, 150)
        r = R.webline_marker_parse(raw)
        self.assertEqual((r['gestures'], r['first_frames'], r['first_frame_valid']), (5, 5, 3))
        self.assertEqual(r['marker_drawn_frames'], {'left': 98, 'right': 79})
        self.assertEqual(r['drawn_of_set_percent'], 98.3)
        self.assertIn('first frame had a valid second target 3 of 5', R.webline_marker_line(r))
        self.assertIsNone(R.webline_marker_parse(raw[:50]))
        self.assertIsNone(R.webline_marker_parse(b'\0' * 104))


class Jitter(unittest.TestCase):
    def test_slow_walk_is_wired_for_low_med_full(self):
        s = src('src/stereo_probe.cpp')
        self.assertIn('spidy::jitter_slow::walk(frameIndex, spidy::jitter_check::maxStoredPixels(level), x, y);', s)
        self.assertIn('level >= spidy::jitter_check::low && level <= spidy::jitter_check::full', s)
        self.assertIn('steps = 32', src('include/spidy/jitter_slow.hpp'))
        self.assertIn('++level79.lrCompared;', s)

    def test_level_counters_parse(self):
        level = struct.pack('<2Q8Q2Q2Q2f', 100, 90, 10, 60, 20, 10, 5, 50, 30, 5, 2, 3, 100, 40, 0.5, 8.0)
        raw = struct.pack('<4IQ', R.JITTER_MAGIC, 1, R.JITTER_SIZE, 2, 1000) + b'\0' * 120 + b'\0' * 120 + level + b'\0' * 240
        self.assertEqual(len(raw), R.JITTER_SIZE)
        r = R.jitter_levels_parse(raw)
        self.assertEqual(r['level'], 'med')
        e = r['levels']['med']
        self.assertEqual(e['lag_percent']['left'], [10.0, 60.0, 20.0, 10.0])
        self.assertEqual(e['lr_differs_percent'], 40.0)
        self.assertEqual(e['lr_diff_max'], 0.5)
        self.assertNotIn('off', r['levels'])
        self.assertIn('eyes differ 40.0%', R.jitter_levels_line(r))
        self.assertIsNone(R.jitter_levels_parse(raw[:100]))


if __name__ == '__main__':
    unittest.main()
