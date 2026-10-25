# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 33/34: SHADOW DISTANCE's record (tools/shadow_distance.py) against the native layout."""
import pathlib
import re
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import shadow_distance as sd  # noqa: E402

HEADER = ROOT / 'include/spidy/shadow_distance.hpp'


def raw(state=2, reason=0, game=56.2, live=56.2, want=0, candidates=(), events=(), seq=2, active=0xffffffff,
        trials=(), env=()):
    b = bytearray(sd.SIZE)
    struct.pack_into('<4I', b, 0, sd.MAGIC, sd.VERSION, sd.SIZE, state | reason << 8)
    struct.pack_into('<q', b, 16, seq)
    struct.pack_into('<4f', b, 24, game, live, want, 1.0)
    struct.pack_into('<4I', b, 40, 3, len(candidates), len(candidates), 1)
    struct.pack_into('<4Q', b, 56, 64 << 20, 10, 40, 900)
    struct.pack_into('<3f', b, 88, 24.6, 28.9, 56.2)
    struct.pack_into('<3f', b, 120, 131, 154, 300)
    struct.pack_into('<I', b, 152, 0x2abe8dd)
    struct.pack_into('<2I', b, 168, active, len(events))
    for i, (addr, rva, origin, form, target, outcome, value) in enumerate(candidates):
        o = sd.CANDIDATES_AT + 32 * i
        struct.pack_into('<QI4B2H3f', b, o, addr, rva, origin, form, target, outcome, 0, 3, *value)
    for i, (ms, kind, code, a, bb) in enumerate(events):
        struct.pack_into('<Q2I2f', b, sd.EVENTS_AT + (i % sd.MAX_EVENTS) * 24, ms, kind, code, a, bb)
    struct.pack_into('<2I', b, sd.TRIALS_AT - 8, len(trials), len(env))
    for i, (cand, scale, matched, verdict, before, after) in enumerate(trials):
        o = sd.TRIALS_AT + (i % sd.MAX_TRIALS) * 80
        struct.pack_into('<I2f4B', b, o, cand, scale, matched, verdict, len(before), len(after), 0)
        struct.pack_into(f'<{len(before)}f', b, o + 16, *before)
        struct.pack_into(f'<{len(after)}f', b, o + 48, *after)
    for i, (addr, reg, total, n, far) in enumerate(env):
        struct.pack_into('<Q4f2B', b, sd.ENV_AT + 32 * i, addr, reg, total, 0, 500, n, far)
    return bytes(b)


class Record(unittest.TestCase):
    def test_layout_matches_native(self):
        h = HEADER.read_text()
        self.assertIn('offsetof(Data, candidate) == 176 && offsetof(Data, event) == 688 && offsetof(Data, trial) == 1464', h)
        self.assertIn('offsetof(Data, env) == 2104 && sizeof(Data) == 2360', h)
        self.assertEqual((sd.CANDIDATES_AT, sd.EVENTS_AT, sd.TRIALS_AT, sd.ENV_AT, sd.SIZE), (176, 688, 1464, 2104, 2360))
        self.assertIn(f'maxTrials = {sd.MAX_TRIALS}, maxEnv = {sd.MAX_ENV}', h)
        self.assertIn('sizeof(Trial) == 80', h)
        self.assertIn('sizeof(Env) == 32', h)
        self.assertIn('0x54534453', h)
        self.assertIn(f'maxCandidates = {sd.MAX_CANDIDATES}, maxEvents = {sd.MAX_EVENTS}', h)
        self.assertIn(f'maxSeen = {sd.MAX_SEEN}', h)
        self.assertIn('sizeof(Candidate) == 32', h)
        self.assertIn('sizeof(Event) == 24', h)
        self.assertIn('dataVersion = 2', h)

    def test_reasons_match_native(self):
        h = HEADER.read_text()
        native = re.findall(r'case \w+: return "([^"]+)";', h.split('inline const char* reasonText')[1].split('}')[0])
        self.assertEqual(native, list(sd.REASONS[1:]))

    def test_parse_and_lines(self):
        d = sd.parse(raw(state=4, live=450, want=100, active=0,
                         candidates=[(0x7ff600001000, 0x5a1000, 1, 0, 1, 1, (112.4, 0, 0)),
                                     (0x2000, 0xffffffff, 2, 2, 0xff, 2, (10, 11.7, 22.8))],
                         events=[(1000, 1, 2, 0, 0), (2000, 3, 100, 8, 450)]))
        self.assertEqual(d['state'], 'applied')
        self.assertEqual(d['live_reach_m'], 450)
        c0, c1 = d['candidates']
        self.assertEqual((c0['label'], c0['rva'], c0['matched'], c0['in_use']),
                         ('confirmed this session', '0x5a1000', 'width', True))
        self.assertEqual((c1['label'], c1['rva'], c1['matched'], len(c1['value'])), ('candidate', None, 'proportions', 3))
        self.assertEqual([e['kind'] for e in d['log']], ['state', 'apply'])
        self.assertIn('now 450', sd.console_line(d))
        self.assertIn('100 cars', sd.console_line(d))
        self.assertEqual(sd.report_fields(d)['shadow_distance'], d)

    def test_not_started_torn_and_mismatch(self):
        self.assertIsNone(sd.parse(bytes(sd.SIZE)))
        self.assertIsNone(sd.parse(raw(seq=3)))
        self.assertIsNone(sd.parse(b'short'))
        bad = bytearray(raw())
        struct.pack_into('<I', bad, 4, 9)
        with self.assertRaises(RuntimeError):
            sd.parse(bytes(bad))
        self.assertEqual(sd.report_fields(None)['shadow_distance']['state'], 'not started')

    def test_trials_and_env_lighting(self):
        d = sd.parse(raw(state=2, trials=[(0, 1.5, 24.09, 1, (24.09, 28.9, 56.2), (24.1, 36.1, 56.2)),
                                          (0xffffffff, 2.5, 0, 0, (24.6,), (61.5,))],
                         env=[(0x1d0000000, 60.0, 250.0, 3, 1)]))
        t0, t1 = d['trial_log']
        self.assertEqual((t0['candidate'], t0['verdict'], t0['against'], t0['after_m']),
                         (0, 'no change', 'box 24.09 m', [24.1, 36.1, 56.2]))
        self.assertEqual((t1['candidate'], t1['verdict'], t1['against']), (None, 'followed', 'all boxes'))
        e = d['env_lighting'][0]
        self.assertEqual((e['reg_lod_range'], e['total_shadow_range'], e['reg_lod_count'], e['label']),
                         (60.0, 250.0, 3, 'candidate'))
        self.assertIn('1 EnvLightingDef', sd.console_line(d))

    def test_reverted_says_why(self):
        d = sd.parse(raw(state=6, reason=3))
        self.assertEqual(d['reason'], 'the shadow boxes went out of range')
        self.assertTrue(sd.console_line(d).startswith('SHADOW DISTANCE: PUT BACK'))

    def test_wired_into_the_session(self):
        text = (ROOT / 'tools/run_game_vr.py').read_text()
        self.assertIn("'SpidyShadowDistanceData'", text)
        self.assertEqual(text.count('shadow_distance.report_fields('), 2)
        self.assertIn('SpidyShadowDistanceData', (ROOT / 'src/native_shadow_distance.cpp').read_text())


if __name__ == '__main__':
    unittest.main()
