"""HERO VR 0.0.73 lights (the owner after test 4: "street lights still flicker and headlights still ghosting").
EYE BLOOM starts OFF (SpidyVR's: the game builds non-active views without bloom) and LENS FLARES starts OFF IN VR;
both move once over a live.txt at the old default; the export carries the EYE BLOOM record."""
import os, pathlib, struct, sys, tempfile, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import live_settings as L  # noqa: E402
import update37_records as R  # noqa: E402


def src(p):
    return open(os.path.join(ROOT, p), encoding='utf-8').read()


class Defaults(unittest.TestCase):
    def test_eye_bloom_off(self):
        self.assertIn('inline constexpr uint32_t defaultMode = off;', src('include/spidy/eye_bloom.hpp'))
        self.assertEqual(L.UPDATE37_DEFAULTS['eye_bloom'], 0)
        self.assertEqual(L.MIGRATE_372['eye_bloom'], 0)  # an old live.txt never gets COPY back

    def test_lens_flares_off_in_vr(self):
        self.assertEqual(L.UPDATE38_DEFAULTS['lens_flares'], 'off_in_vr')
        self.assertTrue(L.lens_flares_off(None))
        self.assertTrue(L.lens_flares_off(''))
        self.assertTrue(L.lens_flares_off('lens_flares = off_in_vr\n'))
        self.assertTrue(L.lens_flares_off('lens_flares = 1\n'))
        self.assertFalse(L.lens_flares_off('lens_flares = game\n'))
        self.assertFalse(L.lens_flares_off('lens_flares = 0\n'))

    def test_subtexts_one_sentence_with_restart(self):
        h = src('include/spidy/eye_bloom.hpp')
        eb = h[h.index('inline constexpr const char* help = "') + 36:]
        eb = eb[:eb.index('";')]
        c = src('include/spidy/combat38.hpp')
        i = c.index('{"lens_flares", "LENS FLARES", "FLARES", "') + 42
        lf = c[i:c.index('"', i)]
        self.assertTrue(eb.endswith('no restart.') and len(eb) <= 80 and eb.count('.') == 1, eb)
        self.assertTrue(lf.endswith('restart VR to apply.') and len(lf) <= 80 and lf.count('.') == 1, lf)

    def test_launcher_uses_the_effective_default(self):
        r = src('tools/run_game_vr.py')
        i = r.index('migrated_073t5 = live_settings.migrate_073t5')
        self.assertLess(i, r.index('live_settings.lens_flares_off(live_text)'))
        self.assertLess(r.index('live_settings.lens_flares_off(live_text)'), r.index('a.session_off = session_off'))


class Migration(unittest.TestCase):
    def run_on(self, text):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text(text, encoding='utf-8')
            first = L.migrate_073t5(p)
            after = p.read_text(encoding='utf-8')
            again = L.migrate_073t5(p)
            return first, L.parse(after)[0], after, again

    def test_old_defaults_move_once(self):
        first, values, after, again = self.run_on('eye_bloom = 1\nlens_flares = game\nhistory = 75\n')
        self.assertEqual((values['eye_bloom'], values['lens_flares'], values['history']), (0, 'off_in_vr', 75))
        self.assertIn('0.0.73 lights', first)
        self.assertIn(L.MIGRATED_073T5, after)
        self.assertIsNone(again)

    def test_other_values_stay(self):
        first, values, _, _ = self.run_on('eye_bloom = 2\nlens_flares = off_in_vr\n')
        self.assertIsNone(first)
        self.assertEqual((values['eye_bloom'], values['lens_flares']), (2, 'off_in_vr'))

    def test_after_the_marker_a_choice_stays(self):
        first, values, _, _ = self.run_on(L.MIGRATED_073T5 + '\neye_bloom = 1\nlens_flares = game\n')
        self.assertIsNone(first)
        self.assertEqual((values['eye_bloom'], values['lens_flares']), (1, 'game'))

    def test_no_live_txt(self):
        with tempfile.TemporaryDirectory() as d:
            self.assertIsNone(L.migrate_073t5(pathlib.Path(d) / 'live.txt'))
        self.assertIn(L.MIGRATED_073T5, L.HEADER)


class Record(unittest.TestCase):
    def test_eye_bloom_record(self):
        self.assertEqual(R.RECORDS['eye_bloom'], ('SpidyEyeBloomData', 0x4d4f4c42, 1, 144))
        self.assertIn('static_assert(sizeof(Data) == 144);', src('include/spidy/eye_bloom.hpp'))
        raw = (struct.pack('<3I', 0x4d4f4c42, 1, 144) + struct.pack('<7I', 0, 1, 0, 1, 0, 0, 0) +
               struct.pack('<5Q', 10, 4, 4, 0, 0) + struct.pack('<6I', 7, 0, 0, 0, 0, 0) + bytes(32) + struct.pack('<Q', 2))
        d = R.parse_eye_bloom(raw)
        self.assertEqual((d['mode'], d['eye_jobs'], d['game_view_bloom_on']), ('OFF', 4, 4))
        self.assertTrue(any(l.startswith('0.0.73 lights: EYE BLOOM OFF') for l in R.console_lines({'eye_bloom': d})))
        odd = raw[:-8] + struct.pack('<Q', 3)
        self.assertIsNone(R.parse_eye_bloom(odd))


if __name__ == '__main__':
    unittest.main()
