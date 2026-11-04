# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.77 INTERACT CARDS AND WAYPOINT (CANDIDATE).

live.txt waypoints (SpidyWaypoints 5580); include/spidy/story077d.hpp (the truck, the pipe, the offers),
include/spidy/story077g.hpp (tests/story077g_tests.cpp), src/waypoint_mark.cpp, world_trace's waypoint.
"""
import pathlib
import re
import subprocess
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402

NEW = ('include/spidy/story077g.hpp', 'tests/story077g_tests.cpp', 'tests/hero077_waypoint_tests.py',
       'include/spidy/waypoint_mark.hpp', 'src/waypoint_mark.cpp', 'include/spidy/story077d.hpp')


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Settings(unittest.TestCase):
    def test_waypoints_setting(self):
        rng, exp, text = L.SETTINGS['waypoints']
        self.assertEqual((rng, exp), ((0, 2), 'SpidyWaypoints'))
        self.assertEqual(L.CHOICE_NAMES['waypoints'], ('scan', 'always', 'off'))
        self.assertEqual([L.CHOICES['waypoints'][n] for n in ('scan', 'always', 'off')], [0, 1, 2])
        self.assertIn('waypoints', L.START_ONLY)
        self.assertIn('0 scan, the default', text)
        self.assertIn('live, no restart needed', text)


class Wiring(unittest.TestCase):
    def setUp(self):
        self.game = src('src/game_xr.cpp')

    def test_export(self):
        g = self.game
        self.assertRegex(g, r'SpidyWaypoints\(void\* v\) \{ return setLive\(v, 0, 2, 5580, waypointsNow, asU32\); \}')
        self.assertEqual(len(re.findall(r'\b5580\b', g)), 1)
        self.assertIn('std::atomic<uint32_t> waypointsNow{story077g::waypointsScan};', g)

    def test_waypoint_draw_is_ours(self):
        g = self.game
        at = g.index('HERO VR 0.0.77 WAYPOINT (story077g.hpp): the yellow diamond')
        body = g[at:g.index('// The T-pose calibration', at)]
        self.assertIn('!saved->flatScreen && waypointHere.shown', body)
        self.assertIn('waypoint_mark::diamond(translucent', body)
        self.assertIn('waypoint_mark::arrow(translucent', body)
        self.assertIn('translucent.resize(from);', body)
        for bad in ('Inset', 'latestScreen', 'setInset'):
            self.assertNotIn(bad, body)

    def test_update_and_gates(self):
        g = self.game
        self.assertIn('const bool waypointWanted = waypointMode != story077g::waypointsOff && vrGameplay;', g)
        self.assertIn('world_trace::waypoint(mark)', g)
        self.assertIn('story077g::scanAlpha(waypointMode', g)
        self.assertIn('HERO VR 0.0.77 WAYPOINT: scan %u - showing', g)
        self.assertIn('story077g::sourceOfType(tn)', src('src/world_trace.cpp'))
        self.assertIn('HERO VR 0.0.77 WAYPOINT (CANDIDATE) %s: frames', g)
        self.assertIn('HERO VR 0.0.77 INTERACT CARDS (CANDIDATE): pull card', g)
        # the pull / Y cards let the click / Y through a swing that owns him
        self.assertIn('reaching || (storyThrow && inFlight) || pullCardGate', g)
        self.assertIn('(reaching || inFlight || yCardGate) && (origins.play() & buttonY) != 0', g)
        self.assertIn('pullCardNear && steering && !calibrationInput', g)
        w = src('src/world_trace.cpp')
        self.assertIn('story077g::pick(marks.data()', w)
        self.assertIn('k->where == story077d::watchOnly || k->where == story077d::marker', w)

    def test_kinds(self):
        d = src('include/spidy/story077d.hpp')
        for cls in ('VehiclePushable', 'WaterPipe', 'EnviroInteractBase', 'HoldOffer', 'PickupCarryableOffer',
                    'InteractLocationOffer', 'InWorldInteractOffer', 'ProximityOffer', 'PointOfInterestPrius',
                    'POIGamePrius'):
            self.assertIn('".?AV%s@@"' % cls, d)
        self.assertIn('static_assert(kindCount <= 64)', d)
        cm = src('CMakeLists.txt')
        self.assertIn('src/waypoint_mark.cpp', cm)
        self.assertIn('add_test(NAME story077g COMMAND spidy_story077g_tests)', cm)

    def test_no_forbidden_words(self):
        for p in NEW:
            text = src(p).lower()
            self.assertNotIn('jim' + 'jam', text, p)
            self.assertNotRegex(text, r'\bmil' + r'es\w*', p)
        added = subprocess.run(['git', 'diff', 'b792f69', '--', 'src', 'include', 'tools', 'tests', 'CMakeLists.txt',
                                '*.md', 'apps'], cwd=ROOT, capture_output=True, text=True).stdout
        plus = '\n'.join(l for l in added.splitlines() if l.startswith('+')).lower()
        self.assertNotIn('jim' + 'jam', plus)
        self.assertNotRegex(plus, r'\bmil' + r'es\w*\b')


if __name__ == '__main__':
    unittest.main()
