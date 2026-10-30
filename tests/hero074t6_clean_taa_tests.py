"""0.0.74 test 6 CLEAN JITTER (src/stereo_probe.cpp, src/native_eye_gpu.cpp, src/game_xr.cpp): CLEAN's own temporal
AA. A +-0.5 pixel Halton step per eye lens, noted by serial, read back by the eye history pass, FXAA off, CAS after
the history, the 10 s CLEAN TAA line; OFF is test 5. Made by Agent Mulder."""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402


def src(rel):
    return (ROOT / rel).read_text(encoding='utf-8')


class Switch(unittest.TestCase):
    def test_clean_jitter(self):
        self.assertEqual(L.UPDATE38_DEFAULTS['clean_jitter'], 'on')
        self.assertEqual(L.CHOICES['clean_jitter']['on'], 0)
        self.assertEqual(L.CHOICES['clean_jitter']['off'], 1)
        self.assertEqual(L.SETTINGS['clean_jitter'][1], 'SpidyCleanJitterOn')
        self.assertIn('clean_jitter', L.UPDATE38_KEYS)
        self.assertIn("('clean_jitter', ('ON', 'OFF'))", src('tools/update37_records.py'))
        g = src('src/game_xr.cpp')
        self.assertIn('setHero007(v, combat38::cleanJitter, 5519)', g)
        self.assertIn('native_gpu::setCleanJitter(cleanAa && c38(combat38::cleanJitter) == 0);', g)

    def test_subtext(self):
        m = re.search(r'\{"clean_jitter", "CLEAN JITTER", "CLJT", "([^"]+)", 2, \{"ON", "OFF"',
                      src('include/spidy/combat38.hpp'))
        self.assertIsNotNone(m)
        self.assertLessEqual(len(m.group(1)), 80)
        self.assertEqual(m.group(1).count('.'), 1)
        self.assertIn('restart', m.group(1))

    def test_notice_test_6(self):
        self.assertIn('set(HERO_TEST_NUMBER "7" CACHE', src('CMakeLists.txt'))  # 0.0.74 test 7


class Lens(unittest.TestCase):
    def test_half_pixel_step_noted(self):
        s = src('src/stereo_probe.cpp')
        block = s[s.index('std::array<float, 2> cleanTaaJitter(unsigned index) {'):]
        block = block[:block.index('\n}\n')]
        # 0.0.74 test 7: scale is 1 unless JITTER PROOF BIG (OFF by default) is on
        self.assertIn('const float px = (halton(phase, 2) - .5f) * scale, py = (halton(phase, 3) - .5f) * scale;', block)
        self.assertIn('native_gpu::jitterProofBig() ? jitter_proof::bigScale : 1.f', block)
        self.assertIn('const std::array<float, 2> j{px / render[0], py / render[1]};', block)
        self.assertNotIn('2 * ', block)
        self.assertIn('native_gpu::noteEyeJitter(latchedSerial.load(std::memory_order_relaxed), index', block)

    def test_off_is_test_5(self):
        s = src('src/stereo_probe.cpp')
        self.assertIn('const auto jitter = cleanTaaStep ? cleanTaaJitter(index) : cleanSent(eyeJitter());', s)
        self.assertIn('            if (!cleanTaaStep)\n                cleanStored(descriptor);', s)
        self.assertIn('!milesSteps() && native_gpu::cleanTaa()', s)


class History(unittest.TestCase):
    def test_shader_reads_the_step_back(self):
        n = src('src/native_eye_gpu.cpp')
        self.assertIn('float4 extra; float4 jitter; };', n)
        self.assertIn('current.SampleLevel(bilinear, position.xy * extra.yz + jitter.xy, 0)', n)
        self.assertIn('constexpr UINT historyConstants = 20;', n)
        self.assertIn('Num32BitValues = historyConstants;', n)
        self.assertIn('SetGraphicsRoot32BitConstants(1, historyConstants, frame[i], 0);', n)
        self.assertNotIn('SetGraphicsRoot32BitConstants(1, 16, frame[i], 0);', n)
        self.assertIn('float frame[2][historyConstants]{};', n)

    def test_no_fxaa_cas_after(self):
        n = src('src/native_eye_gpu.cpp')
        self.assertIn("(aaMode.load() == 1 && !cleanImage.load() && !taa)", n)
        self.assertIn('sharpened && percent && historyRunning.load()', n)
        self.assertIn('static_assert', src('include/spidy/native_eye_gpu.hpp'))

    def test_console_line(self):
        g = src('src/game_xr.cpp')
        self.assertIn('"CLEAN TAA %s, 10 s: ', g)
        self.assertIn('after history %llu', g)
        self.assertIn('uint64_t SpidyCleanTaaLenses();', g)
        self.assertIn('extern "C" uint64_t SpidyCleanTaaLenses()', src('src/stereo_probe.cpp'))


if __name__ == '__main__':
    unittest.main()
