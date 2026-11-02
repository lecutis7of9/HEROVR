# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 SPIDY TAA: our own per-eye TAA in the mod's pass (the default), SMAA per eye (live.txt vr_aa = smaa), the
game's TAA kept off the eyes, and the old SM1STYLE picture only with vr_aa = game."""
import pathlib
import struct
import sys
import types
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests'))
import live_settings as L  # noqa: E402
import vr_aa_record as V  # noqa: E402


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


def args(**kw):
    a = types.SimpleNamespace(render_scale=150, supersample=150, upscale=67, mip_bias='0.5', anisotropy=16,
                              game_sharpening=True, engine_taa=True, no_eye_antialiasing=True, size=0)
    for k, v in kw.items():
        setattr(a, k, v)
    return a


class LiveTxt(unittest.TestCase):
    def test_choices(self):
        for text in (None, '', 'vr_aa = junk\n', 'vr_aa = taa\n', 'vr_aa = ours\n', 'vr_aa = 0\n'):
            self.assertEqual(L.vr_aa_choice(text), 'ours')
        self.assertEqual(L.vr_aa_choice('vr_aa = smaa\n'), 'smaa')
        self.assertEqual(L.vr_aa_choice('vr_aa = 2\n'), 'smaa')
        self.assertEqual(L.vr_aa_choice('vr_aa = keep\n'), 'keep')
        self.assertEqual(L.vr_aa_choice('vr_aa = game\n'), 'game')
        self.assertTrue(L.vr_aa_taa('vr_aa = smaa\n'))
        self.assertFalse(L.vr_aa_taa('vr_aa = keep\n'))

    def test_subtext(self):
        text = L.SETTINGS['vr_aa'][2]
        for word in ('ours', 'smaa', 'keep', 'game', 'RESTART NOTICE'):
            self.assertIn(word, text)
        self.assertEqual(L.parse('vr_aa = smaa\n')[1], [])
        self.assertIn('smaa', L.RESTART['vr_aa'])


class Launcher(unittest.TestCase):
    def test_default_is_ours(self):
        r = run_game_vr()
        a = args(game_aa_taa=True)
        line = r.apply_sm1style(a, None)
        self.assertEqual((a.vr_aa, a.sm1style), ('ours', False))
        self.assertEqual((a.render_scale, a.supersample, a.upscale, a.mip_bias, a.anisotropy),
                         (100, 100, 100, 'game', 0))  # 9aaedd5's -0.5 taken back: 509bdc9's picture
        self.assertFalse(a.game_sharpening or a.engine_taa or a.no_eye_antialiasing)
        self.assertIn('SPIDY TAA', line)
        self.assertIn("the game's mip bias", line)
        self.assertNotIn('RCAS', line)
        self.assertNotIn('supersampled', line)
        self.assertIn('vr_aa = smaa', line)
        self.assertEqual(r.vr_aa_options(a), 0)

    def test_smaa(self):
        r = run_game_vr()
        a = args(game_aa_taa=True)
        line = r.apply_sm1style(a, 'vr_aa = smaa\n')
        self.assertEqual((a.vr_aa, a.mip_bias, a.upscale), ('smaa', '0.5', 100))
        self.assertIn('SMAA', line)
        self.assertEqual(r.vr_aa_options(a), r.VR_AA_SMAA_OPTION)

    def test_game_is_the_old_picture(self):
        r = run_game_vr()
        a = args()
        r.apply_sm1style(a, 'vr_aa = game\n')
        self.assertTrue(a.sm1style)
        self.assertEqual(r.vr_aa_options(a), 0)  # SM1STYLE's own bit says legacy
        a = args()
        r.apply_sm1style(a, 'vr_aa = game\nsm1style = off\n')
        self.assertFalse(a.sm1style)
        self.assertEqual(r.vr_aa_options(a), r.VR_AA_LEGACY_OPTION)

    def test_bits_match_the_dll(self):
        h = src('include/spidy/vr_aa.hpp')
        self.assertIn('smaaOption = 0x2000000', h)
        self.assertIn('legacyOption = 0x4000000', h)
        text = src('tools/run_game_vr.py')
        self.assertIn('VR_AA_SMAA_OPTION = 0x2000000', text)
        self.assertIn('VR_AA_LEGACY_OPTION = 0x4000000', text)
        self.assertIn('vr_aa_options(a)) + \\', text)
        self.assertIn('constexpr uint32_t knownOptions = 0x7ffffff;', src('src/game_xr.cpp'))

    def test_plan_puts_game_aa_at_taa_by_default(self):
        r = run_game_vr()
        names, lines, taa = r.vr_picture_plan(True, True, False, {'AA': 2, 'UpscaleMethod': 2})
        self.assertEqual((names, taa), (('game_aa',), True))


