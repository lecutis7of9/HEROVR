"""Version 0.0.72 THOR HAMMERS + PUNISHER VAN converter: the user's picks (CC BY 4.0, 27 Oct 6:00 PM) to the GLBs
the mod ships, keeping their real geometry, UVs and base-colour textures (the HARD RULE: "please make sure the
new models arent falttened and messed up like weapons": no per-triangle colour, no tiny triangle budget).

  python3 convert_thor_van_072.py <sources: /workspace/hero-0.0.7-models> <out: .../Models> [work dir]

Hammers (Models/Hammer/<NAME>/scene.glb, mesh_model::slotHammer..): the handle's end at the origin, +y the
handle -> head, -z a face, 50 cm from the handle's end to the head's top. A source over HAMMER_TARGET is cut only
by Blender's collapse decimation (bpy_decimate.py, the UV seams kept).

The van (Models/Vehicle/PunisherVan/scene.glb): GMC Vandura 1976 by snafuj, metres, -z forward, x right (the
driver on -x), the ground under the axles' middle at the origin; the body paint recoloured black from its own
texture (its luminance: the weathering and the baked light kept; chrome and rubber left as they are); collapse-
decimated to VAN_TARGET (the 239,168-triangle tyre treads take most of the cut). The glass is left out: the
overlay has no translucency and an opaque windshield would blind the driver.

The source is an empty shell (floor, wheel wells, walls: no seats, dash or wheel), so the cab is BUILT here
(Models/Vehicle/PunisherVan/Interior): a firewall, a dash with a gauge cluster and glovebox, two bucket seats,
the engine cover, door trim with the inside handle, pedals, a mirror, a floor mat, textured from one generated
1024 px texture; and the steering wheel as its own part (Models/Vehicle/PunisherVan/SteeringWheel: the hub at the
origin, the rim in the xy plane, +z the column toward the driver, +y 12 o'clock) so the game turns it.

Texture layers: each model's minor textures share one atlas (each cropped to its used UVs and packed at one
scale, the UVs remapped), so the five new models take 15 of the overlay's 32 layers."""
import json, math, os, shutil, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'models007'))
import convert_models_072 as cm  # noqa: E402

Image.MAX_IMAGE_PIXELS = None
HAMMER_TARGET = 50000      # mesh_model::slotDrawCap for the hammers
VAN_TARGET = 135000        # 31% of the source's 431,452 (mesh_model::slotDrawCap 150000)
HAMMER_LENGTH = .50        # the handle's end to the head's top (m)
ATLAS = 1024

# slot -> (source folder, the materials that keep their own layer, credit)
HAMMERS = {
    'Hammer/BOOOOP': ('marvel_thors_hammer_mjolnir', ('Pattern3', 'Handle2', 'Hammer3', 'Top3')),
    'Hammer/DEVILSEYE': ('mjolnir_thors_hammer', ('mjolnir_hammer',)),
    'Hammer/LOVEANDTHUNDER': ('mjolnir_-_thor_love_and_thunder', ('Grip', 'Hammer')),
    'Hammer/FCCOOPER': ('thors_hammer_mjolnir', ('HammerHead', 'Accent')),
}
VAN_SOURCE = 'gmc_vandura1976_van'
VAN_OWN = ('Body.001', 'Wheels.002', 'Lights.005')
# the source frame (+z front, +x the driver's side, y up) -> the van frame (-z front, x right, the ground at 0)
VAN_AXLES_MID_Z = .2225
VAN_GROUND = .01


