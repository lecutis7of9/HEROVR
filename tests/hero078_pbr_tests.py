# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.78 PBR MATERIALS: the mod's models lit per pixel (normal maps, ORM, GGX, the sky's reflection) in the
game's frame light, the two live.txt switches (5643-5644), the maps' own arrays and the per-vertex fallback."""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Pbr(unittest.TestCase):
    def test_switches(self):
        g = src('src/game_xr.cpp')
        for key, export, code in (('model_pbr', 'SpidyModelPbr', 5643), ('model_textures_hq', 'SpidyModelTexturesHq', 5644)):
            self.assertIn(f'WINAPI {export}(void* v)', g)
            self.assertEqual(len(re.findall(rf'\b{code}\b', g)), 1)
            rng, ex, text = live_settings.SETTINGS[key]
            self.assertEqual((rng, ex), ((0, 1), export))
            self.assertEqual(live_settings.CHOICE_NAMES[key], ('on', 'off'))
            self.assertIn(key, live_settings.START_ONLY)
            self.assertIn('0 on, the default', text)
        self.assertTrue(live_settings.SETTINGS['model_pbr'][2].endswith('- live, no restart needed'))
        self.assertIn('spidy::model_pbr::setEnabled(modelPbrNow.load() == 0);', g)
        self.assertIn('overlay.setModelLight(packed);', g)
        self.assertLess(g.index('overlay.setModelLight(packed);'),
                        g.index('overlay.renderViews(overlayViews, vertices, true, false, translucent, &grade, art, modelVertices);'))
        self.assertIn('prepareTextures(models, &wicDecode, size, levels, mapOptions)', g)
        self.assertIn('overlay.setModelMaps(maps)', g)

    def test_every_model_path_emits_a_surface(self):
        for p in ('src/hero007_visual.cpp', 'src/van_fps.cpp', 'src/suit_wear.cpp'):
            s = src(p)
            self.assertIn('model_pbr::drawsPbr(wl)', s, p)
            self.assertIn('model_pbr::material(', s, p)
            self.assertIn('model_pbr::corner(', s, p)
        for p in ('src/hero007_visual.cpp', 'src/van_fps.cpp'):  # car paint, the cabin's roles and glow keep their looks
            self.assertIn('!painted && !glow && role == 0', src(p), p)
        self.assertIn('model_pbr::reflect(v[i].surface, m.normal)', src('include/spidy/vr_mirror.hpp'))
        self.assertNotIn('model_pbr', src('src/hideout074.cpp'))

    def test_shaders_and_fallback(self):
        r = src('src/d3d12_renderer.cpp')
        self.assertIn('modelPbr_ = compileModelShaders(pbr, "spidy_model_pbr"', r)
        self.assertIn('occludedPbr_ = compileModelShaders(pbr, "spidy_occluded_model_pbr"', r)
        self.assertIn('static_assert(sizeof(Vertex) == 64', r)
        for name in ('lsun', 'lcolor', 'lup', 'ldown', 'leye', 'lmirror', 'SampleGrad'):
            self.assertIn(name, r)
        # no raw-string piece past MSVC's 16380-byte cap
        for m in re.finditer(r'R"\((.*?)\)"', r, re.S):
            self.assertLess(len(m.group(1).encode('utf-8')), 16380)

    def test_dumps_keep_eleven_floats(self):
        for p in ('tools/models072/model_dump.cpp', 'tools/suits/suit_bench.cpp'):
            self.assertIn('offsetof(Vertex, surface)', src(p), p)

    def test_banned_names(self):
        banned = ('jim' + 'jam', 'mi' + 'les')  # spelt apart so this file holds neither
        for p in ('include/spidy/model_pbr.hpp', 'include/spidy/model_pbr_mesh.hpp', 'tests/model_pbr_tests.cpp'):
            low = src(p).lower()
            for b in banned:
                self.assertNotIn(b, low, p)


if __name__ == '__main__':
    unittest.main()
