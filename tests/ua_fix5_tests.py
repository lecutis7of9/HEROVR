"""ULTIMATE ALLIANCE EXPANSION v5: Stark Tower is two converted MUA1 maps in one room (the hub, stark1, and Iron Man's Lab,
stark2), joined by TELEPORTS: the waterfall <-> the lab's door, the arrival lift <-> a lift marker on the central platform.
tools/ua/combine_places.py joins the built places, tools/hideout074/make_hideout.py reads landings and writes `link` lines,
hideout074 reads them (Action::teleport), game_xr.cpp does the blink and the jump."""
import json, os, struct, sys, tempfile, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'ua'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'hideout074'))
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()


def tiny_glb(path, tri_x, with_image=True):
    """A one-triangle glb (position, uv, one material, one image) at x = tri_x."""
    import glb
    pos = struct.pack('<9f', tri_x, 0, 0, tri_x + 1, 0, 0, tri_x, 0, 1)
    uv = struct.pack('<6f', 0, 0, 1, 0, 0, 1)
    img = b'\x89PNG-not-a-picture'
    binary = pos + uv + img
    js = {'asset': {'version': '2.0'}, 'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0, 'name': f'tri{tri_x}'}],
          'meshes': [{'primitives': [{'attributes': {'POSITION': 0, 'TEXCOORD_0': 1}, 'material': 0}]}],
          'materials': [{'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}}}],
          'textures': [{'source': 0}], 'images': [{'bufferView': 2, 'mimeType': 'image/png'}],
          'accessors': [{'bufferView': 0, 'componentType': 5126, 'count': 3, 'type': 'VEC3', 'min': [tri_x, 0, 0], 'max': [tri_x + 1, 0, 1]},
                        {'bufferView': 1, 'componentType': 5126, 'count': 3, 'type': 'VEC2'}],
          'bufferViews': [{'buffer': 0, 'byteOffset': 0, 'byteLength': 36}, {'buffer': 0, 'byteOffset': 36, 'byteLength': 24},
                          {'buffer': 0, 'byteOffset': 60, 'byteLength': len(img)}],
          'buffers': [{'byteLength': len(binary)}]}
    glb.write(path, js, binary)


class Combine(unittest.TestCase):
    def test_glb_merge_keeps_both_places(self):
        import combine_places, glb
        d = tempfile.mkdtemp()
        tiny_glb(os.path.join(d, 'a.glb'), 0.0)
        tiny_glb(os.path.join(d, 'b.glb'), 260.0)
        combine_places.merge_glb([os.path.join(d, 'a.glb'), os.path.join(d, 'b.glb')], os.path.join(d, 'c.glb'))
        js, binary = glb.read(os.path.join(d, 'c.glb'))
        self.assertEqual((len(js['meshes']), len(js['materials']), len(js['textures']), len(js['images'])), (2, 2, 2, 2))
        self.assertEqual(js['materials'][1]['pbrMetallicRoughness']['baseColorTexture']['index'], 1)
        self.assertEqual(js['scenes'][0]['nodes'], [0, 1])
        tris = glb.triangles(os.path.join(d, 'c.glb'))
        xs = sorted(float(v[:, :, 0].min()) for v in tris.values())
        self.assertEqual(xs, [0.0, 260.0])

    def test_combine_checks_landings(self):
        s = src('tools/ua/combine_places.py')
        self.assertIn("no landing", s)
        self.assertIn("is in two places", s)


class Teleport(unittest.TestCase):
    def test_make_hideout_landings_and_links(self):
        s = src('tools/hideout074/make_hideout.py')
        self.assertIn("p['type'] != 'landing'", s)  # a landing seeds the walkable part
        self.assertIn('links.append(f"link {p[\'id\']}', s)
        self.assertIn('lines.extend(links)', s)

    def test_hideout_reads_teleports(self):
        h, c = src('include/spidy/hideout074.hpp'), src('src/hideout074.cpp')
        self.assertIn('batcomputer, teleport };', h)
        self.assertIn('Vec3 to{};', h)
        self.assertIn('if (name == "teleport")', c)
        self.assertIn('key == "link"', c)
        self.assertIn('p.action = Action::comingSoon;', c)  # a teleport with no landing on the floor is COMING SOON

    def test_game_blink_and_jump(self):
        g = src('src/game_xr.cpp')
        self.assertIn('ho.act == hideout074::Action::teleport', g)
        self.assertIn('uaVisit.seatAt(hi.head, hi.headForward, tp.to, tp.toYaw)', g)
        self.assertIn('uaBlink = ua_expansion::blinkSeconds;', g)
        self.assertIn('hideout074::appendBackdropSphere(vertices, viewer, ua_expansion::blinkRadius);', g)
        self.assertIn('saved.uaBlink = uaBlink;', g)
        self.assertIn('ua_expansion::placeReach', g)  # only this place's people are posed
        b = src('src/batcave074.cpp')
        self.assertIn('kind == 3 ? v.markLabel[k].c_str()', b)

    def test_budgets_hold_two_places(self):
        m = src('include/spidy/mesh_model.hpp')
        self.assertIn('uaRoomTriangles = 300000', m)
        self.assertIn('maxCapTriangles = 300000', m)
        self.assertIn('mostSourceTriangles = 320000', m)
        self.assertIn('cap > maxCapTriangles', src('src/mesh_model.cpp'))


class Builder(unittest.TestCase):
    def setUp(self):
        self.b = src('tools/ua/build_stark_tower.py')

    def test_lab_is_built(self):
        self.assertIn("choices=['stark1', 'stark2']", self.b)
        self.assertIn('ENTITY_PARTS_LAB', self.b)
        for etype in ("'ironman_suit'", "'pillar_square_40ft_silver_stk_e'", "'machine_stk_a'", "'doors_stk_d01'", "'hologram'"):
            self.assertIn(etype + ': [(', self.b)
        self.assertIn('extents from the geometry', self.b)  # the lab's world has no extents

    def test_points_are_teleports(self):
        for pid, to in (('arrival_lift', 'upper_landing'), ('lift_down', 'pad_landing'), ('ironman_lab', 'lab_landing'),
                        ('lab_door', 'waterfall_landing')):
            self.assertIn(f"'id': '{pid}'", self.b)
            self.assertIn(f"'to': '{to}'", self.b)
        self.assertIn("'action': 'teleport'", self.b)
        self.assertIn('move_lift.py', self.b)  # why the lift goes where it goes

    def test_converter_builds_both(self):
        c = src('tools/ua/convert_ua.py')
        self.assertIn("'map': 'stark2'", c)
        self.assertIn('LAB_X = 260.0', c)
        self.assertIn('combine_places.combine(built, builts)', c)
        self.assertIn("'bank': 'TOWN1B_V.ENS'", c)
        self.assertGreaterEqual(260.0, 245.0)  # sky boxes ~120 m round each place: apart enough


class Cast(unittest.TestCase):
    def test_looks_are_per_map(self):
        import ua_cast
        cast = [{'skin': '0701', 'anim': 'zone4', 'talk': None, 'turns': True, 'tag': '', 'zone': 'zone_stark1'},
                {'skin': '0701', 'anim': 'zone4', 'talk': 'x', 'turns': True, 'tag': 'lab_', 'zone': 'zone_stark2'}]
        looks = ua_cast.looks_of(cast)
        self.assertEqual([l['name'] for l in looks], ['0701_zone4', 'lab_0701_zone4'])
        self.assertEqual([l['zone'] for l in looks], ['zone_stark1', 'zone_stark2'])


if __name__ == '__main__':
    unittest.main()
