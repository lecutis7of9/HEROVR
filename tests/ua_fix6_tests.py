"""ULTIMATE ALLIANCE EXPANSION v6: the cast's textures (the shared texture array held 256 layers, the UA slots load last), the
lab's roof and windows (tools/ua/build_stark_tower.py), a stand-in suit as the player's body (include/spidy/ua_body.hpp)."""
import os, re, sys, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()


class Textures(unittest.TestCase):
    def test_array_holds_every_model_and_the_ua_slots_that_load_last(self):
        m = re.search(r'mostTextureLayers = (\d+);', src('include/spidy/mesh_model.hpp'))
        self.assertGreaterEqual(int(m.group(1)), 303 + 64)  # the package's 303 distinct images, with margin
        self.assertLessEqual(int(m.group(1)), 2048)
        self.assertIn('layers.size() > mesh_model::mostTextureLayers', src('src/d3d12_renderer.cpp'))


class Lab(unittest.TestCase):
    def setUp(self):
        self.b = src('tools/ua/build_stark_tower.py')

    def test_roof_at_the_walls(self):
        self.assertIn("z = 490 if (a.place == 'stark2' and a.ceiling == 960) else a.ceiling", self.b)
        for n in ('ua_skirt_s', 'ua_skirt_w', 'ua_skirt_e', 'ua_skirt_nw', 'ua_skirt_ne', 'ua_skirt_bay'):
            self.assertIn("'%s'" % n, self.b)

    def test_window_glass_and_strays(self):
        self.assertIn("a.place == 'stark2' and not images_of(o) and hi[2] - lo[2] > 400", self.b)
        self.assertIn('unplaced prototypes at the engine origin removed', self.b)
        self.assertIn("a.place != 'stark2' or ob.type != 'MESH'", self.b)  # the hub's origin is real geometry: untouched


class Body(unittest.TestCase):
    def test_stand_in_suit(self):
        b = src('include/spidy/ua_body.hpp')
        self.assertIn('enum class Source : uint8_t { none, wornSuit, gameHero, standInSuit };', b)
        self.assertIn('constexpr Source pick(bool inUa, bool flatScreen, bool wornSuitReady, bool standInReady, bool nativeBodyDrawn)', b)
        self.assertIn('Suit_daredevil_1', b)
        g = src('src/game_xr.cpp')
        self.assertIn('ua_body::standInSlot(', g)
        self.assertIn('uaSource == ua_body::Source::standInSuit ? standInSlot', g)
        self.assertIn('"UA BODY: "', g)
        self.assertNotIn('combat38::', b)

    def test_sm2_check_kit(self):
        d = os.path.join(ROOT, 'tools/ua/sm2_body_check')
        for f in ('SM2-BODY-CHECK.cmd', 'sm2model.py', 'sm2scan.py'):
            self.assertTrue(os.path.exists(os.path.join(d, f)), f)
        self.assertIn('sm2model.json', src('tools/ua/sm2_body_check/SM2-BODY-CHECK.cmd'))


if __name__ == '__main__':
    unittest.main()
