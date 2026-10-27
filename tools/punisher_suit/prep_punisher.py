"""Version 0.0.72 PUNISHER SUIT kit, box prep (the same route as 0.0.7's CAPTAIN AMERICA suit:
docs/CAP-SUIT-PIPELINE.md and docs/PUNISHER-SUIT.md). Two suits, one chosen with --suit:

  VEST     'The Punisher (Vest-Version)' by floh, CC BY-NC 4.0 (free mod, never monetized), ~25k tris
  WARZONE  'Punisher Warzone' by spybg (Valentin Yovchev), CC BY 4.0, ~2.6k faces (4.8k tris)

Neither is rigged. This reads the Sketchfab download (its OBJ + MTL, or its glTF folder; only read) and
writes a clean glTF for Blender, exactly what tools/cap_suit/blender/fit_suit_to_rig.py and
transfer_weights.py take (they rig and weight it to Peter's skeleton from the game on the PC):

  * orientation: glTF Y-up, facing +Z (the toes measured forward of the shins); OBJ is read as Y-up
    unless --up z; scale: metres, feet on y = 0, centred, HEIGHT_M tall (CANDIDATE: the fit rescales);
  * geometry: polygons fanned to triangles, corners welded, zero-area triangles dropped, each material
    slot (one per colour texture) under Luna Engine IO's 65535 vertices a submesh;
  * prep-report.json and CREDITS.txt next to it. No Marvel credit: only the artist.

  prep_punisher.py --suit vest|warzone <download folder> <out folder> [--up y|z]"""
import argparse, json, os, shutil, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'cap_suit'))
sys.path.insert(0, os.path.join(HERE, '..', 'models007'))
import prep_suit  # noqa: E402  (orient, weld, load for glTF)

HEIGHT_M = prep_suit.HEIGHT_M
SUBMESH_VERTS = prep_suit.SUBMESH_VERTS
SUITS = {
    'vest': dict(title='The Punisher (Vest-Version)', artist='floh', licence='CC BY-NC 4.0',
                 licence_url='https://creativecommons.org/licenses/by-nc/4.0/',
                 url='https://sketchfab.com/3d-models/the-punisher-vest-version-2d7deb3b811e42419f71263871dd7139',
                 uid='2d7deb3b811e42419f71263871dd7139', note=' HERO VR is free and never monetized (NonCommercial).'),
    'warzone': dict(title='Punisher Warzone', artist='spybg (Valentin Yovchev)', licence='CC BY 4.0',
                    licence_url='https://creativecommons.org/licenses/by/4.0/',
                    url='https://sketchfab.com/3d-models/punisher-warzone-20b5eba6b24c463cb3a2666cd1ab4ba6',
                    uid='20b5eba6b24c463cb3a2666cd1ab4ba6', note=''),
}


def credit(suit):
    s = SUITS[suit]
    return ("'%s' by %s, %s, licensed %s (%s).%s Changes: re-oriented, rescaled, triangulated, welded, "
            "material slots by texture (tools/punisher_suit/prep_punisher.py); fitted and weighted to the "
            "game's rig on the user's PC." % (s['title'], s['artist'], s['url'], s['licence'], s['licence_url'], s['note']))


def find(src, ext):
    hits = []
    for root, _, files in os.walk(src):
        hits += [os.path.join(root, f) for f in files if f.lower().endswith(ext)]
    return sorted(hits)


def read_mtl(path):
    mats, cur = {}, None
    for line in open(path, encoding='utf-8', errors='replace'):
        w = line.strip().split(None, 1)
        if not w:
            continue
        if w[0] == 'newmtl':
            cur = w[1].strip() if len(w) > 1 else ''
            mats[cur] = None
        elif w[0] == 'map_Kd' and cur is not None and len(w) > 1:
            # the last token is the file (options like -s 1 1 1 may come first)
            mats[cur] = w[1].split()[-1].replace('\\', '/')
    return mats


