"""Version 0.0.7 CAPTAIN AMERICA suit swap, box prep (step 1 of docs/CAP-SUIT-PIPELINE.md).

Reads the downloaded 'Captain America' by iqbalmulyawan (Sketchfab, CC BY 4.0; no skeleton, T-pose)
from its folder (only read) and writes a clean glTF for Blender:

  * orientation: glTF Y-up, the character facing +Z (Blender's glTF importer turns that into Z-up,
    facing -Y, Blender's front view); the facing is measured (the toes point forward of the ankles);
  * scale: metres, feet on the ground (y = 0), centred over the origin, HEIGHT_M tall (a CANDIDATE
    start only: fit_suit_to_rig.py rescales to the imported rig's own height);
  * geometry: duplicate corners welded (position + normal + uv), zero-area triangles dropped, every
    material slot checked against Luna Engine IO's 65535 vertices a submesh;
  * material slots: the 4 source materials, 3 of which share one texture, merged to 2 named slots
    (cap_suit_body, cap_suit_gear); Sketchfab's emissive copy of the base colour dropped;
  * a report (prep-report.json) and the credit (CREDITS.txt) next to the output.

  prep_suit.py <suit source folder> <out folder>"""
import json, os, shutil, sys
import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'models007'))
import gltf_io  # noqa: E402

HEIGHT_M = 1.80          # CANDIDATE: Peter's in-game height is not known here; the rig decides on the PC
SUBMESH_VERTS = 65535    # Luna Engine IO v2.0.0 Beta: vertices a submesh (its Nexus page)
SLOT_NAMES = {0: 'cap_suit_body', 1: 'cap_suit_gear'}  # by base-colour texture index
CREDIT = ("'Captain America' by iqbalmulyawan, https://sketchfab.com/3d-models/captain-america-d4898ea3aac3474fa0661de3ade9b7c6, "
          "licensed CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/). Changes: re-oriented, rescaled, welded, "
          "material slots merged (tools/cap_suit/prep_suit.py); fitted and weighted to the game's rig on the user's PC.")


def load(src):
    g, parts = gltf_io.load(src)
    bin_ = open(os.path.join(src, g['buffers'][0]['uri']), 'rb').read()
    # normals too (gltf_io.load gives positions + uvs); walk the same nodes for each primitive's NORMAL
    out = []

    def walk(ni, m):
        n = g['nodes'][ni]
        m = m @ gltf_io.node_matrix(n)
        if 'mesh' in n:
            for p in g['meshes'][n['mesh']]['primitives']:
                if p.get('mode', 4) != 4:
                    continue
                pos = gltf_io.accessor(g, bin_, p['attributes']['POSITION']).astype(np.float64)
                pos = (np.c_[pos, np.ones(len(pos))] @ m.T)[:, :3]
                nrm = gltf_io.accessor(g, bin_, p['attributes']['NORMAL']).astype(np.float64) @ np.linalg.inv(m[:3, :3])
                nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-12)
                uv = gltf_io.accessor(g, bin_, p['attributes']['TEXCOORD_0']).astype(np.float64)
                idx = gltf_io.accessor(g, bin_, p['indices']).ravel().reshape(-1, 3)
                tex = g['materials'][p['material']]['pbrMetallicRoughness']['baseColorTexture']['index']
                out.append(dict(material=p['material'], tex=tex, pos=pos, nrm=nrm, uv=uv, idx=idx))
        for c in n.get('children', []):
            walk(c, m)

    for ni in g['scenes'][g.get('scene', 0)]['nodes']:
        walk(ni, np.eye(4))
    assert sum(len(p['idx']) for p in out) == sum(len(q['pos']) for q in parts)
    return g, out


