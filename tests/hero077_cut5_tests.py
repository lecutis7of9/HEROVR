# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.77 CUT5: story sprint off and grounded, cine prompts, waypoints off, the right-wrist minimap."""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Cut5(unittest.TestCase):
    def test_story_sprint_off_and_grounded(self):
        g = src('src/game_xr.cpp')
        self.assertIn('std::atomic<uint32_t> storySprintNow{1};', g)
        self.assertIn('swingState.grounded != 0', g)
        self.assertIn('CUT5 STORY SPRINT', g)

    def test_waypoints_off(self):
        self.assertIn('waypointsNow{story077g::waypointsOff}', src('src/game_xr.cpp'))
        self.assertIn('waypoints = scan', live_settings.SETTINGS['waypoints'][2] + src('apps/launcher/ui.cpp'))

    def test_cine_prompt(self):
        g = src('src/game_xr.cpp')
        self.assertIn('CUT5 CINE PROMPT Y #', g)
        self.assertIn('cineCardSeconds', src('include/spidy/story077d.hpp'))

    def test_wrist_map_wiring(self):
        g = src('src/game_xr.cpp')
        for name, code in (('SpidyWristMap', 5581), ('SpidyWristMapSize', 5582), ('SpidyWristMapAt', 5583),
                           ('SpidyWristMapHud', 5584)):
            self.assertIn('WINAPI %s(void* v)' % name, g)
            self.assertEqual(len(re.findall(r'\b%d\b' % code, g)), 1, code)
        for key in ('wrist_map', 'wrist_map_size', 'wrist_map_at', 'wrist_map_hud'):
            self.assertIn(key, live_settings.SETTINGS)
            self.assertIn('live, no restart needed', live_settings.SETTINGS[key][2])
            self.assertIn(key, live_settings.START_ONLY)
        self.assertEqual(live_settings.CHOICE_NAMES['wrist_map'], ('on', 'off'))
        # The gesture takes the left trigger from its first touch, before the hand's trigger masks.
        at = g.index('story077h::Gesture::Input mapIn')
        self.assertGreater(g.index('if (hudTriggers & (1u << i))', at), at)
        self.assertIn('hudTriggers |= 1u', g[at:at + 6000])
        # The screen copy only while it is shown; the crop from the game's presented frame (no readback).
        self.assertIn('(wristMap.shown() && wristMapNow.load() == story077h::mapOn)', g)
        self.assertIn('story077h::hudCrop(mapImage.width, mapImage.height', g)
        self.assertIn('HERO VR 0.0.77 WRIST MAP:', g)
        self.assertIn('L HAND R WRIST', src('src/phone.cpp'))
        self.assertIn('wrist_map_hud', src('apps/launcher/ui.cpp'))

    def test_no_forbidden_words(self):
        for p in ('src/game_xr.cpp', 'include/spidy/story077h.hpp', 'tests/story077h_tests.cpp', 'tools/live_settings.py'):
            t = src(p).lower()
            self.assertNotIn('jim' + 'jam', t)
        self.assertNotIn('miles', src('include/spidy/story077h.hpp').lower())


if __name__ == '__main__':
    unittest.main()
