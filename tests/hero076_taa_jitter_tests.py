# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 TAA JITTER / TAA STILL (diagnostics for far windows that swim with a still head), in every A/B/C test build:
  vr_taa_jitter = on (default) | off | fixed | half   our own Halton lens step only (taa_jitter.hpp)
  vr_taa_still = 0.97                                  SPIDY TAA's still share only (taa_still.hpp; unset: the build's)
and TAA WATCH's history rejection (the share of counted pixels the clip moved), still head vs moving, every 10 s."""
import pathlib
import re
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests'))
import live_settings as L  # noqa: E402
import taa_watch_record as W  # noqa: E402
from hero076_taa_watch_tests import record, window  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Settings(unittest.TestCase):
    def test_jitter_words(self):
        self.assertEqual(L.SETTINGS['vr_taa_jitter'][:2], ((0, 3), 'SpidyVrTaaJitter'))
        self.assertEqual(L.CHOICE_NAMES['vr_taa_jitter'], ('on', 'off', 'fixed', 'half'))
        for word, n in (('on', 0), ('off', 1), ('fixed', 2), ('half', 3)):
            values, problems = L.parse(f'vr_taa_jitter = {word}\n')
            self.assertEqual(problems, [])
            self.assertEqual(L.export_value('vr_taa_jitter', values['vr_taa_jitter']), n)
        self.assertTrue(L.parse('vr_taa_jitter = quarter\n')[1])

    def test_still_share(self):
        self.assertEqual(L.SETTINGS['vr_taa_still'][:2], ((0.6, 0.99), 'SpidyVrTaaStill'))
        values, problems = L.parse('vr_taa_still = 0.97\n')
        self.assertEqual(problems, [])
        self.assertEqual(L.export_value('vr_taa_still', values['vr_taa_still']), 970)
        self.assertTrue(L.parse('vr_taa_still = 0.5\n')[1])
        self.assertTrue(L.parse('vr_taa_still = 1\n')[1])

    def test_live_never_kept_one_sentence(self):
        for name in ('vr_taa_jitter', 'vr_taa_still'):
            self.assertNotIn(name, L.PERSISTED)
            self.assertNotIn(name, L.START_ONLY)
            self.assertNotIn(name, L.RESTART)
            text = L.SETTINGS[name][2]
            self.assertTrue(text.endswith('- live, no restart needed'))
            self.assertEqual(text.count('. '), 0, name)  # one sentence

    def test_documented(self):
        for doc in ('CHANGELOG.md', 'README-HERO-VR.md', 'BUILD-SM2.md', 'docs/VISUAL-0.0.76.md'):
            text = src(doc)
            for name in ('vr_taa_jitter', 'vr_taa_still'):
                self.assertIn(name, text, doc)


class Native(unittest.TestCase):
    def test_exports_refuse_out_of_range(self):
        n = src('src/native_eye_gpu.cpp')
        self.assertIn('DWORD WINAPI SpidyVrTaaJitter(void* value)', n)
        self.assertIn('return 4611;', n)
        self.assertIn('DWORD WINAPI SpidyVrTaaStill(void* value)', n)
        self.assertIn('return 4612;', n)

    def test_our_step_only(self):
        g = src('src/game_xr.cpp')
        self.assertIn('taa_jitter::step(native_gpu::taaJitterMode(), eyes.serial, jx, jy)', g)
        self.assertNotRegex(g, r'eye_taa::step\(eyes\.serial')
        s = src('src/stereo_probe.cpp')
        self.assertNotIn('taaJitter', s)  # the game's lens jitter is not this
        h = src('include/spidy/taa_jitter.hpp')
        self.assertIn('fixedStep = 1', h)
        self.assertIn('x *= .5f;', h)

    def test_still_only(self):
        t = src('include/spidy/taa_still.hpp')
        self.assertIn('lowest = 600, highest = 990', t)
        self.assertIn('vr_aa::keepMoving', t)
        n = src('src/native_eye_gpu.cpp')
        self.assertIn('keep = taa_still::withStill(keep, stillMilli);', n)

    def test_counts_read_only(self):
        n = src('src/native_eye_gpu.cpp')
        self.assertIn('#ifdef TAA_COUNT', n)
        self.assertIn('counts.InterlockedAdd(4, 1);', n)
        self.assertIn('noteHistoryCount(index);', n)
        shader = n[n.index('const char* taaShader = R"('):]
        shader = shader[:shader.index(')";')]
        body = re.sub(r'#ifdef TAA_COUNT.*?#endif', '', shader, flags=re.S)
        self.assertNotIn('counts', body)  # without the macro the shader is the one before


