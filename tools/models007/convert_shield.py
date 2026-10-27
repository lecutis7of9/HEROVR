"""Version 0.0.7 CAPTAIN AMERICA: Digital Goblin's 'Captain America Shield' (CC BY 4.0) as
the mod's Models/Shield drop-in (convert_models.py calls convert(); it can run alone too):

  python tools/models007/convert_shield.py <source folder (read only)> <output Models folder>

The source is one mesh: the disc (640 large fan triangles, front and back), two leather straps
with their buckles and two mount pads. Baking one colour per large fan triangle smears the
red / white / blue rings and the star into mud, so the disc is REMESHED from the source (a
derivative the CC BY licence allows, credited in THIRD-PARTY-MODELS.md):
  - the front's height z(r) and the back's are read from the source's own vertices;
  - the ring edges and the star (centre, tips, notch radius) are found in the source texture
    along the front's exact UV map (an affine fit of the front vertices, error < 0.001);
  - a polar mesh is laid on those edges (a 5-point star, a blue field, red / white / red rings),
    the rim, the back; each triangle's colour is the mean of 16 texture samples inside it
    (the paint's wear is kept - visual only);
  - the straps, buckles and pads are decimated (quadric) to fill the 700-triangle budget.
The mod frame: metres, the hand strap at the origin, y along the forearm (elbow -> hand), x across,
-z the face's normal (the item's forward is the face), the star's top along -x; that is the LEFT
forearm's fit (the arm across the body, the hand at the wearer's right strap). asset.extras.pivot
is the OTHER strap: for the RIGHT arm drawers turn the shield 180 degrees about the face normal and
put that strap in the hand, so the star stays upright and the right hand grips the mirror strap. Diameter 0.61 m (the MCU prop's 24 in)."""
import os, sys
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gltf_io

BUDGET = 700
DIAMETER_M = .61
SECTORS = 40  # a multiple of 10: the star's tips and notches fall on sector lines


def _lin(c):
    c = np.asarray(c, np.float64)
    return np.where(c <= .04045, c / 12.92, ((c + .055) / 1.055) ** 2.4)


def _components(tris):
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


def analyse(src):
    g, parts = gltf_io.load(src)
    assert len(parts) == 1, 'the shield source is one mesh'
    p = parts[0]
    tris, uv = p['pos'].astype(np.float64), p['uv'].astype(np.float64)
    mat = g['materials'][p['material']]
    uri = g['images'][g['textures'][mat['pbrMetallicRoughness']['baseColorTexture']['index']]['source']]['uri']
    tex = np.asarray(Image.open(os.path.join(src, uri)).convert('RGB'), np.float64) / 255.0
    comp = _components(tris)
    ids, cnt = np.unique(comp, return_counts=True)
    # the disc: the component with the largest area
    area = np.linalg.norm(np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0]), axis=1)
    disc_id = max(ids, key=lambda k: area[comp == k].sum())
    disc = comp == disc_id
    n = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    front = disc & (n[:, 2] > 0)
    back = disc & (n[:, 2] < 0)
    P = tris[front].reshape(-1, 3)
    centre = np.array([(P[:, 0].min() + P[:, 0].max()) / 2, (P[:, 1].min() + P[:, 1].max()) / 2])
    radius = np.hypot(P[:, 0] - centre[0], P[:, 1] - centre[1]).max()

    def profile(sel):
        Q = tris[sel].reshape(-1, 3)
        r = np.round(np.hypot(Q[:, 0] - centre[0], Q[:, 1] - centre[1]), 3)
        rs, idx = np.unique(r, return_index=True)
        return rs, Q[idx, 2]

    def uvmap(sel):
        Q = tris[sel].reshape(-1, 3)
        U = uv[sel].reshape(-1, 2)
        X = np.c_[Q[:, 0], Q[:, 1], np.ones(len(Q))]
        A = np.linalg.lstsq(X, U, rcond=None)[0]
        return A, float(np.abs(X @ A - U).max())
    fA, ferr = uvmap(front)
    bA, berr = uvmap(back)
    return dict(g=g, tris=tris, uv=uv, tex=tex, disc=disc, front=front, back=back, centre=centre, radius=radius,
                zf=profile(front), zb=profile(back), fA=fA, bA=bA, uv_error=max(ferr, berr), comp=comp, area=area)


