"""HERO VR 0.0.78 cut22 PROMPT WORLD (CANDIDATE): only the game's own UI in VR play (the mod's prompt cards off by
default, their Y / pull routing kept), prompt_layer's cards world-locked in the room (prompt_anchor 5647, room by
default), the HUD board pulled into view, the once-only live.txt move of prompt_cards to off, and the Sandman Y
watch log."""
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


class Cut22(unittest.TestCase):
    def test_anchor_switch(self):
        g = src('src/game_xr.cpp')
        self.assertIn('WINAPI SpidyPromptAnchor(void* v) { return setLive(v, 0, 2, 5647, promptAnchorNow, asU32); }', g)
        self.assertEqual(len(re.findall(r'\b5647\b', g)), 1)
        # cut23: split is the default; room (cut22's) is 2
        self.assertIn('std::atomic<uint32_t> promptAnchorNow{prompt_split::anchorSplit};', g)
        key = 'prompt_anchor'
        self.assertEqual(live_settings.SETTINGS[key][:2], ((0, 2), 'SpidyPromptAnchor'))
        self.assertTrue(live_settings.SETTINGS[key][2].endswith('- live, no restart needed'))
        self.assertEqual(live_settings.CHOICES[key]['room'], 2)
        self.assertEqual(live_settings.CHOICES[key]['view'], 1)
        self.assertIn(key, live_settings.START_ONLY)
        self.assertIn('live.txt prompt_anchor = view', src('apps/launcher/ui.cpp'))
        h = src('include/spidy/prompt_world.hpp')
        self.assertIn('enum Anchor : uint32_t { anchorRoom = 2, anchorView = 1 };', h)
        self.assertIn('SpidyPromptAnchor 5647', h)

    def test_cards_room_locked(self):
        g = src('src/game_xr.cpp')
        at = g.index('HERO VR 0.0.78 cut22 PROMPT WORLD (CANDIDATE, prompt_world.hpp): each card a')
        block = g[at:g.index('layerLast = cards.count;', at)]
        # Room space: each eye's tracking pose and lens, the ones the layer is submitted with.
        self.assertIn('saved->trackingEyes[made(eye)]', block)
        self.assertIn('viewMatrix(te.pose)', block)
        self.assertIn('prompt_world::cardRect(', block)
        self.assertIn('worldBoard22.point(', block)
        self.assertIn('prompt_world::markerPoint(worldGridHead22', block)
        self.assertIn('layerGrid.stamp != worldStamp22', block)
        # cut21's head placement only with prompt_anchor = view
        self.assertIn('} else if (!roomAnchor) {', block)
        self.assertIn('prompt_layer::cardRect(', block)
        self.assertNotIn('setInset', block)
        self.assertIn('HERO VR 0.0.78 cut22 PROMPT WORLD (CANDIDATE) %s', g)
        self.assertIn('grid.stamp = layerGridMs;', src('src/native_eye_gpu.cpp'))
        self.assertIn('uint64_t stamp{};', src('include/spidy/prompt_layer.hpp'))

    def test_board_numbers(self):
        h = src('include/spidy/prompt_world.hpp')
        for line in ('inline constexpr float boardDistance = 1.4f;', 'inline constexpr float pull = .55f;',
                     'inline constexpr float turnDegrees = 50.f;', 'inline constexpr uint64_t holdMs = 800;',
                     'inline constexpr float minCardTan = .075f;'):
            self.assertIn(line, h)
        cm = src('CMakeLists.txt')
        self.assertIn('add_executable(spidy_cut22_tests tests/cut22_tests.cpp)', cm)
        self.assertIn('add_test(NAME cut22 COMMAND spidy_cut22_tests)', cm)

    def test_cooked_cards_off(self):
        g = src('src/game_xr.cpp')
        self.assertIn('std::atomic<uint32_t> promptCardsNow{story077d::cardsOff};', g)
        self.assertIn('const bool cardsWanted = vrGameplay;', g)  # routing kept
        self.assertIn('promptCardCount && promptCardsNow.load() == story077d::cardsOn &&', g)  # drawing gated
        self.assertIn('std::atomic<uint32_t> bossYCardNow{story077d::bossYOn};', g)  # Y routing at the boss kept
        self.assertIn("1 off, the default since cut22", live_settings.SETTINGS['prompt_cards'][2])
        self.assertIn('drawn only with', live_settings.SETTINGS['boss_y_card'][2])

    def test_cards_migration(self):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            self.assertIsNone(live_settings.migrate_078_cut22_cards(p))  # no live.txt: nothing
            p.write_text('prompt_cards = on\nreach = 40\n', encoding='utf-8')
            self.assertIn('prompt_cards set once to off', live_settings.migrate_078_cut22_cards(p))
            t = p.read_text(encoding='utf-8')
            self.assertEqual(live_settings.parse(t)[0]['prompt_cards'], 'off')
            self.assertIn(live_settings.MIGRATED_078_CUT22_CARDS, t)
            p.write_text(t.replace('prompt_cards = off', 'prompt_cards = on'), encoding='utf-8')
            self.assertIsNone(live_settings.migrate_078_cut22_cards(p))  # chosen after the marker: stays
            self.assertEqual(live_settings.parse(p.read_text(encoding='utf-8'))[0]['prompt_cards'], 'on')
        self.assertIn(live_settings.MIGRATED_078_CUT22_CARDS, live_settings.HEADER)
        self.assertIn('live_settings.migrate_078_cut22_cards(ROOT/', src('tools/run_game_vr.py'))

    def test_sandman_watch(self):
        g = src('src/game_xr.cpp')
        self.assertIn('HERO VR 0.0.78 cut22 SANDMAN Y #%llu (CANDIDATE)', g)
        self.assertIn('bossFightNow16() && sandYPresses22 < 60', g)

    def test_strings(self):
        for p in ('include/spidy/prompt_world.hpp', 'tests/cut22_tests.cpp'):
            t = src(p).lower()
            self.assertNotIn('jim' + 'jam', t)
            self.assertNotIn('mi' + 'les', t)


if __name__ == '__main__':
    unittest.main()
