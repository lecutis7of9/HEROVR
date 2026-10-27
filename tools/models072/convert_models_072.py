"""Version 0.0.72 REAL MODELS converter: the Sketchfab sources (CC BY) to the GLBs the mod ships, keeping
their real geometry, UVs and base-colour textures (no per-triangle colours, no 700-triangle crush).

  python3 convert_models_072.py <sources: /workspace/hero-0.0.7-models> <out: .../Models>

Each model keeps its source triangles unless it is over its VR target (TARGET below), and then only
Blender's collapse decimation (bpy_decimate.py under the bpy venv) cuts it, which keeps the UV seams.
The frames (grip at the origin, real size, the hinge / strap pivots) are the 0.0.7 converter's
(tools/models007), measured the same way. JOKER (CC BY-NC-ND) is copied byte for byte.

Output GLB: per material one primitive with POSITION, NORMAL, TEXCOORD_0, uint32 indices and a
material with baseColorFactor and (when the source has one) a baseColorTexture whose image is
embedded (the source's own PNG / JPEG, scaled to at most 1024 px for the download size)."""
import io, json, os, shutil, struct, subprocess, sys, tempfile
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'models007'))
import gltf_io  # noqa: E402

BPY = os.environ.get('HERO_BPY', '/workspace/bpy-venv/bin/python')
MAX_TEX = 1024
# The VR targets (triangles a model; mesh_model::drawTriangles is the runtime cap, 24000). A
# source at or under its target ships whole.
TARGET = {'Batarang/EIRA': 7000, 'Batarang/CLOONEY': 1428, 'Pistol': 4707, 'SawedOff/Frame': 9000,
          'SawedOff/Barrels': 12000, 'Baton/Left': 4000, 'Baton/Right': 4000, 'Shield': 24000, 'GrappleGun': 10272}


def decimate(tris, uv, mat, target):
    if len(tris) <= target:
        return tris, uv, mat
    with tempfile.TemporaryDirectory(prefix='sm2072_dec_') as d:
        a, b = os.path.join(d, 'in.npz'), os.path.join(d, 'out.npz')
        np.savez(a, tris=tris, uv=uv, mat=mat, target=target)
        subprocess.run([BPY, os.path.join(HERE, 'bpy_decimate.py'), a, b], check=True, capture_output=True)
        r = np.load(b)
        return r['tris'], r['uv'], r['mat']


def normals(tris, crease_deg=50.0):
    """Per-corner normals: the angle-weighted face normals of the corners at the same point whose face
    turns less than crease_deg from this one (hard edges stay hard)."""
    e1, e2 = tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0]
    fn = np.cross(e1, e2)
    ln = np.linalg.norm(fn, axis=1, keepdims=True)
    fn = np.where(ln > 0, fn / np.maximum(ln, 1e-30), [0, 1, 0])
    pts = tris.reshape(-1, 3)
    key = np.round(pts / 1e-6).astype(np.int64)
    _, inv = np.unique(key, axis=0, return_inverse=True)
    inv = inv.ravel()
    corner_face = np.repeat(np.arange(len(tris)), 3)
    order = np.argsort(inv, kind='stable')
    out = np.zeros((len(pts), 3))
    cosc = np.cos(np.radians(crease_deg))
    bounds = np.r_[0, np.flatnonzero(np.diff(inv[order])) + 1, len(order)]
    for s, e in zip(bounds[:-1], bounds[1:]):
        cs = order[s:e]
        f = fn[corner_face[cs]]
        if len(cs) == 1:
            out[cs] = f
            continue
        sim = (f @ f.T) >= cosc
        acc = sim.astype(np.float64) @ f
        l2 = np.linalg.norm(acc, axis=1, keepdims=True)
        out[cs] = np.where(l2 > 1e-12, acc / np.maximum(l2, 1e-30), f)
    return out.reshape(-1, 3, 3)


def image_bytes(path):
    """The source image, scaled to at most MAX_TEX on its long side, in its own format."""
    im = Image.open(path)
    fmt = 'PNG' if path.lower().endswith('.png') else 'JPEG'
    if max(im.size) > MAX_TEX:
        k = MAX_TEX / max(im.size)
        im = im.resize((max(1, round(im.size[0] * k)), max(1, round(im.size[1] * k))), Image.LANCZOS)
    b = io.BytesIO()
    if fmt == 'JPEG':
        im.convert('RGB').save(b, 'JPEG', quality=92)
    else:
        im.save(b, 'PNG', optimize=True)
    return b.getvalue(), 'image/png' if fmt == 'PNG' else 'image/jpeg', list(im.size)