class Record(unittest.TestCase):
    def test_rejection_split_and_setup(self):
        w = window(still_sampled=40000, still_clipped=2000, moving_sampled=50000, moving_clipped=10000,
                   setup=(1 | 1 << 3) | 970 << 16)
        s = W.taa_watch_report(W.taa_watch_parse(record(windows=[w, w])))
        r = s['windows'][0]
        self.assertEqual(r['rejected_still_pct'], 5.0)
        self.assertEqual(r['rejected_moving_pct'], 20.0)
        self.assertEqual(r['jitter'], 'on/half')
        self.assertEqual(r['still_share'], .97)
        self.assertEqual(s['totals']['rejected_still_pct'], 5.0)
        self.assertEqual(s['totals']['jitter'], 'on/half')
        lines = W.taa_watch_lines(s)
        self.assertIn('history rejected still 5.0% / moving 20.0%', lines[1])
        self.assertIn('rejected still 5.0% / moving 20.0%, jitter on/half, still share 0.97', lines[2])

    def test_no_counts(self):
        w = window(counted=0, still_sampled=0, still_clipped=0, moving_sampled=0, moving_clipped=0, setup=0)
        r = W.taa_watch_report(W.taa_watch_parse(record(windows=[w])))['windows'][0]
        self.assertIsNone(r['rejected_still_pct'])
        self.assertIsNone(r['jitter'])
        self.assertIn('rejected still - / moving -', W.taa_watch_window_lines({'windows': [r]})[0])

    def test_jitter_check(self):
        w = window(jitter_pairs=100, jitter_now=(.3, -.2), jitter_before=(-.25, .17), shift=(.55, .37),
                   jitter_off=(199.0, 4.0, 198.0))
        s = W.taa_watch_report(W.taa_watch_parse(record(windows=[w, w])))
        r = s['windows'][0]
        self.assertEqual(r['jitter_counted'], 'once')
        self.assertEqual(r['jitter_now_px'], [.3, -.2])
        self.assertEqual(r['shift_px'], [.55, .37])
        self.assertEqual(r['jitter_off_px'], [1.99, .04, 1.98])
        self.assertEqual(s['totals']['jitter_counted'], 'once')
        self.assertEqual(s['totals']['jitter_check_pairs'], 200)
        lines = W.taa_watch_lines(s)
        self.assertIn('still step now (+0.30, -0.20) last (-0.25, +0.17) px, history shift (+0.55, +0.37) px, '
                      'step counted once (off by none/once/twice 1.99/0.04/1.98 px, 100 pairs)', lines[2])
        self.assertIn('step counted once', lines[1])
        for twice, name in (((2.0, 1.0, .1), 'twice'), ((.1, 1.0, 2.0), 'none'), ((1.0, 1.0, 1.0), None)):
            r = W.taa_watch_report(W.taa_watch_parse(record(windows=[window(jitter_pairs=10, jitter_off=twice)])))
            self.assertEqual(r['windows'][0]['jitter_counted'], name)
        r = W.taa_watch_report(W.taa_watch_parse(record(windows=[window()])))['windows'][0]
        self.assertIsNone(r['jitter_counted'])
        self.assertNotIn('step counted', W.taa_watch_window_lines({'windows': [r]})[0])

    def test_jitter_check_native(self):
        g = src('src/game_xr.cpp')
        self.assertIn('native_gpu::noteEyeViews(eyes.serial, views, shifted ? jx : 0.f, shifted ? jy : 0.f,', g)
        n = src('src/native_eye_gpu.cpp')
        self.assertIn('taa_watch::noteJitterCheck(SpidyTaaWatchData.current, noted.jitter.data(), jitterBefore.data(),', n)
        self.assertIn('history.shownJitter = noted.jitter;', n)
        h = src('include/spidy/taa_watch.hpp')
        self.assertIn('inline int jitterCounted(const Window& w)', h)

    def test_window_layout(self):
        self.assertEqual(struct.calcsize(W.WINDOW), 120)
        h = src('include/spidy/taa_watch.hpp')
        self.assertIn('static_assert(sizeof(Window) == 120);', h)


if __name__ == '__main__':
    unittest.main()
