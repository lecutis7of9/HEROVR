# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.78 WORLD LIGHT: the mod's models (heroes, suits, props, vehicles) lit by the game's frame light
(g_World, read by stereo_probe.cpp's hook of its fill), the GPU sun shadow from the eyes' scene depth, and the
three live.txt switches (5640-5642)."""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class WorldLight(unittest.TestCase):
    def test_switches(self):
        g = src('src/game_xr.cpp')
        for key, export, code, rng, names in (
                ('world_light', 'SpidyWorldLight', 5640, (0, 2), ('game', 'game_soft', 'studio')),
                ('world_shadow', 'SpidyWorldShadow', 5641, (0, 2), ('on', 'vehicles', 'off')),
                ('world_light_gain', 'SpidyWorldLightGain', 5642, (25, 400), None)):
            self.assertIn(f'WINAPI {export}(void* v)', g)
            self.assertEqual(len(re.findall(rf'\b{code}\b', g)), 1)
            self.assertEqual(live_settings.SETTINGS[key][0], rng)
            self.assertEqual(live_settings.SETTINGS[key][1], export)
            self.assertTrue(live_settings.SETTINGS[key][2].endswith('- live, no restart needed'))
            self.assertIn(key, live_settings.START_ONLY)
            if names:
                self.assertEqual(live_settings.CHOICE_NAMES[key], names)

    def test_every_model_path_takes_the_frame_light(self):
        for p in ('src/hero007_visual.cpp', 'src/van_fps.cpp', 'src/suit_wear.cpp'):
            s = src(p)
            self.assertIn('world_light::current()', s, p)
            self.assertIn('world_light::shadowAlpha(wl, share)', s, p)
        h = src('src/hero007_visual.cpp')
        self.assertIn('vanfps::drawPrepared', h)  # the van, Batmobiles and bikes go through the van path
        self.assertIn('car_paint::shade(', h)
        self.assertNotIn('world_light', src('src/hideout074.cpp'))  # hideouts are not world lit

    def test_hook_and_gate(self):
        p = src('src/stereo_probe.cpp')
        self.assertIn('fillRva', p)
        self.assertIn('worldFillHook', p)
        self.assertIn('SpidyWorldLightData', p)
        g = src('src/game_xr.cpp')
        self.assertIn('wl::setCurrent(light);', g)
        self.assertIn('spidy::world_light::wantsDepthAlways(shadowMode, sunLight)', g)
        self.assertLess(g.index('wl::setCurrent(light);'), g.index('hero007::setModelSink(overlay.modelReady()'))

    def test_shader(self):
        r = src('src/d3d12_renderer.cpp')
        self.assertIn('float blocked(float3 p)', r)
        self.assertIn('parameters[3].Constants.Num32BitValues = 20;', r)
        self.assertIn('SetGraphicsRoot32BitConstants(3, 20, constants, 0)', r)
        self.assertIn('if (bias.z > 0 && d > 1e-7 && d <= 1)', r)

    def test_banned_names(self):
        banned = ('jim' + 'jam', 'mi' + 'les')  # spelt apart so this file holds neither
        for p in ('include/spidy/world_light.hpp', 'src/world_light.cpp', 'tests/world_light_tests.cpp'):
            low = src(p).lower()
            for b in banned:
                self.assertNotIn(b, low)


if __name__ == '__main__':
    unittest.main()
