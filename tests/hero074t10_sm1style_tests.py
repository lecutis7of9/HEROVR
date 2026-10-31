# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.74 test 10 SM1STYLE: SpidyVR's own eye anti-aliasing is the default, and nothing of ours overrides it.

The owner: "try its approach yes try it exactly and make sure nothing of ours can overwrite it". The game's TAA in
each eye (include/spidy/sm1style.hpp), eyes at 67%, render scale / supersample 100, the game's mip bias, its
sharpening lowest; our FXAA, EYE AA history, CAS sharpen and ENGINE TAA locked off. The one off-switch is live.txt
`sm1style = off` (RESTART): the 0.0.72 path.
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


class LiveTxt(unittest.TestCase):
    def test_on_by_default_off_only_by_its_line(self):
        self.assertTrue(L.sm1style_on(None))
        self.assertTrue(L.sm1style_on(''))
        self.assertTrue(L.sm1style_on('sm1style = on\n'))
        self.assertTrue(L.sm1style_on('sm1style = maybe\n'))  # a bad value keeps the default
        self.assertFalse(L.sm1style_on('sm1style = off\n'))
        self.assertFalse(L.sm1style_on('sm1style = 1\n'))

    def test_restart_setting_kept(self):
        self.assertIsNone(L.SETTINGS['sm1style'][1])  # no export: decided when VR starts
        self.assertIn('sm1style', L.RESTART)
        self.assertIn('sm1style', L.START_ONLY)
        self.assertEqual(L.parse('sm1style = off\n')[1], [])

    def test_migrations_keep_it(self):
        for retired in (L.RETIRED_074, L.RETIRED_073, L.RESET_074T10):
            self.assertNotIn('sm1style', retired)
        import tempfile
        d = pathlib.Path(tempfile.mkdtemp())
        f = d / 'live.txt'
        f.write_text('aa = 0\nsm1style = off\n', encoding='utf-8')
        for m in (L.migrate_073t5, L.migrate_074t10, L.migrate_074t10_seat, L.migrate_074t10_72):
            m(f)
        self.assertFalse(L.sm1style_on(f.read_text(encoding='utf-8')))

    def test_no_old_name(self):
        # The owner asked that the old name never be used; build it from parts so this file does not carry it.
        old = ''.join(('m', 'i', 'l', 'e', 's'))
        for p in ('include/spidy/sm1style.hpp', 'tools/live_settings.py', 'docs/SM1STYLE-0.0.74.md'):
            self.assertNotIn('"' + old, src(p).lower())
        self.assertNotIn(old, src('include/spidy/sm1style.hpp').lower())
        self.assertNotIn(old, src('docs/SM1STYLE-0.0.74.md').lower())

class Launcher(unittest.TestCase):
    def apply(self, live, **kw):
        from launch_session_tests import Stubs, TOOLS, REAL  # the stand-ins for the Windows modules
        names = {p.stem for p in TOOLS.glob('*.py')} - REAL - {'run_game_vr'}
        saved = {k: sys.modules.pop(k) for k in list(sys.modules) if k in names | {'run_game_vr'}}
        finder = Stubs(names)
        sys.meta_path.insert(0, finder)
        try:
            import run_game_vr as r
            a = types.SimpleNamespace(render_scale=150, supersample=150, upscale=100, mip_bias='0.5', anisotropy=16,
                                      game_sharpening=True, engine_taa=True, keep_game_settings=True,
                                      game_depth_of_field=False, size=0)
            for k, v in kw.items():
                setattr(a, k, v)
            line = r.apply_sm1style(a, live)
            return r, a, line
        finally:
            sys.meta_path.remove(finder)
            for k in list(sys.modules):
                if k in names | {'run_game_vr'}:
                    sys.modules.pop(k)
            sys.modules.update(saved)

    def test_locks_over_every_argument(self):
        r, a, line = self.apply(None)
        self.assertTrue(a.sm1style)
        self.assertEqual((a.render_scale, a.supersample, a.upscale, a.mip_bias, a.anisotropy),
                         (100, 100, 67, 'game', 0))
        self.assertFalse(a.game_sharpening or a.engine_taa)
        self.assertNotIn('sharpening', r.picture_kept(a))  # keep-settings cannot keep the game's sharpening
        self.assertIn('SM1STYLE ON', line)
        self.assertIn('sm1style = off', line)

    def test_off_is_untouched(self):
        r, a, line = self.apply('sm1style = off\n')
        self.assertFalse(a.sm1style)
        self.assertEqual((a.render_scale, a.supersample, a.mip_bias), (150, 150, '0.5'))
        self.assertIn('SM1STYLE: OFF', line)

    def test_option_sent_after_everything(self):
        text = src('tools/run_game_vr.py')
        self.assertIn('SM1STYLE_OPTION = 0x1000000', text)
        self.assertIn("SM1STYLE_OPTION if getattr(a, 'sm1style', False)", text)
        main = text[text.index('def main():'):]
        self.assertLess(main.index('next_launch_supersample'), main.index('apply_sm1style(a, live_text)'))
        self.assertLess(main.index('apply_sm1style(a, live_text)'), main.index('encode_mip_bias(a.mip_bias, a.anisotropy)'))


class Native(unittest.TestCase):
    def test_constants_match(self):
        h = src('include/spidy/sm1style.hpp')
        self.assertIn('startOption = 0x1000000', h)
        self.assertIn('upscalePercent = 67', h)
        xr = src('src/game_xr.cpp')
        self.assertIn('constexpr uint32_t knownOptions = 0x1ffffff;', xr)
        self.assertIn('spidy::sm1style::eyeTemporalBit << 8', xr)

    def test_nothing_of_ours_overrides(self):
        xr, gpu, probe = src('src/game_xr.cpp'), src('src/native_eye_gpu.cpp'), src('src/stereo_probe.cpp')
        self.assertIn('native_gpu::setSm1StyleLock(live);', xr)
        self.assertIn('(sm1Holds() ? sm1LockedRows : 0u)', xr)        # wrist rows UNAVAILABLE
        self.assertIn('EYE AA AT START: kept off, SM1STYLE', xr)     # live.txt eye_temporal
        self.assertIn('EYE AA: locked off by SM1STYLE', xr)          # the wrist press
        self.assertIn('ENGINE TAA: locked off by SM1STYLE', xr)
        self.assertIn('!sm1StyleActive.load()', xr)                  # no EYE AA lens jitter / JITTER CALM
        self.assertIn('sm1Lock', gpu)
        self.assertIn('SpidySm1StyleStall', probe)
        self.assertIn('SM1STYLE ON:', xr)                            # the one start line


if __name__ == '__main__':
    unittest.main()
