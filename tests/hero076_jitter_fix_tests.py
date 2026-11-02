# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 JITTER FIX: the head views no longer feed the stored (already doubled, y-flipped) jitter back into the
lens; JITTER LOG records each view's jitter for 200 frames; POSE CHECK also compares each layer with the eye its
view and projection were built from, and where its eyes were located (read-only)."""
import pathlib
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import jitter_log_record as J  # noqa: E402
import pose_check_record as P  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Fake:
    def __init__(self, raw):
        self.raw = raw

    def read(self, address, size):
        return self.raw[:size]


class JitterFix(unittest.TestCase):
    def test_head_views_pass_zero_by_default(self):
        s = src('src/stereo_probe.cpp')
        self.assertIn('gameViewJitterOff{1};', s)
        self.assertIn('headJitter[0], headJitter[1], false, false);', s)
        self.assertIn('0, 0, x, y, false, false);', s)
        # The headset eyes keep eyeJitter's raw values.
        self.assertIn('controlled ? -std::tan(fov[2]) : 1, 0, 0, jitter[0], jitter[1], false, false);', s)
        self.assertEqual(s.count('logJitter('), 6)  # the helper and five lens calls
        h = src('include/spidy/jitter_log.hpp')
        self.assertIn('frames = 200', h)

    def test_jitter_log_parse(self):
        raw = bytearray(J.SIZE)
        struct.pack_into('<6I', raw, 0, J.MAGIC, 1, J.SIZE, J.ENTRY, 3, 0)
        struct.pack_into('<2I6f', raw, 48, 7, 0, .25, -.5, 0, 0, 0, 0)          # head view, fixed
        struct.pack_into('<2I6f', raw, 80, 7, 0, .25, -.5, .25, -.5, .5, 1)     # c71a59c's keep: fed back
        struct.pack_into('<2I6f', raw, 112, 7, 2, .25, -.5, .1, .2, .2, -.4)    # left eye, raw
        rec = J.jitter_log_snapshot(Fake(bytes(raw)), 1)
        self.assertEqual([e['pass'] for e in rec['entries']], [1, 2, 1])
        sm = J.jitter_log_summary(rec)
        self.assertEqual(sm['head_view']['doubled_stored'], 1)
        self.assertEqual(sm['head_view']['lens_rule'], 2)
        self.assertEqual(sm['eye_left']['lens_rule'], 1)
        self.assertIn('head_view: 2 passes over 1 frames', J.jitter_log_line(rec))
        self.assertIsNone(J.jitter_log_snapshot(Fake(bytes(10)), 1))
        self.assertIsNone(J.jitter_log_line(None))

    def test_pose_layer_fields(self):
        raw = bytearray(256)
        struct.pack_into('<4I', raw, 0, P.MAGIC, 1, 256, 0)
        struct.pack_into('<2Q', raw, 16, 5, 0)
        struct.pack_into('<2Q', raw, 80, 5, 0)
        struct.pack_into('<2Q', raw, 144, 5, 0)
        rec = P.pose_check_parse(bytes(raw))
        self.assertEqual(rec['located_off'], 0)
        self.assertIn('Layer vs build: SAME', P.pose_check_line(rec))
        struct.pack_into('<Q3fI', raw, 232, 2, .5, 3.0, 0, 1)
        rec = P.pose_check_parse(bytes(raw))
        self.assertEqual(rec['layer']['over'], 1)
        self.assertIn('MISMATCH', P.pose_check_line(rec))
        self.assertIn('2 of 5 submits', P.pose_check_line(rec))

    def test_native_pose_check(self):
        g = src('src/game_xr.cpp')
        self.assertIn('pose_check::noteLayer(check, layer, fovOff)', g)
        self.assertIn('rendered.locatedTime != rendered.predictedDisplayTime', g)
        self.assertIn('tracked.locatedTime = locateInfo.displayTime;', src('src/xr_session.cpp'))
        self.assertIn('out.locatedTime = f.locatedTime;', src('src/game_tracking.cpp'))
        self.assertIn('jitter_log_record.EXPORT', src('tools/run_game_vr.py'))


if __name__ == '__main__':
    unittest.main()
