# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 A-depth TRACE and SFX DIAG: the records' layouts match their headers, the parsers read them, the live.txt
switches, vr_ss 110, and the trace kept out of the existing TAA variants."""
import pathlib
import re
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests'))
import live_settings as L  # noqa: E402
import sfx_diag_record as S  # noqa: E402
import taa_depth_trace_record as T  # noqa: E402
from hero076_spidy_taa_tests import args, run_game_vr  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Game:
    def __init__(self, raw):
        self.raw = raw

    def read(self, address, size):
        return self.raw[:size]


def trace_window(**kw):
    w = dict(ms=10000, pairs=450, wanted=400, used=300, aa=0, depth=1, eye=(2064, 2096), out=(2064, 2096), ss=100,
             now=(380, 360), before=(370, 350), masks=(20, 30, 10, 340), trip=(400, 2.0, 0.01),
             frames=(0, 10, 300, 120, 15, 5), frame_sum=10000.0, frame_max=80.0, hitches=5)
    w.update(kw)
    head = struct.pack(T.WINDOW_HEAD, w['ms'], w['pairs'], w['wanted'], w['used'], w['aa'], w['depth'], *w['eye'],
                       *w['out'], w['ss'], 0, *w['now'], *w['before'], *w['masks'], w['trip'][0], w['trip'][1],
                       w['trip'][2], *w['frames'], w['frame_sum'], w['frame_max'], w['hitches'], 16, 2, 16)
    ages = struct.pack(T.AGES, 10, 300, 60, 200, 40, 1, 10, 0.01, 0.05) * 3
    g = struct.pack(T.GRAZE, 100, 90, 30, 0, 2, 5, 900, 4.5, 0.2, 0.05, 20, 18, 9, 0, 1, 2.0, 6, 5, 8, 6, 1.8, 36, 9)
    return head + ages + g * 18


def trace_raw(closed=2, hitches=1):
    head = struct.pack(T.HEAD, T.MAGIC, 1, T.SIZE, 10000, closed, hitches, 0, 0)
    ring = [trace_window() for _ in range(T.WINDOWS)]
    hitch = struct.pack(T.HITCH, 123456789, 80.0, 77) * T.HITCHES
    return head + trace_window() + b''.join(ring) + hitch


class Trace(unittest.TestCase):
    def test_sizes_match_the_header(self):
        h = src('include/spidy/taa_depth_trace.hpp')
        self.assertIn('static_assert(sizeof(Window) == 4 * 35 + 3 * 36 + 18 * 92); // 1904', h)
        self.assertIn('static_assert(sizeof(Graze) == 92);', h)
        self.assertIn('grazeBytesPerEye = 1024', h)
        self.assertEqual(T.WINDOW_SIZE, 1904)
        self.assertEqual(T.SIZE, 32 + 1904 * 91 + 16 * 64)
        self.assertEqual(len(T.GRAZE_KEYS), 23)
        self.assertIn('gEdgeAgeMax = 22,\n    grazeWords = 23', h)

    def test_parse_and_lines(self):
        raw = trace_raw()
        self.assertEqual(len(raw), T.SIZE)
        rec = T.taa_depth_trace_snapshot(Game(raw), 1)
        self.assertEqual(rec['closed'], 2)
        self.assertEqual(len(rec['windows']), 2)
        self.assertEqual(rec['hitches'][0]['serial'], 77)
        report = T.taa_depth_trace_report(rec)
        r = report['windows'][0]
        self.assertEqual(r['vr_aa'], 'ours')
        self.assertEqual(r['vr_taa_depth'], 'on')
        self.assertEqual(r['depth_used_pct'], 75.0)
        self.assertEqual(r['head_fallback_pct'], 25.0)
        self.assertEqual(r['split_pct'], 10.0)
        self.assertEqual(r['history_age']['still']['match_mean'], 30.0)
        g = r['graze']['all']['grazing']
        self.assertEqual(g['edge_share_pct'], 20.0)
        self.assertEqual(g['edge_clip_only_pct'], 50.0)
        self.assertEqual(g['spike_clipped_pct'], 83.3)
        lines = T.window_lines(r)
        self.assertEqual(len(lines), 4)
        self.assertIn('[vr_aa ours, vr_taa_depth on, vr_ss 100%', lines[0])
        self.assertIn('TAA TRACE EDGES', lines[3])
        self.assertEqual(g['edge_shown_vs_input'], 0.9)
        self.assertEqual((g['edge_age_mean'], g['edge_age_max']), (2.0, 9))
        self.assertIn('RENDER HITCH at qpc 123.457 s', T.hitch_line(rec['hitches'][0], 200.0, 1e9))

    def test_refuses_other_bytes(self):
        self.assertIsNone(T.taa_depth_trace_snapshot(Game(b'\0' * T.SIZE), 1))
        self.assertIsNone(T.taa_depth_trace_snapshot(Game(trace_raw()[:-1]), 1))
        self.assertIsNone(T.taa_depth_trace_snapshot(Game(trace_raw()), 0))

    def test_existing_variants_untouched(self):
        """The trace's shader code is inside TAA_AGE only; its own pipeline (taaDepthAge) only with the depth on."""
        c = src('src/native_eye_gpu.cpp')
        self.assertIn('{"TAA_DEPTH", "1"}, {"TAA_COUNT", "1"}, {"TAA_AGE", "1"}', c)
        self.assertIn('bool lumaSpike(int2 p, int2 last, float y)', c)
        self.assertIn('const bool tracing = counting && depthUsed && prepareTaaAge(', c)
        shader = c.split('const char* taaShader = R"(')[1].split(')";')[0]
        for name in ('lumaSpike(', 'grazeBin(', 'noteGraze(', 'noteShown('):
            for m in re.finditer(re.escape(name), shader):
                before = shader[:m.start()]
                self.assertGreater(before.count('#ifdef TAA_AGE'), 0, name)