def load_obj(path, up):
    """-> prims like prep_suit.load: dict(material, tex, pos, nrm, uv, idx), tex a texture path or ''."""
    V, T, N = [], [], []
    groups = {}  # material -> list of (vi, ti, ni) corner triples
    mats, mat = {}, ''
    base = os.path.dirname(path)
    for line in open(path, encoding='utf-8', errors='replace'):
        if not line or line[0] == '#':
            continue
        w = line.split()
        if not w:
            continue
        if w[0] == 'v':
            V.append([float(x) for x in w[1:4]])
        elif w[0] == 'vt':
            T.append([float(x) for x in (w[1:3] + ['0'])[:2]])
        elif w[0] == 'vn':
            N.append([float(x) for x in w[1:4]])
        elif w[0] == 'mtllib':
            p = os.path.join(base, ' '.join(w[1:]))
            if os.path.isfile(p):
                mats.update(read_mtl(p))
        elif w[0] == 'usemtl':
            mat = ' '.join(w[1:])
        elif w[0] == 'f':
            corners = []
            for c in w[1:]:
                parts = (c.split('/') + ['', ''])[:3]
                fix = lambda s, n: (int(s) - 1 if int(s) > 0 else n + int(s)) if s else -1
                corners.append((fix(parts[0], len(V)), fix(parts[1], len(T)), fix(parts[2], len(N))))
            for i in range(1, len(corners) - 1):
                groups.setdefault(mat, []).extend([corners[0], corners[i], corners[i + 1]])
    if not V or not groups:
        raise SystemExit('%s: no vertices or faces' % path)
    V = np.array(V, np.float64)
    T = np.array(T, np.float64) if T else np.zeros((1, 2))
    N = np.array(N, np.float64) if N else np.zeros((1, 3))
    if up == 'z':  # Z-up -> Y-up
        rot = np.array([[1, 0, 0], [0, 0, 1], [0, -1, 0]], np.float64)
        V, N = V @ rot.T, N @ rot.T
    prims = []
    for m, cs in groups.items():
        c = np.array(cs, np.int64)
        if (c[:, 0] < 0).any() or (c[:, 0] >= len(V)).any():
            raise SystemExit('%s: a face index is out of range' % path)
        pos = V[c[:, 0]]
        uv = T[np.clip(c[:, 1], 0, len(T) - 1)] * (c[:, 1:2] >= 0)
        idx = np.arange(len(c)).reshape(-1, 3)
        if (c[:, 2] >= 0).all() and len(N) > 1:
            nrm = N[c[:, 2]]
        else:  # flat normals from the faces
            a, b, d = pos[idx[:, 0]], pos[idx[:, 1]], pos[idx[:, 2]]
            fn = np.cross(b - a, d - a)
            nrm = np.repeat(fn, 3, axis=0)
        nrm = nrm / np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-12)
        tex = mats.get(m) or ''
        prims.append(dict(material=m, tex=os.path.join(base, tex) if tex else '', pos=pos, nrm=nrm, uv=uv, idx=idx))
    return prims


def load_gltf(src):
    g, prims = prep_suit.load(src)
    for p in prims:
        p['tex'] = os.path.join(src, g['images'][g['textures'][p['tex']]['source']]['uri'])
        p['material'] = g['materials'][p['material']].get('name', str(p['material']))
    return prims


def orient_y_up(prims):
    """OBJ is Y-up: keep Y as up (any pose: T, A or arms down), the depth the smaller other axis; then the
    same +Z facing test as prep_suit.orient (the feet reach forward of the shins)."""
    P = np.concatenate([p['pos'] for p in prims])
    ext = P.max(0) - P.min(0)
    R = np.eye(3)
    if ext[2] > ext[0]:  # the body's width on X
        R = np.array([[0, 0, 1], [0, 1, 0], [-1, 0, 0]], np.float64)
    Q = P @ R.T
    lo, hi = Q.min(0), Q.max(0)
    h = hi[1] - lo[1]
    y = Q[:, 1] - lo[1]
    feet, shins = Q[y < .04 * h, 2], Q[(y > .08 * h) & (y < .2 * h), 2]
    turned = False
    if len(feet) and len(shins) and feet.mean() < shins.mean():
        R = np.diag([-1., 1., -1.]) @ R
        turned = True
    return R, dict(up_axis='y (OBJ)', turned_180=turned, toes_forward_measured=bool(len(feet) and len(shins)))


def write_gltf(out, slots, images, suit):
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
        pbr = {'metallicFactor': 0.0, 'roughnessFactor': 0.6}
        if tex is not None:
            pbr['baseColorTexture'] = {'index': tex}
        mats.append({'name': name, 'doubleSided': True, 'pbrMetallicRoughness': pbr})
        meshes.append({'name': name, 'primitives': [{'attributes': {'POSITION': ip, 'NORMAL': inn, 'TEXCOORD_0': iu},
                                                      'indices': ii, 'material': k}]})
        nodes.append({'name': name, 'mesh': k})
    g = {'asset': {'version': '2.0', 'generator': 'HERO VR 0.0.72 tools/punisher_suit/prep_punisher.py', 'copyright': credit(suit)},
         'scene': 0, 'scenes': [{'name': 'punisher_suit_' + suit, 'nodes': list(range(len(nodes)))}],
         'nodes': nodes, 'meshes': meshes, 'materials': mats,
         'buffers': [{'uri': 'punisher_suit.bin', 'byteLength': len(bin_)}], 'bufferViews': views, 'accessors': accs}
    if images:
        g['textures'] = [{'source': i, 'sampler': 0} for i in range(len(images))]
        g['samplers'] = [{'magFilter': 9729, 'minFilter': 9987, 'wrapS': 10497, 'wrapT': 10497}]
        g['images'] = [{'uri': 'textures/' + os.path.basename(u)} for u in images]
    open(os.path.join(out, 'punisher_suit.bin'), 'wb').write(bytes(bin_))
    json.dump(g, open(os.path.join(out, 'punisher_suit.gltf'), 'w'), indent=1)


