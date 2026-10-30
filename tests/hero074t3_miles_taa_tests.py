"""0.0.74 test 3 MILES TAA (include/spidy/miles_taa074.hpp): the option, its launch bit, the wiring and, when the
2.810 exe is at hand (SM2_EXE or /workspace/sm2exe/Spider-Man2.exe), every byte check against it. Made by Agent Mulder."""
import os
import pathlib
import re
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402


def src(rel):
    return (ROOT / rel).read_text(encoding='utf-8')


class Option(unittest.TestCase):
    def test_default_on_switchable_off(self):
        self.assertEqual(L.UPDATE38_DEFAULTS['miles_taa'], 'on')
        self.assertEqual(L.CHOICES['miles_taa']['on'], 0)
        self.assertEqual(L.CHOICES['miles_taa']['off'], 1)
        self.assertTrue(L.miles_taa_of({}))
        self.assertTrue(L.miles_taa_of({'miles_taa': 'on'}))
        self.assertFalse(L.miles_taa_of({'miles_taa': 'off'}))
        self.assertTrue(L.miles_taa_in_file('/nonexistent/live.txt'))
        self.assertEqual(L.SETTINGS['miles_taa'][1], 'SpidyMilesTaa')

    def test_subtext_one_sentence_with_restart(self):
        h = src('include/spidy/combat38.hpp')
        m = re.search(r'\{"miles_taa", "MILES TAA", "MTAA", "([^"]+)", 2, \{"ON", "OFF"', h)
        self.assertIsNotNone(m)
        self.assertLessEqual(len(m.group(1)), 80)
        self.assertEqual(m.group(1).count('.'), 1)
        self.assertIn('restart', m.group(1))

    def test_heap_guard_still_off(self):
        self.assertEqual(L.UPDATE38_DEFAULTS['heap_guard'], 'off')
        self.assertIn("('heap_guard', ('OFF', 'ON'))", src('tools/update37_records.py'))
        self.assertIn('std::atomic<bool> heapGuardWanted{false};', src('src/native_eye_gpu.cpp'))


class Wiring(unittest.TestCase):
    def test_launch_bit(self):
        r = src('tools/run_game_vr.py')
        self.assertIn('MILES_TAA_OPTION = 0x1000000', r)
        self.assertIn("miles_taa = live_settings.miles_taa_in_file(ROOT/'live.txt')", r)
        g = src('src/game_xr.cpp')
        self.assertIn('constexpr uint32_t knownOptions = 0x1ffffff;', g)
        self.assertIn('(config.options & miles_taa::startOption) ? miles_taa::eyeTemporalBit << 8', g)
        h = src('include/spidy/miles_taa074.hpp')
        self.assertIn('startOption = 0x1000000', h)

    def test_game_xr(self):
        g = src('src/game_xr.cpp')
        self.assertIn('SpidyMilesTaa(void* v) { return setHero007(v, combat38::milesTaa, 5513); }', g)
        self.assertIn('milesStall.frame(md->leftJobs, newPair, milesActive)', g)
        self.assertIn('SpidyMilesTaaStall();', g)
        self.assertIn('native_gpu::setCleanImage(milesActive || picture', g)
        self.assertIn('if (!picture && !milesActive && step.eyeAaOn', g)

    def test_stereo_probe_steps(self):
        p = src('src/stereo_probe.cpp')
        self.assertIn('spidy::miles_taa::jitter(frameIndex, config.width, render[0], render[1])', p)  # step 1
        self.assertIn('engineTaaBuffers(buffers, local, name)', p)  # step 2
        self.assertIn('if (engineTaaLive.load() || miles) {', p)  # step 3
        self.assertIn('((eyeTemporal.load() & 4) || miles)', p)  # step 4
        self.assertIn('milesFallBack(mt::exeDiffers)', p)
        self.assertIn('milesFallBack(spidy::miles_taa::hookFailed)', p)
        self.assertIn('temporalAaHook}', p)  # disabled at the stop


def header_bytes(name):
    h = src('include/spidy/miles_taa074.hpp')
    m = re.search(name + r'\[\]\s*=\s*\{([^}]*)\}', h)
    return bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', m.group(1)))


def header_value(name):
    return int(re.search(name + r'\s*=\s*(0x[0-9a-fA-F]+)', src('include/spidy/miles_taa074.hpp')).group(1), 16)


class Bytes(unittest.TestCase):
    def test_against_the_exe(self):
        exe = pathlib.Path(os.environ.get('SM2_EXE', '/workspace/sm2exe/Spider-Man2.exe'))
        if not exe.exists():
            self.skipTest('no Spider-Man2.exe 2.810 here')
        data = exe.read_bytes()
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        count = struct.unpack_from('<H', data, pe + 6)[0]
        opt = struct.unpack_from('<H', data, pe + 20)[0]
        sections = [struct.unpack_from('<IIII', data, pe + 24 + opt + 40 * i + 8) for i in range(count)]

        def at(rva, n):
            for size, va, raw_size, raw in sections:
                if va <= rva < va + max(size, raw_size):
                    return data[raw + rva - va: raw + rva - va + n]
            return b''
        for rva, arr in (('temporalAaRva', 'temporalAaBytes'), ('temporalAaCheckRva', 'temporalAaCheckBytes'),
                         ('currentTargetRva', 'currentTargetBytes'), ('historyTargetRva', 'historyTargetBytes'),
                         ('queueSiteRva', 'queueSiteBytes')):
            b = header_bytes(arr)
            self.assertLessEqual(len(b), 32)
            self.assertEqual(at(header_value(rva), len(b)), b, rva)


if __name__ == '__main__':
    unittest.main()
