"""ULTIMATE ALLIANCE EXPANSION fix 2 (Stark Tower test 1 feedback): the black chunks outside the windows (the backdrop
a sphere past the sky box), the furniture's flicker (models drawn relative to the eye; duplicate faces dropped) and
the player's body in UA places (include/spidy/ua_body.hpp; v4 retired MUA's Spider-Man body: tools/ua/ua_body.py and
build_body.py are gone, the converter builds no Models/UA/Body)."""
import os, sys, unittest
import numpy as np
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'ua'))
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()


class Wiring(unittest.TestCase):
    def setUp(self):
        self.g = src('src/game_xr.cpp')

    def test_backdrop_reach_in_ua(self):
        self.assertIn('saved->hideoutView.ua ? hideout074::uaBackdropMost : 150.f', self.g)
        h = src('include/spidy/hideout074.hpp')
        self.assertIn('inline constexpr float uaBackdropMost = 190.f;', h)
        self.assertIn('appendBackdropSphere(out, eye, most);', src('src/hideout074.cpp'))

    def test_models_relative_to_the_eye_in_rooms(self):
        self.assertIn('overlay.setModelOrigin(model_origin::of(viewer));', self.g)
        r = src('src/d3d12_renderer.cpp')
        self.assertEqual(r.count('modelViewProjection[eye].data()'), 3)  # the model and the occluded pipelines, fix 3's body cut
        self.assertIn('model_origin::shift(textured, modelOrigin, modelShifted_);', r)

    def test_body_drawn_in_ua(self):
        self.assertIn('ua_body::pick(uaHere, saved->flatScreen, wornReady, nativeDrawn)', self.g)
        self.assertIn('suitRig = &suitRigs[ws - mesh_model::slotSuitFirst];', self.g)
        self.assertIn('t.feet = ua_body::feet(t.eye, saved->hideoutFloor + travel.y, ua_expansion::eyeHeight);', self.g)
        # v4: MUA's body is gone - no rig, never loaded (even from a v3 install's leftover folder), no gloves in UA places
        self.assertNotIn('uaBodyRig', self.g)
        self.assertNotIn('uaBodyReady', self.g)
        self.assertIn('if (mesh_model::slotIsUaBody(slot)) {\n                    models.meshes[slot] = {};', self.g)
        self.assertIn('suitShownNow()) &&\n                        !uaVisit.active())', self.g)
        b = src('include/spidy/ua_body.hpp')
        self.assertNotIn('Source::uaBody', b)
        m = src('include/spidy/mesh_model.hpp')
        self.assertIn('"UA/StarkTower", "UA/StarkTower/Cast", "UA/Body"};', m)  # the slot's number stays (the layout)

    def test_no_combat_code_touched(self):
        b = src('include/spidy/ua_body.hpp')
        self.assertIn('struct Drive', b)  # combat and powers plug in here later
        self.assertNotIn('combat38::', b)

    def test_converter_builds_no_body_any_more(self):
        c = src('tools/ua/convert_ua.py')
        self.assertNotIn('build_body', c)
        self.assertNotIn('--no-body', c)
        self.assertFalse(os.path.exists(os.path.join(ROOT, 'tools/ua/ua_body.py')))
        b = src('tools/ua/build_stark_tower.py')
        self.assertIn('duplicate faces dropped', b)
        self.assertIn('sky lid', b)  # the open-topped sky box closed (the black band at the windows' top)


if __name__ == '__main__':
    unittest.main()