def materials_of(src, g, notes):
    out = []
    for m in g.get('materials', []):
        p = m.get('pbrMetallicRoughness', {})
        factor = list(p.get('baseColorFactor', [1, 1, 1, 1]))
        t = p.get('baseColorTexture')
        img = None if t is None else os.path.join(src, g['images'][g['textures'][t['index']]['source']]['uri'])
        if img is None and max(factor[:3]) < .02:
            # Clooney's "Paint - Enamel Glossy (Dark Grey)" has a black factor (a metal's reflections do
            # the rest); the overlay has no reflections, so it is drawn the dark grey its name says
            notes.append('material %r: no texture and a black base colour; drawn dark grey as its name says' % m.get('name'))
            factor = [.0194, .0194, .0228, factor[3]]
        out.append(dict(name=m.get('name', 'material'), image=img, factor=factor, alpha=m.get('alphaMode', 'OPAQUE'),
                        cutoff=m.get('alphaCutoff', .5)))
    return out


def write_glb(path, tris, uv, mat, mats, extras=None):
    """tris (T,3,3) metres, uv (T,3,2), mat (T,) indexes into mats (materials_of)."""
    nrm = normals(tris)
    binb = bytearray()

    def view(data):
        off = len(binb)
        binb.extend(data)
        binb.extend(b'\0' * (-len(binb) % 4))
        g['bufferViews'].append({'buffer': 0, 'byteOffset': off, 'byteLength': len(data)})
        return len(g['bufferViews']) - 1
    asset = {'version': '2.0', 'generator': 'HERO VR 0.0.72 tools/models072'}
    if extras:
        asset['extras'] = extras
    g = {'asset': asset, 'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0}], 'meshes': [{'primitives': []}],
         'buffers': [{}], 'bufferViews': [], 'accessors': [], 'materials': [], 'textures': [], 'images': [], 'samplers': [{}]}
    images = {}
    used = sorted(set(int(m) for m in mat))
    for mi in used:
        m = mats[mi]
        gm = {'name': m['name'], 'pbrMetallicRoughness': {'baseColorFactor': m['factor'], 'metallicFactor': 0}, 'doubleSided': True}
        if m['alpha'] != 'OPAQUE':
            gm['alphaMode'] = 'MASK'
            gm['alphaCutoff'] = m['cutoff']
        if m['image']:
            if m['image'] not in images:
                data, mime, size = image_bytes(m['image'])
                g['images'].append({'bufferView': view(data), 'mimeType': mime, 'name': os.path.basename(m['image'])})
                g['textures'].append({'source': len(g['images']) - 1, 'sampler': 0})
                images[m['image']] = len(g['textures']) - 1
            gm['pbrMetallicRoughness']['baseColorTexture'] = {'index': images[m['image']]}
        g['materials'].append(gm)
        sel = mat == mi
        P = tris[sel].reshape(-1, 3).astype(np.float32)
        N = nrm[sel].reshape(-1, 3).astype(np.float32)
        U = uv[sel].reshape(-1, 2).astype(np.float32)
        key = np.c_[P, N, U]
        uniq, inv = np.unique(np.round(key, 6), axis=0, return_index=False, return_inverse=True)
        inv = inv.ravel().astype(np.uint32)
        first = np.zeros(len(uniq), np.int64)
        first[inv[::-1]] = np.arange(len(inv))[::-1]
        P, N, U = P[first], N[first], U[first]
        acc = len(g['accessors'])
        g['accessors'] += [
            {'bufferView': view(P.tobytes()), 'componentType': 5126, 'count': len(P), 'type': 'VEC3', 'min': P.min(0).tolist(), 'max': P.max(0).tolist()},
            {'bufferView': view(N.tobytes()), 'componentType': 5126, 'count': len(N), 'type': 'VEC3'},
            {'bufferView': view(U.tobytes()), 'componentType': 5126, 'count': len(U), 'type': 'VEC2'},
            {'bufferView': view(inv.tobytes()), 'componentType': 5125, 'count': len(inv), 'type': 'SCALAR'}]
        g['meshes'][0]['primitives'].append({'attributes': {'POSITION': acc, 'NORMAL': acc + 1, 'TEXCOORD_0': acc + 2},
                                             'indices': acc + 3, 'material': len(g['materials']) - 1})
    for k in ('textures', 'images'):
        if not g[k]:
            del g[k]
    if 'textures' not in g:
        del g['samplers']
    g['buffers'][0]['byteLength'] = len(binb)
    j = json.dumps(g, separators=(',', ':')).encode()
    j += b' ' * (-len(j) % 4)
    body = struct.pack('<II', len(j), 0x4E4F534A) + j + struct.pack('<II', len(binb), 0x004E4942) + bytes(binb)
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', 0x46546C67, 2, 12 + len(body)) + body)


