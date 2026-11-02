# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 VISUAL: the game's own TAA in each eye at native 100%, with nothing spatial on top.

The owner: in VR the game's native temporal AA in each eye, never FSR, spatial filters or CAS; sharpening at its
lowest; no upscaling, native 100%. live.txt (RESTART, the player's own put back when the game closes):
  vr_aa = taa (default) | keep       the game's AA at TAA with no upscaler (AA 2, UpscaleMethod 0)
  vr_textures = high (default) | keep Texture Quality at least High (2), Texture Filtering 16x (4), only raised
  vr_render = native (default) | upscaled  SM1STYLE's eyes at 100% or SpidyVR's 67%
"""
import pathlib
import sys
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


class LiveTxt(unittest.TestCase):
    def test_defaults_and_keeps(self):
        for text in (None, '', 'vr_aa = junk\n'):
            self.assertTrue(L.vr_aa_taa(text))
            self.assertTrue(L.vr_textures_high(text))
            self.assertFalse(L.vr_render_upscaled(text))
        self.assertFalse(L.vr_aa_taa('vr_aa = keep\n'))
        self.assertFalse(L.vr_aa_taa('vr_aa = 1\n'))
        self.assertFalse(L.vr_textures_high('vr_textures = keep\n'))
        self.assertTrue(L.vr_render_upscaled('vr_render = upscaled\n'))
        self.assertTrue(L.vr_render_upscaled('vr_render = 67\n'))

    def test_restart_settings_with_subtext(self):
        for name in ('vr_aa', 'vr_textures', 'vr_render'):
            self.assertIsNone(L.SETTINGS[name][1])  # decided when VR starts
            self.assertIn('RESTART NOTICE', L.SETTINGS[name][2])
            self.assertIn(name, L.RESTART)
            self.assertIn(name, L.START_ONLY)
            self.assertEqual(L.parse(f'{name} = 1\n')[1], [])


class Session(unittest.TestCase):
    def test_encodings(self):
        # vr_display needs winreg (Windows): its text, CONFIRMED beside the game's [Settings] log lines.
        text = src('tools/vr_display.py')
        self.assertIn("game_aa=dict(AA=2, UpscaleMethod=0)", text)
        self.assertIn("textures=dict(TextureQuality=2)", text)
        self.assertIn("filtering=dict(TextureFiltering=4)", text)

    def test_plan(self):
        r = run_game_vr()
        dlss_medium = {'AA': 2, 'UpscaleMethod': 2, 'TextureQuality': 1, 'TextureFiltering': 4}
        names, lines, taa = r.vr_picture_plan(True, True, True, dlss_medium)
        self.assertEqual(names, ('game_aa', 'textures'))
        self.assertTrue(taa)
        self.assertTrue(any('put back' in line for line in lines))
        names, lines, taa = r.vr_picture_plan(True, False, True, dlss_medium)
        self.assertEqual((names, taa), (('textures',), False))
        names, lines, taa = r.vr_picture_plan(False, True, False, dlss_medium)  # SM1STYLE off: the player's AA
        self.assertEqual(names, ())
        names, _, taa = r.vr_picture_plan(True, True, True, {'TextureQuality': 3, 'TextureFiltering': 0})
        self.assertEqual(names, ('game_aa', 'filtering'))  # never lowered, only raised
        names, _, taa = r.vr_picture_plan(True, False, True, {})
        self.assertEqual((names, taa), ((), None))

    def test_no_fsr_or_upscaler_forced(self):
        text = src('tools/vr_display.py') + src('tools/run_game_vr.py')
        self.assertNotIn('UpscaleMethod=1', text)
        self.assertNotIn('UpscaleMethod=2', text)


class Dll(unittest.TestCase):
    def test_native_percent(self):
        h = src('include/spidy/sm1style.hpp')
        self.assertIn('uint32_t wanted = upscalePercent', h)
        self.assertIn('eyeRenderPercent.load());  // 0.0.76 VISUAL', src('src/stereo_probe.cpp'))
        self.assertIn('SM1STYLE_NATIVE = 100', src('tools/run_game_vr.py'))


if __name__ == '__main__':
    unittest.main()