def sample(tex, A, pts):
    """Texture colours (linear) at model xy points through the affine uv map A (3x2)."""
    H, W, _ = tex.shape
    U = np.c_[pts[..., 0].ravel(), pts[..., 1].ravel(), np.ones(pts[..., 0].size)] @ A
    u = np.clip((np.mod(U[:, 0], 1) * W).astype(int), 0, W - 1)
    v = np.clip((np.mod(U[:, 1], 1) * H).astype(int), 0, H - 1)
    return _lin(tex[v, u]).reshape(pts.shape[:-1] + (3,))


def classify(c):
    """0 white, 1 red, 2 blue, 3 dark (sRGB 0-1 rows)."""
    out = np.full(len(c), 3)
    out[c.min(1) > .5] = 0
    out[(c[:, 0] > c[:, 1] + .15) & (c[:, 0] > c[:, 2] + .15)] = 1
    out[(c[:, 2] > c[:, 0] + .08) & (c.min(1) <= .5)] = 2
    return out


def find_design(a):
    """The ring edges and the star, from the texture along the front's uv map."""
    tex, A, cen, R = a['tex'], a['fA'], a['centre'], a['radius']
    H, W, _ = tex.shape
    ang = np.linspace(0, 2 * np.pi, 720, endpoint=False)

    def cls_at(r):
        pts = np.stack([cen[0] + r * np.cos(ang), cen[1] + r * np.sin(ang)], -1)
        U = np.c_[pts, np.ones(len(pts))] @ A
        c = tex[np.clip((U[:, 1] * H).astype(int), 0, H - 1), np.clip((U[:, 0] * W).astype(int), 0, W - 1)]
        return classify(c)
    rs = np.linspace(0, R * .995, 400)
    major = []
    for r in rs:
        k = np.bincount(cls_at(r), minlength=4)
        major.append(int(k.argmax()))
    major = np.array(major)
    # the outer rings: from the rim inward the classes red, white, red, then the blue field (with the star)
    edges = []
    for i in range(len(rs) - 1, 0, -1):
        if major[i] != major[i - 1]:
            edges.append(((rs[i] + rs[i - 1]) / 2, major[i], major[i - 1]))
    # keep the edges outside the blue field: red->white, white->red, red->blue
    bands = []
    for r, outer, inner in edges:
        bands.append((r, outer, inner))
        if inner == 2 or inner == 0 and r < R * .3:
            break
    blue_edge = [r for r, o, i in bands if i == 2]
    assert blue_edge, 'no blue field found: %r' % edges
    rb = blue_edge[0]
    rings = []
    for r in sorted(r for r, o, i in bands if r >= rb - 1e-6):
        if not rings or r - rings[-1] > R * .03:  # a worn speck is not a ring
            rings.append(r)
    # the star: notches (the field's blue reaching in) at the radius just past the white centre
    # the star: a regular 5-point star with its tips on the blue field's edge (its inner radius
    # 0.382 of the outer, the regular star's ratio); its turn is the one whose star shape best
    # matches the texture (white inside, blue outside: the paint's wear makes edge tracing unreliable)
    r_out = rb * .97
    r_in = r_out * .382
    g = np.mgrid[-1:1:.02, -1:1:.02].reshape(2, -1).T * rb
    g = g[np.hypot(*g.T) < rb * .98]
    U = np.c_[g + cen, np.ones(len(g))] @ A
    gc = classify(tex[np.clip((U[:, 1] * H).astype(int), 0, H - 1), np.clip((U[:, 0] * W).astype(int), 0, W - 1)])
    def inside(t0, off):
        ga, gr = np.arctan2(g[:, 1] - off[1], g[:, 0] - off[0]), np.hypot(g[:, 0] - off[0], g[:, 1] - off[1])
        # the star's boundary radius at angle a: the line between a tip and the next notch
        rel = np.mod(ga - t0, 2 * np.pi / 5)
        rel = np.minimum(rel, 2 * np.pi / 5 - rel)  # 0 at a tip, pi/5 at a notch
        tip, notch = np.array([r_out, 0.]), np.array([r_in * np.cos(np.pi / 5), r_in * np.sin(np.pi / 5)])
        # intersect the ray at angle rel with the tip-notch segment
        dvec = notch - tip
        den = np.cos(rel) * dvec[1] - np.sin(rel) * dvec[0]
        tpar = (tip[0] * dvec[1] - tip[1] * dvec[0]) / den
        return gr <= tpar
    best, tip0, soff = -1, 0., np.zeros(2)
    for dx in np.arange(-3, 3.01, 1.):  # the star may sit a little off the disc's centre in the paint
        for dy in np.arange(-3, 3.01, 1.):
            for t0 in np.radians(np.arange(0, 72, 1.)):
                m = inside(t0, (dx, dy))
                sc = (gc[m] == 0).mean() + (gc[~m] == 2).mean()
                if sc > best:
                    best, tip0, soff = sc, t0, np.array([dx, dy])
    for t0 in tip0 + np.radians(np.arange(-1, 1.01, .25)):
        m = inside(t0, soff)
        sc = (gc[m] == 0).mean() + (gc[~m] == 2).mean()
        if sc > best:
            best, tip0 = sc, t0
    star_fit = best / 2
    r_out = min(r_out, rb - np.hypot(*soff) - .01 * R)  # the tips stay inside the blue field
    r_in = r_out * .382
    assert star_fit > .75, 'the star does not match the texture (%.2f)' % star_fit
    return dict(rings=[float(r) for r in rings], blue=float(rb), star_in=float(r_in), star_out=float(r_out),
                tip0=float(tip0), radius=float(R), star_fit=float(star_fit), star_offset=soff.tolist())


