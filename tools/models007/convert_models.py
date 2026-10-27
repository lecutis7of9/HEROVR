"""Version 0.0.7: turn the downloaded Sketchfab models into the mod's VR drop-ins.

  python tools/models007/convert_models.py <source folder (read only)> <output Models folder>

needs numpy, Pillow and pyfqmr (quadric decimation). For each CC BY model: flatten its node tree,
bake each triangle's colour from its base-colour texture (sampled from a 256 px copy made in
memory - the overlay draws one colour a triangle, no textures), decimate to the VR budget, put it
in the mod's frame (metres, x right, y up, -z forward, the grip at the origin) and write
<out>/<slot>/scene.glb. The JOKER batarang (CC BY-NC-ND) is copied byte for byte - never
converted, decimated or retextured. The DAREDEVIL batons (Yanez Designs, CC BY): the connecting
rope is found by connectivity and removed, and the pair split into a left and a right baton.
Writes <out>/conversion-report.json (triangles, texture sizes, what was done)."""
import json, os, shutil, sys
import numpy as np
from PIL import Image
import pyfqmr
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gltf_io

BUDGET = 700  # mesh_model::drawTriangles


def srgb_to_linear(c):
    c = np.asarray(c, np.float64)
    return np.where(c <= .04045, c / 12.92, ((c + .055) / 1.055) ** 2.4)


def face_colours(src, g, part, notes):
    mat = g['materials'][part['material']] if part['material'] is not None else {}
    pbr = mat.get('pbrMetallicRoughness', {})
    factor = np.array(pbr.get('baseColorFactor', [1, 1, 1, 1])[:3], np.float64)
    tex = pbr.get('baseColorTexture')
    n = len(part['pos'])
    if tex is None or part['uv'] is None:
        if factor.max() < .02:  # e.g. Clooney's "Paint - Enamel Glossy (Dark Grey)" with a black factor
            notes.append('material %r: no texture and a black base colour; drawn dark grey as its name says' % mat.get('name'))
            factor = srgb_to_linear([.16, .16, .17])
        return np.tile(factor, (n, 1))
    uri = g['images'][g['textures'][tex['index']]['source']]['uri']
    im = Image.open(os.path.join(src, uri)).convert('RGB')
    small = np.asarray(im.resize((256, 256), Image.LANCZOS), np.float64) / 255.0
    uv = part['uv'].astype(np.float64)
    samples = np.concatenate([uv, uv.mean(1, keepdims=True)], axis=1)  # 3 corners + centroid
    u = np.mod(samples[..., 0], 1.0) * 255.999
    v = np.mod(samples[..., 1], 1.0) * 255.999
    rgb = srgb_to_linear(small[v.astype(int), u.astype(int)]).mean(1)
    return rgb * factor


def decimate(tris, colours, target):
    if len(tris) <= target:
        return tris, colours
    verts, inv = np.unique(np.round(tris.reshape(-1, 3), 7), axis=0, return_inverse=True)
    faces = inv.reshape(-1, 3)
    s = pyfqmr.Simplify()
    s.setMesh(verts, faces)
    s.simplify_mesh(target_count=target, aggressiveness=7, preserve_border=False, verbose=False)
    v2, f2, _ = s.getMesh()
    goal = target
    for _ in range(8):  # pyfqmr can stop a little over: tighten a few times
        if len(f2) <= target:
            break
        goal = int(goal * .85)
        s.setMesh(v2, f2)
        s.simplify_mesh(target_count=goal, aggressiveness=9, preserve_border=False, verbose=False)
        v2, f2, _ = s.getMesh()
    out = v2[f2]
    area = np.linalg.norm(np.cross(out[:, 1] - out[:, 0], out[:, 2] - out[:, 0]), axis=1)
    out = out[np.argsort(-area)[:target]]  # last resort: drop the smallest slivers
    out = out[np.linalg.norm(np.cross(out[:, 1] - out[:, 0], out[:, 2] - out[:, 0]), axis=1) > 1e-12]
    # each new triangle takes the colour of the nearest original triangle (by centroid)
    c0, c1 = tris.mean(1), out.mean(1)
    pick = np.empty(len(out), int)
    for a in range(0, len(out), 256):
        d = ((c1[a:a + 256, None, :] - c0[None, :, :]) ** 2).sum(-1)
        pick[a:a + 256] = d.argmin(1)
    return out, colours[pick]


def textures_of(src, g):
    out = {}
    for im in g.get('images', []):
        p = os.path.join(src, im['uri'])
        if os.path.isfile(p):
            out[im['uri']] = list(Image.open(p).size)
    return out


