"""HERO VR 0.0.74 test 10 FLY FEEL (pt1424u, a player's comment: "the way it automatically starts flight instead of
having a held button makes it so you maneuver kind of stiffly and if you hold your hand up when youre upside down or
sideways you just go up"): FLY HOLD (a grip held to enter flight, then it stays), FLY STEER (the climb along the
body's up, eased turning), FLY FLIPS (REALISTIC FLIPS readier while flying) and FLY LOOK UP (the toggled view: a tap of
Y while flying smoothly pans the view up to look ahead when the body lies flat, off each session until tapped) - live.txt only (the wrist HUD is full), ids 5548-5551. No game address.
Made by Agent Mulder."""
import os, re, sys, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import live_settings  # noqa: E402
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()

SWITCHES = (  # key, export, id, atomic, choices (index 0 = the default)
    ('fly_hold', 'SpidyFlyHold', 5548, 'flyHoldNow', ('on', 'off')),
    ('fly_steer', 'SpidyFlySteer', 5549, 'flySteerNow', ('on', 'off')),
    ('fly_flips', 'SpidyFlyFlips', 5550, 'flyFlipsNow', ('on', 'off')),
    ('fly_look_up', 'SpidyFlyLookUp', 5551, 'flyLookUpNow', ('on', 'off')),
)

class FlyFeel(unittest.TestCase):
    def test_live_txt_options(self):
        for key, export, _, _, choices in SWITCHES:
            (lo, hi), exp, text = live_settings.SETTINGS[key]
            self.assertEqual((lo, hi, exp), (0, 1, export))
            self.assertIn('live, no restart', text)
            self.assertEqual(live_settings.CHOICE_NAMES[key], choices)
            self.assertEqual(live_settings.export_value(key, choices[0]), 0)  # the default is 0
            self.assertEqual(live_settings.export_value(key, choices[1]), 1)
            self.assertEqual(live_settings.parse('%s = %s\n' % (key, choices[1])), ({key: choices[1]}, []))
            self.assertIn(key, live_settings.START_ONLY)
            self.assertNotIn(key, live_settings.UPDATE38_KEYS)  # live.txt only: the wrist list is full
    def test_exports_are_ours_alone(self):
        g = src('src/game_xr.cpp')
        for _, export, ident, atomic, _ in SWITCHES:
            self.assertIn('WINAPI %s(void* v) {\n    return setLive(v, 0, 1, %d, %s, asU32);' % (export, ident, atomic), g)
            self.assertEqual(len(re.findall(r'\b%d\b' % ident, g)), 1)
        self.assertIn('std::atomic<uint32_t> flyHoldNow{0}, flySteerNow{0}, flyFlipsNow{0}, flyLookUpNow{0};', g)
    def test_wiring(self):
        g = src('src/game_xr.cpp')
        word = g.index('input.reserved7[1] = flyLive ? fly_feel::pack(holdOn, enter, steerOn) : 0u;')
        self.assertLess(g.index('input.reserved6 = flyLive ? extras385::fly::pack('), word)
        self.assertLess(word, g.index('submitSwing(&input)', word))
        flips = g.index('const auto flipRows = fly_feel::flying(realisticFlips(values)')
        self.assertLess(flips, g.index('rig.automaticFlips(vehicle_flips::realistic(flipRows, carDriving, noFlips));'))  # the van wins
        level = g.index('rig.flyLookUp(std::isfinite(pan) ? pan : 0.f);')
        self.assertLess(level, g.index('motion = rig.update(controller, feet', level))
        # LOOK UP: live.txt is the master, the Y tap only while FLY flies and the game is not naming Y, a cue each way
        look = g[g.index('// 0.0.74 test 10 FLY LOOK UP (fly_feel074.hpp'):level]
        self.assertIn('enabled && flyLookUpSession && flyingNow', look)
        self.assertIn('fly_feel::levelOn(flyLookUpNow.load())', look)
        self.assertIn('bool flyLookUpSession = false;', g)  # OFF every session until pressed
        self.assertIn('flyingNow && yFree && !calibrationInput', look)
        self.assertIn('NamedPress::controlY', look)
        self.assertIn('(frame.buttons & buttonY) != 0', look)
        self.assertIn('runtime.haptic(0,', look)
        self.assertIn('message(3, 0, flyLookUpSession ?', look)
        self.assertIn('flyLookUpNow = 1;', look)  # a failure turns it off
        # Y while FLY flies is FLY's (Web Wings taken from the game's pad), so the tap reaches nothing else
        self.assertIn('if (flyNow.flying && mapping == game_pad::Mapping::gameplay)\n                            pad.buttons = static_cast<uint16_t>(pad.buttons & ~game_pad::y);', g)
        s = src('src/game_swing.cpp')
        self.assertIn('fly_feel::unpack(c.version >= 7 && c.bytes == sizeof(Command) ? c.reserved7[1] : 0u)', s)
        self.assertIn('fi.bodyUp = fly_feel::bodyUp(toWorld);', s)
        self.assertLess(s.index('fi.bodyUp = fly_feel::bodyUp(toWorld);'), s.index('flight.update(fi)'))
        f = src('include/spidy/extras385.hpp')
        self.assertIn('if (in.holdEnter && !in.enter)\n                return o;', f)
        t = src('src/game_tracking.cpp')
        lv = t.index('fly_feel::pitchUpTilt(placed.tilt, ahead, flyLookUp_)')
        self.assertLess(lv, t.index('out.flyRoll = flyRoll_;'))  # the look up first, the stick's roll on top
        self.assertNotIn('out.flipTilt = fly', t)  # the body keeps its pose
        self.assertIn('if (flyLookUp_ > 0 && !holds && std::isfinite(flyLookUp_))', t)
    def test_bad_value_is_off(self):
        self.assertEqual(live_settings.parse('fly_look_up = banana\n')[0], {'fly_look_up': 'off'})
        self.assertEqual(live_settings.parse('fly_look_up = 7\n')[0], {'fly_look_up': 'off'})
        self.assertEqual(live_settings.export_value('fly_look_up', 'off'), 1)
        h = src('include/spidy/fly_feel074.hpp')
        self.assertIn('inline bool levelOn(uint32_t value) { return value == 0; }', h)
        self.assertNotIn('lookUpDegrees', h)  # no fixed tilt
    def test_pure_header(self):
        h = src('include/spidy/fly_feel074.hpp')
        self.assertEqual(re.findall(r'#include "([^"]+)"', h), ['spidy/math.hpp', 'spidy/realistic_flip.hpp'])
        for name in ('holdHelp', 'steerHelp', 'flipsHelp', 'lookUpHelp'):
            m = re.search(name + r' =\s*((?:"[^"]*"\s*)+);', h)
            text = ''.join(re.findall(r'"([^"]*)"', m.group(1)))
            self.assertIn('restart', text)
            self.assertEqual(text.count('. '), 0)  # one sentence
        self.assertNotIn('0x', re.sub(r'//.*', '', h))  # no game address
    def test_no_forbidden_name(self):
        for p in ('include/spidy/fly_feel074.hpp', 'include/spidy/extras385.hpp', 'tests/hero074t10_fly_feel_tests.py',
                  'tools/live_settings.py', 'src/game_xr.cpp', 'src/game_swing.cpp', 'src/game_tracking.cpp',
                  'include/spidy/game_tracking.hpp', 'include/spidy/game_swing.hpp', 'tests/core_tests.cpp'):
            self.assertNotIn('jim' + 'jam', src(p).lower())

if __name__ == '__main__':
    unittest.main()