def remesh(a, d):
    cen, R = a['centre'], a['radius']
    zf = lambda r: np.interp(r, *a['zf'])
    zb = lambda r: np.interp(r, *a['zb'])
    N = SECTORS
    th = d['tip0'] + np.arange(N) * 2 * np.pi / N  # sector lines; every 4th a tip or a notch
    P = lambda r, t, z: np.array([cen[0] + r * np.cos(t), cen[1] + r * np.sin(t), z])
    tris, cols, kinds, cells, bands = [], [], [], [], []

    def add(a_, b_, c_, kind, pair=False, band=-1):
        # pair: the second half of a quad cell (one colour a cell: the texture's baked brushed-metal
        # highlights otherwise make the two halves of each cell differ, a saw-tooth along the rings)
        tris.append([a_, b_, c_])
        kinds.append(kind)
        bands.append(band)
        cells.append(cells[-1] if pair and cells else (cells[-1] + 1 if cells else 0))
    # the star: 10 points (tips at star_out, notches at star_in), a fan from the centre
    ro, ri = d['star_out'], d['star_in']
    star = []
    for k in range(10):
        r = ro if k % 2 == 0 else ri
        t = d['tip0'] + k * np.pi / 5
        star.append((r, t))
    so = np.asarray(d['star_offset'], np.float64)

    def S(r, t):  # a star point (about the star's own centre), on the front's surface
        x, y = cen[0] + so[0] + r * np.cos(t), cen[1] + so[1] + r * np.sin(t)
        return np.array([x, y, zf(np.hypot(x - cen[0], y - cen[1]))])
    c0 = S(0, 0)
    for k in range(10):
        (r1, t1), (r2, t2) = star[k], star[(k + 1) % 10]
        add(c0, S(r1, t1), S(r2, t2), 'front', band=0)
    # the blue field: each star edge (split in 4) to its 36-degree arc of the blue circle (4 sectors)
    rb = d['blue']
    per = N // 10
    for k in range(10):
        (r1, t1), (r2, t2) = star[k], star[(k + 1) % 10]
        p1, p2 = S(r1, t1), S(r2, t2)
        inner = []
        for s in range(per + 1):
            q = p1 + (p2 - p1) * (s / per)
            rq = np.hypot(q[0] - cen[0], q[1] - cen[1])
            inner.append(np.array([q[0], q[1], zf(rq)]))
        outer = [P(rb, t1 + (t2 - t1) * (s / per) if abs(t2 - t1) < np.pi else t1 + (np.angle(np.exp(1j * (t2 - t1)))) * (s / per),
                   zf(rb)) for s in range(per + 1)]
        for s in range(per):
            add(inner[s], outer[s], outer[s + 1], 'front', band=1)
            add(inner[s], outer[s + 1], inner[s + 1], 'front', True, band=1)
    # the rings: one band per colour edge, out to the rim
    radii = [rb] + [r for r in d['rings'] if r > rb + 1e-6] + [R]
    radii = sorted(set(np.round(radii, 6)))
    for i in range(len(radii) - 1):
        r1, r2 = radii[i], radii[i + 1]
        for s in range(N):
            ta, tb = th[s], th[(s + 1) % N] + (2 * np.pi if s == N - 1 else 0)
            add(P(r1, ta, zf(r1)), P(r2, ta, zf(r2)), P(r2, tb, zf(r2)), 'front', band=2 + i)
            add(P(r1, ta, zf(r1)), P(r2, tb, zf(r2)), P(r1, tb, zf(r1)), 'front', True, band=2 + i)
    # the rim: the front's edge to the back's
    for s in range(N):
        ta, tb = th[s], th[(s + 1) % N]
        add(P(R, ta, zf(R)), P(R, ta, zb(R)), P(R, tb, zb(R)), 'rim')
        add(P(R, ta, zf(R)), P(R, tb, zb(R)), P(R, tb, zf(R)), 'rim', True)
    # the back: a fan and one ring (at its profile's knee, 0.45 R)
    rk = .45 * R
    cb = P(0, 0, zb(0))
    for s in range(N):
        ta, tb = th[s], th[(s + 1) % N]
        add(cb, P(rk, tb, zb(rk)), P(rk, ta, zb(rk)), 'back')
        add(P(rk, ta, zb(rk)), P(rk, tb, zb(rk)), P(R, tb, zb(R)), 'back')
        add(P(rk, ta, zb(rk)), P(R, tb, zb(R)), P(R, ta, zb(R)), 'back', True)
    tris = np.array(tris)
    # one winding: every normal points out of the shield (front +z, back -z, rim away from the
    # axis) so a drawer can paint the side turned away first (the disc is thin: a centroid sort
    # alone lets the back's long triangles cut through the face)
    n = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    want = np.zeros_like(n)
    for i, kind in enumerate(kinds):
        if kind == 'front':
            want[i] = (0, 0, 1)
        elif kind == 'back':
            want[i] = (0, 0, -1)
        else:
            m = tris[i].mean(0)
            want[i] = (m[0] - cen[0], m[1] - cen[1], 0)
    flip = (n * want).sum(1) < 0
    tris[flip] = tris[flip][:, [0, 2, 1]]
    # colours: 16 samples inside each triangle through the face's own uv map
    bary = np.array([(i, j, 1 - i - j) for i in np.linspace(.04, .92, 6) for j in np.linspace(.04, .92, 6) if i + j <= .96])
    bary = bary[np.linspace(0, len(bary) - 1, 16).astype(int)]
    for t, kind in zip(tris, kinds):
        pts = bary @ t
        if kind == 'back':
            c = sample(a['tex'], a['bA'], pts[:, :2]).mean(0)
        elif kind == 'rim':  # the edge: the outer ring's paint, a little darker (worn metal edge)
            dirn = (t.mean(0)[:2] - cen)
            dirn /= np.linalg.norm(dirn)
            q = cen + dirn * (R * .985)
            c = sample(a['tex'], a['fA'], np.array([q])).mean(0) * .8
        else:
            c = sample(a['tex'], a['fA'], pts[:, :2]).mean(0)
        cols.append(c)
    cols, cells, bands = np.array(cols), np.array(cells), np.array(bands)
    for k in np.unique(cells):
        cols[cells == k] = cols[cells == k].mean(0)
    # The painted front: each band (the star, the blue field, each ring) is ONE paint - the most
    # common colour class over the band, its median colour - and a cell only varies its brightness
    # with the texture's wear (0.8-1.08 of the band's): the texture's baked brushed-metal shading
    # and dark scuffs otherwise turn whole cells black or grey in a one-colour-a-cell overlay.
    lum = lambda c: c @ np.array([.2126, .7152, .0722])
    for b in np.unique(bands[bands >= 0]):
        m = bands == b
        pts = np.concatenate([bary @ t for t in tris[m]])
        raw = a['tex'][np.clip(((np.c_[pts[:, :2], np.ones(len(pts))] @ a['fA'])[:, 1] * a['tex'].shape[0]).astype(int), 0,
                               a['tex'].shape[0] - 1),
                       np.clip(((np.c_[pts[:, :2], np.ones(len(pts))] @ a['fA'])[:, 0] * a['tex'].shape[1]).astype(int), 0,
                               a['tex'].shape[1] - 1)]
        k = classify(raw)
        cls = np.bincount(k[k < 3], minlength=3).argmax() if (k < 3).any() else 3
        paint = np.median(_lin(raw[k == cls]), 0)
        wear = np.clip(lum(cols[m]) / max(lum(paint), 1e-4), .8, 1.08)
        cols[m] = paint[None, :] * wear[:, None]
    # the back and the straps are near-black metal and leather: a floor keeps their shape readable
    cols = np.maximum(cols, .018)
    return tris, cols, kinds