# ---- atlases --------------------------------------------------------------------------------------------
def atlas(mats, uv, mat, group, path, size=ATLAS, pad=6, gap=2):
    """Packs the textures of the materials `group` into one image at `path`: each cropped to its used UVs (+pad
    texels), all at one scale (the largest that fits), the UVs remapped. -> (uv, mat, mats) with the group as one
    material (its own name list), the others unchanged."""
    cells = []
    for mi in group:
        sel = mat == mi
        im = Image.open(mats[mi]['image']).convert('RGB')
        W, H = im.size
        q = np.clip(uv[sel].reshape(-1, 2), 0, 1)
        x0, y0 = max(0, int(math.floor(q[:, 0].min() * W)) - pad), max(0, int(math.floor(q[:, 1].min() * H)) - pad)
        x1, y1 = min(W, int(math.ceil(q[:, 0].max() * W)) + pad), min(H, int(math.ceil(q[:, 1].max() * H)) + pad)
        cells.append(dict(mi=mi, im=im, W=W, H=H, box=(x0, y0, x1, y1)))

    def pack(k):
        order = sorted(range(len(cells)), key=lambda i: -(cells[i]['box'][3] - cells[i]['box'][1]))
        x = y = row = 0
        at = {}
        for i in order:
            b = cells[i]['box']
            w, h = max(1, int((b[2] - b[0]) * k)), max(1, int((b[3] - b[1]) * k))
            if w > size or h > size:
                return None
            if x + w > size:
                x, y, row = 0, y + row + gap, 0
            if y + h > size:
                return None
            at[i] = (x, y, w, h)
            x += w + gap
            row = max(row, h)
        return at
    lo, hi = 1e-4, 1.0
    if pack(hi) is not None:
        lo = hi
    for _ in range(40):
        mid = (lo + hi) / 2
        if pack(mid) is not None:
            lo = mid
        else:
            hi = mid
    k = lo
    at = pack(k)
    out = Image.new('RGB', (size, size), (0, 0, 0))
    uv = uv.copy()
    for i, c in enumerate(cells):
        x, y, w, h = at[i]
        b = c['box']
        out.paste(c['im'].crop(b).resize((w, h), Image.LANCZOS), (x, y))
        sel = mat == c['mi']
        q = np.clip(uv[sel], 0, 1)
        sx, sy = w / (b[2] - b[0]), h / (b[3] - b[1])
        q[..., 0] = (x + (q[..., 0] * c['W'] - b[0]) * sx) / size
        q[..., 1] = (y + (q[..., 1] * c['H'] - b[1]) * sy) / size
        uv[sel] = q
    out.save(path, 'PNG', optimize=True)
    new = len(mats)
    mats = mats + [dict(name='atlas(' + ','.join(mats[i]['name'] for i in group) + ')', image=path, factor=[1, 1, 1, 1],
                        alpha='OPAQUE', cutoff=.5)]
    mat = mat.copy()
    for mi in group:
        mat[mat == mi] = new
    return uv, mat, mats, round(k, 4)


def with_atlas(mats, uv, mat, own, path):
    group = [i for i in sorted(set(mat.tolist())) if mats[i]['name'] not in own and mats[i]['image']]
    if len(group) < 1:
        return uv, mat, mats, None, []
    uv, mat, mats, k = atlas(mats, uv, mat, group, path)
    return uv, mat, mats, k, [mats[i]['name'] for i in group]


# ---- the hammers ----------------------------------------------------------------------------------------
def surface_points(tris, n=200000):
    """Area-weighted points on the triangles (a low-poly handle has vertices only at its ends)."""
    rng = np.random.default_rng(72)
    a = np.linalg.norm(np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0]), axis=1)
    i = rng.choice(len(tris), n, p=a / a.sum())
    r1, r2 = rng.random((2, n, 1))
    s1 = np.sqrt(r1)
    t = tris[i]
    return (1 - s1) * t[:, 0] + s1 * (1 - r2) * t[:, 1] + s1 * r2 * t[:, 2]


