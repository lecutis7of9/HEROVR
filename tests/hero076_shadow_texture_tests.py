# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 SHADOW STABLE and TEXTURE DETAIL (BUILD-SM2.md section 77), both off by default and independent.

live.txt:
  vr_shadow_stable = off (default) | on      live: the far building shadows made once a frame from the head's view
  vr_texture_detail = off (default) | on | aniso | quality   RESTART: 16x filtering / Very High textures
Neither touches the TAA core, the lens jitter, the headset eye path or sharpening.
"""
import pathlib
import sys
import types
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests'))
import live_settings as L  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


def run_game_vr():
    from launch_session_tests import Stubs, TOOLS, REAL  # the stand-ins for the Windows modules
    names = {p.stem for p in TOOLS.glob('*.py')} - REAL - {'run_game_vr'}
    saved = {k: sys.modules.pop(k) for k in list(sys.modules) if k in names | {'run_game_vr'}}
    finder = Stubs(names)
    sys.meta_path.insert(0, finder)
    try:
        import run_game_vr as r
        return r
    finally:
        sys.meta_path.remove(finder)
        for k in list(sys.modules):
            if k in names | {'run_game_vr'}:
                sys.modules.pop(k)
        sys.modules.update(saved)


class ShadowStable(unittest.TestCase):
    def test_setting_live(self):
        self.assertEqual(L.SETTINGS['vr_shadow_stable'][:2], ((0, 1), 'SpidyShadowStable'))
        self.assertIn('no restart', L.SETTINGS['vr_shadow_stable'][2])
        self.assertNotIn('vr_shadow_stable', L.RESTART)
        self.assertIn('vr_shadow_stable', L.START_ONLY)

    def test_words(self):
        values, errors = L.parse('vr_shadow_stable = on\n')
        self.assertEqual(errors, [])
        self.assertEqual(L.export_value('vr_shadow_stable', values['vr_shadow_stable']), 1)
        values, _ = L.parse('vr_shadow_stable = off\n')
        self.assertEqual(L.export_value('vr_shadow_stable', values['vr_shadow_stable']), 0)

    def test_native(self):
        n = src('src/stereo_probe.cpp')
        self.assertIn('DWORD WINAPI SpidyShadowStable(void* value)', n)
        self.assertIn('spidy::shadow_stable::Data SpidyShadowStableData', n)
        self.assertIn('std::atomic<uint32_t> shadowStableOn{spidy::shadow_stable::off}', n)
        h = src('include/spidy/shadow_stable.hpp')
        self.assertIn('static_assert(sizeof(Data) == 72', h)

    def test_reader(self):
        import far_shadows as F
        self.assertEqual(F.STABLE_EXPORT, 'SpidyShadowStableData')
        self.assertEqual(F.STABLE_SIZE, 72)


class TextureDetail(unittest.TestCase):
    def test_setting_restart(self):
        self.assertEqual(L.SETTINGS['vr_texture_detail'][:2], ((0, 1, 2, 3), None))
        self.assertIn('vr_texture_detail', L.RESTART)
        self.assertIn('vr_texture_detail', L.START_ONLY)
        self.assertIn('RESTART', L.SETTINGS['vr_texture_detail'][2])

    def test_choice(self):
        self.assertEqual(L.vr_texture_detail(None), (False, False))
        self.assertEqual(L.vr_texture_detail(''), (False, False))
        self.assertEqual(L.vr_texture_detail('vr_texture_detail = on\n'), (True, True))
        self.assertEqual(L.vr_texture_detail('vr_texture_detail = aniso\n'), (True, False))
        self.assertEqual(L.vr_texture_detail('vr_texture_detail = quality\n'), (False, True))
        self.assertEqual(L.vr_texture_detail('vr_texture_detail = off\n'), (False, False))

    def test_session_off(self):
        self.assertIn('textures_very_high=dict(TextureQuality=3)', src('tools/vr_display.py'))

    def test_plan(self):
        r = run_game_vr()
        names, lines, _ = r.vr_picture_plan(True, True, True, {'TextureQuality': 2, 'TextureFiltering': 4},
                                            very_high=True)
        self.assertIn('textures_very_high', names)
        self.assertNotIn('textures', names)
        names, _, _ = r.vr_picture_plan(True, True, True, {'TextureQuality': 3, 'TextureFiltering': 4},
                                        very_high=True)
        self.assertNotIn('textures_very_high', names)
        names, _, _ = r.vr_picture_plan(True, True, True, {'TextureQuality': 1, 'TextureFiltering': 4})
        self.assertIn('textures', names)
        self.assertNotIn('textures_very_high', names)

    def test_aniso_no_sharpening(self):
        r = run_game_vr()

        def args():
            return types.SimpleNamespace(vr_aa='ours', render_scale=100, supersample=100, upscale=100, mip_bias='game', anisotropy=16,
                                         game_sharpening=False, engine_taa=False, no_eye_antialiasing=False,
                                         game_aa_taa=True, size=0)
        a = args()
        r.apply_vr_aa(a, 100, True)
        self.assertEqual((a.anisotropy, a.mip_bias), (16, 'game'))
        self.assertIn("return anisotropy << 16", src('tools/probe_stereo.py'))  # 'game': filtering only, bias kept
        a = args()
        r.apply_vr_aa(a, 100)
        self.assertEqual(a.anisotropy, 0)
        a = args()
        r.apply_sm1style(a, '')
        self.assertEqual(a.anisotropy, 0)
        a = args()
        r.apply_sm1style(a, 'vr_texture_detail = aniso\n')
        self.assertEqual(a.anisotropy, 16)
        a = args()
        r.apply_sm1style(a, 'vr_texture_detail = quality\n')
        self.assertEqual(a.anisotropy, 0)


class Docs(unittest.TestCase):
    def test_docs(self):
        for doc in ('BUILD-SM2.md', 'docs/VISUAL-0.0.76.md', 'CHANGELOG.md', 'README-HERO-VR.md'):
            for name in ('vr_shadow_stable', 'vr_texture_detail'):
                self.assertIn(name, src(doc), doc)


if __name__ == '__main__':
    unittest.main()
