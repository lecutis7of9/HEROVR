"""HERO VR 0.0.76 DOUBLE HEALTH and the HEALTH block at the top of POWERS (owner: "INVINCIBLE is way too hard to
find"): every hit costs half, for every hero, through INVINCIBLE's CONFIRMED health record (vtable 0x541edd0,
+0xd0 HP, +0xa0 max, +0x208 reduce), never above the cap and never a revive. Live, OFF by default, a wrist row
next to INVINCIBLE, two buttons on the tiles' POWERS front and live.txt double_health (id 5554).
Made by Agent Mulder."""
import os, re, sys, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import live_settings  # noqa: E402
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()

KEY, EXPORT, IDENT = 'double_health', 'SpidyDoubleHealth', 5554

class DoubleHealth(unittest.TestCase):
    def test_live_txt(self):
        (lo, hi), exp, text = live_settings.SETTINGS[KEY]
        self.assertEqual((lo, hi, exp), (0, 1, EXPORT))
        self.assertIn('live, no restart', text)
        self.assertNotIn(KEY, live_settings.START_ONLY)  # a cheat: OFF every session, as INVINCIBLE
    def test_export_is_ours_alone(self):
        g = src('src/game_xr.cpp')
        self.assertIn('WINAPI %s(void* on) {\n    return setLive(on, 0, 1, %d, doubleHealthOn, asBool);' % (EXPORT, IDENT), g)
        self.assertEqual(len(re.findall(r'\b%d\b' % IDENT, g)), 1)
        self.assertIn('std::atomic<bool> doubleHealthOn{false};', g)
    def test_confirmed_path_only(self):
        g = src('src/game_xr.cpp')
        at = g.index('// 0.0.76 DOUBLE HEALTH: half of each fall in HP given back')
        block = g[at:at + 1400]
        self.assertIn('record && !invincibleOn.load()', block)
        self.assertIn('extras385::invincible::reduceAt', block)
        self.assertIn('writeFloatIf(h + game_player::hitPointsAt, hitPoints, to)', block)
        self.assertNotRegex(re.sub(r'//.*', '', block), r'0x[0-9a-fA-F]+')  # no new address
        h = src('include/spidy/extras385.hpp')
        self.assertIn('healthTable = 0x541edd0; // CONFIRMED', h)
    def test_help_one_sentence_with_restart(self):
        h = src('include/spidy/extras385.hpp')
        m = re.search(r'namespace double_health \{\ninline constexpr const char\* help = "([^"]+)";', h)
        text = m.group(1)
        self.assertLessEqual(len(text), 80)
        self.assertEqual(text.count('.'), 1)
        self.assertIn('restart', text)
    def test_top_of_powers(self):
        w = src('src/wrist_hud.cpp')
        p = w.index('    case powersPage: {\n')
        first = w[p:p + 600]
        self.assertLess(first.index('heading(healthHeading);'), first.index('row(Control::invincible);'))
        self.assertLess(first.index('row(Control::invincible);'), first.index('row(Control::doubleHealth);'))
        self.assertLess(first.index('row(Control::doubleHealth);'), first.index('Item b = itemOf(Kind::banner);'))
        self.assertIn('const char* const healthHeading = "HEALTH - EVERY HERO";', w)
        self.assertIn('constexpr Control healthRows[] = {C::invincible, C::doubleHealth};', w)
        g = w.index('group(std::vector<Control>(std::begin(healthRows)')
        self.assertLess(g, w.index('group(heroOwnRows(h), false);'))
        r = src('src/holo_roster.cpp')
        self.assertIn('Button{healthInvincible,', r)
        self.assertIn('Button{healthDouble,', r)
        self.assertIn('case healthDouble: a.doubleHealth = true; break;', r)
        self.assertEqual(w.count('r.doubleHealth = r.doubleHealth || a.doubleHealth;'), 2)
    def test_no_forbidden_name(self):
        for p in ('include/spidy/extras385.hpp', 'src/holo_roster.cpp', 'src/wrist_hud.cpp', 'src/game_xr.cpp',
                  'tools/live_settings.py', 'tests/hero076_double_health_tests.py'):
            self.assertNotIn('jim' + 'jam', src(p).lower())

if __name__ == '__main__':
    unittest.main()
