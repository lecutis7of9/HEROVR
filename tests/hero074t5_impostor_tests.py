"""0.0.74 test 5 IMPOSTORS (include/spidy/view_gates.hpp, src/stereo_probe.cpp): the stand-in installed without
createViews 32, the left eye the only yes at the five calls, no +0x88 write, the console lines, IMPOSTOR EYE and the
VGAT record version 2; CLEAN JITTER. Made by Agent Mulder."""
import pathlib
import re
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402
import update37_records as R  # noqa: E402

SITES = (0x2a6af90, 0x2a6c346, 0x2a6c356, 0x2a6c467, 0x2a6c477)


def src(rel):
    return (ROOT / rel).read_text(encoding='utf-8')


class Install(unittest.TestCase):
    def test_no_flag_32_and_every_byte_check(self):
        s = src('src/stereo_probe.cpp')
        self.assertNotIn('(config.createViews & 128) && (config.createViews & 32)', s)
        self.assertIn('uint32_t why = (config.createViews & 128) ? vg::impOptedOut : vg::impFine;', s)
        self.assertIn('why = vg::impCall0 + i;', s)
        self.assertIn('why = vg::impReachFirst;', s)
        self.assertIn('why = vg::impReachLast;', s)
        self.assertIn('impostors = why == vg::impFine;', s)
        self.assertIn('constexpr bool farWindowsCandidateWrite = false;', s)

    def test_five_calls_target_is_active_view(self):
        s = src('src/stereo_probe.cpp')
        for rva in SITES:
            m = re.search(r'\{0x%x, \{0xe8, (0x[0-9a-f]{2}), (0x[0-9a-f]{2}), (0x[0-9a-f]{2}), (0x[0-9a-f]{2})\}\}' % rva, s)
            self.assertIsNotNone(m, hex(rva))
            rel = struct.unpack('<i', bytes(int(b, 16) for b in m.groups()))[0]
            self.assertEqual(rva + 5 + rel, 0x2c991a0, hex(rva))
        h = src('include/spidy/view_gates.hpp')
        self.assertIn('impostorCallRvas[5] = {0x2a6af90, 0x2a6c346, 0x2a6c356, 0x2a6c467, 0x2a6c477}', h)
        self.assertIn('inline constexpr bool impostorAnswer(int who) { return who == 0; }', h)

    def test_hook_without_rested_left_eye_only(self):
        s = src('src/stereo_probe.cpp')
        block = s[s.index('const int site = vg::impostorSite('):s.index('return answer;')]
        self.assertNotIn('rested', block)
        self.assertIn('latchedImmersive', block)
        self.assertIn('vg::impostorAnswer(who)', block)
        self.assertIn('impostorBySite[site].fetch_add', block)
        self.assertNotIn('189daa0 has callers', s)
        self.assertIn('stale, CANDIDATE at best', s)

    def test_console_lines(self):
        g = src('src/game_xr.cpp')
        self.assertIn('"IMPOSTORS: impostors installed = "', g)
        self.assertIn('nowTick - impostorTick >= 10000', g)
        self.assertIn('af90 %llu c346 %llu c356 %llu c467 %llu c477 %llu', g)
        self.assertIn('"CLEAN JITTER: %u eye frames', g)
        self.assertIn('SpidyCleanSet(cleanAa ? 1u : 0u);', g)  # 0.0.74 test 7: cleanAa holds AA MODE CLEAN less ENGINE TAA STOCK's stand-down

    def test_now_bit_13_is_installed(self):
        self.assertIn('(impostors ? 1u : 0u) << 13', src('src/stereo_probe.cpp'))


class Switch(unittest.TestCase):
    def test_impostor_eye(self):
        self.assertEqual(L.UPDATE38_DEFAULTS['impostor_eye'], 'on')
        self.assertEqual(L.CHOICES['impostor_eye']['on'], 0)
        self.assertEqual(L.CHOICES['impostor_eye']['off'], 1)
        self.assertEqual(L.SETTINGS['impostor_eye'][1], 'SpidyImpostorEye')
        self.assertIn('impostor_eye', L.UPDATE38_KEYS)
        self.assertIn("('impostor_eye', ('ON', 'OFF'))", src('tools/update37_records.py'))
        self.assertIn('setHero007(v, combat38::impostorEye, 5518)', src('src/game_xr.cpp'))
        self.assertIn('SpidyViewGate(reinterpret_cast<void*>(static_cast<uintptr_t>(3u << 16 | want)))', src('src/game_xr.cpp'))

    def test_subtext(self):
        m = re.search(r'\{"impostor_eye", "IMPOSTOR EYE", "IMPE", "([^"]+)", 2, \{"ON", "OFF"', src('include/spidy/combat38.hpp'))
        self.assertIsNotNone(m)
        self.assertLessEqual(len(m.group(1)), 80)
        self.assertEqual(m.group(1).count('.'), 1)
        self.assertIn('restart', m.group(1))


class Record(unittest.TestCase):
    def test_vgat_v2(self):
        self.assertEqual(R.RECORDS['view_gates'][2:], (2, 432))
        raw = bytearray(432)
        struct.pack_into('<3I', raw, 0, 0x54414756, 2, 432)
        struct.pack_into('<9I', raw, 12, 0, 0, 0, 1, 150, 1, 1, 0, 0)
        struct.pack_into('<10Q', raw, 128, 50, 20, 0, 0, 10, 10, 10, 10, 10, 0)
        struct.pack_into('<Q', raw, 424, 2)
        v = R.parse_view_gates(bytes(raw))
        self.assertIsNotNone(v)
        self.assertTrue(v['impostors_installed'])
        self.assertEqual(v['impostors_why'], 'installed')
        self.assertEqual(v['impostor_asked'], 50)
        self.assertEqual(v['impostor_yes'], dict(left=20, right=0, game=0))
        self.assertEqual(list(v['impostor_by_site'].values()), [10] * 5)
        self.assertIn('static_assert(sizeof(Data) == 432);', src('include/spidy/view_gates.hpp'))


class Jitter(unittest.TestCase):
    def test_clean_zeroes(self):
        s = src('src/stereo_probe.cpp')
        self.assertIn('const auto jitter = cleanTaaStep ? cleanTaaJitter(index) : cleanSent(eyeJitter());', s)  # 0.0.74 test 6: OFF as test 5
        self.assertIn('cleanStored(descriptor);', s)
        self.assertIn('float& x = lens.values[0x428 / 4];', s)
        self.assertIn('return keep ? j : std::array<float, 2>{0, 0};', s)


if __name__ == '__main__':
    unittest.main()
