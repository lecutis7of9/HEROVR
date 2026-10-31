"""HERO VR 0.0.74 test 10 VAN NO FLIPS (pt1380u: "the van should auto toggle realistic flips off ... it kept flipping
me when id take a sharp turn"): while he drives the van or a Batmobile the rig gets REALISTIC FLIPS and FLIPS off,
frame by frame, without writing his saved settings; live.txt van_no_flips = off keeps his choice in the car.
Made by Agent Mulder."""
import os, re, sys, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import live_settings  # noqa: E402
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()

class VanNoFlips(unittest.TestCase):
    def test_live_txt_option(self):
        (lo, hi), export, text = live_settings.SETTINGS['van_no_flips']
        self.assertEqual((lo, hi, export), (0, 1, 'SpidyVanNoFlips'))
        self.assertIn('restart', text)
        self.assertEqual(live_settings.CHOICE_NAMES['van_no_flips'], ('on', 'off'))
        self.assertEqual(live_settings.export_value('van_no_flips', 'on'), 0)
        self.assertEqual(live_settings.export_value('van_no_flips', 'off'), 1)
        self.assertEqual(live_settings.parse('van_no_flips = off\n'), ({'van_no_flips': 'off'}, []))
        self.assertIn('van_no_flips', live_settings.START_ONLY)  # an OFF is kept from session to session
        self.assertNotIn('van_no_flips', live_settings.UPDATE38_KEYS)  # live.txt only: the wrist list is full
    def test_export_5540_is_ours_alone(self):
        g = src('src/game_xr.cpp')
        self.assertIn('WINAPI SpidyVanNoFlips(void* v) {\n    return setLive(v, 0, 1, 5540, vanNoFlipsNow, asU32);', g)
        self.assertEqual(len(re.findall(r'\b5540\b', g)), 1)
    def test_override_reaches_the_rig_before_update(self):
        g = src('src/game_xr.cpp')
        # 0.0.74 test 10 FLY FEEL: FLY FLIPS readies the rows first (fly_feel::flying); the van's override still wraps them last.
        j = g.index('rig.automaticFlips(vehicle_flips::realistic(flipRows, carDriving, noFlips));')
        self.assertIn('const auto flipRows = fly_feel::flying(realisticFlips(values)', g[j - 400:j])
        self.assertIn('rig.flips(vehicle_flips::stickFlips(values.flips, carDriving, noFlips));', g[j:j + 300])
        self.assertLess(j, g.index('rig.update(', j))
        self.assertIn('const bool carDriving = van.state == van072::driving;', g[j - 700:j])
    def test_saved_settings_never_written(self):
        h = src('include/spidy/vehicle_flips.hpp')
        self.assertNotIn('vr_settings', h.split('#include')[1])
        self.assertNotIn('save', re.sub(r'//.*', '', h).replace('saved', ''))
    def test_no_forbidden_name(self):
        for p in ('include/spidy/vehicle_flips.hpp', 'tests/hero074t10_van_flips_tests.py', 'tools/live_settings.py'):
            self.assertNotIn('jim' + 'jam', src(p).lower())

if __name__ == '__main__':
    unittest.main()
