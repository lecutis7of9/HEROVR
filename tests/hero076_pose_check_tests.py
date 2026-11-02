# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 POSE CHECK: each eye's rendered camera against the pose the headset is given for it (far shake).

Read-only telemetry, no setting: SpidyPoseCheckData (include/spidy/pose_check.hpp), read by
tools/pose_check_record.py into the run report's `pose_check` and a console line.
"""
import pathlib
import re
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import pose_check_record as P  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Fake:
    def __init__(self, raw):
        self.raw = raw

    def read(self, address, size):
        return self.raw[:size]


def record(left=(10, 0, 0, 0, .01, .02, .3, .9, .1, 3.0), right=(10, 1, 2, 1, .2, .5, 2.5, 4.0, 2.0, 25.0),
           submits=20, reused=10, turn=(.01, .3, .5, 3), travel=1500., age=(11.1, 22.2, 300.), command=(0, .4, 0, 3.)):
    raw = bytearray(256)
    struct.pack_into('<4I', raw, 0, P.MAGIC, 1, 256, 0)
    for i, eye in enumerate((left, right)):
        struct.pack_into('<4Q4f2d', raw, 16 + 64 * i, *eye)
    struct.pack_into('<2Q2fdQ', raw, 144, submits, reused, turn[0], turn[1], turn[2], turn[3])
    struct.pack_into('<4fd', raw, 184, travel, age[0], age[1], 0, age[2])
    struct.pack_into('<Q4f', raw, 208, 7, *command)
    return bytes(raw)


class PoseCheck(unittest.TestCase):
    def test_layout_matches_the_header(self):
        h = src('include/spidy/pose_check.hpp')
        self.assertIn('magic = 0x4b434850', h)
        self.assertIn('static_assert(sizeof(Data) == 256', h)
        self.assertIn('static_assert(sizeof(Eye) == 64)', h)
        self.assertIn('overDegrees = .05f, overMm = 2.f', h)
        self.assertEqual((P.OVER_DEGREES, P.OVER_MM), (0.05, 2.0))
        # Fields in the parser's order.
        data = h[h.index('struct Data {'):h.index('static_assert(sizeof(Data)')]
        order = ['magic', 'eyes[2]', 'submits', 'worldTurnLast', 'worldTurnSum', 'worldTurnOver', 'travelMaxMm',
                 'ageSumMs', 'sequence', 'commandMaxDegrees[2]', 'reserved[3]']
        self.assertEqual(sorted(order, key=data.index), order)

    def test_exported_with_c_linkage(self):
        x = src('src/game_xr.cpp')
        block = x[x.index('extern "C" {'):]
        self.assertIn('__declspec(dllexport) spidy::pose_check::Data SpidyPoseCheckData;',
                      block[:block.index('\n}')])

    def test_measured_at_submit(self):
        x = src('src/game_xr.cpp')
        at = x.index('native_eyes::renderOffset(imageGeneration, travel);')
        submit = x[at:at + 3000]
        self.assertIn('native_eyes::renderedEye(imageGeneration, made(eye), camera)', submit)
        self.assertIn('compose(saved->worldFromTracking,', submit)
        self.assertIn('saved->trackingEyes[made(eye)].pose', submit)
        self.assertIn('!saved->flatScreen && !saved->stereoScreen', submit)
        # The layer's pose is the same tracking eye (unchanged by the check).
        self.assertIn('const auto& pose = saved->trackingEyes[made(eye)].pose;', x[at - 2000:at])
        p = src('src/stereo_probe.cpp')
        self.assertIn('read(reinterpret_cast<uintptr_t>(view), camera.data(), sizeof(camera))', p)
        self.assertIn('bool spidy::native_eyes::renderedEye(uint64_t frame, unsigned eye, Mat4& pose)', p)
        self.assertIn('bool renderedEye(uint64_t generation, unsigned eye, Mat4& pose);',
                      src('include/spidy/native_eye_frame.hpp'))

    def test_report_wiring(self):
        r = src('tools/run_game_vr.py')
        self.assertIn('import pose_check_record', r)
        self.assertIn('pose_check_record.EXPORT))', r)  # optional: an older module has none
        self.assertEqual(len(re.findall(r'pose_check=pose_check', r)), 2)
        self.assertIn("sample['pose_check']", r)
        self.assertIn('pose_check_record.pose_check_line(result.get(', r)

    def test_parse(self):
        rec = P.pose_check_snapshot(Fake(record()), 0x1000)
        self.assertEqual(rec['left']['checked'], 10)
        self.assertEqual(rec['right']['unread'], 1)
        self.assertAlmostEqual(rec['left']['mean_degrees'], .01, 4)
        self.assertAlmostEqual(rec['right']['mean_mm'], 2.5, 3)
        self.assertEqual((rec['submits'], rec['reused'], rec['sequence']), (20, 10, 7))
        self.assertAlmostEqual(rec['world_turn']['max_degrees'], .3, 4)
        self.assertAlmostEqual(rec['world_turn']['mean_degrees'], .025, 4)
        self.assertEqual(rec['world_turn']['over'], 3)
        self.assertAlmostEqual(rec['image_age_ms']['mean'], 15.0, 2)
        self.assertEqual(rec['travel_max_mm'], 1500.0)
        self.assertAlmostEqual(rec['right']['command_max_degrees'], .4, 4)
        self.assertAlmostEqual(rec['right']['command_max_mm'], 3.0, 3)
        self.assertEqual(P.pose_check_verdict(rec), 'differs')
        line = P.pose_check_line(rec)
        self.assertIn('DIFFERS', line)
        self.assertIn('right max 0.500 deg / 4.0 mm', line)

    def test_match_and_nothing(self):
        rec = P.pose_check_parse(record(right=(10, 0, 0, 0, .01, .02, .3, .9, .1, 3.0)))
        self.assertEqual(P.pose_check_verdict(rec), 'match')
        self.assertIn('MATCH', P.pose_check_line(rec))
        empty = P.pose_check_parse(record(left=(0,) * 4 + (0,) * 6, right=(0,) * 10, submits=0))
        self.assertIsNone(P.pose_check_line(empty))
        self.assertIsNone(P.pose_check_snapshot(Fake(record()), 0))
        self.assertIsNone(P.pose_check_parse(record()[:255]))
        self.assertIsNone(P.pose_check_parse(b'\0' * 256))

    def test_no_forbidden_names(self):
        for p in ('include/spidy/pose_check.hpp', 'tools/pose_check_record.py', 'tests/hero076_pose_check_tests.py'):
            t = src(p).lower()
            self.assertNotIn('jim' + 'jam', t)
            self.assertNotIn('mi' + 'les', t)


if __name__ == '__main__':
    unittest.main()