def hammer_frame(tris, mat, mats):
    """-> (origin, rot rows x y z, length) in the source's units: +y the handle -> head, -z a face."""
    strap = np.array(['strap' in mats[int(m)]['name'].lower() for m in mat])
    P = surface_points(tris[~strap])
    c = P.mean(0)
    w, v = np.linalg.eigh(np.cov((P - c).T))
    ax = v[:, -1]
    t = (P - c) @ ax
    L = t.max() - t.min()
    radial = np.linalg.norm((P - c) - np.outer(t, ax), axis=1)
    lo_end, hi_end = t < t.min() + .25 * L, t > t.max() - .25 * L
    if np.percentile(radial[lo_end], 90) > np.percentile(radial[hi_end], 90):
        ax, t = -ax, -t
        lo_end, hi_end = hi_end, lo_end
    # the handle's axis line: the centre of the handle's middle third (the head is off the PCA line on some)
    mid = (t > t.min() + .2 * L) & (t < t.min() + .55 * L)
    q = P[mid]
    d = np.linalg.svd(q - q.mean(0), full_matrices=False)[2][0]
    d = d if d @ ax > 0 else -d
    tt = (P - q.mean(0)) @ d
    origin = q.mean(0) + d * tt.min()
    length = tt.max() - tt.min()
    head = P[tt > tt.max() - .4 * length]
    hq = head - head.mean(0)
    hq = hq - np.outer(hq @ d, d)
    w2, v2 = np.linalg.eigh(np.cov(hq.T))
    z = v2[:, -1]
    z = z - d * (z @ d)
    z /= np.linalg.norm(z)
    if np.mean((hq @ z) ** 3) < 0:
        z = -z
    x = np.cross(d, z)
    return origin, np.array([x, d, z]), length


def hammers(src, out, work, report):
    for slot, (name, own) in HAMMERS.items():
        s, g, tris, uv, mat, mesh, mats, notes = cm.take(src, name)
        n0 = len(tris)
        origin, rot, length = hammer_frame(tris, mat, mats)
        t = cm.frame(tris, rot, origin, length, HAMMER_LENGTH)
        # keep the decimation's material indices: decimate all, then atlas
        t2, u2, m2 = cm.decimate(t, uv, mat, HAMMER_TARGET)
        u2, m2, mats2, k, packed = with_atlas(mats, u2, m2, own, os.path.join(work, slot.replace('/', '_') + '_atlas.png'))
        d = os.path.join(out, *slot.split('/'))
        os.makedirs(d, exist_ok=True)
        cm.write_glb(os.path.join(d, 'scene.glb'), t2, u2, m2, mats2)
        shutil.copyfile(os.path.join(s, 'license.txt'), os.path.join(d, 'license.txt'))
        textured = sorted({os.path.basename(mats2[int(i)]['image']) for i in set(m2.tolist()) if mats2[int(i)]['image']})
        lo, hi = t2.reshape(-1, 3).min(0), t2.reshape(-1, 3).max(0)
        report[slot] = dict(source=name, source_triangles=int(n0), triangles=int(len(t2)),
                            reduction='kept whole' if len(t2) == n0 else 'collapse-decimated %d -> %d (UVs kept)' % (n0, len(t2)),
                            textures=textured, uv=True, atlas=dict(materials=packed, scale=k) if packed else None,
                            size_m=(hi - lo).round(3).tolist(),
                            notes=notes + ['the handle\'s end at the origin, +y handle -> head, -z a face, %.0f cm' % (HAMMER_LENGTH * 100)])


# ---- the van --------------------------------------------------------------------------------------------
def to_van(p):
    p = np.asarray(p, np.float64)
    return np.stack([-p[..., 0], p[..., 1] + VAN_GROUND, -(p[..., 2] - VAN_AXLES_MID_Z)], -1)


def black_paint(src_path, dst_path):
    """The body texture in black: the teal paint (blue-green over red) to its own luminance, darkened (the
    weathering, panel lines and baked light kept); the neutral chrome, rubber and glass areas unchanged."""
    im = np.asarray(Image.open(src_path).convert('RGB')).astype(np.float64) / 255
    r, g, b = im[..., 0], im[..., 1], im[..., 2]
    lum = .2126 * r + .7152 * g + .0722 * b
    teal = (np.minimum(g, b) - r)
    mask = np.clip((teal - .015) / .05, 0, 1)
    mask = np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.5))) / 255.
    black = (.045 + .34 * lum ** 1.15)[..., None] * np.array([1, 1, 1.04])
    outp = im * (1 - mask[..., None]) + black * mask[..., None]
    Image.fromarray((np.clip(outp, 0, 1) * 255 + .5).astype(np.uint8)).save(dst_path, 'JPEG', quality=92)
    return float(mask.mean())