def straps(a, target):
    """The leather straps, buckles and pads (everything but the disc), decimated, colours baked."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import convert_models  # noqa: E402 (pyfqmr decimation and texture sampling)
    sel = ~a['disc']
    tris = a['tris'][sel]
    uvs = a['uv'][sel]
    H, W, _ = a['tex'].shape
    smp = np.concatenate([uvs, uvs.mean(1, keepdims=True)], axis=1)
    u = np.clip((np.mod(smp[..., 0], 1) * W).astype(int), 0, W - 1)
    v = np.clip((np.mod(smp[..., 1], 1) * H).astype(int), 0, H - 1)
    cols = _lin(a['tex'][v, u]).mean(1)
    t2, c2 = convert_models.decimate(tris, cols, target)
    # wind each strap piece outward from its own centre (see remesh: the side turned away is painted first)
    comp = _components(t2)
    for k in np.unique(comp):
        sel = np.where(comp == k)[0]
        ctr = t2[sel].reshape(-1, 3).mean(0)
        n = np.cross(t2[sel, 1] - t2[sel, 0], t2[sel, 2] - t2[sel, 0])
        flip = sel[(n * (t2[sel].mean(1) - ctr)).sum(1) < 0]
        t2[flip] = t2[flip][:, [0, 2, 1]]
    return tris, t2, np.maximum(c2, .018)


def convert(src, write):
    """Writes Models/Shield/scene.glb through write(slot, tris, cols, extras); returns the report entry."""
    a = analyse(src)
    d = find_design(a)
    disc, dcols, kinds = remesh(a, d)
    room = BUDGET - len(disc)
    strap_src, st, sc = straps(a, room)
    tris = np.concatenate([disc, st])
    cols = np.concatenate([dcols, sc])
    # the mod frame, for the LEFT forearm: the star's top is the source's +y and the straps run along
    # it, so the forearm crosses the star's axis; worn on the left arm with the face out and the star
    # upright, the hand is at the wearer's right = the source's -x strap and the elbow at the +x one
    nond = ~a['disc']
    pts = a['tris'][nond].reshape(-1, 3)
    hand_side = pts[pts[:, 0] < a['centre'][0]]
    elbow_side = pts[pts[:, 0] > a['centre'][0]]
    zback = float(np.interp(abs(hand_side[:, 0].mean() - a['centre'][0]), *a['zb']))

    def strap_centre(q):
        return np.array([np.median(q[:, 0]), (q[:, 1].min() + q[:, 1].max()) / 2, (q[:, 2].min() + zback) / 2])
    origin = strap_centre(hand_side)
    elbow = strap_centre(elbow_side)
    # rows: mod x = -src y (so the star's top is the item's -right), mod y = -src x (elbow -> hand),
    # mod z = -src z (the face's normal is the item's forward); a proper rotation (det +1)
    rot = np.array([[0, -1, 0], [-1, 0, 0], [0, 0, -1]], np.float64)
    assert abs(np.linalg.det(rot) - 1) < 1e-9
    tip = d['tip0'] + 2 * np.pi / 5 * np.round((np.pi / 2 - d['tip0']) / (2 * np.pi / 5))
    assert abs(np.degrees(tip) - 90) < 15, 'the star\'s top tip is expected along the source\'s +y'
    k = DIAMETER_M / (2 * a['radius'])
    tf = (tris - origin) @ rot.T * k
    pivot = ((elbow - origin) @ rot.T * k).round(5).tolist()
    centre_m = ((np.array([a['centre'][0], a['centre'][1], np.interp(0, *a['zf'])]) - origin) @ rot.T * k).round(4).tolist()
    # extras.layer [disc triangle count, the back's centre (x y z), the face's normal (x y z)], model
    # space: the drawer (hero007_visual's shield) paints the straps (the triangles after the disc's)
    # BEFORE the disc when the eye is on the face's side of that plane, after it otherwise
    back_c = ((np.array([a['centre'][0], a['centre'][1], np.interp(0, *a['zb'])]) - origin) @ rot.T * k).round(5).tolist()
    layer = [int(len(disc))] + back_c + [0.0, 0.0, -1.0]
    n = write('Shield', tf, cols, {'pivot': pivot, 'layer': layer})
    return dict(source='captain_america_shield', source_triangles=int(len(a['tris'])), triangles=int(n),
                disc_triangles=int(len(disc)), strap_triangles=int(len(st)), strap_source_triangles=int(len(strap_src)),
                design=dict(blue_field_radius=round(d['blue'] / a['radius'], 4), ring_edges=[round(r / a['radius'], 4) for r in d['rings']],
                            star_outer=round(d['star_out'] / a['radius'], 4), star_inner=round(d['star_in'] / a['radius'], 4),
                            star_tip_angle_deg=round(float(np.degrees(d['tip0'])), 2), star_fit=round(d['star_fit'], 3),
                            star_offset=[round(v / a['radius'], 4) for v in d['star_offset']]),
                uv_fit_error=round(a['uv_error'], 5), diameter_m=DIAMETER_M, other_strap_pivot_m=pivot, disc_centre_m=centre_m, layer=layer,
                runtime_textures='none (baked: one colour a triangle, 16 texture samples each)',
                notes=['the disc remeshed from the source (its own heights, its texture\'s ring edges and star) so the '
                       'one-colour-a-triangle overlay keeps the rings and the star sharp; straps, buckles and pads decimated (quadric) '
                       '%d -> %d' % (len(strap_src), len(st)),
                       'mod frame (LEFT arm): the hand strap at the origin, y along the forearm (elbow to hand), -z the face, the star top -x; extras.pivot the other strap (the RIGHT arm turns 180 degrees about it)',
                       'paint wear kept from the texture (visual only)'])


if __name__ == '__main__':
    import json
    out = sys.argv[2]

    def _write(slot, tris, cols, extras=None):
        dd = os.path.join(out, *slot.split('/'))
        os.makedirs(dd, exist_ok=True)
        gltf_io.write_glb(os.path.join(dd, 'scene.glb'), tris, cols, extras)
        return len(tris)
    print(json.dumps(convert(sys.argv[1], _write), indent=1))
