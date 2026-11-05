"""HERO VR 0.0.78 cut23 PROMPT SPLIT (CANDIDATE): the game's HUD pieces locked to the view (a point of the head's
frame), its world markers placed through the game's real head lens from the current head, split by the classifier
(prompt_split.hpp); prompt_cards (the mod's cooked cards) still off; the cut23 LIGHTS A/B (game_view_lift, off by
default)."""
import pathlib
import re
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Cut23(unittest.TestCase):
    def test_split_default_and_header(self):
        h = src('include/spidy/prompt_split.hpp')
        self.assertIn('namespace spidy::prompt_split', h)
        self.assertIn('anchorSplit = 0', h)
        self.assertIn('class Split', h)
        g = src('src/game_xr.cpp')
        self.assertIn('#include "spidy/prompt_split.hpp"', g)
        self.assertIn('std::atomic<uint32_t> promptAnchorNow{prompt_split::anchorSplit};', g)
        self.assertEqual(live_settings.CHOICES['prompt_anchor']['split'], 0)

    def test_marker_and_hud_branches(self):
        g = src('src/game_xr.cpp')
        at = g.index('// cut24: HUD locked to the view (a point of the head\'s frame). A world')
        body = g[at:at + 4500]
        # a world marker is the capture camera's world point, projected into this frame
        self.assertIn('heldMarker23[i].ok &&', body)
        self.assertIn('split23.kind(i) == prompt_split::kindMarker', body)
        self.assertIn('prompt_split::trackingOfWorld(saved->worldFromTracking,', body)
        self.assertIn('prompt_split::markerLockMetres', body)
        self.assertNotIn('metres = prompt_split::markerMetres;', g)
        # HUD stays view-locked. A marker with no capture lock is not glued to the head.
        self.assertIn('prompt_split::hudPlace(', body)
        self.assertIn('metres = prompt_split::hudMetres;', body)
        self.assertIn('prompt_split::headPoint(splitHead, cx, cy, metres)', body)
        self.assertIn('continue; // no capture lock: do not glue it to this head', body)
        self.assertIn('native_gpu::promptLayerSourceSerial()', g)
        self.assertIn('history.findSerial(srcSerial)', g)
        self.assertIn('prompt_split::matchedCapture(', g)
        self.assertNotIn('src = saved', g)
        self.assertIn('prompt_split::lockPoint(', g)
        self.assertIn('HERO VR 0.0.78 cut24 WAYPOINT LOCK (CANDIDATE)', g)
        gpu = src('src/native_eye_gpu.cpp')
        self.assertIn('SpidyLatchedEyeSerial()', gpu)
        self.assertIn('mask->sourceSerial', gpu)
        self.assertIn('if (!mask)', gpu)

    def test_migration(self):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text('prompt_anchor = room\n', encoding='utf-8')
            self.assertTrue(live_settings.migrate_078_cut23_anchor(p))
            self.assertEqual(live_settings.parse(p.read_text(encoding='utf-8'))[0].get('prompt_anchor'), 'split')
            p.write_text(p.read_text(encoding='utf-8').replace('= split', '= room'), encoding='utf-8')
            self.assertFalse(live_settings.migrate_078_cut23_anchor(p))  # once only: a later room stays
        self.assertIn('migrate_078_cut23_anchor', src('tools/run_game_vr.py'))

    def test_cmake_and_cooked_cards_off(self):
        c = src('CMakeLists.txt')
        self.assertIn('add_executable(spidy_cut23_tests tests/cut23_tests.cpp)', c)
        self.assertIn('add_test(NAME cut23 COMMAND spidy_cut23_tests)', c)
        g = src('src/game_xr.cpp')
        self.assertIn('std::atomic<uint32_t> promptCardsNow{story077d::cardsOff};', g)
        self.assertEqual(live_settings.CHOICES['prompt_cards']['off'], 1)

    def test_lights_ab(self):
        s = src('src/stereo_probe.cpp')
        self.assertIn('std::atomic<uint32_t> gameLiftOn{0};', s)
        self.assertIn('eyesKnown && gameLiftOn.load() ? sl::gameViewLift(', s)
        self.assertIn('WINAPI SpidyGameViewLift(void* v)', s)
        self.assertEqual(len(re.findall(r'\b5648\b', s)), 2)
        self.assertEqual(live_settings.SETTINGS['game_view_lift'][:2], ((0, 1), 'SpidyGameViewLift'))
        self.assertEqual(live_settings.CHOICES['game_view_lift'], {'off': 0, 'on': 1, '0': 0, '1': 1})
        self.assertIn('game_view_lift', live_settings.START_ONLY)
        # the 0.0.76 LIGHTS LOCK stays: the game's flares off in VR for every live.txt
        self.assertTrue(live_settings.lens_flares_off('lens_flares = game\n'))


if __name__ == '__main__':
    unittest.main()
