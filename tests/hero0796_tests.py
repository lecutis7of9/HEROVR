"""79.6: GAME WALL ANIMS (the wrist toggle for the game's own wall crawl / run animations) and its WALL WATCH."""
import pathlib
import re
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402
import wall_anims_record as W  # noqa: E402


def src(path):
    return (ROOT / path).read_text(encoding='utf-8', errors='replace')


class Toggle(unittest.TestCase):
    def test_row_is_in_vr_settings_body_and_view(self):
        w = src('src/wrist_hud.cpp')
        self.assertIn('C::bodyAnimations, C::gameWallAnims, C::weblineMarker, C::combatCalibLook', w)  # the BODY AND VIEW section
        self.assertIn('return "GAME WALL ANIMS";', w)
        self.assertIn('r.gameWallAnims = true;', w)
        h = src('include/spidy/wrist_hud.hpp')
        self.assertIn('controls = 252', h)
        self.assertIn('Control::weblineMarker) == controls - 1', h)

    def test_off_is_the_default_and_persisted(self):
        self.assertIn('game_wall_anims', L.SETTINGS)
        self.assertEqual(L.SETTINGS['game_wall_anims'][0], (0, 1))
        self.assertEqual(L.SETTINGS['game_wall_anims'][1], 'SpidyGameWallAnims')
        self.assertIn('game_wall_anims', L.PERSISTED)
        self.assertEqual(L.CONTROL_KEYS[23], 'game_wall_anims')
        self.assertEqual(L.SELECTOR['game_wall_anims'], 10)
        self.assertEqual(L.decode_hud_settings(10, L.MARKER | 1 << 5)['game_wall_anims'], 'on')
        self.assertEqual(L.decode_hud_settings(10, L.MARKER)['game_wall_anims'], 'off')
        g = src('src/game_xr.cpp')
        self.assertIn('std::atomic<bool> gameWallAnimsWanted{false};', g)
        self.assertIn('WINAPI SpidyGameWallAnims(void* v) { return setLive(v, 0, 1, 5660, gameWallAnimsWanted, asBool); }', g)
        self.assertEqual(len(re.findall(r'\b5660\b', g)), 1)
        self.assertIn('wristChanged |= 1u << 23;', g)
        self.assertIn('1u << 22 | 1u << 23', g)
        self.assertIn('(gameWallAnimsWanted.load() ? 1u : 0u) << 5', g)

    def test_on_applies_the_three_live_settings_and_off_restores(self):
        g = src('src/game_xr.cpp')
        self.assertRegex(g, r'gameWallCrawl = true;[^}]*climbOn = true;[^}]*handClimbAsked = 1;[^}]*wallViewNow = 0;')
        self.assertRegex(g, r'gameWallCrawl = false;[^}]*climbOn = wallPriorClimb;[^}]*wallViewNow = wallPriorView;')
        self.assertIn('swingState.wall == 1', g)  # ON waits until HEROVR's own wall lets go
        self.assertIn('wall_anims::deferred', g)

    def test_safety_and_diagnostics_are_wired(self):
        g = src('src/game_xr.cpp')
        self.assertIn('win.yawJumpDegrees = motion.active ? motion.yawJumpDegrees : 0.f;', g)
        self.assertIn('wallWatch.update(', g)
        self.assertIn('wallAct.autoOff', g)
        self.assertIn('SpidyWallAnimsData = wallWatch.data();', g)
        self.assertIn('wall_anims::summary(', g)
        h = src('include/spidy/wall_anims.hpp')
        self.assertIn('79.7 WALL', h)
        self.assertIn('yawJumpDegrees = 60.f', h)
        self.assertIn('autoOffDrops = 3', h)
        r = src('tools/run_game_vr.py')
        self.assertIn('wall_anims=wall_anims_last', r)
        self.assertIn('wall_anims_record.wall_anims_line(', r)
        self.assertIn('wall_anims_record.EXPORT', r)


class Record(unittest.TestCase):
    def build(self, flags=3, events=()):
        head = struct.pack('<4I', W.MAGIC, 1, W.SIZE, flags)
        head += struct.pack('<6I', 0, 0, 0, 2, 1, 0)
        head += struct.pack('<8I', 1, 2, 1, 0, 1, 1, 3, 0)
        head += struct.pack('<6f', 0, 1.5, 0, 3.0, 12.25, 0)
        head += struct.pack('<2f', 91.0, 7.5)
        head += struct.pack('<4I', 9, len(events), 42500, 0)
        for ev in events:
            head += W.EVENT.pack(*ev)
        return head.ljust(W.SIZE, b'\0')

    def test_parse(self):
        raw = self.build(events=[(1500, 0, 0, 4, 0, 6.0, 16.0), (9000, 3, 4, 1, 0, -8.0, -3.0)])
        self.assertEqual(len(raw), 512)
        r = W.wall_anims_parse(raw)
        self.assertEqual(r['game_wall_anims'], 'on')
        self.assertEqual(r['contacts'], {'own wall': 2, 'game wall': 1, 'floor crawl': 0})
        self.assertEqual((r['drops'], r['drops_off_game_wall'], r['view_flips'], r['hand_overs']), (2, 1, 1, 1))
        self.assertEqual(r['seconds']['game wall'], 12.2)
        self.assertEqual(r['events'][0]['kind'], 'contact')
        self.assertEqual(r['events'][1]['kind'], 'drop')
        self.assertFalse(r['hero_state_read'])
        line = W.wall_anims_line(r)
        self.assertTrue(line.startswith('79.7 WALL'))
        self.assertIn('the game\'s hero state is not read', line)

    def test_bad_records(self):
        self.assertIsNone(W.wall_anims_parse(None))
        self.assertIsNone(W.wall_anims_parse(b'\0' * 512))
        self.assertIsNone(W.wall_anims_parse(self.build()[:100]))
        self.assertIsNone(W.wall_anims_line(None))


class Version(unittest.TestCase):
    def test_tag_is_79_9(self):
        c = src('CMakeLists.txt')
        self.assertIn('set(HERO_TEST_VERSION "0.0.79.10"', c)
        self.assertIn('set(HERO_TEST_NUMBER "10"', c)
        self.assertIn('HERO VR 79.10 TEST (commit', src('.github/workflows/dynamic-body-0791.yml'))
        self.assertIn('HERO VR 79.9 TEST (includes 79.8)', src('src/game_xr.cpp'))
        self.assertIn('wall_anims_tests', src('.github/workflows/dynamic-body-0791.yml'))


if __name__ == '__main__':
    unittest.main()
