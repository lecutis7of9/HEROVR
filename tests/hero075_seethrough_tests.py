# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.75 SEE-THROUGH: the eye jobs' scene depth is found by command list (any thread), or by the only eye job open
when the G-buffer pass is recorded, at the eye's size or its lower render size; the console line says where the
passes went."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class SeeThrough(unittest.TestCase):

    def test_tags_by_list(self):
        gpu = src('src/native_eye_gpu.cpp')
        self.assertIn('std::unordered_map<ID3D12GraphicsCommandList*, DepthTag> depthTags', gpu)
        self.assertNotIn('foveaTag.gbuffer', gpu)
        self.assertIn('depthTargets(list, n, dsv);', gpu)
        self.assertIn('depthUntag(list); // 0.0.75', gpu)

    def test_render_size(self):
        self.assertIn('scene_depth::sizeFits(d.Width, d.Height, w, h)', src('src/native_eye_gpu.cpp'))

    def test_job_window(self):
        probe = src('src/stereo_probe.cpp')
        self.assertIn('native_gpu::openJob(eye, serial, generation)', probe)
        self.assertIn('native_gpu::closeJob(eye)', probe)

    def test_console_line(self):
        gpu = src('src/native_eye_gpu.cpp')
        for words in ('lists marked', 'by open job', 'unclaimed', 'most colour targets', 'depths out'):
            self.assertIn(words, gpu)


if __name__ == '__main__':
    unittest.main()