def orient(prims):
    """-> (R 3x3, report): Y-up (the tallest of the two non-arm axes ... the height is measured as the
    axis whose extent is the largest once the T-pose arm span is excluded), facing +Z."""
    P = np.concatenate([p['pos'] for p in prims])
    ext = P.max(0) - P.min(0)
    # the arm span is the largest extent in a T-pose; of the other two, the larger is the height
    span = int(np.argmax(ext))
    rest = [a for a in range(3) if a != span]
    up = max(rest, key=lambda a: ext[a])
    depth = [a for a in rest if a != up][0]
    R = np.zeros((3, 3))
    R[0, span], R[1, up], R[2, depth] = 1, 1, 1
    if np.linalg.det(R) < 0:
        R[0] *= -1
    Q = P @ R.T
    lo, hi = Q.min(0), Q.max(0)
    h = hi[1] - lo[1]
    if np.mean(Q[:, 1] > lo[1] + .5 * h) < .3:  # most of a body is above its middle (torso, head, arms)
        R[1] *= -1; R[0] *= -1
        Q = P @ R.T
        lo, hi = Q.min(0), Q.max(0)
    # the facing: the feet (the lowest 4 %) reach forward of the shins (8 to 20 % up)
    y = Q[:, 1] - lo[1]
    feet = Q[y < .04 * h, 2].mean()
    shins = Q[(y > .08 * h) & (y < .2 * h), 2].mean()
    if feet < shins:  # toes along -Z: turn 180 degrees about Y
        R[0] *= -1; R[2] *= -1
        Q = P @ R.T
    assert abs(np.linalg.det(R) - 1) < 1e-9
    return R, dict(span_axis='xyz'[span], up_axis='xyz'[up], toes_forward_m_source_units=float(abs(feet - shins)))


def weld(pos, nrm, uv, idx):
    key = np.round(np.c_[pos * 1e5, nrm * 1e3, uv * 1e5]).astype(np.int64)
    _, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    idx = inv.ravel()[idx]
    a, b, c = pos[first][idx[:, 0]], pos[first][idx[:, 1]], pos[first][idx[:, 2]]
    good = np.linalg.norm(np.cross(b - a, c - a), axis=1) > 1e-12
    used, idx2 = np.unique(idx[good], return_inverse=True)
    return pos[first][used], nrm[first][used], uv[first][used], idx2.reshape(-1, 3), int((~good).sum())


def write_gltf(out, slots, images):
    """slots: name -> (pos, nrm, uv, idx). Textures copied beside it."""
    bin_ = bytearray()
    views, accs, meshes, nodes, mats = [], [], [], [], []

    def add(arr, ctype, typ, target, minmax=False):
        while len(bin_) % 4:
            bin_.append(0)
        b = arr.tobytes()
        views.append({'buffer': 0, 'byteOffset': len(bin_), 'byteLength': len(b), 'target': target})
        bin_.extend(b)
        a = {'bufferView': len(views) - 1, 'componentType': ctype, 'count': int(len(arr)), 'type': typ}
        if minmax:
            a['min'], a['max'] = arr.min(0).tolist(), arr.max(0).tolist()
        accs.append(a)
        return len(accs) - 1

    for k, (name, (pos, nrm, uv, idx, tex)) in enumerate(slots.items()):
        ip = add(pos.astype(np.float32), 5126, 'VEC3', 34962, True)
        inn = add(nrm.astype(np.float32), 5126, 'VEC3', 34962)
        iu = add(uv.astype(np.float32), 5126, 'VEC2', 34962)
        ii = add(idx.astype(np.uint32).ravel(), 5125, 'SCALAR', 34963)
        mats.append({'name': name, 'doubleSided': True,
                     'pbrMetallicRoughness': {'baseColorTexture': {'index': tex}, 'metallicFactor': 0.0, 'roughnessFactor': 0.6}})
        meshes.append({'name': name, 'primitives': [{'attributes': {'POSITION': ip, 'NORMAL': inn, 'TEXCOORD_0': iu},
                                                      'indices': ii, 'material': k}]})
        nodes.append({'name': name, 'mesh': k})
    g = {'asset': {'version': '2.0', 'generator': 'HERO VR 0.0.7 tools/cap_suit/prep_suit.py',
                   'copyright': CREDIT},
         'scene': 0, 'scenes': [{'name': 'cap_suit', 'nodes': list(range(len(nodes)))}],
         'nodes': nodes, 'meshes': meshes, 'materials': mats,
         'textures': [{'source': i, 'sampler': 0} for i in range(len(images))],
         'samplers': [{'magFilter': 9729, 'minFilter': 9987, 'wrapS': 10497, 'wrapT': 10497}],
         'images': [{'uri': 'textures/' + os.path.basename(u)} for u in images],
         'buffers': [{'uri': 'cap_suit.bin', 'byteLength': len(bin_)}], 'bufferViews': views, 'accessors': accs}
    open(os.path.join(out, 'cap_suit.bin'), 'wb').write(bytes(bin_))
    json.dump(g, open(os.path.join(out, 'cap_suit.gltf'), 'w'), indent=1)


