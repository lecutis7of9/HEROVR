"""HERO VR 0.0.74 test 10 CAR COLLISION (pt1387u: "make it where you cant just walk into a vehicle it needs
collision"): on foot the parked van or Batmobile is a solid box - the walk into it is taken off, a body inside is
carried out (the swing command's CONFIRMED carry), a leaning head is held off its side (HeadClearance); the door
handle, getting in or out and driving are never blocked; live.txt car_collision = off walks through as before.
Made by Agent Mulder."""
import os, re, sys, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import live_settings  # noqa: E402
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()

class CarCollision(unittest.TestCase):
    def test_live_txt_option(self):
        (lo, hi), export, text = live_settings.SETTINGS['car_collision']
        self.assertEqual((lo, hi, export), (0, 1, 'SpidyCarCollision'))
        self.assertIn('restart', text)
        self.assertEqual(live_settings.CHOICE_NAMES['car_collision'], ('on', 'off'))
        self.assertEqual(live_settings.export_value('car_collision', 'on'), 0)
        self.assertEqual(live_settings.export_value('car_collision', 'off'), 1)
        self.assertEqual(live_settings.parse('car_collision = off\n'), ({'car_collision': 'off'}, []))
        self.assertIn('car_collision', live_settings.START_ONLY)
        self.assertNotIn('car_collision', live_settings.UPDATE38_KEYS)  # live.txt only: the wrist list is full
    def test_export_5547_is_ours_alone(self):
        g = src('src/game_xr.cpp')
        self.assertIn('WINAPI SpidyCarCollision(void* v) {\n    return setLive(v, 0, 1, 5547, carCollisionNow, asU32);', g)
        self.assertEqual(len(re.findall(r'\b5547\b', g)), 1)
        self.assertIn('std::atomic<uint32_t> carCollisionNow{0};', g)  # ON by default
    def test_wiring(self):
        g = src('src/game_xr.cpp')
        step = g.index('carBump = car_collision::step(cin);')
        self.assertLess(g.index('vout = vanDriver.step(vin, ci, vanCarry'), step)  # after the car's own step
        cmd = g.index('const Vec3 walk = car_collision::clip(input.move, carBump);')
        self.assertLess(cmd, g.index('if (vanCarry.carry && input.focused && finite(vanCarry.velocity)', cmd))  # the van's carry wins
        self.assertLess(cmd, g.index('submitSwing(&input)', cmd))
        head = g.index('rig.surfaceFound({carBump.facePoint, carBump.faceNormal});')
        self.assertLess(head, g.index('motion = rig.update(controller, feet', head))
    def test_pure_header(self):
        h = src('include/spidy/car_collision074.hpp')
        self.assertEqual(re.findall(r'#include "([^"]+)"', h), ['spidy/van072.hpp'])
        self.assertIn('restart', h)
        self.assertNotIn('0x', re.sub(r'//.*', '', h))  # no game address
    def test_no_forbidden_name(self):
        for p in ('include/spidy/car_collision074.hpp', 'tests/hero074t10_car_collision_tests.py', 'tools/live_settings.py',
                  'src/game_xr.cpp', 'tests/core_tests.cpp'):
            self.assertNotIn('jim' + 'jam', src(p).lower())

if __name__ == '__main__':
    unittest.main()