def prep(suit, src, out, up='y'):
    suit = suit.lower()
    if suit not in SUITS:
        raise SystemExit('--suit is vest or warzone')
    objs = find(src, '.obj')
    if objs:
        prims, R, orep = None, None, None
        prims = load_obj(max(objs, key=os.path.getsize), up)
        R, orep = orient_y_up(prims)
    elif find(src, '.gltf'):
        prims = load_gltf(os.path.dirname(find(src, '.gltf')[0]))
        R, orep = prep_suit.orient(prims)
    else:
        raise SystemExit('%s: no .obj or .gltf (unzip the Sketchfab download there)' % src)
    P = np.concatenate([p['pos'] for p in prims]) @ R.T
    lo, hi = P.min(0), P.max(0)
    if not (hi[1] - lo[1]) > 0:
        raise SystemExit('the model is flat')
    k = HEIGHT_M / (hi[1] - lo[1])
    shift = np.array([(lo[0] + hi[0]) / 2, lo[1], (lo[2] + hi[2]) / 2])
    os.makedirs(out, exist_ok=True)
    texs = sorted({p['tex'] for p in prims})
    images = [t for t in texs if t and os.path.isfile(t)]
    if images:
        os.makedirs(os.path.join(out, 'textures'), exist_ok=True)
        for t in images:
            shutil.copyfile(t, os.path.join(out, 'textures', os.path.basename(t)))
    slots, rep_slots, dropped = {}, {}, 0
    for n, t in enumerate(texs):
        sel = [p for p in prims if p['tex'] == t]
        pos, nrm, uv, idx, off = [], [], [], [], 0
        for p in sel:
            pos.append((p['pos'] @ R.T - shift) * k)
            nrm.append(p['nrm'] @ R.T)
            uv.append(p['uv'])
            idx.append(p['idx'] + off)
            off += len(p['pos'])
        pos, nrm, uv, idx, bad = prep_suit.weld(np.concatenate(pos), np.concatenate(nrm), np.concatenate(uv), np.concatenate(idx))
        dropped += bad
        name = 'punisher_%s_%d' % (suit, n)
        if len(pos) > SUBMESH_VERTS:
            raise SystemExit('%s has %d vertices (Luna Engine IO takes %d a submesh)' % (name, len(pos), SUBMESH_VERTS))
        slots[name] = (pos, nrm, uv, idx, images.index(t) if t in images else None)
        rep_slots[name] = dict(source_materials=sorted({str(p['material']) for p in sel}),
                               texture=os.path.basename(t) if t in images else None, vertices=int(len(pos)), triangles=int(len(idx)))
    write_gltf(out, slots, images, suit)
    allp = np.concatenate([s[0] for s in slots.values()])
    lo2, hi2 = allp.min(0), allp.max(0)
    rep = dict(suit=suit, source='%s (%s, %s)' % (SUITS[suit]['title'], SUITS[suit]['artist'], SUITS[suit]['licence']),
               source_triangles=int(sum(len(p['idx']) for p in prims)), triangles=int(sum(len(s[3]) for s in slots.values())),
               zero_area_dropped=dropped, orientation=dict(orep, out='glTF Y-up, facing +Z (Blender: Z-up, facing -Y)'),
               scale_to_metres=round(float(k), 6), height_m=round(float(hi2[1] - lo2[1]), 4),
               height_note='CANDIDATE start (%.2f m); fit_suit_to_rig.py rescales to the imported rig' % HEIGHT_M,
               bounds_m=dict(low=np.round(lo2, 4).tolist(), high=np.round(hi2, 4).tolist()), slots=rep_slots,
               submesh_vertex_limit=SUBMESH_VERTS,
               skeleton='none (the source has no rig: weights come from the game rig on the PC, tools/cap_suit/blender)')
    json.dump(rep, open(os.path.join(out, 'prep-report.json'), 'w'), indent=1)
    open(os.path.join(out, 'CREDITS.txt'), 'w').write(credit(suit) + '\n')
    return rep


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--suit', required=True, choices=sorted(SUITS))
    ap.add_argument('--up', default='y', choices=('y', 'z'))
    ap.add_argument('src')
    ap.add_argument('out')
    a = ap.parse_args()
    print(json.dumps(prep(a.suit, a.src, a.out, a.up), indent=1))