def prep(src, out):
    g, prims = load(src)
    R, orep = orient(prims)
    P = np.concatenate([p['pos'] for p in prims]) @ R.T
    lo, hi = P.min(0), P.max(0)
    k = HEIGHT_M / (hi[1] - lo[1])
    shift = np.array([(lo[0] + hi[0]) / 2, lo[1], (lo[2] + hi[2]) / 2])
    os.makedirs(os.path.join(out, 'textures'), exist_ok=True)
    texs = sorted({p['tex'] for p in prims})
    images = []
    for t in texs:
        uri = g['images'][g['textures'][t]['source']]['uri']
        shutil.copyfile(os.path.join(src, uri), os.path.join(out, 'textures', os.path.basename(uri)))
        images.append(uri)
    slots, rep_slots, dropped = {}, {}, 0
    for t in texs:
        sel = [p for p in prims if p['tex'] == t]
        pos, nrm, uv, idx, off = [], [], [], [], 0
        for p in sel:
            pos.append((p['pos'] @ R.T - shift) * k)
            nrm.append(p['nrm'] @ R.T)
            uv.append(p['uv'])
            idx.append(p['idx'] + off)
            off += len(p['pos'])
        pos, nrm, uv, idx, bad = weld(np.concatenate(pos), np.concatenate(nrm), np.concatenate(uv), np.concatenate(idx))
        dropped += bad
        assert len(pos) <= SUBMESH_VERTS, '%s has %d vertices (Luna Engine IO takes %d a submesh)' % (SLOT_NAMES[t], len(pos), SUBMESH_VERTS)
        slots[SLOT_NAMES[t]] = (pos, nrm, uv, idx, texs.index(t))
        rep_slots[SLOT_NAMES[t]] = dict(source_materials=sorted({g['materials'][p['material']]['name'] for p in sel}),
                                        texture=os.path.basename(images[texs.index(t)]),
                                        vertices=int(len(pos)), triangles=int(len(idx)))
    write_gltf(out, slots, images)
    allp = np.concatenate([s[0] for s in slots.values()])
    lo2, hi2 = allp.min(0), allp.max(0)
    # the T-pose arms: the shoulder line and the wrists (for the fit script's arm check)
    rep = dict(source='captain_america (iqbalmulyawan, CC BY 4.0)', source_triangles=int(sum(len(p['idx']) for p in prims)),
               triangles=int(sum(len(s[3]) for s in slots.values())), zero_area_dropped=dropped,
               orientation=dict(orep, out='glTF Y-up, facing +Z (Blender: Z-up, facing -Y)'),
               scale_to_metres=round(float(k), 5), height_m=round(float(hi2[1] - lo2[1]), 4),
               height_note='CANDIDATE start (%.2f m); fit_suit_to_rig.py rescales to the imported rig' % HEIGHT_M,
               bounds_m=dict(low=np.round(lo2, 4).tolist(), high=np.round(hi2, 4).tolist()),
               arm_span_m=round(float(hi2[0] - lo2[0]), 4), slots=rep_slots, submesh_vertex_limit=SUBMESH_VERTS,
               pose='T-pose (arms along X)', skeleton='none (the source has no rig: weights come from the game rig on the PC)')
    json.dump(rep, open(os.path.join(out, 'prep-report.json'), 'w'), indent=1)
    open(os.path.join(out, 'CREDITS.txt'), 'w').write(CREDIT + '\n')
    return rep


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    print(json.dumps(prep(sys.argv[1], sys.argv[2]), indent=1))
