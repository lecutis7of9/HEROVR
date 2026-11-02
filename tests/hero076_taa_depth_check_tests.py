# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 A-depth CHECKED (include/spidy/taa_depth_check.hpp, tools/taa_depth_check_record.py).

live.txt vr_taa_depth (still off by default): the depth is used only once its convention check has passed; an eye
pair missing either eye's depth is turned by head motion for both eyes; a pixel with no usable depth falls back to
head motion (kept, not rejected); every 10 s the rejection split clip / depth / both per still, moving, fast swing.
"""
import pathlib
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402
import taa_depth_check_record as R  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


def record(state=1, reason=0, best=0, closed=1, trip=(0.0001, 0.0002)):
    head = struct.pack(R.HEAD, R.MAGIC, 1, R.SIZE, 10000, closed, 0)
    check = struct.pack(R.CHECK, 3, 2, 0, 1, state, best, reason, 120, .9, .5, .05, .02, .01, trip[0], trip[1],
                        .01, .01, 2.0, 2.0, 0, 0, .02, 2000.0, 1, 0, best, 77, 0)
    counts = b''
    for p in R.PATHS:
        for c in R.CATEGORIES:
            if (p, c) == ('depth', 'fast'):
                counts += struct.pack(R.COUNTS, 10, 1000, 800, 200, 100, 50, 40, 760)
            else:
                counts += struct.pack(R.COUNTS, 0, 0, 0, 0, 0, 0, 0, 0)
    window = struct.pack(R.WINDOW_HEAD, 10000, 450, 450, 440, 430, 400, 30, 10, 10, 400, 40.0, 400, 240.0, 40,
                         20.0, 10, 10, .0002, .0001) + counts + b'\0' * 4
    empty = b'\0' * R.WINDOW_SIZE
    return head + check + empty + window + empty * (R.WINDOWS - 1)


class DepthCheck(unittest.TestCase):
    def test_parse_and_lines(self):
        raw = record()
        self.assertEqual(len(raw), R.SIZE)
        section = R.taa_depth_check_report(R.taa_depth_check_parse(raw))
        row = section['windows'][0]
        self.assertEqual(row['fps'], 45.0)
        self.assertEqual(row['found_pct'], [97.8, 95.6])
        fast = row['split']['depth']['fast']
        self.assertEqual((fast['clip_pct'], fast['depth_pct'], fast['both_pct']), (25.0, 12.5, 6.2))
        self.assertEqual(fast['fallback_pct'], 4.0)
        # the depth pass alone: the copies (0.1) plus the draw with depth (0.6) less without (0.5)
        self.assertAlmostEqual(row['depth_pass_ms_per_pair'], 0.2, places=3)
        line = R.taa_depth_check_window_lines(section)[0]
        for words in ('depth found L 97.8% R 95.6%', 'round trip 0.0001 px (max 0.0002)', 'fast clip 25.0% depth 12.5% both 6.2%',
                      'fallback 4.0%', '45.0 fps', 'depth pass 0.200 ms GPU a pair'):
            self.assertIn(words, line)
        said = R.check_line(section)
        self.assertIn('TAA DEPTH CHECK PASSED', said)
        self.assertIn('reversed-Z, D3D 0..1, reciprocal, finite far', said)
        self.assertIn('fast swing = head travel >= 6 m/s or turn >= 120 deg/s', said)

    def test_failed_says_the_likely_error(self):
        section = R.taa_depth_check_report(R.taa_depth_check_parse(record(state=2, reason=5, best=3)))
        said = R.check_line(section)
        self.assertIn('FAILED', said)
        self.assertIn('Depth path NOT enabled: likely convention error - the depth reads as standard Z, GL -1..1', said)
        section = R.taa_depth_check_report(R.taa_depth_check_parse(record(state=2, reason=1, trip=(.3, .3))))
        self.assertIn('round trip over 0.05 px', R.check_line(section))

    def test_wrong_bytes(self):
        self.assertIsNone(R.taa_depth_check_parse(b'\0' * 10))
        self.assertIsNone(R.taa_depth_check_parse(b'\0' * R.SIZE))

    def test_native_gating(self):
        n = src('src/native_eye_gpu.cpp')
        self.assertIn('__declspec(dllexport) taa_depth_check::Data SpidyTaaDepthCheckData;', n)
        self.assertIn('std::atomic<uint32_t> depthCheckState{taa_depth_check::waiting};', n)
        # both eyes' depth (this pair's and the shown one's) or neither eye uses it; only after the check passed
        self.assertIn('bool ready = history.valid && history.shownSerial && depthFound[0] && depthFound[1];', n)
        self.assertIn('else if (checkState != taa_depth_check::passed || inForce >= taa_depth_check::conventions)', n)
        # invalid depth: the head-motion place for that pixel (fallback), counted, not rejected
        self.assertIn('const float3 turned = q;', n)
        self.assertIn('if (d > 0 && d < 1) { // false for NaN', n)
        self.assertIn('counts.InterlockedAdd(20, 1);', n)
        self.assertIn('parameters[4].Constants = {1, 0, 12};', n)
        self.assertIn('history.counts->GetGPUVirtualAddress() + 64 * slot + 32 * i', n)
        self.assertIn('noteDepthCheck(index);', n)
        self.assertIn('A-depth CHECK pair %llu: lens convention D3D 0..1, reversed-Z, reciprocal, finite far', n)

    def test_lens_call_unchanged(self):
        s = src('src/stereo_probe.cpp')
        self.assertIn('''                controlled ? -std::tan(fov[2]) : 1, 0, 0, jitter[0], jitter[1], false, false);
            near_clip::noteEyeLens(nearZ, main.farZ());''', s)
        self.assertEqual(s.count('noteEyeLens('), 2)  # its definition and the one note

    def test_core_unchanged(self):
        a = src('include/spidy/vr_aa.hpp')
        self.assertIn('inline constexpr float keepStill = .9f, keepMoving = .6f;', a)
        n = src('src/native_eye_gpu.cpp')
        self.assertIn('const float3 boxLo = max(lo, mean - extra.w * sigma);', n)
        self.assertIn('const float a = (1 - keep) / (1 + middle.x), b = keep / (1 + before.x);', n)
        self.assertIn('std::atomic<uint32_t> taaDepthOn{taa_depth::off};', n)

    def test_setting(self):
        self.assertIn('only used once its convention check has passed', L.SETTINGS['vr_taa_depth'][2])

    def test_report_wired(self):
        run = src('tools/run_game_vr.py')
        self.assertIn('import taa_depth_check_record', run)
        self.assertIn('taa_depth_check_record.EXPORT,  # 0.0.76 A-depth CHECKED', run)
        self.assertIn('taa_depth_check=taa_depth_check_record.taa_depth_check_report(depth_check_last),', run)

    def test_docs(self):
        for doc in ('CHANGELOG.md', 'README-HERO-VR.md', 'BUILD-SM2.md', 'docs/VISUAL-0.0.76.md'):
            self.assertIn('A-depth CHECKED', src(doc), doc)


if __name__ == '__main__':
    unittest.main()
