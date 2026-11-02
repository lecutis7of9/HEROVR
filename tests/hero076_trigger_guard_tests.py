"""HERO VR 0.0.76 TRIGGER GUARD (the user on c8974f4: "often ill hit trigger and that will trigger like a webzip to
punch"). The web-shooting arm's thrust passed the punch test and sent the game's web strike (Y: a zip to the enemy and
a punch) - 15 PUNCH DASHES and 2 lock lunges in the session. live.txt trigger_guard, id 5557, ON by default, OFF is
c8974f4's rules. The behaviour itself is pinned in tests/core_tests.cpp ("TRIGGER GUARD")."""
import os, re, sys, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import live_settings  # noqa: E402
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()

KEY, EXPORT, IDENT, ATOMIC = 'trigger_guard', 'SpidyTriggerGuard', 5557, 'triggerGuardNow'

class TriggerGuard(unittest.TestCase):
    def test_live_txt_option(self):
        (lo, hi), exp, text = live_settings.SETTINGS[KEY]
        self.assertEqual((lo, hi, exp), (0, 1, EXPORT))
        self.assertTrue(text.endswith('live, no restart needed'))  # the subtext says no restart
        self.assertEqual(live_settings.CHOICE_NAMES[KEY], ('on', 'off'))
        self.assertEqual(live_settings.export_value(KEY, 'on'), 0)  # ON is the default (0)
        self.assertEqual(live_settings.export_value(KEY, 'off'), 1)  # players can always go back
        self.assertEqual(live_settings.parse('%s = off\n' % KEY), ({KEY: 'off'}, []))
        self.assertIn(KEY, live_settings.START_ONLY)  # an OFF kept between sessions
        self.assertNotIn(KEY, live_settings.UPDATE38_KEYS)
    def test_export_is_ours_alone(self):
        g = src('src/game_xr.cpp')
        self.assertIn('WINAPI %s(void* v) {\n    return setLive(v, 0, 1, %d, %s, asU32);' % (EXPORT, IDENT, ATOMIC), g)
        self.assertEqual(len(re.findall(r'\b%d\b' % IDENT, g)), 1)
        self.assertIn('std::atomic<uint32_t> %s{0};' % ATOMIC, g)
    def test_spider_man_only(self):
        g = src('src/game_xr.cpp')
        self.assertIn('c.triggerGuard = triggerGuardNow.load() == 0 && heroNow() == hero_modes::spiderMan;', g)
        self.assertIn('VERSION 0.0.76 TRIGGER GUARD:', g)
    def test_machine(self):
        h = src('include/spidy/combat_input.hpp')
        self.assertIn('bool triggerGuard = false;', h)  # older tests and replays keep their rules
        # Every web strike (lock lunge, PUNCH DASH, zip) asks the guard; the Y waits guardConfirm for a late trigger.
        self.assertGreaterEqual(h.count('yGuarded(f, c, i)'), 3)
        self.assertIn('guardConfirm', h)
        self.assertNotIn('jimjam', h.lower())
        # The ball path is untouched: still the press frame.
        self.assertIn('ballInstant', h)

if __name__ == '__main__':
    unittest.main()
