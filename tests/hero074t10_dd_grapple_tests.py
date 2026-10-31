"""HERO VR 0.0.74 test 10 DD QUICK GRAPPLE (pt1440u, a tester on DAREDEVIL: "the grapple feels a little slow though and
it would be cool if he had a midair grapple cancel like the arkham games to redirect especiall with the dual weild"):
DAREDEVIL's pull faster and eased in (no jerk), and a press from either baton while pulled cancels the line, keeps
the speed and fires along the new aim (a miss coasts on) - live.txt only (the wrist HUD is full), id 5552, ON by
default. No game address.
Made by Agent Mulder."""
import os, re, sys, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import live_settings  # noqa: E402
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()

KEY, EXPORT, IDENT, ATOMIC = 'dd_quick_grapple', 'SpidyDdQuickGrapple', 5552, 'ddQuickGrappleNow'

class DdQuickGrapple(unittest.TestCase):
    def test_live_txt_option(self):
        (lo, hi), exp, text = live_settings.SETTINGS[KEY]
        self.assertEqual((lo, hi, exp), (0, 1, EXPORT))
        self.assertIn('live, no restart', text)
        self.assertEqual(live_settings.CHOICE_NAMES[KEY], ('on', 'off'))
        self.assertEqual(live_settings.export_value(KEY, 'on'), 0)  # ON is the default
        self.assertEqual(live_settings.export_value(KEY, 'off'), 1)
        self.assertEqual(live_settings.parse('%s = off\n' % KEY), ({KEY: 'off'}, []))
        self.assertIn(KEY, live_settings.START_ONLY)
        self.assertNotIn(KEY, live_settings.UPDATE38_KEYS)  # live.txt only: the wrist list is full
    def test_export_is_ours_alone(self):
        g = src('src/game_xr.cpp')
        self.assertIn('WINAPI %s(void* v) {\n    return setLive(v, 0, 1, %d, %s, asU32);' % (EXPORT, IDENT, ATOMIC), g)
        self.assertEqual(len(re.findall(r'\b%d\b' % IDENT, g)), 1)
        self.assertIn('std::atomic<uint32_t> %s{0};' % ATOMIC, g)
        for p in ('src/game_swing.cpp', 'tools/live_settings.py'):
            self.assertEqual(len(re.findall(r'\b%d\b' % IDENT, src(p))), 0)
    def test_wiring(self):
        g = src('src/game_xr.cpp')
        tilt = g.index('input.reserved7[0] = cape_glide::glideWord(')
        dd = g.index('input.reserved7[0] |= dd_grapple074::word(heroNow() == hero_modes::daredevil,')
        self.assertLess(tilt, dd)  # after the glide word is set, never overwritten by it
        self.assertIn('dd_grapple074::onByDefault(ddQuickGrappleNow.load())', g[dd:dd + 300])
        self.assertLess(dd, g.index('submitSwing(&input)', dd))
        s = src('src/game_swing.cpp')
        self.assertIn('const uint32_t ddWord = grappleOnly && c.version >= 7 && c.bytes == sizeof(Command) ? c.reserved7[0] : 0u;', s)
        self.assertIn('(capeGrapple.phase() == cape_glide::idle || redirect) && !takingOff', s)
        self.assertIn('held && !grappleHeldWas[i]', s)  # a new press only (the rising edge, either hand)
        self.assertIn('capeGrapple.tune = dd_grapple074::tuneFor(ddQuick);', s)
        self.assertEqual(s.count('capeGrapple.carry(carried);'), 2)  # a surface or an enemy
        self.assertIn('ddCoast.start(carried);', s)
        self.assertIn('caped = pulled.drive || coasted.drive || glided.drive;', s)
        self.assertIn('ddCoast.stop(); // 0.0.74 test 10 DD QUICK GRAPPLE: no coast past a cancel either', s)
        self.assertIn('|| ddCoast.on();', s)  # the game's fall resumes from the coast (solver.reset)
    def test_pure_header(self):
        h = src('include/spidy/dd_grapple074.hpp')
        self.assertEqual(re.findall(r'#include "([^"]+)"', h), ['spidy/cape_glide.hpp', 'spidy/math.hpp'])
        m = re.search(r'help =\s*((?:"[^"]*"\s*)+);', h)
        text = ''.join(re.findall(r'"([^"]*)"', m.group(1)))
        self.assertIn('restart', text)
        self.assertEqual(text.count('. '), 0)  # one sentence
        self.assertNotIn('0x', re.sub(r'//.*', '', h))  # no game address
        self.assertIn('CANDIDATE', h)
        self.assertIn('CONFIRMED', h)
    def test_no_forbidden_name(self):
        for p in ('include/spidy/dd_grapple074.hpp', 'include/spidy/cape_glide.hpp', 'include/spidy/dd_baton073.hpp',
                  'tests/hero074t10_dd_grapple_tests.py', 'tools/live_settings.py', 'src/game_xr.cpp',
                  'src/game_swing.cpp', 'tests/core_tests.cpp'):
            self.assertNotIn('jim' + 'jam', src(p).lower())

if __name__ == '__main__':
    unittest.main()
