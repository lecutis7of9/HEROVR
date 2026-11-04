"""HERO VR 0.0.78 PUNISHER ARSENAL: the owner's two Sketchfab downloads -> Models/Punisher078/{Pump,Pistol}/...
(the sniper, rifle and red dot are not in this build: their slots stay MODEL MISSING).

  python3 tools/models078/convert_guns_078.py --pistol beretta_m9_w_supressor.glb --pump remington_870.glb \
      --out <Models dir> [--work <dir>] [--bpy /workspace/bpy-venv/bin/python]

Per gun:
  1. dump_gun_078.py (Blender, read only): the evaluated, triangulated scene, its parts by object (pistol) or rigid
     bone (the 870's skin), corner normals and uvs;
  2. split into the game's parts (punisher078 / hero007::Part078): body, the moving part (pistol slide, pump forend
     with its bolt), the magazine (pistol: the model's floor plate on a magazine built here) / one loose shell (built
     here); nothing is decimated (both are far under mesh_model's 60,000 / 20,000 caps);
  3. into the arsenal frame: metres, real scale, the grip point (the middle of the grip, half way down it) at the
     origin, -z along the barrel, +y up, +x right; every corner normal turned with it;
  4. the model's own PBR maps (base colour, normal, its packed occlusion-roughness-metal map) from 4096 to 2048 px
     JPEG (mesh_model's HQ size), TANGENT per vertex (convert_suit.corner_tangents);
  5. scene.glb + license.txt per folder, and conversion-report078.json with the measured points (pointsOf) and
     the triangle counts."""
import argparse, io, json, math, os, struct, subprocess, sys
import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'suits'))
from convert_suit import corner_tangents  # noqa: E402

HQ = 2048
BODY_CAP, PART_CAP = 60000, 20000  # mesh_model::arsenalTriangles, arsenalPartTriangles

PISTOL = dict(
    title='BERETTA M9 W/ SUPRESSOR', author='drcrazzie', author_url='https://sketchfab.com/drcrazzie',
    source='https://sketchfab.com/3d-models/beretta-m9-w-supressor-93cda4e59523467ea20559ec4ca5b5cd')
PUMP = dict(
    title='Remington 870', author='White-Horse', author_url='https://sketchfab.com/white-horse',
    source='https://sketchfab.com/3d-models/remington-870-e9342d1df32645faa2e50fd33a9e1800')
LICENSE = 'CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)'
# The 870's file is in about millimetres but drawn slim: its barrel (18.5 units across) and receiver (25.9 wide) match a
# real 870's (about 21 mm and 29 mm) at 1.12 mm a unit; 48.4 cm long then (a short-barrel, pistol-grip 870). CANDIDATE.
PUMP_SCALE = .00112


def license_text(g, changes):
    return ('Model Information:\n* title:\t%s\n* source:\t%s\n* author:\t%s (%s)\n\nModel License:\n* license type:\t%s\n'
            '* requirements:\tAuthor must be credited. Commercial use is allowed.\n\nChanges (HERO VR 0.0.78, '
            'tools/models078/convert_guns_078.py): %s\n' % (g['title'], g['source'], g['author'], g['author_url'], LICENSE, changes))


def read_glb(path):
    b = open(path, 'rb').read()
    n = struct.unpack('<I', b[12:16])[0]
    g = json.loads(b[20:20 + n])
    return g, b[20 + n + 8:]


def glb_image(g, binb, tex):
    im = g['images'][g['textures'][tex]['source']]
    bv = g['bufferViews'][im['bufferView']]
    o = bv.get('byteOffset', 0)
    return Image.open(io.BytesIO(binb[o:o + bv['byteLength']]))


def jpeg(img, quality, size=HQ):
    img = img.convert('RGB')
    if max(img.size) > size:
        img = img.resize((size, size), Image.LANCZOS)
    b = io.BytesIO()
    img.save(b, 'JPEG', quality=quality, subsampling=0)
    return b.getvalue()


def source_materials(path):
    """The file's materials -> [{name, base, normal, orm}] (JPEG bytes at 2048; its MR map is already the packed
    ORM: R its occlusion (the same image is its occlusionTexture), G roughness, B metal)."""
    g, binb = read_glb(path)
    out = []
    for m in g['materials']:
        p = m['pbrMetallicRoughness']
        mr = p['metallicRoughnessTexture']['index']
        assert m.get('occlusionTexture', {}).get('index', mr) == mr, 'a separate occlusion map'
        out.append(dict(name=m['name'], base=jpeg(glb_image(g, binb, p['baseColorTexture']['index']), 90),
                        normal=jpeg(glb_image(g, binb, m['normalTexture']['index']), 95),
                        orm=jpeg(glb_image(g, binb, mr), 92)))
    return out


