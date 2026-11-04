# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.77 CUT8: the boss HP read by RTTI (BotHealth and classes derived from it), the throw watch (5598) and
the POI TARGET diagnostic (no marker drawn from it)."""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Cut8(unittest.TestCase):
    def test_throw_watch_switch(self):
        g = src('src/game_xr.cpp')
        self.assertIn('WINAPI SpidyThrowWatch(void* v)', g)
        self.assertEqual(len(re.findall(r'\b5598\b', g)), 1)
        key = 'throw_watch'
        self.assertEqual(live_settings.SETTINGS[key][1], 'SpidyThrowWatch')
        self.assertTrue(live_settings.SETTINGS[key][2].endswith('- live, no restart needed'))
        self.assertIn(key, live_settings.START_ONLY)
        self.assertEqual(live_settings.CHOICE_NAMES[key], ('on', 'off'))
        self.assertIn('CUT8 THROW WATCH: throw %u', g)
        self.assertIn('throw_watch', src('apps/launcher/ui.cpp'))

    def test_boss_hp_by_rtti(self):
        w = src('src/world_trace.cpp')
        self.assertIn('!std::strcmp(name, ".?AVBotHealth@@")', w)
        self.assertIn('exact || found->second.botHealth', w)
        # the class lookup now runs before the BotHealth match
        self.assertLess(w.index('found = classes.emplace(head.vtable, classify(head.vtable)).first;'),
                        w.index('exact || found->second.botHealth'))
        self.assertIn('CUT8 BOSS HP: %s health', src('src/game_xr.cpp'))

    def test_poi_target_is_log_only(self):
        w = src('src/world_trace.cpp')
        self.assertIn('CUT8 POI TARGET %u', w)
        body = w[w.index('const auto poiTargets = [&]'):w.index('const auto botStep = [&]')]
        self.assertNotIn('marks.', body)
        self.assertNotIn('waypointPub', body)


if __name__ == '__main__':
    unittest.main()