def van(src, out, work, report):
    s, g, tris, uv, mat, mesh, mats, notes = cm.take(src, VAN_SOURCE)
    n0 = len(tris)
    names = [m['name'] for m in mats]
    glass = mat == names.index('Windows.001')
    tris, uv, mat = tris[~glass], uv[~glass], mat[~glass]
    t = to_van(tris)
    mats = [dict(m) for m in mats]
    bi = names.index('Body.001')
    painted = os.path.join(work, 'Body.001_black.jpg')
    share = black_paint(mats[bi]['image'], painted)
    mats[bi]['image'] = painted
    mats[bi]['name'] = 'Body.001 (black)'
    t2, u2, m2 = cm.decimate(t, uv, mat, VAN_TARGET)
    per = {mats[i]['name']: [int((mat == i).sum()), int((m2 == i).sum())] for i in sorted(set(mat.tolist()))}
    own = set(VAN_OWN) | {'Body.001 (black)'}
    u2, m2, mats2, k, packed = with_atlas(mats, u2, m2, own, os.path.join(work, 'Van_atlas.png'))
    d = os.path.join(out, 'Vehicle', 'PunisherVan')
    os.makedirs(d, exist_ok=True)
    cm.write_glb(os.path.join(d, 'scene.glb'), t2, u2, m2, mats2, {'frame': 'metres, -z forward, x right, the ground at 0'})
    shutil.copyfile(os.path.join(s, 'license.txt'), os.path.join(d, 'license.txt'))
    lo, hi = t2.reshape(-1, 3).min(0), t2.reshape(-1, 3).max(0)
    textured = sorted({os.path.basename(mats2[int(i)]['image']) for i in set(m2.tolist()) if mats2[int(i)]['image']})
    report['Vehicle/PunisherVan'] = dict(
        source=VAN_SOURCE, source_triangles=int(n0), triangles=int(len(t2)),
        reduction='collapse-decimated %d -> %d (UVs kept; the glass, %d, left out)' % (n0, len(t2), int(glass.sum())),
        per_material=per, textures=textured, uv=True, atlas=dict(materials=packed, scale=k), size_m=(hi - lo).round(3).tolist(),
        paint=dict(recoloured='Body.001 to black from its own luminance', share_of_texels=round(share, 3)),
        notes=notes + ['the source is a closed shell with an empty cab (no seats, dash or steering wheel): '
                       'Vehicle/PunisherVan/Interior and /SteeringWheel are built (convert_thor_van_072.py)',
                       'the glass (Windows.001) left out: the overlay has no translucency; an opaque windshield blinds the driver'])
    return lo, hi


# ---- the built cab --------------------------------------------------------------------------------------
# The interior texture's regions (u0, v0, u1, v1; v down)
R = dict(seat=(0, 0, .5, .5), dash=(.5, 0, 1, .25), gauges=(.5, .25, 1, .5), mat=(0, .5, .5, .75),
         grip=(.5, .5, .75, .75), chrome=(.75, .5, 1, .75), door=(0, .75, .5, 1), metal=(.5, .75, 1, 1))


