"""HERO VR 0.0.74 test 2 SPIDYVR PICTURE and FLARE BLOCK (the owner, 6:36 PM ET: "AA is awful"; his video: jaggies
and shimmer when still, ghost trails on head turns, a green flare at the top centre that moves with the head).
SPIDYVR PICTURE (on by default, CUSTOM to go back) runs the game's TemporalAa in each eye with SpidyVR's jitter and
nothing of the mod's over it; it is set once over an older live.txt. FLARE BLOCK (on, OFF to go back) keeps the
game's lens flares and the light glow sprites out of VR. Made by Agent Mulder."""
import os, pathlib, sys, tempfile, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import live_settings as L  # noqa: E402


def src(p):
    return open(os.path.join(ROOT, p), encoding='utf-8').read()


OWNER_TEST1 = ('# kept by 0.0.74 test 1\nimage = classic\naa = 1\nsharpen = 140\nsharpen_match = on\n'
               'image_style = off\ntaa_jitter = 0\ntaa_jitter_scale = 0\nlens_flares = game\nhistory = 80\n')


class Preset(unittest.TestCase):
    def test_defaults_on(self):
        self.assertEqual(L.UPDATE38_DEFAULTS['spidyvr_picture'], 'on')
        self.assertEqual(L.UPDATE38_DEFAULTS['flare_block'], 'on')
        self.assertTrue(L.picture_preset_of({}))
        self.assertTrue(L.picture_preset_in_file('/nonexistent/live.txt'))
        self.assertFalse(L.picture_preset_of({'spidyvr_picture': 'custom'}))
        self.assertTrue(L.flare_block_of({}))
        self.assertFalse(L.flare_block_of({'flare_block': 'off'}))

    def test_the_preset_is_spidyvrs(self):
        p = L.PICTURE_PRESET
        self.assertEqual((p['aa'], p['sharpen'], p['sharpen_match'], p['image'], p['image_style']),
                         (0, 0, 'off', 'spidyvr', 'spidyvr'))
        self.assertEqual((p['taa_jitter'], p['taa_jitter_scale']), (0, 100))  # OURS (Halton 2, 3) at FULL
        self.assertEqual(L.PICTURE_SUPERSAMPLE, 100)
        for name, value in p.items():
            L.export_value(name, value)  # every value sends
        self.assertEqual(L.image_style_of({'image_style': 'off'}), 'spidyvr')
        # 0.0.74 test 3 SPIDYVR STOCK (on by default) leaves the game's lens flares alone; off: FLARE BLOCK's.
        self.assertFalse(L.lens_flares_off('lens_flares = game\n'))
        self.assertTrue(L.lens_flares_off('spidyvr_stock = off\nlens_flares = game\n'))
        self.assertFalse(L.lens_flares_off('spidyvr_stock = off\nflare_block = off\nlens_flares = game\n'))

    def test_stock_preset(self):
        """0.0.74 test 3 SPIDYVR STOCK: the picture preset plus every HERO render layer at SpidyVR's value."""
        s = L.STOCK_PRESET
        for name, value in L.PICTURE_PRESET.items():
            self.assertEqual(s[name], value)
        self.assertEqual((s['history'], s['trail'], s['eye_temporal'], s['eye_bloom'], s['bloom_fx'], s['shared_lod'],
                          s['far_shadows'], s['lod_fade'], s['lod_reach'], s['impostors'], s['reach'], s['color_fx'],
                          s['foveated'], s['vignette']),
                         (0, 0, 0, 0, 0, 0, 0, 0, 100, 1, 150, 0, 'off', 0))
        for name, value in s.items():
            L.export_value(name, value)
        self.assertEqual(L.STOCK_SUPERSAMPLE, 100)
        self.assertTrue(L.stock_of({}) and L.stock_of({'spidyvr_stock': 'on'}) and not L.stock_of({'spidyvr_stock': 'off'}))
        self.assertEqual(L.UPDATE38_DEFAULTS['spidyvr_stock'], 'on')
        self.assertEqual(L.render_preset_in_file('/nonexistent/live.txt')[1], 'SPIDYVR STOCK')
        self.assertEqual(L.image_style_of({'spidyvr_picture': 'custom'}), 'spidyvr')  # stock implies the game's TAA
        h = src('include/spidy/combat38.hpp')
        self.assertIn('"spidyvr_stock", "SPIDYVR STOCK"', h)
        self.assertIn('stockValue', src('src/game_xr.cpp'))
        probe = src('src/stereo_probe.cpp')
        self.assertIn('engineTaaLive.load() && !stockOn.load(std::memory_order_relaxed))\n                taaHoldJob', probe)
        self.assertIn('!stockImage.load() && (colorOn.load()', src('src/native_eye_gpu.cpp'))

    def test_help_lines(self):
        h = src('include/spidy/combat38.hpp')
        for line in ("ON: the game's TAA per eye like SpidyVR, no mod AA or sharpening; no restart.",
                     'ON hides lens flares and light glow sprites in VR, restart VR to apply.'):
            self.assertIn(line, h)
            self.assertLessEqual(len(line), 80)