def frame(tris, rot, origin_model, length_model, length_m):
    k = length_m / length_model
    return (tris - origin_model) @ np.asarray(rot, np.float64).T * k


def components(tris):
    v = np.round(tris.reshape(-1, 3), 2)
    _, inv = np.unique(v, axis=0, return_inverse=True)
    inv = inv.ravel()
    par = np.arange(inv.max() + 1)

    def f(a):
        while par[a] != a:
            par[a] = par[par[a]]
            a = par[a]
        return a
    for t in inv.reshape(-1, 3):
        a = f(t[0])
        par[f(t[1])] = a
        par[f(t[2])] = a
    return np.array([f(t) for t in inv.reshape(-1, 3)[:, 0]])


def take(src, name):
    s = os.path.join(src, name)
    g, parts = gltf_io.load(s)
    notes = []
    mats = materials_of(s, g, notes)
    if not mats:
        mats = [dict(name='material', image=None, factor=[.5, .5, .5, 1], alpha='OPAQUE', cutoff=.5)]
    tris = np.concatenate([p['pos'] for p in parts]).astype(np.float64)
    uv = np.concatenate([p['uv'] if p['uv'] is not None else np.zeros((len(p['pos']), 3, 2)) for p in parts]).astype(np.float64)
    mat = np.concatenate([np.full(len(p['pos']), p['material'] if p['material'] is not None else 0) for p in parts])
    mesh = np.concatenate([np.full(len(p['pos']), p['mesh']) for p in parts])
    return s, g, tris, uv, mat, mesh, mats, notes