def interior_texture(path, size=1024):
    rng = np.random.default_rng(72)
    img = Image.new('RGB', (size, size))
    d = ImageDraw.Draw(img)
    S = size

    def box(r):
        return int(r[0] * S), int(r[1] * S), int(r[2] * S), int(r[3] * S)

    def noise(w, h, amp, base):
        n = rng.normal(0, amp, (h, w, 1))
        return np.clip(np.asarray(base, np.float64)[None, None, :] + n, 0, 255)

    def put(r, arr):
        x0, y0, x1, y1 = box(r)
        img.paste(Image.fromarray(arr.astype(np.uint8)), (x0, y0))
    # seat: black tuck-and-roll vinyl (horizontal pleats, a stitched border)
    x0, y0, x1, y1 = box(R['seat'])
    w, h = x1 - x0, y1 - y0
    a = noise(w, h, 3, (30, 29, 31))
    yy = np.arange(h)[:, None]
    pleat = .5 + .5 * np.cos((yy % 40) / 40 * 2 * np.pi)
    a = a * (.62 + .45 * pleat[..., None])
    a[(yy % 40 < 2).ravel()] *= .5
    a[:12], a[-12:], a[:, :12], a[:, -12:] = a[:12] * .6, a[-12:] * .6, a[:, :12] * .6, a[:, -12:] * .6
    put(R['seat'], a)
    # dash: dark grained vinyl
    x0, y0, x1, y1 = box(R['dash'])
    a = noise(x1 - x0, y1 - y0, 4, (38, 37, 36))
    put(R['dash'], np.asarray(Image.fromarray(a.astype(np.uint8)).filter(ImageFilter.GaussianBlur(.8))))
    # gauges: black face, chrome bezels, white ticks, red needles (speedo, fuel / temp)
    x0, y0, x1, y1 = box(R['gauges'])
    put(R['gauges'], noise(x1 - x0, y1 - y0, 2, (14, 14, 16)))
    for cx, rad, ticks in ((.33, .2, 12), (.67, .2, 8)):
        X, Y = x0 + cx * (x1 - x0), (y0 + y1) / 2
        r_ = rad * (x1 - x0)
        d.ellipse([X - r_ - 9, Y - r_ - 9, X + r_ + 9, Y + r_ + 9], fill=(170, 172, 176))
        d.ellipse([X - r_ - 4, Y - r_ - 4, X + r_ + 4, Y + r_ + 4], fill=(90, 92, 96))
        d.ellipse([X - r_, Y - r_, X + r_, Y + r_], fill=(10, 10, 12))
        for i in range(ticks + 1):
            ang = math.radians(225 - 270 * i / ticks)
            p0 = (X + math.cos(ang) * r_ * .78, Y - math.sin(ang) * r_ * .78)
            p1 = (X + math.cos(ang) * r_ * .93, Y - math.sin(ang) * r_ * .93)
            d.line([p0, p1], fill=(235, 235, 225), width=4)
        ang = math.radians(225 - 270 * .3)
        d.line([(X, Y), (X + math.cos(ang) * r_ * .85, Y - math.sin(ang) * r_ * .85)], fill=(210, 40, 30), width=5)
        d.ellipse([X - 8, Y - 8, X + 8, Y + 8], fill=(60, 60, 64))
    # floor mat: ribbed black rubber
    x0, y0, x1, y1 = box(R['mat'])
    a = noise(x1 - x0, y1 - y0, 3, (24, 24, 25))
    xx = np.arange(x1 - x0)[None, :]
    a *= (.75 + .35 * (xx % 16 < 8))[..., None]
    put(R['mat'], a)
    # grip: the wheel's rubber, finger grooves
    x0, y0, x1, y1 = box(R['grip'])
    a = noise(x1 - x0, y1 - y0, 2, (22, 21, 22))
    xx = np.arange(x1 - x0)[None, :]
    a *= (.8 + .3 * np.cos((xx % 21) / 21 * 2 * np.pi))[..., None]
    put(R['grip'], a)
    # chrome: a bright brushed gradient
    x0, y0, x1, y1 = box(R['chrome'])
    yy = np.linspace(0, 1, y1 - y0)[:, None, None]
    a = 120 + 120 * np.abs(np.sin(yy * 5.5)) + rng.normal(0, 4, (y1 - y0, x1 - x0, 1))
    put(R['chrome'], np.repeat(a, 3, axis=2))
    # door trim: black vinyl with stitched horizontal panels
    x0, y0, x1, y1 = box(R['door'])
    a = noise(x1 - x0, y1 - y0, 3, (33, 32, 33))
    yy = np.arange(y1 - y0)[:, None]
    a[((yy % 64) < 3).ravel()] *= .45
    a[(((yy % 64) > 30) & ((yy % 64) < 33)).ravel()] *= 1.5
    put(R['door'], a)
    # metal: dark painted steel (the column, pedals, firewall)
    x0, y0, x1, y1 = box(R['metal'])
    put(R['metal'], noise(x1 - x0, y1 - y0, 5, (44, 45, 48)))
    img.save(path, 'PNG', optimize=True)


