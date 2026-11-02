# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 TAA WATCH: the read-only report every A/B/C test build carries - fps, GPU ms a pair and the still / moving
split every 10 s, and the eye depth log (CONFIRMED / CANDIDATE)."""
import pathlib
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import taa_watch_record as W  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


def window(ms=10000, pairs=450, moving=40, eased=90, still=200, still_eased=10, still_moving=2, taa=450,
           taa_ms=200.0, eye=450, eye_ms=9000.0, depth=0, missed=0, restarted=3):
    return struct.pack(W.WINDOW, ms, pairs, moving, eased, still, still_eased, still_moving, taa, taa_ms, eye, eye_ms,
                       depth, missed, restarted)


def record(closed=2, build=0, depth=(200, 199, 19, 2688, 2880, 1, 1, 0, 0, 3, 2, 6952, 0x1000, 0x2000),
           windows=None):
    raw = struct.pack(W.HEAD, W.MAGIC, 1, W.SIZE, 10000, closed, build) + struct.pack(W.DEPTH, *depth)
    raw += window(ms=0, pairs=10)
    ring = windows or [window() for _ in range(closed)]
    ring = ring + [bytes(struct.calcsize(W.WINDOW))] * (W.WINDOWS - len(ring))
    return raw + b''.join(ring)


class Record(unittest.TestCase):
    def test_size_matches_the_header(self):
        self.assertEqual(W.SIZE, 5184)
        h = src('include/spidy/taa_watch.hpp')
        self.assertIn('static_assert(sizeof(Data) == 5184)', h)
        self.assertIn('uint32_t bytes = 5184', h)

    def test_parse_and_report(self):
        r = W.taa_watch_parse(record())
        self.assertEqual(r['build'], 'shared')
        self.assertEqual(r['depth']['verdict'], 'CONFIRMED')
        self.assertEqual(r['depth']['format_name'], 'R32G8X24_TYPELESS')
        s = W.taa_watch_report(r)
        w = s['windows'][0]
        self.assertEqual(w['fps'], 45.0)
        self.assertAlmostEqual(w['eye_gpu_ms_per_pair'], 20.0)
        self.assertAlmostEqual(w['taa_gpu_ms_per_pair'], .444, places=3)
        self.assertEqual(w['still_head_classed_moving_pct'], 5.0)
        self.assertEqual(s['windows'][1]['ends_s'], 20.0)
        self.assertFalse(s['thresholds_wrong'])
        self.assertEqual(s['totals']['pairs'], 900)  # the closed windows only

    def test_thresholds_wrong_over_20_percent(self):
        r = W.taa_watch_parse(record(windows=[window(still=100, still_eased=30)] * 2))
        s = W.taa_watch_report(r)
        self.assertTrue(s['thresholds_wrong'])
        self.assertTrue(any('OVER 20%' in line for line in W.taa_watch_lines(s)))

    def test_lines_and_bad_records(self):
        s = W.taa_watch_report(W.taa_watch_parse(record()))
        lines = W.taa_watch_lines(s)
        self.assertTrue(lines[0].startswith('TAA WATCH eye depth: CONFIRMED'))
        self.assertIn('fps', lines[1])
        self.assertEqual(len(W.taa_watch_window_lines(s)), 2)
        self.assertIsNone(W.taa_watch_parse(b'\0' * W.SIZE))
        self.assertIsNone(W.taa_watch_parse(record()[:-1]))
        self.assertIsNone(W.taa_watch_snapshot(None, 0))
        self.assertIsNone(W.taa_watch_report(None))
        self.assertEqual(W.taa_watch_lines(None), [])

    def test_no_windows_yet(self):
        s = W.taa_watch_report(W.taa_watch_parse(record(closed=0)))
        self.assertEqual(s['windows'], [])
        self.assertIsNone(s['totals']['fps'])

    def test_wired_in(self):
        n = src('src/native_eye_gpu.cpp')
        self.assertIn('__declspec(dllexport) taa_watch::Data SpidyTaaWatchData;', n)
        self.assertIn('watchDepth(t.eye, t.stamp.serial, res, d)', n)
        self.assertIn('resolveEyeTime(slot->list.Get(), index, stamp.serial)', n)
        r = src('tools/run_game_vr.py')
        self.assertIn('taa_watch_record.EXPORT))', r)
        self.assertIn('taa_watch=taa_watch_record.taa_watch_report(taa_watch_last)', r)

    def test_read_only_and_no_forbidden_changes(self):
        h = src('include/spidy/taa_watch.hpp')
        self.assertIn('Nothing here changes', h)
        a = src('include/spidy/vr_aa.hpp')
        self.assertRegex(a, r'keepMoving = \.6f')


if __name__ == '__main__':
    unittest.main()
