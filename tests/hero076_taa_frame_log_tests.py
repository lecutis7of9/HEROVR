# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 TAA FRAME LOG (jitter diagnostic build): SPIDY TAA's history pass, eye 0, the last 300 pairs - both views,
the step on each, the centre pixel's history UV and shift, the drift with the step taken out, the shift's change, a
still flag - and the static line from the code: matrices jittered, jitter applied once, sign +."""
import pathlib
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import taa_frame_log_record as F  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


def entry(serial, flags, now, before, shift, size=(2016, 2240), mode=0):
    expected = (now[0] - before[0], before[1] - now[1]) if flags & F.HISTORY else (0, 0)
    uv = (.5 + shift[0] / size[0], .5 + shift[1] / size[1]) if flags & F.HISTORY else (0, 0)
    drift = (shift[0] - expected[0], shift[1] - expected[1]) if flags & F.HISTORY else (0, 0)
    world = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    return struct.pack(F.ENTRY_FORMAT, serial, flags | mode << 8, .0001, .00002, *now, *before, *uv, *shift,
                       *expected, *drift, 0, 0, *world, -1, 1, -1, 1, *world, -1, 1, -1, 1)


def record(entries, written=None, size=(2016, 2240)):
    written = len(entries) if written is None else written
    raw = bytearray(F.SIZE)
    struct.pack_into('<8Ii3I', raw, 0, F.MAGIC, 1, F.SIZE, F.ENTRY, F.CAPACITY, written, 1, 1, 1, *size, 0)
    for n, e in enumerate(entries):
        raw[F.HEADER + F.ENTRY * (n % F.CAPACITY):F.HEADER + F.ENTRY * (n % F.CAPACITY + 1)] = e
    return bytes(raw)


class FrameLog(unittest.TestCase):
    def test_layout_matches_the_header(self):
        h = src('include/spidy/taa_frame_log.hpp')
        self.assertIn('static_assert(sizeof(Entry) == 232);', h)
        self.assertIn('inline constexpr uint32_t capacity = 300;', h)
        self.assertEqual(struct.calcsize(F.ENTRY_FORMAT), F.ENTRY)
        self.assertEqual(F.SIZE, 48 + 232 * 300)
        self.assertIn('0x474c4654', h)

    def test_static_line(self):
        r = F.taa_frame_log_parse(record([]))
        self.assertEqual(F.static_line(r), 'matrices: jittered, jitter applied: once, sign: +')
        h = src('include/spidy/taa_frame_log.hpp')
        for line in ('matricesJittered = 1', 'appliedTimes = 1', 'appliedSign = 1'):
            self.assertIn(line, h)

    def test_wrong_record_refused(self):
        self.assertIsNone(F.taa_frame_log_parse(b'\0' * F.SIZE))
        self.assertIsNone(F.taa_frame_log_parse(b'\0' * 10))
        self.assertIsNone(F.taa_frame_log_snapshot(None, 0))

    def test_once_counted_drift_and_gain(self):
        steps = [(.3, -.2), (-.25, .17), (.1, .4), (-.4, -.1)]
        entries = [entry(0, 0, steps[0], (0, 0), (0, 0))]
        for n in range(1, 40):
            now, before = steps[n % 4], steps[(n - 1) % 4]
            ex = (now[0] - before[0], before[1] - now[1])
            entries.append(entry(n, F.HISTORY | F.STILL | F.OURS, now, before, (ex[0] + .01, ex[1])))
        s = F.taa_frame_log_summary(F.taa_frame_log_parse(record(entries)))
        self.assertEqual((s['frames'], s['history_frames'], s['still_frames']), (40, 39, 39))
        self.assertAlmostEqual(s['drift_still_mean_px'], .01, places=3)
        self.assertAlmostEqual(s['drift_still_max_px'], .01, places=3)
        self.assertAlmostEqual(s['measured_gain'], 1, places=1)
        line = F.taa_frame_log_line(s)
        self.assertIn('(once)', line)
        self.assertIn('39 head still', line)

    def test_twice_and_wrong_sign_shown(self):
        for times, word in ((2, 'twice'), (-1, 'WRONG SIGN'), (0, 'none')):
            entries = []
            for n in range(20):
                now, before = ((.3, -.2), (-.25, .17)) if n % 2 else ((-.25, .17), (.3, -.2))
                ex = (now[0] - before[0], before[1] - now[1])
                entries.append(entry(n, F.HISTORY | F.STILL, now, before, (times * ex[0], times * ex[1])))
            self.assertIn(f'({word})', F.taa_frame_log_line(F.taa_frame_log_summary(F.taa_frame_log_parse(
                record(entries)))))

    def test_ring_oldest_first_and_best_block(self):
        entries = [entry(n, F.HISTORY | (F.STILL if n >= 300 else 0), (0, 0), (0, 0), (0, 0)) for n in range(350)]
        r = F.taa_frame_log_parse(record(entries))
        self.assertEqual([f['serial'] for f in r['frames']][:2], [50, 51])
        self.assertEqual(r['frames'][-1]['serial'], 349)
        moving = F.taa_frame_log_parse(record(entries[:300]))
        self.assertIs(F.better_block(F.better_block(None, r), moving), r)
        self.assertIs(F.better_block(r, None), r)

    def test_native_logs_both_views_read_only(self):
        s = src('src/native_eye_gpu.cpp')
        self.assertIn('__declspec(dllexport) taa_frame_log::Data SpidyTaaFrameLogData;', s)
        self.assertIn('frameEntry.worldBefore[k] = history.shown[0].world[k];', s)
        self.assertIn('taa_watch::jitterShift(frameEntry.jitterNow, frameEntry.jitterBefore, 1.f, frameEntry.expected);', s)
        self.assertIn('taa_frame_log::note(SpidyTaaFrameLogData, frameEntry);', s)
        run = src('tools/run_game_vr.py')
        self.assertIn('taa_frame_log_record.EXPORT))  # 0.0.76 TAA FRAME LOG', run)
        self.assertIn('taa_frame_log=dict(most_still=', run)

    def test_documented(self):
        doc = src('docs/VISUAL-0.0.76.md')
        self.assertIn('TAA FRAME LOG', doc)
        self.assertIn('matrices: jittered, jitter applied: once, sign: +', doc)
        self.assertIn('TAA FRAME LOG', src('CHANGELOG.md'))


if __name__ == '__main__':
    unittest.main()
