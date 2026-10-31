# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.74 test 10 BELT ZONES: BATMAN's left hip = grapple, belt front = batarang, other forearm = smoke.

The owner: "with batman its grab left for grapple, center waitline for batarand, and grab on forearm for smoke
pellet ... right now grabbing your side you can grab either batarang or grapple its anyones guess". A live
setting, ON by default; the zone geometry itself is tested in core_tests.cpp.
"""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Setting(unittest.TestCase):
    def test_live_setting(self):
        self.assertEqual(L.SETTINGS['belt_zones'][:2], ((0, 1), 'SpidyBeltZones'))
        self.assertEqual(L.UPDATE38_DEFAULTS['belt_zones'], 'on')
        self.assertIn('belt_zones', L.UPDATE38_KEYS)
        self.assertEqual((L.export_value('belt_zones', 'on'), L.export_value('belt_zones', 'off')), (0, 1))
        values, problems = L.parse('belt_zones = off\n')
        self.assertEqual((values.get('belt_zones'), problems), ('off', []))
        self.assertIn('live', L.SETTINGS['belt_zones'][2])

    def test_option_row_one_sentence_with_restart(self):
        c = src('include/spidy/combat38.hpp')
        row = re.search(r'\{"belt_zones", "BELT ZONES", "[^"]+", "([^"]+)", 2, \{"ON", "OFF"', c)
        self.assertIsNotNone(row)
        text = row.group(1)
        self.assertLessEqual(len(text), 80)
        self.assertTrue(text.endswith('.') and text.count('.') == 1)
        self.assertIn('no restart', text)
        self.assertIn('inline bool beltZonesOn(uint32_t value) { return value != 1; }', c)

    def test_export_and_both_inputs(self):
        g = src('src/game_xr.cpp')
        self.assertIn('WINAPI SpidyBeltZones(void* v) { return setHero007(v, combat38::beltZones, 5533); }', g)
        self.assertEqual(g.count('hin.beltZones = combat38::beltZonesOn(c38(combat38::beltZones));'), 2)


class Zones(unittest.TestCase):
    def test_play_uses_the_zones(self):
        h = src('include/spidy/hero007.hpp')
        p = src('include/spidy/hero007_play.hpp')
        self.assertIn('inline ZonePick pickZone(holster::BodyPoint p, float armGap)', h)
        self.assertIn('bool beltZones = true;', p)
        self.assertIn("hip = !in.beltZones || beltPick[i] == pickGrapple", p)
        self.assertIn('in.beltZones ? smokeAt : nullptr', p)
        self.assertIn('in.beltZones ? batarangAt : nullptr', p)

    def test_gap_and_margin(self):
        h = src('include/spidy/hero007.hpp')
        num = lambda name: float(re.search(name + r' = (\.?[0-9.]+)f', h).group(1))
        self.assertGreaterEqual(num('hipInner') - num('buckleSide'), num('zoneMargin') - 1e-9)
        self.assertGreater(num('zoneMargin'), 0)


if __name__ == '__main__':
    unittest.main()