class Migration(unittest.TestCase):
    def run_once(self, body, supersample=150):
        with tempfile.TemporaryDirectory() as d:
            live, nxt = pathlib.Path(d, 'live.txt'), pathlib.Path(d, 'next-launch.txt')
            live.write_text(body, encoding='utf-8')
            L.write_next_launch(nxt, supersample)
            line = L.migrate_074t2(live, nxt)
            text = live.read_text(encoding='utf-8')
            again = L.migrate_074t2(live, nxt)
            return line, L.parse(text)[0], text, again, live.read_text(encoding='utf-8'), L.next_launch_supersample(nxt)

    def test_owner_test1_live_txt(self):
        line, values, text, again, after, scale = self.run_once(OWNER_TEST1)
        self.assertIn('SPIDYVR PICTURE on', line)
        self.assertIn('150% -> 100%', line)
        self.assertEqual(scale, 100)
        for name in ('aa', 'sharpen', 'taa_jitter'):
            self.assertEqual(float(values[name]), 0, name)
        self.assertEqual(float(values['taa_jitter_scale']), 100)
        self.assertEqual(str(values['image']), 'spidyvr')
        self.assertEqual(str(values['image_style']), 'spidyvr')
        self.assertEqual(str(values['sharpen_match']), 'off')
        self.assertEqual(str(values['lens_flares']), 'off_in_vr')
        self.assertEqual(str(values['spidyvr_picture']), 'on')
        self.assertEqual(str(values['flare_block']), 'on')
        self.assertIn('history = 80', text)
        self.assertIn(L.MIGRATED_074T2, text)
        self.assertIsNone(again)
        self.assertEqual(text, after)  # once only

    def test_a_choice_after_the_marker_stays(self):
        line, values, *_ = self.run_once(L.MIGRATED_074T2 + '\nspidyvr_picture = custom\naa = 1\n')
        self.assertIsNone(line)
        self.assertEqual(str(values['spidyvr_picture']), 'custom')
        self.assertFalse(L.picture_preset_of(values))

    def test_none_and_header(self):
        self.assertIsNone(L.migrate_074t2('/nonexistent/dir/live.txt'))
        self.assertIn(L.MIGRATED_074T2, L.HEADER)
        line, values, *_ = self.run_once('', 100)
        self.assertIn('SPIDYVR PICTURE on', line)
        self.assertNotIn('->', line)

    def test_run_game_vr_wiring(self):
        r = src('tools/run_game_vr.py')
        self.assertLess(r.index('live_settings.migrate_0073(ROOT/'), r.index('live_settings.migrate_074t2(ROOT/'))
        self.assertLess(r.index('live_settings.migrate_074t2(ROOT/'), r.index('game_taa_look = live_settings.image_style_in_file'))
        self.assertLess(r.index('live_settings.migrate_074t2(ROOT/'), r.index('migrated_next = live_settings.migrate_next_launch')
                        if 'migrated_next = live_settings.migrate_next_launch' in r else len(r))
        self.assertIn("for name, value in live_settings.PICTURE_PRESET.items():", r)
        self.assertIn("mip_bias_asked = '0'", r)


class Dll(unittest.TestCase):
    def test_game_xr_holds_the_preset(self):
        g = src('src/game_xr.cpp')
        self.assertIn('const bool picture = combat38::spidyvrPictureOn(c38(combat38::spidyvrPicture));', g)
        self.assertIn('native_gpu::setCleanImage(picture || (image_style::spidy(style) &&', g)
        self.assertIn('SpidyEngineTaaJitterNow() != et::packJitter(et::ours, et::fullJitterScale)', g)
        self.assertIn('if (!picture && step.eyeAaOn', g)
        self.assertIn('combat38::flareBlockOn(c38(combat38::flareBlock)) ? combat38::glowHidden', g)
        self.assertIn('setHero007(v, combat38::spidyvrPicture, 5496)', g)
        self.assertIn('setHero007(v, combat38::flareBlock, 5497)', g)


if __name__ == '__main__':
    unittest.main()