class Tune(unittest.TestCase):
    """0.0.76 SPIDY TAA TUNE, after 9aaedd5's run: 509bdc9's TAA, mip bias and picture again (the longer average,
    looser clip, 10-bit history, RCAS and mip -0.5 taken back); kept: live.txt vr_ss (off by default) and the report's
    last vr_aa / jitter_log records."""
    def test_vr_ss_choices(self):
        for text in (None, '', 'vr_ss = junk\n', 'vr_ss = off\n', 'vr_ss = 100\n', 'vr_ss = 0\n'):
            self.assertEqual(L.vr_supersample(text), 100)
        self.assertEqual(L.vr_supersample('vr_ss = 125\n'), 125)
        self.assertEqual(L.vr_supersample('vr_ss = 1\n'), 125)
        self.assertEqual(L.vr_supersample('vr_ss = 150\n'), 150)
        self.assertEqual(L.vr_supersample('vr_ss = 2\n'), 150)

    def test_vr_ss_subtext(self):
        self.assertIsNone(L.SETTINGS['vr_ss'][1])
        for word in ('off', '125', '150', 'RESTART NOTICE'):
            self.assertIn(word, L.SETTINGS['vr_ss'][2])
        self.assertIn('vr_ss', L.RESTART)
        self.assertIn('vr_ss', L.START_ONLY)
        self.assertEqual(L.parse('vr_ss = 125\n')[1], [])

    def test_vr_ss_reaches_the_eyes(self):
        r = run_game_vr()
        a = args(game_aa_taa=True)
        line = r.apply_sm1style(a, 'vr_ss = 125\n')
        self.assertEqual((a.supersample, a.upscale, a.mip_bias), (125, 100, 'game'))
        self.assertIn('supersampled 125%', line)
        a = args(game_aa_taa=True, size=2000)  # a fixed eye size: no supersample
        r.apply_sm1style(a, 'vr_ss = 150\n')
        self.assertEqual(a.supersample, 100)
        a = args(game_aa_taa=False)
        r.apply_sm1style(a, None)
        self.assertEqual(a.mip_bias, '0.5')  # the game's AA not TAA: as before

    def test_509bdc9_taa_again(self):
        h, gpu = src('include/spidy/vr_aa.hpp'), src('src/native_eye_gpu.cpp')
        for line in ('keepStill = .9f', 'keepMoving = .6f', 'gammaStill = 1.25f', 'gammaMoving = .85f'):
            self.assertIn(line, h)
        self.assertNotIn('rcasShader', gpu)
        self.assertNotIn('history.rcas', gpu)
        self.assertNotIn('history.deep', gpu)

    def test_report_keeps_the_last_record(self):
        text = src('tools/run_game_vr.py')
        self.assertIn('or vr_aa_last', text)
        self.assertIn('or jitter_log_last', text)


class Record(unittest.TestCase):
    def test_parse_and_line(self):
        raw = struct.pack(V.FORMAT, V.MAGIC, 1, 72, 0, 1, 0, 0, 0, 5000, 0, 0, 0.0, 0)
        self.assertEqual(V.SIZE, 72)
        rec = V.vr_aa_parse(raw)
        self.assertEqual((rec['asked'], rec['running'], rec['taa_pairs']), ('ours', 'spidy_taa', 5000))
        self.assertIn('SPIDY TAA on 5000', V.vr_aa_line(rec))
        raw = struct.pack(V.FORMAT, V.MAGIC, 1, 72, 0, 2, 0, 2, 1, 10, 900, 0, 30.0, 100)
        line = V.vr_aa_line(V.vr_aa_parse(raw))
        self.assertIn('SMAA on 900', line)
        self.assertIn('0.30 ms', line)
        self.assertIn('fell back', line)
        self.assertIn('SPIDY TAA could not start', line)
        self.assertIsNone(V.vr_aa_parse(b'\0' * 72))
        self.assertIsNone(V.vr_aa_parse(None))
        self.assertIsNone(V.vr_aa_snapshot(None, 0))

    def test_exported(self):
        self.assertIn('vr_aa::Data SpidyVrAaData', src('src/native_eye_gpu.cpp'))
        self.assertIn('vr_aa_record.EXPORT', src('tools/run_game_vr.py'))


class Dll(unittest.TestCase):
    def test_game_taa_kept_off_the_eyes(self):
        probe, xr = src('src/stereo_probe.cpp'), src('src/game_xr.cpp')
        self.assertIn('engineTaaAuto = !native_gpu::vrAaLocks()', probe.replace('spidy::', ''))
        self.assertIn('ENGINE TAA: refused, the eyes run SPIDY TAA or SMAA', probe)
        self.assertIn('native_gpu::setVrAa(vrAa);', xr)

    def test_spidy_taa_shader(self):
        gpu = src('src/native_eye_gpu.cpp')
        self.assertIn('taaShader', gpu)
        self.assertIn('history.taa', gpu)
        self.assertIn('drawSmaa(', gpu)

    def test_smaa_notice(self):
        self.assertIn('SMAA', src('THIRD_PARTY_NOTICES.md'))


if __name__ == '__main__':
    unittest.main()
