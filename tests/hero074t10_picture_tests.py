# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.74 test 10: the picture is version 0.0.73's again; the 0.0.74 vehicle work stays.

The owner: "our build looked best in the last public build so go back to that one". Every image / AA setting
the 0.0.74 tests 2-9 added is retired: parse() skips it, the DLL has no such option or export, and
live_settings.migrate_074t10 takes the lines (and test 2's SPIDYVR PICTURE overwrites) out of live.txt once.
"""
import pathlib
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402

# The 0.0.73 release's picture defaults (live_settings at the v0.0.73 tag).
DEFAULTS_0073 = {'image': 'spidyvr', 'image_style': 'off', 'aa': 0, 'sharpen': 0, 'sharpen_match': 'on',
                 'taa_jitter': 3, 'lens_flares': 'off_in_vr'}
TEST2_FILE = ('crawl_cap = off\n'
              'image = spidyvr\nimage_style = spidyvr\naa = 0\nsharpen = 0\nsharpen_match = off\n'
              'taa_jitter = 0\ntaa_jitter_scale = 100\nlens_flares = off_in_vr\n'
              '# Version 0.0.74 test 2: SPIDYVR PICTURE - the picture settings set once to SpidyVR\'s '
              '(migrated; a value chosen after this line stays: spidyvr_picture = custom uses yours)\n'
              'spidyvr_picture = on\nflare_block = on\nspidyvr_stock = on\naa_mode = clean\nmiles_taa = off\n'
              'view_jitter_fix = on\nclean_jitter = on\nheap_guard = off\nbatmobile = off\ncave_quiet = on\n')


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


def value(values, name):
    if name in values:
        return values[name]
    return L.UPDATE38_DEFAULTS.get(name, L.UPDATE37_DEFAULTS.get(name))


class Retired(unittest.TestCase):
    def test_no_picture_settings_left(self):
        for name in L.RETIRED_074:
            self.assertNotIn(name, L.SETTINGS, name)
            self.assertNotIn(name, L.UPDATE38_DEFAULTS, name)
            self.assertNotIn(name, L.UPDATE38_KEYS, name)
            self.assertNotIn(f'"{name}"', src('include/spidy/combat38.hpp'), name)

    def test_parse_skips_them_quietly(self):
        values, warnings = L.parse('aa_mode = clean\nspidyvr_picture = on\nview_jitter_fix = on\ncave_quiet = off\n')
        self.assertEqual(warnings, [])
        self.assertEqual(set(values) & set(L.RETIRED_074), set())
        self.assertEqual(values.get('cave_quiet'), 'off')  # the vehicle work's settings still read

    def test_picture_defaults_are_0073(self):
        for name, want in DEFAULTS_0073.items():
            self.assertEqual(value({}, name), want, name)
        self.assertEqual(L.image_style_of({}), 'off')
        self.assertTrue(L.lens_flares_off(None))
        self.assertFalse(hasattr(L, 'screen_shake_off'))

    def test_removed_sources_gone(self):
        for f in ('heap_guard', 'jitter_proof', 'miles_taa074', 'taa_diag074', 'taa_gate074', 'taa_hold074',
                  'taa_link074', 'view_jitter'):
            self.assertFalse((ROOT / 'include/spidy' / f'{f}.hpp').exists(), f)
            self.assertNotIn(f'spidy/{f}.hpp', src('src/game_xr.cpp'), f)
        for export in ('SpidySpidyvrPicture', 'SpidyAaMode', 'SpidyMilesTaa', 'SpidyViewJitterFix', 'SpidyCleanJitterOn',
                       'SpidyHeapGuard', 'SpidyTaaHold', 'SpidyEngineTaaStock'):
            self.assertNotIn(f'WINAPI {export}(', src('src/game_xr.cpp'), export)

    def test_vehicle_work_kept(self):
        g = src('src/game_xr.cpp')
        for export in ('SpidyBatmobile', 'SpidyCrimeGps', 'SpidyFingerRay', 'SpidyCaveQuiet', 'SpidyDashInside',
                       'SpidyCockpitGlow', 'SpidyWheelFirst', 'SpidyMpSoloTest', 'SpidyHideouts', 'SpidyCarMusic'):
            self.assertIn(f'WINAPI {export}(', g, export)


class Migrate(unittest.TestCase):
    def run_on(self, text):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text(text, encoding='utf-8')
            shown = L.migrate_074t10(p)
            once = p.read_text(encoding='utf-8')
            again = L.migrate_074t10(p)
            self.assertIsNone(again)
            self.assertEqual(p.read_text(encoding='utf-8'), once, 'a second run changed live.txt')
            return shown, once

    def test_a_test2_file_gets_the_0073_picture(self):
        shown, text = self.run_on(TEST2_FILE)
        self.assertIn('0.0.73', shown)
        self.assertIn(L.MIGRATED_074T10, text)
        self.assertNotIn(L.OLD_MARKER_074T2, text)
        values, warnings = L.parse(text)
        self.assertEqual(warnings, [])
        for name in L.RETIRED_074 + L.RESET_074T10:
            self.assertNotIn(name, values, name)
        for name, want in DEFAULTS_0073.items():
            self.assertEqual(value(values, name), want, name)
        self.assertEqual(values['crawl_cap'], 'off')    # the player's other settings stay
        self.assertEqual(values['batmobile'], 'off')
        self.assertEqual(values['cave_quiet'], 'on')

    def test_a_0073_file_keeps_its_picture(self):
        own = 'image = hero_vr_classic\naa = 2\nsharpen = 140\nimage_style = spidyvr\n'
        shown, text = self.run_on(own)
        self.assertIsNone(shown)  # nothing taken out: no message
        self.assertTrue(text.startswith(own))
        self.assertIn(L.MIGRATED_074T10, text)

    def test_retired_lines_out_even_without_the_test2_marker(self):
        shown, text = self.run_on('sharpen = 120\naa_mode = off\nmiles_taa = on\n')
        self.assertIn('aa_mode', shown)
        self.assertIn('sharpen = 120', text)
        self.assertNotIn('aa_mode', text.replace(L.MIGRATED_074T10, ''))
        self.assertNotIn('miles_taa', text)

    def test_choice_after_the_marker_stays(self):
        _, text = self.run_on('')
        text += 'sharpen = 90\n'
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text(text, encoding='utf-8')
            self.assertIsNone(L.migrate_074t10(p))
            self.assertIn('sharpen = 90', p.read_text(encoding='utf-8'))

    def test_never_raises(self):
        self.assertIsNone(L.migrate_074t10('/nonexistent/dir/live.txt'))
        self.assertIn(L.MIGRATED_074T10, L.HEADER)  # a new live.txt starts migrated

    def test_launcher_runs_it_before_reading_the_picture(self):
        r = src('tools/run_game_vr.py')
        at = r.index("live_settings.migrate_074t10(ROOT/'live.txt')")
        self.assertLess(r.index("live_settings.migrate_073t5("), at)
        self.assertLess(at, r.index('game_taa_look = live_settings.image_style_in_file'))
        self.assertLess(at, r.index('lens_flares_off('))


if __name__ == '__main__':
    unittest.main()