def main(src, out):
    os.makedirs(out, exist_ok=True)
    report = {}

    def write(slot, tris, uv, mat, mats, extras=None, source_n=None):
        d = os.path.join(out, *slot.split('/'))
        os.makedirs(d, exist_ok=True)
        n0 = len(tris)
        t2, u2, m2 = decimate(tris, uv, mat, TARGET[slot])
        if extras and 'layer' in extras:  # the shield: the disc's triangles first (see extras.layer)
            order, split = extras.pop('layer_order')(t2)
            t2, u2, m2 = t2[order], u2[order], m2[order]
            extras['layer'][0] = int(split)
        write_glb(os.path.join(d, 'scene.glb'), t2, u2, m2, mats, extras)
        textured = sorted({os.path.basename(mats[int(i)]['image']) for i in set(m2.tolist()) if mats[int(i)]['image']})
        return dict(source_triangles=int(source_n or n0), triangles=int(len(t2)),
                    reduction='kept whole' if len(t2) == n0 else 'collapse-decimated %d -> %d (UVs kept)' % (n0, len(t2)),
                    textures=textured, uv=True)

    for name, slot in (('batarang', 'Batarang/EIRA'), ('batarang_clooney', 'Batarang/CLOONEY')):
        s, g, tris, uv, mat, mesh, mats, notes = take(src, name)
        report[slot] = dict(source=name, notes=notes + ['the loader fits it to a 20 cm span'], **write(slot, tris, uv, mat, mats))
    # JOKER (CC BY-NC-ND): byte for byte
    s = os.path.join(src, 'batarang_joker')
    d = os.path.join(out, 'Batarang', 'JOKER')
    os.makedirs(d, exist_ok=True)
    for f in ('scene.gltf', 'scene.bin', 'license.txt'):
        shutil.copyfile(os.path.join(s, f), os.path.join(d, f))
    shutil.copytree(os.path.join(s, 'textures'), os.path.join(d, 'textures'), dirs_exist_ok=True)
    g, parts = gltf_io.load(s)
    n = sum(len(p['pos']) for p in parts)
    report['Batarang/JOKER'] = dict(source='batarang_joker', source_triangles=n, triangles=n, reduction='none (ND: copied byte for byte)',
                                    textures=['Material.001_baseColor.png'], uv=True,
                                    notes=['the game decodes and scales its 4096 px texture in memory; the files are unchanged'])
    # the pistol: muzzle at -x in the source
    s, g, tris, uv, mat, mesh, mats, notes = take(src, '9mm_pistol')
    rot = [[0, 0, -1], [0, 1, 0], [1, 0, 0]]
    lo, hi = tris.reshape(-1, 3).min(0), tris.reshape(-1, 3).max(0)
    grip = np.array([lo[0] + .74 * (hi[0] - lo[0]), lo[1] + .66 * (hi[1] - lo[1]), (lo[2] + hi[2]) / 2])
    report['Pistol'] = dict(source='9mm_pistol', notes=notes + ['19 cm, grip at the origin, both hands'],
                            **write('Pistol', frame(tris, rot, grip, hi[0] - lo[0], .19), uv, mat, mats))
    # the sawed-off: barrels split at the hinge
    s, g, tris, uv, mat, mesh, mats, notes = take(src, 'sawed-off_shotgun')
    lo, hi = tris.reshape(-1, 3).min(0), tris.reshape(-1, 3).max(0)
    hinge_z = .07
    cen = tris.mean(1)
    barrels = (cen[:, 2] < hinge_z) & ((cen[:, 1] > .088) | (mesh == 2))
    grip, pivot, eye3 = np.array([0, .12, .30]), np.array([0, .085, hinge_z]), np.eye(3)
    L = hi[2] - lo[2]
    pivot_mod = frame(pivot[None, None, :], eye3, grip, L, .58)[0, 0]
    # both parts in the grip frame (the barrels' pivot in extras), as 0.0.7 (the barrels about the pivot)
    fr = write('SawedOff/Frame', frame(tris[~barrels], eye3, grip, L, .58), uv[~barrels], mat[~barrels], mats)
    br = write('SawedOff/Barrels', frame(tris[barrels], eye3, pivot, L, .58), uv[barrels], mat[barrels], mats,
               {'pivot': pivot_mod.round(5).tolist()})
    report['SawedOff/Frame'] = dict(source='sawed-off_shotgun', notes=notes + ['58 cm, split at the hinge'], **fr)
    report['SawedOff/Barrels'] = dict(source='sawed-off_shotgun', notes=['the barrels about their hinge (extras.pivot)'], **br)
    # Daredevil's batons: rope removed by connectivity, split in two
    s, g, tris, uv, mat, mesh, mats, notes = take(src, 'daredevils_baton')
    comp = np.zeros(len(tris), int)
    base = 0
    for m_ in np.unique(mesh):
        sel = mesh == m_
        r = components(tris[sel])
        comp[sel] = r + base
        base += r.max() + 1
    rope = np.zeros(len(tris), bool)
    for k in np.unique(comp):
        sel = comp == k
        size = np.ptp(tris[sel].reshape(-1, 3), axis=0)
        if size[1] < 20 and max(size[0], size[2]) > 600:
            rope |= sel
    assert rope.any()
    keep = ~rope
    cen = tris.mean(1)
    a = keep & (np.abs(cen[:, 2]) < 100)
    for side, sel in (('Left', a), ('Right', keep & ~a)):
        pts = tris[sel].reshape(-1, 3)
        mean = pts.mean(0)
        axis = np.linalg.svd(pts - mean, full_matrices=False)[2][0]
        proj = (pts - mean) @ axis
        rope_c = tris[rope].reshape(-1, 3).mean(0)
        end_lo, end_hi = mean + axis * proj.min(), mean + axis * proj.max()
        butt, tip = (end_lo, end_hi) if np.linalg.norm(end_lo - rope_c) < np.linalg.norm(end_hi - rope_c) else (end_hi, end_lo)
        length = np.linalg.norm(tip - butt)
        fwd = (tip - butt) / length
        up = np.array([0, 1, 0.]) - fwd * fwd[1]
        up /= np.linalg.norm(up)
        rot = np.array([np.cross(-fwd, up), up, -fwd])
        g_ = butt + fwd * (length * .12)
        report['Baton/' + side] = dict(source='daredevils_baton', notes=notes + ['rope removed, 57 cm, grip a hand-width from the butt'],
                                       **write('Baton/' + side, frame(tris[sel], rot, g_, length, .57), uv[sel], mat[sel], mats))
    # Captain America's shield: the 0.0.7 frame (convert_shield), the source mesh itself (no remesh)
    import convert_shield
    A = convert_shield.analyse(os.path.join(src, 'captain_america_shield'))
    s, g, tris, uv, mat, mesh, mats, notes = take(src, 'captain_america_shield')
    nond = ~A['disc']
    pts = A['tris'][nond].reshape(-1, 3)
    hand_side, elbow_side = pts[pts[:, 0] < A['centre'][0]], pts[pts[:, 0] > A['centre'][0]]
    zback = float(np.interp(abs(hand_side[:, 0].mean() - A['centre'][0]), *A['zb']))

    def strap_centre(q):
        return np.array([np.median(q[:, 0]), (q[:, 1].min() + q[:, 1].max()) / 2, (q[:, 2].min() + zback) / 2])
    origin, elbow = strap_centre(hand_side), strap_centre(elbow_side)
    rot = np.array([[0, -1, 0], [-1, 0, 0], [0, 0, -1]], np.float64)
    k = convert_shield.DIAMETER_M / (2 * A['radius'])
    tf = (tris - origin) @ rot.T * k
    pivot_m = ((elbow - origin) @ rot.T * k).round(5).tolist()
    back_c = ((np.array([A['centre'][0], A['centre'][1], np.interp(0, *A['zb'])]) - origin) @ rot.T * k).round(5).tolist()
    radius_m = convert_shield.DIAMETER_M / 2
    centre_m = (np.array([A['centre'][0], A['centre'][1], 0]) - origin) @ rot.T * k

    def layer_order(t):  # the disc: triangles within the rim (in the face's plane) that are not straps
        if len(t) == len(A['disc']) and len(A['tris']) == len(tris) and np.allclose(A['tris'], tris, atol=1e-6):
            # 0.0.72: shipped whole, so the source analysis' own disc / strap split (the geometric test below
            # takes every triangle of the whole mesh for the disc)
            order = np.r_[np.flatnonzero(A['disc']), np.flatnonzero(~A['disc'])]
            return order, int(A['disc'].sum())
        c = t.mean(1)
        r = np.hypot(c[:, 0] - centre_m[0], c[:, 1] - centre_m[1])
        disc = (r > .2 * radius_m) | (np.abs(c[:, 2] - back_c[2]) < .012)
        order = np.r_[np.flatnonzero(disc), np.flatnonzero(~disc)]
        return order, int(disc.sum())
    report['Shield'] = dict(source='captain_america_shield', diameter_m=convert_shield.DIAMETER_M, pivot=pivot_m,
                            notes=['the source mesh and its texture (no remesh), the 0.0.7 frame: the hand strap at the origin, '
                                   'y elbow to hand, -z the face; extras.pivot the other strap'],
                            **write('Shield', tf, uv, mat, mats, {'pivot': pivot_m, 'layer': [0] + back_c + [0.0, 0.0, -1.0],
                                                                 'layer_order': layer_order}, source_n=len(tris)))
    # the grapple gun (Falxx, CC BY): the 0.0.7 frame (its conversion-report: grip_source, muzzle_source;
    # 22 cm, barrel along -z, no turn), pivot = the muzzle, extras.trigger as 0.0.7
    s, g, tris, uv, mat, mesh, mats, notes = take(src, 'grapple_gun')
    grip_s, muzzle_s = np.array([0, -2.49, 7.503]), np.array([0, 2.621, -10.506])
    kk = .1815 / abs(muzzle_s[2] - grip_s[2])
    t = (tris - grip_s) * kk
    report['GrappleGun'] = dict(source='grapple_gun', notes=notes + ['22 cm, the grip at the origin, barrel along -z (the 0.0.7 frame)'],
                                **write('GrappleGun', t, uv, mat, mats,
                                        {'pivot': ((muzzle_s - grip_s) * kk).round(5).tolist(), 'trigger': [0, .01603, -.04941]}))
    lic = os.path.join(src, 'grapple_gun', 'license.txt')
    if os.path.isfile(lic):
        shutil.copyfile(lic, os.path.join(out, 'GrappleGun', 'license.txt'))
    json.dump(report, open(os.path.join(out, 'conversion-report.json'), 'w'), indent=1)
    print(json.dumps({k: (v['source_triangles'], v['triangles'], v['textures']) for k, v in report.items()}, indent=1))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
