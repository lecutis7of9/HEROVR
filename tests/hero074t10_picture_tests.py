# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.74 test 10: the picture is version 0.0.72's again, with only 0.0.73's light-flicker fixes; the 0.0.74
vehicle work stays.

The owner: "our build looked best in the last public build so go back to that one", then "Don't you mean going to
72 since returning it off on 73 made it work for him" and "Keep whatever fixed the flickering lights". Every
image / AA setting the 0.0.74 tests 2-9 added is retired (migrate_074t10), and so are 0.0.73's picture switches
0.0.72 never had (RETIRED_073, migrate_074t10_72): parse() skips them quietly, the DLL has no such option or
export. The flicker fixes stay: EYE BLOOM OFF, LENS FLARES OFF IN VR, LIGHT HOLD, SHADOW MATCH, CULL MARGIN,
LIGHT CHECKS.
"""
import pathlib
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402

# The 0.0.72 release's picture defaults (live_settings at the v0.0.72 tag), with 0.0.73's light-flicker fixes.
DEFAULTS_0072 = {'aa': 1, 'sharpen': 140, 'taa_jitter': 3, 'history': 80, 'eye_temporal': 1,
                 # kept from 0.0.73 (the flickering lights)
                 'lens_flares': 'off_in_vr', 'eye_bloom': 0, 'light_hold': 'on', 'shadow_match': 'on',
                 'cull_margin': '10deg', 'light_checks': 'on'}
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

    def test_picture_defaults_are_0072(self):
        for name, want in DEFAULTS_0072.items():
            self.assertEqual(value({}, name), want, name)
        for name in L.RETIRED_073:
            self.assertNotIn(name, L.SETTINGS, name)
            self.assertNotIn(name, L.UPDATE38_DEFAULTS, name)
            self.assertNotIn(name, L.UPDATE38_KEYS, name)
            self.assertNotIn(f'"{name}"', src('include/spidy/combat38.hpp'), name)
        self.assertFalse(hasattr(L, 'image_style_of'))
        self.assertIn("p.add_argument('--mip-bias', default='0.5',", src('tools/run_game_vr.py'))  # 0.0.72's
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

    def test_a_test2_file_gets_the_0072_picture(self):
        shown, text = self.run_on(TEST2_FILE)
        self.assertIn('0.0.73', shown)
        self.assertIn(L.MIGRATED_074T10, text)
        self.assertNotIn(L.OLD_MARKER_074T2, text)
        values, warnings = L.parse(text)
        self.assertEqual(warnings, [])
        for name in L.RETIRED_074 + L.RESET_074T10:
            self.assertNotIn(name, values, name)
        for name in ('aa', 'sharpen', 'taa_jitter', 'lens_flares'):
            self.assertEqual(value(values, name), DEFAULTS_0072[name], name)
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
        at72 = r.index("live_settings.migrate_074t10_72(ROOT/'live.txt')")
        self.assertLess(at, at72)  # the 0.0.74 test lines first, then 0.0.73's picture lines
        self.assertLess(at72, r.index('lens_flares_off('))
        self.assertLess(at72, r.index('for name, value in live_settings.UPDATE37_DEFAULTS.items()'))
        self.assertNotIn('image_style_in_file', r)


# A 0.0.73 release live.txt: IMAGE SPIDYVR's migration marker and its aa 0 / sharpen 0, GAME TAA, STREAMER MODE.
FILE_0073 = ('crawl_cap = off\n'
             '# Version 0.0.73 IMAGE: test 2/3 AA values moved to SPIDYVR once (migrated)\n'
             'image = spidyvr\nimage_style = off\naa = 0\nsharpen = 0\nsharpen_match = on\nstreamer_mode = on\n'
             'streamer_smooth = low\nstreamer_eye = left_eye\nwindow_pop = on\nsl_watch = on\nglow_check = game\n'
             'light_hold = strong\nlight_checks = off\n')


class Migrate72(unittest.TestCase):
    def run_on(self, text):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text(text, encoding='utf-8')
            shown = L.migrate_074t10_72(p)
            once = p.read_text(encoding='utf-8')
            again = L.migrate_074t10_72(p)
            self.assertIsNone(again)
            self.assertEqual(p.read_text(encoding='utf-8'), once, 'a second run changed live.txt')
            return shown, once

    def test_old_marker_is_0073s(self):
        self.assertTrue(L.OLD_MARKER_073T4.startswith('# Version 0.0.73 IMAGE'))

    def test_a_0073_file_gets_the_0072_picture_and_keeps_the_light_fixes(self):
        shown, text = self.run_on(FILE_0073)
        self.assertIn('0.0.72', shown)
        self.assertIn(L.MIGRATED_074T10_72, text)
        values, warnings = L.parse(text)
        self.assertEqual(warnings, [])
        for name in L.RETIRED_073:
            self.assertNotIn(name, values, name)
        self.assertEqual(float(values['aa']), 1)
        self.assertEqual(float(values['sharpen']), 140)
        self.assertEqual(values['light_hold'], 'strong')   # the player's flicker-fix choices stay
        self.assertEqual(values['light_checks'], 'off')    # (the two-GPU pop-up's answer)
        self.assertEqual(values['crawl_cap'], 'off')

    def test_aa_and_sharpen_only_with_the_0073_marker(self):
        shown, text = self.run_on('aa = 0\nsharpen = 0\nimage = spidyvr\n')
        values, _ = L.parse(text)
        self.assertEqual(float(values['aa']), 0)        # set by the player (no 0.0.73 marker): stays
        self.assertEqual(float(values['sharpen']), 0)
        self.assertIn('image', shown)
        self.assertNotIn('image', values)

    def test_other_values_stay(self):
        _, text = self.run_on(FILE_0073.replace('sharpen = 0', 'sharpen = 90'))
        values, _ = L.parse(text)
        self.assertEqual(float(values['aa']), 1)        # 0.0.73 SPIDYVR's 0: back to 0.0.72's FXAA
        self.assertEqual(float(values['sharpen']), 90)  # a sharpen the player chose stays

    def test_a_0072_file_untouched_but_marked(self):
        own = 'aa = 1\nsharpen = 140\nhistory = 80\n'
        shown, text = self.run_on(own)
        self.assertIsNone(shown)
        self.assertTrue(text.startswith(own))
        self.assertIn(L.MIGRATED_074T10_72, text)

    def test_choice_after_the_marker_stays(self):
        _, text = self.run_on('')
        text += L.OLD_MARKER_073T4 + '\nsharpen = 0\n'
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text(text, encoding='utf-8')
            self.assertIsNone(L.migrate_074t10_72(p))
            self.assertIn('sharpen = 0', p.read_text(encoding='utf-8'))

    def test_never_raises_and_header_marked(self):
        self.assertIsNone(L.migrate_074t10_72('/nonexistent/dir/live.txt'))
        self.assertIn(L.MIGRATED_074T10_72, L.HEADER)

    def test_parse_skips_retired_quietly(self):
        values, warnings = L.parse(''.join(f'{n} = on\n' for n in L.RETIRED_073) + 'light_hold = on\n')
        self.assertEqual(warnings, [])
        self.assertEqual(set(values) & set(L.RETIRED_073), set())
        self.assertEqual(values.get('light_hold'), 'on')


class Monitor(unittest.TestCase):
    """0.0.74 test 10 MONITOR (owner's choice (a)): the PC window always shows the headset's left eye with the mod's
    drawings (0.0.73's STREAMER copy path, always on): no wrist row, no live.txt key, no launcher switch. Sharpening
    does not stop it. On any problem the window keeps the game's own view (0.0.72) for the session; VR goes on."""

    def test_always_on_in_play(self):
        g = src('src/game_xr.cpp')
        self.assertIn('const bool inPlay = drawn == Presentation::immersive;', g)
        self.assertIn('native_gpu::streamer(inPlay);', g)
        self.assertIn('const auto& shown = targets[0];', g)
        self.assertIn('native_gpu::keepShownEye(shown.texture, orientation, fov);', g)
        self.assertIn('native_gpu::keepShownEye(targets[0].texture);', g)  # menus: 0.0.72's mirror
        self.assertNotIn('combat38::streamer', g)
        self.assertNotIn('SpidyStreamer', g)

    def test_not_blocked_by_sharpening(self):
        n = src('src/native_eye_gpu.cpp')
        self.assertIn('(sharpening && !streaming)', n)
        self.assertIn('(!sharpening || shownView.streamer)', n)
        self.assertIn('else if (mirrorWanted || monitorStreaming())', n)

    def test_fail_safe_falls_back_for_the_session(self):
        n = src('src/native_eye_gpu.cpp')
        for why in ('otherGpu', 'floatBackBuffer', 'deviceLost', 'pipelineFailed', 'noCopy'):
            self.assertIn('monitorFallBack(sv0::' + why + ')' if why != 'noCopy'
                          else 'monitorFallBack(spidy::streamer_view::noCopy)', n, why)
        self.assertIn('FAILED(device->GetDeviceRemovedReason())', n)
        self.assertIn('GetContainingOutput', n)
        self.assertIn('device->GetAdapterLuid()', n)
        self.assertIn('return; // never the old mirror over the game\'s own view', n)
        v = src('include/spidy/streamer_view.hpp')
        self.assertIn('struct Latch', v)
        self.assertIn('constexpr uint32_t monitorSmooth = smoothLow;', v)
        self.assertNotIn('modeOn', v)
        self.assertNotIn('eyeIndex', v)

    def test_one_console_line(self):
        g = src('src/game_xr.cpp')
        self.assertIn('consoleLine(streamer_view::pathLine(path));', g)
        self.assertEqual(g.count('streamer_view::pathLine(path)'), 2)

    def test_no_setting_anywhere(self):
        self.assertNotIn('streamer', src('include/spidy/combat38.hpp').lower().replace('0.0.73 streamer', ''))
        self.assertNotIn('combatStreamer', src('include/spidy/wrist_hud.hpp'))
        self.assertNotIn('combatStreamer', src('src/wrist_hud.cpp'))
        live = src('tools/live_settings.py')
        for key in ('streamer_mode', 'streamer_smooth', 'streamer_eye'):
            self.assertIn(key, L.RETIRED_073)  # an old live.txt line is skipped quietly
            self.assertNotIn(key, L.UPDATE38_DEFAULTS)
        self.assertNotIn('Streamer', src('tools/launch-game-vr.ps1'))
        self.assertNotIn('streamer', src('tools/run_game_vr.py').lower())
        self.assertIn("    'monitor': ((0, 240), 'SpidyMonitorRate',", live)

if __name__ == '__main__':
    unittest.main()