class Builder:
    """Triangles with UVs in one texture's regions; a part's faces are mapped by their dominant axis."""

    def __init__(self):
        self.t, self.u = [], []

    def add(self, tris, uvs):
        self.t.append(np.asarray(tris, np.float64).reshape(-1, 3, 3))
        self.u.append(np.asarray(uvs, np.float64).reshape(-1, 3, 2))

    def convex(self, pts, region, faces=None):
        """A convex part from its corner points (scipy's hull), each face planar-mapped into `region` by its
        dominant axis (over the part's own bounds); faces: {axis sign key: region} overrides, e.g. {'+z': ...}."""
        from scipy.spatial import ConvexHull
        pts = np.asarray(pts, np.float64)
        h = ConvexHull(pts)
        c = pts.mean(0)
        lo, hi = pts.min(0), pts.max(0)
        span = np.maximum(hi - lo, 1e-6)
        for simplex, eq in zip(h.simplices, h.equations):
            tri = pts[simplex]
            n = eq[:3]
            if np.dot(np.cross(tri[1] - tri[0], tri[2] - tri[0]), n) < 0:
                tri = tri[[0, 2, 1]]
            k = int(np.argmax(np.abs(n)))
            key = ('+' if n[k] > 0 else '-') + 'xyz'[k]
            reg = (faces or {}).get(key, region)
            a, b = [(1, 2), (0, 2), (0, 1)][k]
            uu = (tri[:, a] - lo[a]) / span[a]
            vv = 1 - (tri[:, b] - lo[b]) / span[b]
            if k == 1:
                vv = (tri[:, 2] - lo[2]) / span[2]
            if key in ('-x', '+z'):
                uu = 1 - uu if key == '-x' else uu
            u0, v0, u1, v1 = reg
            self.add(tri, np.c_[u0 + (u1 - u0) * (.02 + .96 * uu), v0 + (v1 - v0) * (.02 + .96 * vv)])
        del c

    def box(self, x0, x1, y0, y1, z0, z1, region, bevel=0., faces=None, rot=None, about=None):
        b = bevel
        pts = []
        for x in (x0, x1):
            for y in (y0, y1):
                for z in (z0, z1):
                    if b <= 0:
                        pts.append((x, y, z))
                        continue
                    sx, sy, sz = (1 if x == x1 else -1), (1 if y == y1 else -1), (1 if z == z1 else -1)
                    pts += [(x - sx * b, y, z - sz * b), (x, y - sy * b, z - sz * b), (x - sx * b, y - sy * b, z)]
        pts = np.array(pts, np.float64)
        if rot is not None:
            pts = (pts - about) @ np.asarray(rot).T + about
        self.convex(pts, region, faces)

    def cylinder(self, a, b, r, region, seg=16):
        a, b = np.asarray(a, np.float64), np.asarray(b, np.float64)
        ax = b - a
        ax /= np.linalg.norm(ax)
        p = np.cross(ax, [0, 1, 0] if abs(ax[1]) < .9 else [1, 0, 0])
        p /= np.linalg.norm(p)
        q = np.cross(ax, p)
        u0, v0, u1, v1 = region
        for i in range(seg):
            t0, t1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
            e0, e1 = (p * math.cos(t0) + q * math.sin(t0)) * r, (p * math.cos(t1) + q * math.sin(t1)) * r
            U0, U1 = u0 + (u1 - u0) * i / seg, u0 + (u1 - u0) * (i + 1) / seg
            self.add([[a + e0, b + e0, b + e1], [a + e0, b + e1, a + e1]], [[U0, v0, U0, v1, U1, v1], [U0, v0, U1, v1, U1, v0]])
            cu, cv = (u0 + u1) / 2, (v0 + v1) / 2
            self.add([[a, a + e1, a + e0], [b, b + e0, b + e1]], [[cu, cv, U1, v0, U0, v0], [cu, cv, U0, v1, U1, v1]])

    def torus(self, R_, r, region, seg=64, ring=12):
        u0, v0, u1, v1 = region
        for i in range(seg):
            for j in range(ring):
                def P(ii, jj):
                    a, b = 2 * math.pi * ii / seg, 2 * math.pi * jj / ring
                    return np.array([(R_ + r * math.cos(b)) * math.cos(a), (R_ + r * math.cos(b)) * math.sin(a), r * math.sin(b)])

                a, b, c, d = P(i, j), P(i + 1, j), P(i + 1, j + 1), P(i, j + 1)
                ua = [u0 + (u1 - u0) * ((i % 8) / 8), v0 + (v1 - v0) * j / ring]
                ub = [u0 + (u1 - u0) * ((i % 8 + 1) / 8), v0 + (v1 - v0) * j / ring]
                uc = [ub[0], v0 + (v1 - v0) * (j + 1) / ring]
                ud = [ua[0], uc[1]]
                self.add([[a, b, c], [a, c, d]], [ua + ub + uc, ua + uc + ud])

    def arrays(self):
        t = np.concatenate(self.t)
        u = np.concatenate(self.u)
        return t, u, np.zeros(len(t), np.int64)