# ---- meshes: corners (C,3) P / N, (C,2) U, a material per triangle ----------------------------------------------
class Part:
    def __init__(self):
        self.P, self.N, self.U, self.M = [], [], [], []

    def add(self, P, N, U, M):
        self.P.append(np.asarray(P, np.float64).reshape(-1, 3)); self.N.append(np.asarray(N, np.float64).reshape(-1, 3))
        self.U.append(np.asarray(U, np.float64).reshape(-1, 2)); self.M.append(np.asarray(M, np.int64).reshape(-1))

    def arrays(self):
        return np.concatenate(self.P), np.concatenate(self.N), np.concatenate(self.U), np.concatenate(self.M)

    def triangles(self):
        return sum(len(m) for m in self.M)


def write_glb(path, part, mats, extras):
    """mats: [{name, base, normal, orm}] JPEG bytes (shared bytes give the game one layer)."""
    P_all, N_all, U_all, mat = part.arrays()
    N_all = N_all / np.maximum(np.linalg.norm(N_all, axis=1, keepdims=True), 1e-12)
    T_all = corner_tangents(P_all.astype(np.float32), N_all.astype(np.float32), U_all.astype(np.float32))
    binb = bytearray()
    g = {'asset': {'version': '2.0', 'generator': 'HERO VR 0.0.78 tools/models078/convert_guns_078.py', 'extras': extras},
         'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0}], 'meshes': [{'primitives': []}], 'buffers': [{}],
         'bufferViews': [], 'accessors': [], 'materials': [], 'textures': [], 'images': [], 'samplers': [{}]}

    def view(data):
        off = len(binb)
        binb.extend(data)
        binb.extend(b'\0' * (-len(binb) % 4))
        g['bufferViews'].append({'buffer': 0, 'byteOffset': off, 'byteLength': len(data)})
        return len(g['bufferViews']) - 1
    seen = {}

    def texture(data):
        if data not in seen:
            g['images'].append({'bufferView': view(data), 'mimeType': 'image/jpeg'})
            g['textures'].append({'source': len(g['images']) - 1, 'sampler': 0})
            seen[data] = len(g['textures']) - 1
        return seen[data]
    cm = np.repeat(mat, 3)
    for mi in sorted(set(mat.tolist())):
        m = mats[mi]
        o = texture(m['orm'])
        g['materials'].append({'name': m['name'], 'doubleSided': False,
                               'pbrMetallicRoughness': {'baseColorTexture': {'index': texture(m['base'])},
                                                        'metallicRoughnessTexture': {'index': o}, 'metallicFactor': 1, 'roughnessFactor': 1},
                               'normalTexture': {'index': texture(m['normal'])}, 'occlusionTexture': {'index': o}})
        sel = cm == mi
        P, N, U, Tc = P_all[sel].astype(np.float32), N_all[sel].astype(np.float32), U_all[sel].astype(np.float32), T_all[sel]
        key = np.c_[np.round(P, 6), np.round(N, 4), np.round(U, 6), Tc[:, 3]]
        _, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
        inv = inv.ravel().astype(np.uint32)
        T = np.zeros((len(first), 3))
        np.add.at(T, inv, Tc[:, :3])
        Nf = N[first].astype(np.float64)
        T -= Nf * (Nf * T).sum(1, keepdims=True)
        ln = np.linalg.norm(T, axis=1, keepdims=True)
        Tw = np.c_[np.where(ln > 1e-9, T / np.maximum(ln, 1e-9), [1, 0, 0]), np.where(ln[:, 0] > 1e-9, Tc[first, 3], 1)].astype(np.float32)
        Tw[Tw[:, 3] == 0, 3] = 1
        P, N, U = P[first], N[first], U[first]
        acc = len(g['accessors'])
        g['accessors'] += [
            {'bufferView': view(P.tobytes()), 'componentType': 5126, 'count': len(P), 'type': 'VEC3', 'min': P.min(0).tolist(), 'max': P.max(0).tolist()},
            {'bufferView': view(N.tobytes()), 'componentType': 5126, 'count': len(N), 'type': 'VEC3'},
            {'bufferView': view(U.tobytes()), 'componentType': 5126, 'count': len(U), 'type': 'VEC2'},
            {'bufferView': view(Tw.tobytes()), 'componentType': 5126, 'count': len(Tw), 'type': 'VEC4'},
            {'bufferView': view(inv.tobytes()), 'componentType': 5125, 'count': len(inv), 'type': 'SCALAR'}]
        g['meshes'][0]['primitives'].append({'attributes': {'POSITION': acc, 'NORMAL': acc + 1, 'TEXCOORD_0': acc + 2, 'TANGENT': acc + 3},
                                             'indices': acc + 4, 'material': len(g['materials']) - 1})
    g['buffers'][0]['byteLength'] = len(binb)
    j = json.dumps(g, separators=(',', ':')).encode()
    j += b' ' * (-len(j) % 4)
    body = struct.pack('<II', len(j), 0x4E4F534A) + j + struct.pack('<II', len(binb), 0x004E4942) + bytes(binb)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', 0x46546C67, 2, 12 + len(body)) + body)


