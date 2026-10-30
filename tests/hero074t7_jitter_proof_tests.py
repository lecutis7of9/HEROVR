"""0.0.74 test 7 (src/native_eye_gpu.cpp, src/game_xr.cpp, src/stereo_probe.cpp, include/spidy/jitter_proof.hpp):
CLEAN JITTER PROOF (a 128 x 128 raw-eye box read back every 2 s with the head still, the shift measured on the CPU),
JITTER PROOF BIG (a 4 pixel raw step, OFF), ENGINE TAA STOCK (CLEAN stands down while ENGINE TAA is on), the IMAGE
row held under CLEAN, the HEADSET SHARPENING line, TAA COVERAGE and ENGINE TAA COST every 10 s. Made by Agent Mulder."""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402

ROWS = (('jitter_proof', 'JITTER PROOF', 'SpidyJitterProof', 'jitterProof', 5520, ('on', 'off')),
        ('jitter_proof_big', 'JITTER PROOF BIG', 'SpidyJitterProofBig', 'jitterProofBig', 5521, ('off', 'on')),
        ('engine_taa_stock', 'ENGINE TAA STOCK', 'SpidyEngineTaaStock', 'engineTaaStock', 5522, ('on', 'off')))


def src(rel):
    return (ROOT / rel).read_text(encoding='utf-8')


class Switches(unittest.TestCase):
    def test_rows(self):
        c = src('include/spidy/combat38.hpp')
        g = src('src/game_xr.cpp')
        for key, label, export, name, code, values in ROWS:
            with self.subTest(key=key):
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
        self.assertIn('constexpr int combatControls = 177;', src('include/spidy/wrist_hud.hpp'))

    def test_notice_test_7(self):
        self.assertIn('set(HERO_TEST_NUMBER "7" CACHE', src('CMakeLists.txt'))

    def test_no_name(self):
        for rel in ('include/spidy/jitter_proof.hpp', 'src/native_eye_gpu.cpp', 'src/game_xr.cpp',
                    'tests/hero074t7_jitter_proof_tests.py'):
            self.assertNotIn('jim' + 'jam', src(rel).lower())


class StandDown(unittest.TestCase):
    def test_clean_falls_through_to_stock(self):
        g = src('src/game_xr.cpp')
        self.assertIn('bool cleanStandsDown() {', g)
        self.assertIn('engine_taa::stateOf(SpidyEngineTaaNow()) == engine_taa::on;', g)
        self.assertIn("combat38Values[combat38::aaMode].load())) &&\n        !cleanStandsDown())\n"
                      "        return combat38::cleanValue(option, own);", g)
        self.assertIn('const bool cleanAa = combat38::aaModeClean(c38(combat38::aaMode)) && !standDown;', g)
        self.assertIn('SpidyCleanSet(cleanAa ? 1u : 0u);', g)
        self.assertIn('"CLEAN TAA OFF (ENGINE TAA): CLEAN stands down; ENGINE TAA runs "', g)

    def test_no_clean_step_over_engine_taa(self):
        s = src('src/stereo_probe.cpp')
        self.assertIn('!engineTaaLive.load(std::memory_order_relaxed); // 0.0.74 test 7', s)


class Proof(unittest.TestCase):
    def test_probe(self):
        n = src('src/native_eye_gpu.cpp')
        self.assertIn('#include "spidy/jitter_proof.hpp"', n)
        self.assertIn('namespace proof {', n)
        self.assertIn('D3D12_HEAP_TYPE_READBACK', n)
        self.assertIn('TrySubmitThreadpoolCallback', n)
        self.assertIn('proof::step(slot->list.Get(), stamp.serial);', n)
        self.assertIn('proof::submitted(sent, slot->fence);', n)
        h = src('include/spidy/jitter_proof.hpp')
        self.assertIn('inline constexpr uint32_t box = 128;', h)
        self.assertIn('inline constexpr uint32_t everyMs = 2000;', h)
        self.assertIn('inline constexpr float range = 2.f;', h)

    def test_lines(self):
        g = src('src/game_xr.cpp')
        self.assertIn('"CLEAN JITTER PROOF: sent %s (%.2f, %.2f) -> measured (%.2f, %.2f) px "', g)
        self.assertIn('"CLEAN JITTER PROOF: off for this session (%s)"', g)
        self.assertIn('native_gpu::setJitterProof(c38(combat38::jitterProof) == 0);', g)
        self.assertIn('native_gpu::setJitterProofBig(c38(combat38::jitterProofBig) == 1);', g)

    def test_big_shows_raw(self):
        n = src('src/native_eye_gpu.cpp')
        self.assertIn('const bool bigRaw = stepped && proof::big.load(', n)
        self.assertIn('keep > 0 && !bigRaw', n)


class Logs(unittest.TestCase):
    def test_sharpening_and_image(self):
        g = src('src/game_xr.cpp')
        self.assertIn('"HEADSET SHARPENING: now %u%% sent to the headset (was %u%%)"', g)
        self.assertIn('"IMAGE: CLASSIC is held by AA MODE CLEAN; sharpening stays %u%% "', g)
        line = L.image_line('classic', {'aa': 1, 'sharpen': 40, 'history': 80}, 'AA MODE CLEAN')
        self.assertIn('AA MODE CLEAN sends', line)
        self.assertIn('headset sharpening 40%', line)
        self.assertNotIn('live.txt keeps it', line)

    def test_coverage_and_cost(self):
        g = src('src/game_xr.cpp')
        self.assertIn('"TAA COVERAGE 10 s [ENGINE TAA %s]: L TemporalAa ran %.0f%% called %.0f%% of "', g)
        self.assertIn('"ENGINE TAA COST 10 s [ENGINE TAA %s] head %s: game %.1f eye pairs/s, headset "', g)
        self.assertIn('native_gpu::copyTiming(gpuNow, pairNow);', g)


if __name__ == '__main__':
    unittest.main()