# where the driver sits (van frame, metres); van072.hpp's constants are these
CAB = dict(floor=.455, wheel_hub=(-.45, 1.18, -1.08), wheel_axis=(0, .42, .91), wheel_radius=.19,
           seat_eye=(-.45, 1.60, -.55), door_handle_in=(-.855, 1.08, -.83))


def build_interior():
    b = Builder()
    fl = CAB['floor']
    b.box(-.88, .88, fl, fl + .006, -1.55, -.22, R['mat'])                                     # the floor mat
    b.box(-.90, .90, fl, 1.02, -1.62, -1.55, R['metal'])                                       # the firewall
    b.box(-.90, .90, .95, 1.27, -1.60, -1.28, R['dash'], bevel=.03)                            # the dash
    b.box(-.72, -.18, 1.22, 1.33, -1.46, -1.27, R['dash'], bevel=.02)                          # the cluster hood
    b.box(-.70, -.20, 1.03, 1.24, -1.30, -1.265, R['dash'], faces={'+z': R['gauges']})          # the gauges
    b.box(.25, .66, 1.02, 1.19, -1.285, -1.27, R['chrome'], faces={'+z': R['dash']}, bevel=.006)  # the glovebox
    b.box(-.24, .24, fl, .78, -1.55, -.74, R['dash'], bevel=.05)                               # the engine cover
    for side in (-1, 1):                                                                       # the bucket seats
        x0, x1 = sorted((side * .18, side * .78))
        b.box(x0 + .06, x1 - .06, fl, .70, -.72, -.36, R['metal'])                             # the pedestal
        b.box(x0, x1, .70, .88, -.80, -.28, R['seat'], bevel=.04)                              # the cushion
        a = math.radians(-11)
        rot = np.array([[1, 0, 0], [0, math.cos(a), -math.sin(a)], [0, math.sin(a), math.cos(a)]])
        b.box(x0, x1, .86, 1.56, -.32, -.18, R['seat'], bevel=.04, rot=rot, about=np.array([0, .86, -.25]))  # the backrest
        dx = side * .89
        b.box(min(dx, dx + side * .025), max(dx, dx + side * .025), .55, 1.32, -1.20, -.48, R['door'])        # the door trim
        b.box(min(dx, dx - side * .03), max(dx, dx - side * .03), .98, 1.01, -1.15, -.55, R['door'], bevel=.008)  # the arm rest lip
    hx, hy, hz = CAB['door_handle_in']
    b.box(-.875, -.865, hy - .03, hy + .03, hz - .07, hz + .07, R['chrome'], bevel=.003)       # the handle's escutcheon
    b.box(-.868, -.842, hy - .012, hy + .012, hz - .06, hz + .05, R['chrome'], bevel=.004)     # the inside handle lever
    for x0, x1, top in ((-.36, -.28, .66), (-.56, -.45, .64)):                                 # the gas and brake pedals
        b.box(x0, x1, top - .1, top, -1.40, -1.37, R['metal'], bevel=.005)
    hub, axis = np.array(CAB['wheel_hub']), np.array(CAB['wheel_axis']) / np.linalg.norm(CAB['wheel_axis'])
    b.cylinder(hub - axis * .06, hub - axis * .34, .032, R['metal'])                           # the steering column
    b.box(-.12, .12, 1.72, 1.78, -1.24, -1.22, R['chrome'], faces={'+z': R['metal']}, bevel=.008)  # the mirror
    b.cylinder((0, 1.78, -1.23), (0, 1.86, -1.20), .008, R['metal'], seg=8)
    return b.arrays()