# ---- procedural parts (the files have none): textures from height maps, meshes by lathe / extrusion ------------
def normal_from_height(h, strength):
    """A tangent-space normal map (glTF: +x along u, +y up the image) from a height map (rows down = +v)."""
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * .5 * strength
    gv = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * .5 * strength
    n = np.stack([-gx, gv, np.ones_like(h)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return Image.fromarray(((n * .5 + .5) * 255 + .5).astype(np.uint8))


def maps(base, rough, metal, height, strength, occ=None):
    occ = np.ones_like(rough) if occ is None else occ
    b = Image.fromarray((np.clip(base, 0, 1) ** (1 / 2.2) * 255 + .5).astype(np.uint8))  # linear -> sRGB
    o = Image.fromarray((np.clip(np.stack([occ, rough, metal], -1), 0, 1) * 255 + .5).astype(np.uint8))
    return dict(base=jpeg(b, 92, 512), normal=jpeg(normal_from_height(height, strength), 95, 512), orm=jpeg(o, 95, 512))


def noise(rng, shape, blur):
    a = rng.standard_normal(shape)
    f = np.fft.rfft2(a)
    ky, kx = np.meshgrid(np.fft.fftfreq(shape[0]), np.fft.rfftfreq(shape[1]), indexing='ij')
    a = np.fft.irfft2(f * np.exp(-(kx ** 2 + ky ** 2) * (blur ** 2) * 20), s=shape)
    return a / (a.std() + 1e-9)


def lathe(part, prof, seg, mat, u0, u1, axis_back=1.):
    """prof [(r, z)] (z along the file's z), revolved about the z axis; u across [u0, u1] by profile length, v around."""
    prof = np.asarray(prof, np.float64)
    L = np.r_[0, np.cumsum(np.linalg.norm(np.diff(prof, axis=0), axis=1))]
    L /= L[-1]
    a = np.linspace(0, 2 * np.pi, seg + 1)
    d = np.diff(prof, axis=0)
    for i in range(len(prof) - 1):
        (r0, z0), (r1, z1) = prof[i], prof[i + 1]
        tz, tr = d[i, 1], d[i, 0]
        nr, nz = tz, -tr  # the profile's outward normal (r out)
        ln = math.hypot(nr, nz) or 1
        nr, nz = nr / ln, nz / ln
        for k in range(seg):
            q = []
            for (r, z, l), t in ((prof[i].tolist() + [L[i]], a[k]), (prof[i + 1].tolist() + [L[i + 1]], a[k]),
                                 (prof[i + 1].tolist() + [L[i + 1]], a[k + 1]), (prof[i].tolist() + [L[i]], a[k + 1])):
                q.append(((r * math.cos(t), r * math.sin(t), z), (nr * math.cos(t), nr * math.sin(t), nz),
                          (u0 + (u1 - u0) * l, t / (2 * np.pi))))
            for tri in ((0, 1, 2), (0, 2, 3)):
                cs = [q[j] for j in tri]
                P = np.array([c[0] for c in cs]); N = np.array([c[1] for c in cs]); U = np.array([c[2] for c in cs])
                if np.linalg.norm(np.cross(P[1] - P[0], P[2] - P[0])) < 1e-14:
                    continue
                if np.dot(np.cross(P[1] - P[0], P[2] - P[0]), N.mean(0)) < 0:
                    P, N, U = P[::-1], N[::-1], U[::-1]
                part.add(P, N, U, [mat])


def disc(part, r, z, normal_z, seg, mat, uc, ur):
    """A flat disc at z facing normal_z (+1 / -1), uv planar about uc with radii ur (u, v)."""
    a = np.linspace(0, 2 * np.pi, seg + 1)
    for k in range(seg):
        P = np.array([[0, 0, z], [r * math.cos(a[k]), r * math.sin(a[k]), z], [r * math.cos(a[k + 1]), r * math.sin(a[k + 1]), z]])
        if (np.cross(P[1] - P[0], P[2] - P[0])[2] > 0) != (normal_z > 0):
            P = P[[0, 2, 1]]
        U = np.c_[uc[0] + P[:, 0] / r * ur[0], uc[1] + P[:, 1] / r * ur[1]]
        part.add(P, np.tile([0, 0, normal_z], (3, 1)), U, [mat])


def shell_part(rng):
    """One 12-gauge 2 3/4" shell, unfired (63.5 mm with its folded crimp): a brass head (12.7 mm, rim 22.4 mm), a red
    ribbed plastic hull (20.5 mm), a six-fold star crimp at the front; about its middle, the crimp toward -z."""
    S = 256
    yy, xx = np.mgrid[0:S, 0:2 * S] / S  # atlas 512 x 256: u [0,.25) brass side, [.25,.75) hull, rear and crimp discs on the right
    base = np.zeros((S, 2 * S, 3)); rough = np.zeros((S, 2 * S)); metal = np.zeros((S, 2 * S)); h = noise(rng, (S, 2 * S), .02) * .05
    u = xx / 2
    brass = u < .25
    hull = (u >= .25) & (u < .75)
    base[brass] = [.62, .42, .17]; rough[brass] = .32; metal[brass] = 1
    base[hull] = [.42, .025, .02]; rough[hull] = .42
    ribs = np.sin(yy * 2 * np.pi * 26) * .5  # the hull's ribs along the shell (v runs around it)
    h += np.where(hull, ribs, 0)
    right = u >= .75  # the right quarter (128 x 256 px): the rear disc (primer) about (.875, .25), the crimp about (.875, .75), radius 64 px
    rr = np.hypot((u - .875) * 8, (yy - .25) * 4)
    rc = np.hypot((u - .875) * 8, (yy - .75) * 4)
    ang = np.arctan2((yy - .75) * 4, (u - .875) * 8)
    rear = right & (yy < .5)
    crimp = right & (yy >= .5)
    base[rear] = [.62, .42, .17]; rough[rear] = .3; metal[rear] = 1
    prim = rear & (rr < .32)
    base[prim] = [.55, .45, .33]; rough[prim] = .25
    h += np.where(rear, np.where(rr < .32, .6, 0) + np.where(np.abs(rr - .34) < .03, -.8, 0), 0)
    base[crimp] = [.36, .02, .02]; rough[crimp] = .5
    fold = np.cos(ang * 6) * np.clip(rc, 0, 1)
    h += np.where(crimp, fold * 1.2 - np.where(rc < .12, .8, 0), 0)
    occ = np.where(crimp, .75 + .25 * (fold * .5 + .5), 1.)
    occ = np.where(rear & (np.abs(rr - .34) < .04), .6, occ)
    mat = maps(base, rough, metal, h, 6., occ)
    mat['name'] = '12ga shell'
    part = Part()
    L = .0635
    zb, zf = L / 2, -L / 2  # rear (brass) at +z, the crimp at -z
    lathe(part, [(.0112, zb), (.0112, zb - .0015), (.0103, zb - .0017), (.0103, zb - .0127), (.01025, zb - .0129)], 48, 0, .0, .25)
    lathe(part, [(.01025, zb - .0129), (.01025, zf + .003), (.0098, zf + .001), (.0088, zf)], 48, 0, .25, .75)
    disc(part, .0112, zb, 1, 48, 0, (.875, .25), (.124, .248))
    disc(part, .0088, zf, -1, 48, 0, (.875, .75), (.124, .248))
    return part, mat


def mag_part(rng, bottom, top, depth, width):
    """The M9's 15-round magazine (the model has only its floor plate): a blued double-stack box along the grip's
    slant (bottom -> top, file frame), rounded front, witness holes at the back, a 9 mm round on its follower."""
    S = 512
    yy, xx = np.mgrid[0:S, 0:S] / S
    base = np.zeros((S, S, 3)); rough = np.zeros((S, S)); metal = np.zeros((S, S)); h = noise(rng, (S, S), .01) * .08
    steel = xx < .5
    brushed = noise(rng, (S, S), .002)
    base[steel] = [.035, .036, .04]; rough[steel] = .38 + .06 * np.clip(brushed[steel], -1, 1); metal[steel] = 1
    h += np.where(steel, brushed * .04, 0)
    # the witness holes: at the back (u .375 +- of the box's perimeter, v down the magazine), 15 rounds
    for k in range(5):
        d = np.hypot((xx - .375) * 2 * S / 40, (yy - (.2 + .13 * k)) * S / 20)
        base[steel & (d < .9)] = [.004, .004, .005]
        h += np.where(steel & (d < 1.1), -1.5 * np.clip(1.1 - d, 0, .4), 0)
    caseu = (xx >= .5) & (xx < .75)
    bullet = xx >= .75
    base[caseu] = [.66, .45, .19]; rough[caseu] = .27; metal[caseu] = 1
    base[bullet] = [.6, .3, .17]; rough[bullet] = .3; metal[bullet] = 1
    base[(xx >= .5) & (yy > .9)] = [.02, .02, .02]; rough[(xx >= .5) & (yy > .9)] = .6; metal[(xx >= .5) & (yy > .9)] = 0  # the follower
    occ = np.ones((S, S))
    mat = maps(base, rough, metal, h, 5., occ)
    mat['name'] = 'M9 magazine (HERO VR)'
    part = Part()
    up = np.asarray(top, float) - np.asarray(bottom, float)
    H = np.linalg.norm(up)
    up /= H
    fwd = np.array([0, 0, -1.])
    fwd -= up * fwd.dot(up)
    fwd /= np.linalg.norm(fwd)
    right = np.cross(fwd, up)  # file frame: x right = forward x up
    # the cross section (a forward, b right): a rounded rectangle, the front corners 7 mm, the back 3 mm
    pts = []
    a0, b0 = depth / 2, width / 2
    for (ca, cb, r, t0) in ((a0 - .007, b0 - .007, .007, 0), (-a0 + .003, b0 - .003, .003, 90), (-a0 + .003, -b0 + .003, .003, 180), (a0 - .007, -b0 + .007, .007, 270)):
        for t in np.linspace(t0, t0 + 90, 7):
            pts.append((ca + r * math.cos(math.radians(t)), cb + r * math.sin(math.radians(t))))
    pts = np.array(pts + [pts[0]])
    per = np.r_[0, np.cumsum(np.linalg.norm(np.diff(pts, axis=0), axis=1))]
    per /= per[-1]
    # shift u so the back (a = -a0, b = 0) is at u .375 of the steel half (.75 of the perimeter): find it
    shift = .75 - (per[13] + per[14]) / 2  # the back wall runs from point 13 (the end of the second corner) to 14
    rings = np.linspace(0, 1, 12)
    for i in range(len(pts) - 1):
        for j in range(len(rings) - 1):
            cs = []
            for (pi, rj) in ((i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)):
                a, b = pts[pi]
                P = np.asarray(bottom) + up * (H * rings[rj]) + fwd * a + right * b
                cs.append((P, ((per[pi] + shift) % 1.0001) * .5, 1 - rings[rj]))
            # outward normal of the side: the section's own
            t2 = pts[i + 1] - pts[i]
            n2 = np.array([t2[1], -t2[0]])
            n2 /= np.linalg.norm(n2) or 1
            n = fwd * n2[0] + right * n2[1]
            for tri in ((0, 1, 2), (0, 2, 3)):
                P = np.array([cs[k][0] for k in tri]); U = np.array([[cs[k][1], cs[k][2] * .88] for k in tri])
                if abs(U[:, 0].max() - U[:, 0].min()) > .25:  # the seam where u wraps: keep it on one side
                    U[:, 0] = np.where(U[:, 0] < .25, U[:, 0] + .5, U[:, 0]) - (.0 if U[:, 0].max() <= .5 else 0)
                    U[:, 0] = np.clip(U[:, 0], 0, .499)
                if np.dot(np.cross(P[1] - P[0], P[2] - P[0]), n) < 0:
                    P, U = P[::-1], U[::-1]
                part.add(P, np.tile(n, (3, 1)), U, [0])
    # the follower plate 4 mm under the top, then the top round lying on it, nose forward
    ftop = np.asarray(bottom) + up * (H - .004)
    poly = pts[:-1] * .92
    for i in range(len(poly)):
        P = np.array([ftop, ftop + fwd * poly[i][0] + right * poly[i][1], ftop + fwd * poly[(i + 1) % len(poly)][0] + right * poly[(i + 1) % len(poly)][1]])
        if np.dot(np.cross(P[1] - P[0], P[2] - P[0]), up) < 0:
            P = P[[0, 2, 1]]
        part.add(P, np.tile(up, (3, 1)), [[.6, .95], [.62, .95], [.61, .97]], [0])
    rnd = Part()
    rc = .00495
    case = [(0, .0), (.0048, 0), (.00495, -.0012), (.00475, -.0013), (.00495, -.0016), (.0049, -.0192)]
    lathe(rnd, case, 32, 0, .5, .75)
    nose = [(.00451, -.0192), (.00451, -.0222)] + [(.00451 * math.cos(t), -.0222 - .0072 * math.sin(t)) for t in np.linspace(0, math.pi / 2, 7)[1:]]
    lathe(rnd, nose, 32, 0, .75, 1)
    disc(rnd, .0048, 0, 1, 32, 0, (.6, .5), (.001, .001))
    centre = ftop + up * rc - fwd * (depth / 2 - .0015)  # the case head at the back wall
    Pr, Nr, Ur, Mr = rnd.arrays()
    # the round's own z -> forward (its nose at -z, so -z maps to +fwd), x -> right, y -> up
    R = np.c_[right, up, -fwd]
    part.add(centre + Pr @ R.T, Nr @ R.T, Ur, Mr)
    return part, mat


# ---- the guns ---------------------------------------------------------------------------------------------------
def load_dump(src, work, bpy):
    out = os.path.join(work, os.path.splitext(os.path.basename(src))[0] + '.npz')
    if not os.path.exists(out) or os.path.getmtime(out) < os.path.getmtime(src):
        subprocess.run([bpy, os.path.join(HERE, 'dump_gun_078.py'), src, out], check=True, capture_output=True)
    d = np.load(out)
    return d['P'].reshape(-1, 3, 3), d['N'].reshape(-1, 3, 3), d['U'].reshape(-1, 3, 2), d['M'], d['L'], [str(x) for x in d['names']]


def components(T):
    key = np.round(T.reshape(-1, 3), 6)
    _, inv = np.unique(key, axis=0, return_inverse=True)
    inv = inv.reshape(-1, 3)
    par = np.arange(inv.max() + 1)

    def f(x):
        while par[x] != x:
            par[x] = par[par[x]]
            x = par[x]
        return x
    for a, b, c in inv:
        for y in (b, c):
            ra, rb = f(a), f(y)
            if ra != rb:
                par[ra] = rb
    return np.array([f(t[0]) for t in inv])


def pistol(src, work, bpy, rng):
    P, N, U, M, L, names = load_dump(src, work, bpy)
    lab = np.array(names)[L]
    # Blender frame: the barrel along -y, z up, x the gun's left; metres already (40.0 cm with the can, 21.5 cm without:
    # an M9 is 217 mm long, 137 mm high - this one 215 / 143 with its sights). CONFIRMED real scale.
    slide = np.isin(lab, ['9mm_Bodytop_Low_Top_0', '9mm_PartC_Low_Top_0', '9mm_PartB_Low_Top_0'])  # slide, its safety lever, extractor
    plate = np.zeros(len(L), bool)
    c = lab == '9mm_PartC_Low_Main Body_0'
    comp = components(P[c])
    for k in np.unique(comp):
        s = comp == k
        if P[c][s][..., 2].max() < -.12:  # the floor plate at the grip's base (with the magazine, not the frame)
            idx = np.where(c)[0][s]
            plate[idx] = True
    body = ~slide & ~plate
    frame = P[lab == '9mm_MainBody_Low_Main Body_0'].reshape(-1, 3)
    can = P[lab == '9mm_Silencer_Low_Barrell_0'].reshape(-1, 3)
    slideP = P[slide].reshape(-1, 3)
    plateP = P[plate].reshape(-1, 3)
    # the grip point: half way down the grip (under the slide's rail, -0.036, to its base), the middle of the grip there
    gz = (-.036 + frame[:, 2].min()) / 2
    band = frame[np.abs(frame[:, 2] - gz) < .005]
    gy = (band[:, 1].min() + band[:, 1].max()) / 2
    o = np.array([0, gy, gz])

    def to_file(p):  # Blender (x left, y back, z up) -> file (x right, y up, z back)
        p = np.asarray(p, float)
        return np.stack([-p[..., 0], p[..., 2], p[..., 1]], -1)
    Pf = to_file(P - o)
    Nf = to_file(N)

    def local(pb):  # a Blender point -> punisher078::Local {forward, up, right}
        f = to_file(np.asarray(pb) - o)
        return [round(float(-f[2]), 4), round(float(f[1]), 4), round(float(f[0]), 4)]
    bore = (can[:, 2].min() + can[:, 2].max()) / 2
    pts = dict(
        muzzle=local([0, can[:, 1].min(), bore]),
        port=local([0, (slideP[:, 1].min() + slideP[:, 1].max()) / 2 + .02, (slideP[:, 2].min() + slideP[:, 2].max()) / 2 - .005]),
        # the slide's rear serrations: the back 2 cm of the slide, its middle height
        rack=local([0, slideP[:, 1].max() - .012, (slideP[:, 2].min() + slideP[:, 2].max()) / 2]),
        magWell=local([0, (plateP[:, 1].min() + plateP[:, 1].max()) / 2, plateP[:, 2].min()]),
        sightEye=local([0, slideP[:, 1].max() + .10 + 0, P[..., 2].max()]),
        rackTravel=.035,  # an M9's slide travel (about 35 mm to lock back)
        grip=[round(float(x), 4) for x in o])
    mats = source_materials(src)
    out = {}
    for name, sel in (('Pistol', body), ('Pistol/Slide', slide)):
        part = Part()
        part.add(Pf[sel], Nf[sel], U[sel], M[sel])
        out[name] = (part, mats)
    # the magazine: in the gun's frame, seated: its body from the floor plate's top up the grip's slant to under the slide
    plf = to_file(plateP - o)
    bottom = np.array([0, plf[:, 1].max() - .001, (plf[:, 2].min() + plf[:, 2].max()) / 2])
    # the grip's slant: the backstrap's middle 4 cm higher sits .2 x 4 cm further forward (measured 0.16-0.20)
    top_y = float(to_file(np.array([0, 0, slideP[:, 2].min() + .001]) - o)[1])
    top = bottom + np.array([0, top_y - bottom[1], -.18 * (top_y - bottom[1])])
    magp, magmat = mag_part(rng, bottom, top, .034, .0175)
    mag = Part()
    mag.add(Pf[plate], Nf[plate], U[plate], M[plate])
    Pm, Nm, Um, Mm = magp.arrays()
    mag.add(Pm, Nm, Um, Mm + len(mats))
    out['Pistol/Mag'] = (mag, mats + [magmat])
    return out, pts


def pump(src, work, bpy, rng):
    P, N, U, M, L, names = load_dump(src, work, bpy)
    lab = np.array(names)[L]
    # Blender frame: the barrel along +x, z up, y the gun's left; units about mm (PUMP_SCALE). Skin bones: root_00 the
    # body, trigger_01, bolt_02 (the bolt in the ejection port: it rides the action bars with the forend), handguard_03
    # (the forend), mag door_04 (the shell elevator, stays).
    fore = np.isin(lab, ['handguard_03', 'bolt_02'])
    body = ~fore
    R = P[lab == 'root_00'].reshape(-1, 3)
    # the pistol grip: half way down it (the receiver's floor at about -20 to its base), its middle there
    gz = (-20 + R[:, 2].min()) / 2
    band = R[(np.abs(R[:, 2] - gz) < 3) & (R[:, 0] < -150)]
    gx = (band[:, 0].min() + band[:, 0].max()) / 2
    o = np.array([gx, 0, gz])
    s = PUMP_SCALE

    def to_file(p, scale=s):  # Blender (x forward, y left, z up) -> file (x right, y up, z back)
        p = np.asarray(p, float)
        return np.stack([-p[..., 1], p[..., 2], -p[..., 0]], -1) * scale
    Pf = to_file(P - o)
    Nf = to_file(N, 1)

    def local(pb):
        f = to_file(np.asarray(pb) - o)
        return [round(float(-f[2]), 4), round(float(f[1]), 4), round(float(f[0]), 4)]
    barrel = R[(R[:, 0] > 100) & (R[:, 0] < 170)]
    bore = (barrel[:, 2].min() + barrel[:, 2].max()) / 2
    H = P[lab == 'handguard_03'].reshape(-1, 3)
    D = P[lab == 'mag door_04'].reshape(-1, 3)
    recvFront = R[(R[:, 2] > 5) & (R[:, 2] < 20) & (np.abs(R[:, 1]) > 11) & (R[:, 0] < 0)][:, 0].max()  # the receiver's front face
    pts = dict(
        muzzle=local([R[:, 0].max(), 0, bore]),
        port=local([(D[:, 0].min() + D[:, 0].max()) / 2, 0, D[:, 2].min() - 2]),
        forend=local([(H[:, 0].min() + H[:, 0].max()) / 2, 0, (H[:, 2].min() + H[:, 2].max()) / 2]),
        sightEye=local([gx - 18, 0, R[:, 2].max() + 8]),
        rackTravel=round(float((H[:, 0].min() - recvFront - 4) * s), 4),  # the forend's room to the receiver, less 4 mm
        grip=[round(float(x), 4) for x in o], receiverFront=round(float(recvFront), 1))
    pts['rack'] = pts['forend']
    mats = source_materials(src)
    out = {}
    for name, sel in (('Pump', body), ('Pump/Forend', fore)):
        part = Part()
        part.add(Pf[sel], Nf[sel], U[sel], M[sel])
        out[name] = (part, mats)
    shell, smat = shell_part(rng)
    out['Pump/Shell'] = (shell, [smat])
    return out, pts


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--pistol', required=True)
    ap.add_argument('--pump', required=True)
    ap.add_argument('--out', required=True, help='the Models folder (Punisher078 is made in it)')
    ap.add_argument('--work', default='/tmp/guns078')
    ap.add_argument('--bpy', default=os.environ.get('SUITS_BPY', '/workspace/bpy-venv/bin/python'))
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)
    rng = np.random.default_rng(78)
    report = {}
    for key, fn, src, info, changes in (
            ('pistol', pistol, a.pistol, PISTOL, 'split into the frame (barrel, suppressor, controls), the slide (with its safety '
             'lever and extractor) and the magazine (the floor plate on a magazine and 9 mm round modelled for HERO VR); turned '
             'into the game\'s frame (grip point at the origin, -z the barrel); textures 4096 -> 2048 px JPEG; tangents added.'),
            ('pump', pump, a.pump, PUMP, 'split into the body and the forend (with the bolt it drives), plus a 12-gauge shell '
             'modelled for HERO VR; scaled 1.12 mm a unit (48 cm), turned into the game\'s frame (grip point at the origin, -z '
             'the barrel); textures 4096 -> 2048 px JPEG; tangents added.')):
        parts, pts = fn(src, a.work, a.bpy, rng)
        report[key] = dict(points=pts, parts={})
        for name, (part, mats) in parts.items():
            tri = part.triangles()
            cap = BODY_CAP if '/' not in name else PART_CAP
            assert tri <= cap, '%s: %d triangles over the %d cap' % (name, tri, cap)
            path = os.path.join(a.out, 'Punisher078', *name.split('/'), 'scene.glb')
            extras = dict(title=info['title'] + (' - ' + name.split('/')[-1] if '/' in name else ''), author='%s (%s)' % (info['author'], info['author_url']),
                          license=LICENSE, source=info['source'], hero_vr=changes)
            write_glb(path, part, mats, extras)
            with open(os.path.join(os.path.dirname(path), 'license.txt'), 'w') as f:
                f.write(license_text(info, changes))
            Pp = part.arrays()[0]
            report[key]['parts'][name] = dict(triangles=tri, bytes=os.path.getsize(path), low=Pp.min(0).round(4).tolist(), high=Pp.max(0).round(4).tolist())
            print('%-14s %6d triangles  %8d bytes  %s .. %s' % (name, tri, os.path.getsize(path), Pp.min(0).round(3), Pp.max(0).round(3)))
        print(key, json.dumps(pts))
    with open(os.path.join(a.out, 'Punisher078', 'conversion-report078.json'), 'w') as f:
        json.dump(report, f, indent=1)


if __name__ == '__main__':
    main()