def frame(tris, rot, origin_model, length_model, length_m):
    """Rotate (3x3 rows: model -> mod axes), move the grip to the origin, scale to real size."""
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


def main(src, out):
    os.makedirs(out, exist_ok=True)
    report = {}

    def write(slot, tris, cols, extras=None):
        d = os.path.join(out, *slot.split('/'))
        os.makedirs(d, exist_ok=True)
        gltf_io.write_glb(os.path.join(d, 'scene.glb'), tris, cols, extras)
        return len(tris)

    def take(name):
        s = os.path.join(src, name)
        g, parts = gltf_io.load(s)
        notes = []
        cols = [face_colours(s, g, p, notes) for p in parts]
        return s, g, parts, cols, notes

    # --- batarangs (CC BY): decimated, colours baked; the loader fits them to a 20 cm span ---
    for name, slot in (('batarang', 'Batarang/EIRA'), ('batarang_clooney', 'Batarang/CLOONEY')):
        s, g, parts, cols, notes = take(name)
        tris = np.concatenate([p['pos'] for p in parts])
        c = np.concatenate(cols)
        src_n = len(tris)
        t2, c2 = decimate(tris, c, BUDGET)
        report[slot] = dict(source=name, source_triangles=src_n, triangles=write(slot, t2, c2),
                            source_textures=textures_of(s, g), runtime_textures='none (baked: one colour a triangle, from a 256 px copy)',
                            notes=notes + ['decimated (quadric) %d -> %d' % (src_n, len(t2))])
    # --- JOKER (CC BY-NC-ND): byte-for-byte copy, nothing converted ---
    s = os.path.join(src, 'batarang_joker')
    d = os.path.join(out, 'Batarang', 'JOKER')
    os.makedirs(d, exist_ok=True)
    for f in ('scene.gltf', 'scene.bin', 'license.txt'):
        shutil.copyfile(os.path.join(s, f), os.path.join(d, f))
    shutil.copytree(os.path.join(s, 'textures'), os.path.join(d, 'textures'), dirs_exist_ok=True)
    g, parts = gltf_io.load(s)
    report['Batarang/JOKER'] = dict(source='batarang_joker', source_triangles=sum(len(p['pos']) for p in parts),
                                    triangles=sum(len(p['pos']) for p in parts), source_textures=textures_of(s, g),
                                    runtime_textures='none: the overlay cannot sample textures; the files are shipped unmodified and unused',
                                    notes=['copied byte for byte (ND: no conversion, decimation or texture change)'])
    # --- the pistol (CC BY), both hands: muzzle at -x in the source ---
    s, g, parts, cols, notes = take('9mm_pistol')
    tris = np.concatenate([p['pos'] for p in parts])
    c = np.concatenate(cols)
    rot = [[0, 0, -1], [0, 1, 0], [1, 0, 0]]  # source x (muzzle -x) -> -z forward
    lo, hi = tris.reshape(-1, 3).min(0), tris.reshape(-1, 3).max(0)
    grip = np.array([lo[0] + .74 * (hi[0] - lo[0]), lo[1] + .66 * (hi[1] - lo[1]), (lo[2] + hi[2]) / 2])
    t = frame(tris, rot, grip, hi[0] - lo[0], .19)
    t2, c2 = decimate(t, c, BUDGET)
    report['Pistol'] = dict(source='9mm_pistol', source_triangles=len(tris), triangles=write('Pistol', t2, c2),
                            source_textures=textures_of(s, g), runtime_textures='none (baked)',
                            notes=notes + ['19 cm long, grip at the origin, one model for both hands'])
    # --- the sawed-off (CC BY): barrels split off at the hinge so it can break open ---
    s, g, parts, cols, notes = take('sawed-off_shotgun')
    tris = np.concatenate([p['pos'] for p in parts])
    c = np.concatenate(cols)
    mesh = np.concatenate([[p['mesh']] * len(p['pos']) for p in parts])
    lo, hi = tris.reshape(-1, 3).min(0), tris.reshape(-1, 3).max(0)
    hinge_z = .07
    cen = tris.mean(1)
    barrels = (cen[:, 2] < hinge_z) & ((cen[:, 1] > .088) | (mesh == 2))
    grip = np.array([0, .12, .30])
    pivot = np.array([0, .085, hinge_z])
    eye3 = np.eye(3)
    tf = frame(tris[~barrels], eye3, grip, hi[2] - lo[2], .58)
    tb = frame(tris[barrels], eye3, pivot, hi[2] - lo[2], .58)
    pivot_mod = frame(pivot[None, None, :], eye3, grip, hi[2] - lo[2], .58)[0, 0]
    nf = int(BUDGET * .6)
    f2, fc = decimate(tf, c[~barrels], nf)
    b2, bc = decimate(tb, c[barrels], BUDGET - nf)
    report['SawedOff'] = dict(source='sawed-off_shotgun', source_triangles=len(tris),
                              triangles=write('SawedOff/Frame', f2, fc) + write('SawedOff/Barrels', b2, bc, {'pivot': pivot_mod.round(5).tolist()}),
                              frame_triangles=len(f2), barrel_triangles=len(b2), source_textures=textures_of(s, g),
                              runtime_textures='none (baked)',
                              notes=notes + ['58 cm long; split at the hinge (z=%.2f in the source): barrels + fore-end turn about the pivot' % hinge_z])
    # --- DAREDEVIL's batons (CC BY): rope removed by connectivity, split into two ---
    s, g, parts, cols, notes = take('daredevils_baton')
    tris = np.concatenate([p['pos'] for p in parts])
    c = np.concatenate(cols)
    owner = np.concatenate([np.full(len(p['pos']), i) for i, p in enumerate(parts)])
    comp = np.zeros(len(tris), int)
    base = 0
    for i, p in enumerate(parts):
        r = components(p['pos'])
        comp[owner == i] = r + base
        base += r.max() + 1
    rope = np.zeros(len(tris), bool)
    ropes = []
    for k in np.unique(comp):
        sel = comp == k
        pts = tris[sel].reshape(-1, 3)
        size = pts.max(0) - pts.min(0)
        # the rope: a flat strip (thinner than a tenth of a baton's 148-unit girth) that spans both batons
        if size[1] < 20 and max(size[0], size[2]) > 600:
            rope |= sel
            ropes.append(dict(triangles=int(sel.sum()), size=size.round(1).tolist(), mesh=int(owner[sel][0])))
    assert len(ropes) == 1, ropes
    keep = ~rope
    # baton A lies along x at z=0; baton B is the other (diagonal) one
    cen = tris.mean(1)
    a = keep & (np.abs(cen[:, 2]) < 100)
    b = keep & ~a
    out_b = {}
    for side, sel in (('Left', a), ('Right', b)):
        pts = tris[sel].reshape(-1, 3)
        mean = pts.mean(0)
        u, sv, vt = np.linalg.svd(pts - mean, full_matrices=False)
        axis = vt[0]
        proj = (pts - mean) @ axis
        # the rope was tied to the ridged grip end (a few rope triangles run on inside baton A and go
        # with it): the end nearer the rope's centre is the grip; the far end has the cap
        rope_c = tris[rope].reshape(-1, 3).mean(0)
        end_lo, end_hi = mean + axis * proj.min(), mean + axis * proj.max()
        butt, tip = (end_lo, end_hi) if np.linalg.norm(end_lo - rope_c) < np.linalg.norm(end_hi - rope_c) else (end_hi, end_lo)
        pommel = butt
        length = np.linalg.norm(tip - pommel)
        fwd = (tip - pommel) / length
        up = np.array([0, 1, 0.]) - fwd * fwd[1]
        up /= np.linalg.norm(up)
        right = np.cross(-fwd, up)  # x right, y up, z back (= -forward)
        rot = np.array([right, up, -fwd])
        grip = pommel + fwd * (length * .12)  # a hand's width in from the butt of the ridged grip
        t = frame(tris[sel], rot, grip, length, .57)
        t2, c2 = decimate(t, c[sel], 400)
        out_b[side] = dict(source_triangles=int(sel.sum()), triangles=write('Baton/' + side, t2, c2))
    report['Baton'] = dict(source='daredevils_baton', source_triangles=len(tris), rope=ropes, batons=out_b,
                           source_textures=textures_of(s, g), runtime_textures='none (baked)',
                           notes=notes + ['rope removed (found by connectivity: one flat strip joining both batons), split into a left and a right baton, 57 cm, pivot a hand-width in from the butt of the ridged grip'])
    # --- Captain America's shield (CC BY): remeshed disc + decimated straps, colours baked (convert_shield.py) ---
    if os.path.isdir(os.path.join(src, 'captain_america_shield')):
        import convert_shield
        report['Shield'] = convert_shield.convert(os.path.join(src, 'captain_america_shield'), write)
    json.dump(report, open(os.path.join(out, 'conversion-report.json'), 'w'), indent=1)
    print(json.dumps(report, indent=1))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