def build_wheel():
    """The steering wheel: the hub at the origin, the rim in the xy plane, +z toward the driver, +y 12 o'clock."""
    b = Builder()
    Rr = CAB['wheel_radius']
    b.torus(Rr, .016, R['grip'])
    for ang in (0, 180, 270):  # 3, 9 and 6 o'clock spokes, dished toward the column
        a = math.radians(ang)
        tip = np.array([math.cos(a) * (Rr - .01), math.sin(a) * (Rr - .01), 0])
        b.cylinder((0, 0, -.035), tip, .010, R['chrome'], seg=10)
    b.cylinder((0, 0, -.06), (0, 0, -.015), .052, R['dash'], seg=24)                         # the hub
    b.cylinder((0, 0, -.015), (0, 0, .004), .040, R['chrome'], seg=24)                         # the horn cap
    return b.arrays()


# Version 0.0.72: the built parts' own note (they are HERO VR's, not the van artist's).
BUILT_LICENSE = ('''Model Information:
* title:	%s (built)
* author:	Agent Mulder, for HERO VR 0.0.72 (tools/models072/convert_thor_van_072.py)

Not from a downloaded model: the GMC Vandura 1976 by snafuj (CC-BY-4.0, ../license.txt) has an empty cab,
so its cab (seats, dash, gauges, door card) and steering wheel are built here, with one generated texture.
Shipped with HERO VR under the project's licence (GPLv3); HERO VR is free and never sold.
''')


def cab(out, work, report):
    tex = os.path.join(work, 'Interior_texture.png')
    interior_texture(tex)
    mats = [dict(name='Interior (built)', image=tex, factor=[1, 1, 1, 1], alpha='OPAQUE', cutoff=.5)]
    for slot, (t, u, m) in (('Vehicle/PunisherVan/Interior', build_interior()), ('Vehicle/PunisherVan/SteeringWheel', build_wheel())):
        d = os.path.join(out, *slot.split('/'))
        os.makedirs(d, exist_ok=True)
        cm.write_glb(os.path.join(d, 'scene.glb'), t, u, m, mats, {'built': 'HERO VR 0.0.72 (Agent Mulder); not from the source model'})
        with open(os.path.join(d, 'license.txt'), 'w') as f:
            f.write(BUILT_LICENSE % slot)
        lo, hi = t.reshape(-1, 3).min(0), t.reshape(-1, 3).max(0)
        report[slot] = dict(source='BUILT', source_triangles=int(len(t)), triangles=int(len(t)), reduction='built',
                            textures=['Interior_texture.png'], uv=True, size_m=(hi - lo).round(3).tolist(), cab=CAB,
                            notes=['built for the empty source cab: one generated texture (vinyl, tuck-and-roll, gauges, '
                                   'rubber, chrome), shared with the other built part (one layer)'])


def main(src, out, work):
    os.makedirs(work, exist_ok=True)
    p = os.path.join(out, 'conversion-report.json')
    report = json.load(open(p)) if os.path.exists(p) else {}
    for k in [k for k in report if k == 'Hammer' or k.startswith('Hammer/') or k.startswith('Vehicle/')]:
        del report[k]
    for old in ('Hammer', os.path.join('Vehicle', 'PunisherVan')):
        q = os.path.join(out, old)
        if os.path.isdir(q):
            shutil.rmtree(q)
    hammers(src, out, work, report)
    van(src, out, work, report)
    cab(out, work, report)
    json.dump(report, open(p, 'w'), indent=1)
    for k, v in report.items():
        if k.startswith(('Hammer', 'Vehicle')):
            print('%-34s %7d -> %7d  %s' % (k, v['source_triangles'], v['triangles'], v.get('size_m')))


if __name__ == '__main__':
    a = sys.argv[1:] + [None] * 3
    main(a[0] or '/workspace/hero-0.0.7-models', a[1] or os.path.join(HERE, '..', '..', 'build', 'h7out', 'Models'),
         a[2] or '/tmp/sm2073_work/gen')
