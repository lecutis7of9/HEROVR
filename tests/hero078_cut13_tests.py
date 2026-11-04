# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.78 cut13: the regressions of the cut12 report (161552) kept fixed.

1. The mod's model pipeline: cut12's prepareModel declared 3 root parameters over a 2-entry array (the PBR layout went
   into prepareArt) - "MODELS: UNTEXTURED from now - Model pipeline unusable (Serialize model root signature
   HRESULT=2147942487)" - so the van was the flat untextured stand-in, the holo roster showed initials discs and the
   suits never went on (all gated on overlay.modelReady()). Every root signature's count must be its array's own size
   and the model root's slots must be the ones its draw binds (the Windows ctest cut13_root serializes both).
2. The black headset until the main menu: the OpenXR session must start AFTER the models are prepared.
cut14: the PBR change is reverted - the roots are cut8's again (still counted by their arrays).
3. The shipped van pack (when present on this machine): every primitive's vertex streams sized and strided right."""
import json
import os
import pathlib
import re
import struct
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


# the cut12 report's console, in order (161552-console.log lines 285, 325, 338, 342)
CUT12_CONSOLE = [
    'Wrist hologram: opened',
    'MODELS: UNTEXTURED from now - Model pipeline unusable (Serialize model root signature HRESULT=2147942487)',
    'PUNISHER VAN (EXPERIMENTAL): no safe spot here, no van',
    'HOLO ROSTER: PLAY AS PUNISHER',
]


def function_body(s, signature):
    a = s.index(signature)
    b = s.index('{', a)
    depth = 0
    for i in range(b, len(s)):
        depth += {'{': 1, '}': -1}.get(s[i], 0)
        if depth == 0:
            return s[b:i + 1]
    raise AssertionError(signature)


class RootSignatures(unittest.TestCase):
    def test_every_count_is_its_array(self):
        s = src('src/d3d12_renderer.cpp')
        found = 0
        for m in re.finditer(r'D3D12_ROOT_SIGNATURE_DESC rs\{([^,]+),\s*parameters', s):
            decl = list(re.finditer(r'D3D12_ROOT_PARAMETER parameters\[(\d+)\]', s[:m.start()]))[-1]
            count = m.group(1).strip()
            self.assertTrue(count == 'static_cast<UINT>(std::size(parameters))' or count == decl.group(1),
                            f'root signature at line {s.count(chr(10), 0, m.start()) + 1}: {count} over [{decl.group(1)}]')
            found += 1
        self.assertGreaterEqual(found, 5)

    def test_model_root_matches_its_draw(self):
        # cut14: the roots as before the PBR change (cut8's), each counted by its own array
        s = src('src/d3d12_renderer.cpp')
        model = function_body(s, 'HRESULT serializeModelRoot(')
        art = function_body(s, 'HRESULT serializeArtRoot(')
        occluded = function_body(s, 'HRESULT serializeOccludedRoot(')
        self.assertIn('D3D12_ROOT_PARAMETER parameters[2]', model)
        self.assertIn('D3D12_ROOT_PARAMETER parameters[2]', art)
        self.assertIn('D3D12_ROOT_PARAMETER parameters[4]', occluded)
        self.assertIn('{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0}', model)  # t0 the colours at heap 0
        self.assertNotIn('ShaderRegister = 2', model)  # no PBR light (b2)
        self.assertIn('Constants.Num32BitValues = 20;', occluded)  # the depth match and WORLD SHADOW's sun
        for body in (model, art, occluded):
            self.assertIn('static_cast<UINT>(std::size(parameters))', body)
        self.assertIn('hr(serializeModelRoot(signature, error), "Serialize model root signature")', s)
        self.assertIn('hr(serializeArtRoot(signature, error), "Serialize art root signature")', s)
        self.assertIn('hr(serializeOccludedRoot(signature, error), "Serialize occluded model root signature")', s)
        self.assertIn('NumDescriptors = modelHeapDescriptors', s)
        self.assertIn('NumDescriptors = artHeapDescriptors', s)
        self.assertIn('list_->SetGraphicsRoot32BitConstants(3, 20, constants, 0);', s)
        self.assertNotIn('SetGraphicsRoot32BitConstants(2, 24', s)
        h = src('include/spidy/d3d12_renderer.hpp')
        self.assertIn('modelHeapDescriptors = 3, artHeapDescriptors = 1', h)
        cm = src('CMakeLists.txt')
        self.assertIn('add_test(NAME cut13_root COMMAND spidy_cut13_root_tests)', cm)

    def test_vertex_layout(self):
        # the three model-drawing layouts read POSITION @0, COLOR @12, TEXCOORD @28 of the 44-byte Vertex (no tangent)
        s = src('src/d3d12_renderer.cpp')
        self.assertEqual(s.count('{"TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 28,'), 4)
        self.assertNotIn('TANGENT', s)
        v = src('include/spidy/vertex.hpp')
        self.assertIn('Vec3 position, color;', v)
        self.assertNotIn('tangent', v.lower())
        t = src('tests/cut13_root_tests.cpp')
        self.assertIn('sizeof(Vertex) == 44', t)
        self.assertIn('offsetof(Vertex, shape) == 28', t)

    def test_what_the_report_lost_is_gated_on_the_pipeline(self):
        # replay: the failure came while the menu and holo were up; everything it took away waits on modelReady()
        self.assertIn('HRESULT=2147942487', CUT12_CONSOLE[1])
        self.assertEqual(0x80070057, 2147942487)  # E_INVALIDARG: NumParameters past the array
        g = src('src/game_xr.cpp')
        self.assertIn('suitsRigged.load() && overlay.modelReady()', g)  # the suit swap
        self.assertIn('overlay.modelReady() ? mesh_model::library().model(mesh_model::slotRoster) : nullptr', g)  # portraits
        self.assertIn('hero007::setModelSink(overlay.modelReady() ? &modelVertices : nullptr);', g)  # the van and gear
        t = src('tests/cut13_root_tests.cpp')
        self.assertIn('serializeModelRoot', t)
        self.assertIn('serializeArtRoot', t)