def sfx_raw(closed=1, noted=2):
    head = struct.pack(S.HEAD, S.MAGIC, 1, S.SIZE, 10000, closed, noted, 1, 0)
    fmt = struct.pack(S.FORMAT, 1, 48000, 2, 3, 0, 48000, 2, 32, 44100, 48000, 1, 2, 16, 12, 4, 9,
                      'Headphones (Virtual Desktop Audio)'.encode())
    win = struct.pack(S.WINDOW, 10000, 40, 40, 2, 3, 1, 0, 0, 1000, 10000000, 20000, 100, 50000, 3000, 1, 2)
    moment = struct.pack(S.MOMENT, 5000000, 1, 7)
    return head + fmt + win * (S.WINDOWS + 1) + moment * S.MOMENTS


class Sfx(unittest.TestCase):
    def test_size_matches_the_header(self):
        self.assertIn('static_assert(sizeof(Data) == 32 + 128 + 64 * 91 + 16 * 64);', src('include/spidy/sfx_diag.hpp'))
        self.assertEqual(S.SIZE, 32 + 128 + 64 * 91 + 16 * 64)

    def test_parse_and_lines(self):
        rec = S.sfx_diag_snapshot(Game(sfx_raw()), 1)
        self.assertTrue(rec['layer_on'])
        self.assertIn("'Headphones (Virtual Desktop Audio)' mix 48000 Hz 2 ch 32-bit", S.format_line(rec))
        r = S.sfx_diag_report(rec)['windows'][0]
        self.assertEqual(r['pass_gap_ms_mean'], 10.0)
        self.assertEqual(r['wake_late_ms_max'], 3.0)
        self.assertIn('layer ON', S.window_line(r))
        self.assertIn('XAudio2 glitch', S.moment_line(rec['moments'][-1], 10.0, 1e9))
        self.assertIsNone(S.sfx_diag_snapshot(Game(sfx_raw()[:-1]), 1))

    def test_no_disk_logging_in_callbacks(self):
        c = src('src/sfx_win.cpp')
        for word in ('fopen', 'fwrite', 'CreateFile', 'ofstream'):
            self.assertNotIn(word, c)


class Switches(unittest.TestCase):
    def test_vr_sfx_layer(self):
        export, text = L.SETTINGS['vr_sfx_layer'][1:3]
        self.assertEqual(export, 'SpidySfxLayer')
        self.assertIn('no restart needed', text)
        self.assertIn('0 on, the default', text)
        self.assertIn('SpidySfxLayer', src('src/game_xr.cpp'))
        self.assertIn('void setLayer(bool) {}', src('src/sfx_quiet.cpp'))

    def test_vr_ss_110(self):
        self.assertEqual(L.vr_supersample('vr_ss = 110\n'), 110)
        self.assertEqual(L.vr_supersample('vr_ss = 3\n'), 110)
        self.assertEqual(L.vr_supersample(None), 100)
        self.assertIn('1.21x', L.SETTINGS['vr_ss'][2])
        self.assertIn('RESTART NOTICE', L.SETTINGS['vr_ss'][2])

    def test_vr_ss_kept_with_ours_only(self):
        r = run_game_vr()
        a = args(game_aa_taa=True)
        r.apply_sm1style(a, 'vr_ss = 110\n')
        self.assertEqual(a.supersample, 110)
        a = args(game_aa_taa=True)
        r.apply_sm1style(a, 'vr_aa = smaa\nvr_ss = 125\n')
        self.assertEqual(a.supersample, 100)
        a = args(game_aa_taa=True)
        r.apply_sm1style(a, '')
        self.assertEqual(a.supersample, 100)
        h = src('include/spidy/vr_aa.hpp')
        self.assertIn('return mode == ours && (percent == 110 || percent == 125 || percent == 150);', h)
        self.assertIn('spidy::vr_aa::supersampleKept(vrAa, ss)', src('src/game_xr.cpp'))

    def test_wired(self):
        run = src('tools/run_game_vr.py')
        for name in ('taa_depth_trace_record.EXPORT,', 'sfx_diag_record.EXPORT))',
                     'taa_depth_trace=taa_depth_trace_record.taa_depth_trace_report(trace_last),',
                     'sfx_diag=sfx_diag_record.sfx_diag_report(sfx_last),'):
            self.assertIn(name, run)


if __name__ == '__main__':
    unittest.main()
