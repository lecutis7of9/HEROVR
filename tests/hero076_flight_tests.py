# HERO VR 0.0.76 FLIGHT: the reworked flight's wiring (fly_classic, its export, A/B owned by the flight, walls for
# Spider-Man only) in the SM2 build and the SM1 add-on. Made by Agent Mulder.
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as ls

XR = (ROOT / 'src' / 'game_xr.cpp').read_text(encoding='utf-8')
SM1 = (ROOT / 'src' / 'sm1' / 'sm1_layer.cpp').read_text(encoding='utf-8')


class FlyClassicSetting(unittest.TestCase):
    def test_off_and_on(self):
        values, problems = ls.parse('fly_classic = on\n')
        self.assertEqual(problems, [])
        self.assertEqual(values['fly_classic'], 'on')
        self.assertEqual(ls.export_value('fly_classic', 'on'), 1)
        self.assertEqual(ls.export_value('fly_classic', 'off'), 0)

    def test_a_bad_value_is_a_problem(self):
        values, problems = ls.parse('fly_classic = maybe\n')
        self.assertNotIn('fly_classic', values)
        self.assertTrue(any('fly_classic must be off or on' in p for p in problems), problems)

    def test_help_has_restart_info_and_is_start_only(self):
        self.assertIn('live, no restart', ls.SETTINGS['fly_classic'][2])
        self.assertEqual(ls.SETTINGS['fly_classic'][1], 'SpidyFlyClassic')
        self.assertIn('fly_classic', ls.START_ONLY)


class Wiring(unittest.TestCase):
    def test_export_once(self):
        self.assertEqual(XR.count('SpidyFlyClassic(void* v)'), 1)
        self.assertEqual(len(re.findall(r'\b5553\b', XR)), 1)

    def test_a_and_b_belong_to_the_flight(self):
        self.assertIn('if (fly076Flying)\n                        keys &= ~swingJumpKey;', XR)
        self.assertRegex(XR, r'if \(fly076Flying\)[^\n]*\n\s*walk\.dodge = false;')

    def test_only_spider_man_climbs(self):
        self.assertIn('cc.on = climbOn.load() && heroClimbs();', XR)
        self.assertIn('wallPushOff.kicked(swingState.wallNormal);', XR)
        self.assertIn('if (!heroClimbs()) {', XR)

    def test_nobody_but_spider_man_climbs(self):
        h = (ROOT / 'include' / 'spidy' / 'hero072.hpp').read_text(encoding='utf-8')
        body = re.search(r'inline bool climbAllowed\([^)]*\) \{(.*?)\n\}', h, re.S).group(1)
        self.assertEqual(body.strip(), 'return hero == hero_modes::spiderMan;')
        self.assertIn('Retired in 0.0.76', (ROOT / 'include' / 'spidy' / 'combat38.hpp').read_text(encoding='utf-8'))
        self.assertIn('OFF no longer lets other heroes', (ROOT / 'include' / 'spidy' / 'combat38.hpp').read_text(encoding='utf-8'))
        self.assertNotIn('(CLAW CLIMB on)', (ROOT / 'include' / 'spidy' / 'hero_controls.hpp').read_text(encoding='utf-8'))

    def test_sm1_status(self):
        self.assertIn('"FLIGHT: ARMS UP OR HOLD A"', SM1)
        self.assertIn('"FLIGHT: BOTH ARMS UP"', SM1)

    def test_no_miles_name(self):  # (the old-name check is v006_tests')
        for p in [ROOT / 'include' / 'spidy' / 'fly076.hpp', ROOT / 'tests' / 'fly076_tests.cpp']:
            text = p.read_text(encoding='utf-8').lower()
            self.assertNotIn('miles', text)


if __name__ == '__main__':
    unittest.main()
