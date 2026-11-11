"""ULTIMATE ALLIANCE EXPANSION fix 2 (Stark Tower test 1 feedback): the black chunks outside the windows (the backdrop
a sphere past the sky box), the furniture's flicker (models drawn relative to the eye; duplicate faces dropped) and
the player's body in UA places (Models/UA/Body, include/spidy/ua_body.hpp, tools/ua/ua_body.py)."""
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
        self.assertEqual(r.count('modelViewProjection[eye].data()'), 2)  # the model and the occluded pipelines
        self.assertIn('model_origin::shift(textured, modelOrigin, modelShifted_);', r)

    def test_body_drawn_in_ua(self):
        self.assertIn('ua_body::pick(uaHere, saved->flatScreen, wornReady,', self.g)
        self.assertIn('suitRig = mesh_model::slotIsUaBody(ws) ? &uaBodyRig : &suitRigs[ws - mesh_model::slotSuitFirst];', self.g)
        self.assertIn('t.feet = ua_body::feet(t.eye, saved->hideoutFloor + travel.y, ua_expansion::eyeHeight);', self.g)
        self.assertIn('ok = suit_wear::makeRig(*ub, uaBodyRig, "UA_Body");', self.g)
        self.assertIn('(uaVisit.active() && !flatScreen && !suitShownNow())', self.g)  # the gloves when no body
        m = src('include/spidy/mesh_model.hpp')
        self.assertIn('"UA/StarkTower", "UA/StarkTower/Cast", "UA/Body"};', m)

    def test_no_combat_code_touched(self):
        b = src('include/spidy/ua_body.hpp')
        self.assertIn('struct Drive', b)  # combat and powers plug in here later
        self.assertNotIn('combat38::', b)

    def test_converter_writes_the_body(self):
        c = src('tools/ua/convert_ua.py')
        self.assertIn("build_body(a, wad, work, out.parent / 'Body', env)", c)
        self.assertIn('--no-body', c)
        b = src('tools/ua/build_stark_tower.py')
        self.assertIn('duplicate faces dropped', b)
        self.assertIn('sky lid', b)  # the open-topped sky box closed (the black band at the windows' top)


class BodySuit(unittest.TestCase):
    def test_suit_space_and_hand_back(self):
        import ua_body
        p = ua_body.to_suit_space(np.array([[1.0, 2.0, 3.0]]))
        self.assertEqual(p.tolist(), [[2.0, 3.0, 1.0]])  # MUA1 z up / +x forward / +y left -> y up / +z forward / +x left
        self.assertEqual(ua_body.FOLDER, 'UA/Body')
        self.assertTrue(ua_body.BODY_PACKAGE.endswith('spiderman_0901.fb'))

    def test_made_suit_when_built(self):
        glb = os.environ.get('HV_UA_BODY_GLB')
        if not glb or not os.path.exists(glb):
            self.skipTest('no converted Models/UA/Body (bring your own game); HV_UA_BODY_GLB=<scene.glb>')
        import json, struct
        data = open(glb, 'rb').read()
        n = struct.unpack_from('<I', data, 12)[0]
        j = json.loads(data[20:20 + n])
        suit = j['asset']['extras']['suit']
        self.assertEqual((len(suit['joints']), len(suit['ends']), len(suit['handBack'])), (48, 9, 6))
        a = j['meshes'][0]['primitives'][0]['attributes']
        for k in ('POSITION', 'NORMAL', 'TEXCOORD_0', 'JOINTS_0', 'WEIGHTS_0'):
            self.assertIn(k, a)
        J = np.array(suit['joints']).reshape(16, 3)
        self.assertGreater(J[:, 1].max(), 1.3)  # a standing body, metres


if __name__ == '__main__':
    unittest.main()
