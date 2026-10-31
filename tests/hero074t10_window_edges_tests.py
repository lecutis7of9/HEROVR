# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.74 test 10 WINDOW EDGES: lit windows stay lit at the outer edges of each eye.

The eyes' symmetric lens summaries (+410/+414/+420/+424) cover the whole off-centre lens from SetPose to Submit
(include/spidy/eye_cover.hpp). On by default, no wrist row, no live.txt key; guarded by the lens setter's two
summary writes in 2.810.0.0, one console line when they differ.
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class WindowEdges(unittest.TestCase):
    def test_every_eye_submit_goes_through_the_cover(self):
        s = src('src/stereo_probe.cpp')
        self.assertEqual(s.count('submitEye(eye, descriptor);'), 2)
        # Only submitEye itself and the mirror view call 2c95200 directly.
        direct = re.findall(r'reinterpret_cast<Submit>\(base \+ 0x2c95200\)\((\w+), &descriptor\)', s)
        self.assertEqual(sorted(direct), ['eye', 'eye', 'view'])
        body = s[s.index('void submitEye('):s.index('// 1796d20 sets up a view')]
        self.assertLess(body.index('ec::apply'), body.index('(eye, &descriptor);\n    ec::restore'))
        self.assertIn('if (!eyeCoverReady)', body)

    def test_cover_after_set_pose(self):
        s = src('src/stereo_probe.cpp')
        for m in re.finditer(r'submitEye\(eye, descriptor\);', s):
            before = s[max(0, m.start() - 400):m.start()]
            self.assertIn('SetPose', before)

    def test_guard_and_fallback_line(self):
        s = src('src/stereo_probe.cpp')
        h = src('include/spidy/eye_cover.hpp')
        self.assertIn('ec::halfWidthWriteRva, ec::halfWidthWriteBytes', s)
        self.assertIn('ec::halfHeightWriteRva, ec::halfHeightWriteBytes', s)
        self.assertIn('OutputDebugStringA(ec::fallbackLine);', s)
        self.assertEqual(s.count('OutputDebugStringA(ec::fallbackLine);'), 1)
        self.assertIn('0xc5, 0xfa, 0x11, 0x83, 0x20, 0x04, 0x00, 0x00', h)
        self.assertIn('0xc5, 0xfa, 0x11, 0x8b, 0x24, 0x04, 0x00, 0x00', h)
        self.assertIn('2.810.0.0', h)

    def test_request_scale_uses_exact_tangents(self):
        s = src('src/stereo_probe.cpp')
        start = s.index('uint64_t setupModels(void* context')
        body = s[start:s.index('// Update 35: FAR SHADOWS', start)]
        self.assertNotIn('view + 0x420', body)
        self.assertIn('read(view + 0x400, lens, sizeof(lens))', body)

    def test_no_switch(self):
        live = src('tools/live_settings.py')
        self.assertNotIn('window_edges', live)
        self.assertNotIn('eye_cover', live)

    def test_names(self):
        for p in ('include/spidy/eye_cover.hpp', 'tests/hero074t10_window_edges_tests.py'):
            text = src(p).lower()
            self.assertNotIn('mil' + 'es', text)


if __name__ == '__main__':
    unittest.main()