class BootScreen(unittest.TestCase):
    def test_session_starts_after_the_preparation(self):
        g = src('src/game_xr.cpp')
        run = g[g.index('DWORD WINAPI run(void*) {'):]
        start = run.index('runtime.initialize(device.Get(), queue, config.eyeSize, config.renderScale);')
        self.assertEqual(run.count('runtime.initialize('), 1)
        for prep in ('overlay.initialize(device.Get(), queue);', 'prepareTextures(models, &wicDecode, size, levels)',
                     'MODELS: TEXTURED - the real models are drawn', 'MODELS: prepared in %.1f s'):
            self.assertLess(run.index(prep), start, prep)
        self.assertLess(start, run.index('message(2, 0, "Waiting for the headset");'))
        self.assertLess(run.index('XrRuntime runtime;'), run.index('D3D12Renderer overlay;'))  # torn down after the overlay

    def test_cut12_timings(self):
        # 161552 game log: the main menu's save slots read 16:16:26.988; the first frame's hitch at 16:16:45.188 with
        # qpc 153815.752 -> the session's first frame ~16:16:32: ~5 s of black headset after the menu
        menu = 16 * 3600 + 16 * 60 + 26.988
        first_frame = 16 * 3600 + 16 * 60 + 32
        self.assertGreater(first_frame, menu)


def van_pack():
    for base in filter(None, (os.environ.get('HV_MODELS'), str(ROOT / 'Models'), str(ROOT / 'build' / 'h7out' / 'Models'))):
        p = pathlib.Path(base) / 'Vehicle' / 'PunisherVan' / 'scene.glb'
        if p.exists():
            return p
    return None


SIZES = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}
COMP = {5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4}


def read_glb(p):
    data = p.read_bytes()
    magic, _, total = struct.unpack_from('<III', data, 0)
    assert magic == 0x46546C67 and total == len(data)
    jl, jt = struct.unpack_from('<II', data, 12)
    gltf = json.loads(data[20:20 + jl])
    o = 20 + jl
    bl, bt = struct.unpack_from('<II', data, o)
    return gltf, data[o + 8:o + 8 + bl]


@unittest.skipUnless(van_pack(), 'the shipped van pack is not on this machine (HV_MODELS=<HV077>/Models)')
class VanPack(unittest.TestCase):
    def test_streams_round_trip(self):
        import numpy as np
        gltf, binary = read_glb(van_pack())
        acc, views = gltf['accessors'], gltf['bufferViews']

        def read(i):
            a = acc[i]
            v = views[a['bufferView']]
            n, comp = SIZES[a['type']], COMP[a['componentType']]
            stride = v.get('byteStride', n * comp)
            self.assertGreaterEqual(stride, n * comp)
            start = v.get('byteOffset', 0) + a.get('byteOffset', 0)
            self.assertLessEqual(start + stride * (a['count'] - 1) + n * comp, v.get('byteOffset', 0) + v['byteLength'])
            dt = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
            raw = np.frombuffer(binary, np.uint8, stride * (a['count'] - 1) + n * comp, start)
            rows = np.lib.stride_tricks.as_strided(raw, (a['count'], n * comp), (stride, 1))
            return np.ascontiguousarray(rows).view(dt[a['componentType']]).reshape(a['count'], n)

        tris = 0
        for mesh in gltf['meshes']:
            for prim in mesh['primitives']:
                at = prim['attributes']
                pos = read(at['POSITION'])
                self.assertTrue(np.isfinite(pos).all())
                for key in ('NORMAL', 'TEXCOORD_0'):
                    self.assertIn(key, at)
                    self.assertEqual(len(read(at[key])), len(pos), key)
                nrm = read(at['NORMAL'])
                self.assertTrue((np.abs(np.linalg.norm(nrm, axis=1) - 1) < .02).mean() > .99, 'unit normals')
                idx = read(prim['indices']).ravel()
                self.assertEqual(len(idx) % 3, 0)
                self.assertLess(int(idx.max()), len(pos))
                tris += len(idx) // 3
                mat = gltf['materials'][prim['material']]
                self.assertIn('baseColorTexture', mat.get('pbrMetallicRoughness', {}), mat.get('name'))
        self.assertGreater(tris, 1000)


if __name__ == '__main__':
    unittest.main()
