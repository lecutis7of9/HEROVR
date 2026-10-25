"""Update 26: with EYE TEST SAME PICTURE the right headset layer is given the view its picture was made from."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class SamePictureLayer(unittest.TestCase):
    def setUp(self):
        self.xr = (ROOT / 'src' / 'game_xr.cpp').read_text(encoding='utf-8')
        self.gpu = (ROOT / 'src' / 'native_eye_gpu.cpp').read_text(encoding='utf-8')

    def test_the_pair_remembers_its_mode(self):
        # The mode the copied pair was made with (read once per pair), not the live setting.
        self.assertIn('pairDebugMode = debug;', self.gpu)
        self.assertIn('uint32_t pairEyeDebug()', self.gpu)
        self.assertIn('uint32_t pairEyeDebug();', (ROOT / 'include' / 'spidy' / 'native_eye_gpu.hpp').read_text())

    def test_layer_and_drawings_use_the_view_the_picture_was_made_from(self):
        self.assertIn('eye_debug::eyeDebugSource(native_gpu::pairEyeDebug(), 1) == 0', self.xr)
        self.assertIn('saved->trackingEyes[made(eye)].pose', self.xr)
        self.assertIn('saved->trackingEyes[made(eye)].fov', self.xr)
        # Spidy's drawings and the slow-motion grade follow the same view.
        self.assertEqual(len(re.findall(r'rendered\.fovs\[made\(eye\)\]', self.xr)), 2)
        self.assertIn('rendered.eyes[made(eye)]', self.xr)
        self.assertNotRegex(self.xr, r'trackingEyes\[eye\]')

    def test_fov_order_reaches_openxr_unchanged(self):
        # The tracked fov is (left, right, down, up); XrFovf is (left, right, up, down).
        self.assertIn('targets[eye].view.fov = {f.left, f.right, f.up, f.down};', self.xr)
        session = (ROOT / 'src' / 'xr_session.cpp').read_text(encoding='utf-8')
        self.assertIn('{fov.angleLeft, fov.angleRight, fov.angleDown, fov.angleUp}', session)


if __name__ == '__main__':
    unittest.main()
