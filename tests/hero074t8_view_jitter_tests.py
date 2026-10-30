"""0.0.74 test 8 (src/stereo_probe.cpp, src/game_xr.cpp, src/native_eye_gpu.cpp, include/spidy/view_jitter.hpp,
include/spidy/jitter_proof.hpp): VIEW JITTER FIX (the head shadow view 0, the game view and its head view the game's
own jitter or 0 while rested, under ENGINE TAA too; OFF = test 7), the VIEW JITTER per-view lines, the relaxed head
STILL, CLEAN BLEND and the CLEAN DISPLAY PROOF readback. Made by Agent Mulder."""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402

ROWS = (('view_jitter_fix', 'VIEW JITTER FIX', 'SpidyViewJitterFix', 'viewJitterFix', 5523, ('on', 'off')),)


def src(rel):
    return (ROOT / rel).read_text(encoding='utf-8')


class Switch(unittest.TestCase):
    def test_row(self):
        c = src('include/spidy/combat38.hpp')
        g = src('src/game_xr.cpp')
        for key, label, export, name, code, values in ROWS:
            self.assertEqual(L.UPDATE38_DEFAULTS[key], values[0])
            self.assertEqual(L.CHOICES[key][values[0]], 0)
            self.assertEqual(L.CHOICES[key][values[1]], 1)
            self.assertEqual(L.CHOICE_NAMES[key], values)
            self.assertEqual(L.SETTINGS[key][1], export)
            self.assertIn(key, L.UPDATE38_KEYS)
            self.assertIn(f"('{key}', ('{values[0].upper()}', '{values[1].upper()}'))", src('tools/update37_records.py'))
            self.assertIn(f'setHero007(v, combat38::{name}, {code})', g)
            self.assertEqual(g.count(f', {code}); }}'), 1)
            m = re.search(r'\{"%s", "%s", "[A-Z]{4}", "([^"]+)", 2, \{"%s", "%s"' %
                          (key, label, values[0].upper(), values[1].upper()), c)
            self.assertIsNotNone(m)
            self.assertLessEqual(len(m.group(1)), 80)
            self.assertEqual(m.group(1).count('.'), 1)
            self.assertIn('restart', m.group(1))
        self.assertIn('constexpr int combatControls = 178;', src('include/spidy/wrist_hud.hpp'))

    def test_notice_test_8(self):
        self.assertIn('set(HERO_TEST_NUMBER "8" CACHE', src('CMakeLists.txt'))

    def test_no_name(self):
        for rel in ('include/spidy/view_jitter.hpp', 'src/stereo_probe.cpp', 'src/game_xr.cpp',
                    'src/native_eye_gpu.cpp', 'tests/hero074t8_view_jitter_tests.py'):
            self.assertNotIn('jim' + 'jam', src(rel).lower())


class LensCalls(unittest.TestCase):
    def test_fixed_args(self):
        s = src('src/stereo_probe.cpp')
        self.assertIn('#include "spidy/view_jitter.hpp"', s)
        self.assertIn('const auto gameArgs = vj::fixedArgs(vj::gameView, vjFixOn(), vjGameRendered(),', s)
        self.assertIn('onHead(lens, near_clip::metres(), vj::shadowHead)', s)
        self.assertIn('onHead(viewLens(lens), gameNear, vj::gameHead)', s)
        self.assertIn('extern "C" void SpidyViewJitterFixSet(uint32_t on)', s)
        h = src('include/spidy/view_jitter.hpp')
        self.assertIn('return use == useZero ? Args{0.f, 0.f} : use == useOwn ? argsFor(ownX, ownY) : testSeven;', h)

    def test_pushed_each_frame(self):
        g = src('src/game_xr.cpp')
        self.assertIn('SpidyViewJitterFixSet(vjFix ? 1u : 0u);', g)


class Lines(unittest.TestCase):
    def test_lines(self):
        g = src('src/game_xr.cpp')
        self.assertIn('"VIEW JITTER f%llu pass %llu [FIX %s, ENGINE TAA %s, game view %s]: game own (%+.3f, %+.3f) px"', g)
        self.assertIn('"VIEW JITTER 10 s %s [FIX %s, ENGINE TAA %s, game view rested %llu of %llu frames]: "', g)
        self.assertIn('still %u of %u s"', g)
        self.assertIn('"CLEAN BLEND 10 s: history kept avg %.0f%% (least %.0f%%, most %.0f%%), new picture avg "', g)
        self.assertIn('"CLEAN DISPLAY PROOF %s: history %s; displayed vs raw mean diff %.4f "', g)

    def test_display_proof(self):
        n = src('src/native_eye_gpu.cpp')
        self.assertIn('constexpr unsigned regions = 6;', n)
        self.assertIn('proof::afterHistory(slot->list.Get(), true); // 0.0.74 test 8 DISPLAY PROOF', n)
        self.assertIn('proof::afterHistory(slot->list.Get(), false);', n)
        self.assertIn('copyBoxes(list, 2);', n)
        self.assertIn('inline float meanAbsDiff(', src('include/spidy/jitter_proof.hpp'))

    def test_still(self):
        h = src('include/spidy/view_jitter.hpp')
        self.assertIn('windowTravel = .10f, windowTurnDeg = 8.f, secondTravel = .02f, secondTurnDeg = 2.f;', h)


if __name__ == '__main__':
    unittest.main()
