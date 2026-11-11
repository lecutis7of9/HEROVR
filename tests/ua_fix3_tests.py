"""ULTIMATE ALLIANCE EXPANSION fix 3 (Stark Tower test 2 feedback): the player's OWN SM2 hero in UA places (the game's
body posed by native_body, shown through the room by a body-shaped depth cut; include/spidy/ua_body_cut.hpp, the
renderer's setBodyCut) with the UA body only as the fallback, and the room's solid materials opaque (tools/ua/ua_alpha.py:
no stray alpha holes flickering in the furniture)."""
import io, json, os, struct, sys, tempfile, unittest
import numpy as np
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'ua'))
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()


class Wiring(unittest.TestCase):
    def setUp(self):
        self.g = src('src/game_xr.cpp')
        self.r = src('src/d3d12_renderer.cpp')

    def test_native_body_wanted_in_ua(self):
        self.assertIn('gameBodyDrawn(values.body || strictBody || uaVisit.active(),', self.g)

    def test_game_hero_over_the_ua_body(self):
        self.assertIn('ua_body_cut::gameHero(uaHere, saved->flatScreen, native_body::drawn(),', self.g)
        self.assertIn('const uint32_t bodySlot = uaGameHero ? 0u :', self.g)
        self.assertIn('overlay.setBodyCut(std::move(cut));', self.g)
        self.assertIn('who.footY = rendered.anchor.y + travel.y;', self.g)
        # the UA body never locks the game's body out
        self.assertIn('!mesh_model::slotIsUaBody(ws))\n                                        suitShownAtMs.store', self.g)
        # the scene depth asked for in UA places
        self.assertIn('(seeThroughCount || sunShadow || uaCutDepth) && !saved->stereoScreen', self.g)

    def test_renderer_cut_before_the_room(self):
        cut = self.r.index('list_->SetPipelineState(bodyCutPipeline_.Get());')
        box = self.r.index('list_->DrawInstanced(static_cast<UINT>(vertices.size()), 1, 0, 0);')
        self.assertLess(cut, box)
        self.assertIn('RenderTargetWriteMask = 0; // depth only', self.r)
        self.assertIn('out float depth : SV_Depth', self.r)
        self.assertIn('if (p.y < eye.w) discard;', self.r)
        self.assertIn('bodyCutFault_ = std::string("Body cut off (")', self.r)

    def test_no_combat_code_touched(self):
        h = src('include/spidy/ua_body_cut.hpp')
        self.assertNotIn('combat38', h)


def png(alpha):
    from PIL import Image
    a = np.asarray(alpha, np.uint8)
    rgba = np.dstack([np.full_like(a, 200)] * 3 + [a])
    b = io.BytesIO()
    Image.fromarray(rgba, 'RGBA').save(b, 'PNG')
    return b.getvalue()


class Alpha(unittest.TestCase):
    def test_solid_opaque_cutouts_kept(self):
        import ua_alpha
        solid = np.full((32, 32), 255); solid[0, :5] = 0          # a few stray clear texels: solid
        cut = np.full((32, 32), 255); cut[:12, :] = 0              # a real cut-out
        images = [png(solid), png(cut)]
        blob, views = b'', []
        for im in images:
            views.append({'buffer': 0, 'byteOffset': len(blob), 'byteLength': len(im)})
            blob += im + b'\0' * (-len(im) % 4)
        doc = {'asset': {'version': '2.0'}, 'buffers': [{'byteLength': len(blob)}], 'bufferViews': views,
               'images': [{'bufferView': 0, 'mimeType': 'image/png'}, {'bufferView': 1, 'mimeType': 'image/png'}],
               'textures': [{'source': 0}, {'source': 1}],
               'materials': [{'name': 'desk', 'alphaMode': 'BLEND', 'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}}},
                             {'name': 'plant', 'alphaMode': 'BLEND', 'pbrMetallicRoughness': {'baseColorTexture': {'index': 1}}},
                             {'name': 'glass', 'alphaMode': 'BLEND',
                              'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}, 'baseColorFactor': [1, 1, 1, .3]}}]}
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, 'scene.glb')
            ua_alpha.write_glb(p, doc, blob)
            opaque, kept = ua_alpha.solidify(p)
            self.assertEqual(opaque, ['desk'])
            self.assertEqual(sorted(kept), ['glass', 'plant'])
            back, blob2 = ua_alpha.read_glb(p)
            self.assertEqual([m['alphaMode'] for m in back['materials']], ['OPAQUE', 'BLEND', 'BLEND'])
            self.assertEqual(blob2[:len(blob)], blob)

    def test_converter_runs_it(self):
        c = src('tools/ua/convert_ua.py')
        self.assertIn("solid, cut = ua_alpha.solidify(str(out / 'scene.glb'))", c)
        # mesh_model already ignores an OPAQUE material's alpha (no discard there)
        self.assertIn('if (!im.cutout) // opaque materials: their alpha is not a cut', src('src/mesh_model.cpp'))


if __name__ == '__main__':
    unittest.main()
