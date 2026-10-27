"""Version 0.0.7 CAPTAIN AMERICA previews, no headset: the converted shield (Models/Shield)
placed the way the mod places it (the same frame as hero007_visual's appendShieldModel: the grip at the
item's `at`, x right, y up = the forearm elbow -> hand, -z forward = the face; the RIGHT arm turned 180
degrees about the face normal around extras.pivot) on a grey stand-in body (pose only, not the suit):
on the RIGHT forearm (SHIELD ARM's default) and the LEFT one, on the back holster, the backhand throw; plus the flight / chain diagram and the
suit's source model. Painted like the mod's shield drawer: the faces turned away first, then far to near.

  render_cap.py <Models folder> <suit source folder (read only)> <out folder>"""
import os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gltf_io
from render_models import read_glb, lin2srgb

SIZE = (760, 600)
LIGHT = np.array([.3, 1., .25]) / np.linalg.norm([.3, 1., .25])
SKIN = np.array([.20, .26, .40])  # the stand-in body: a muted navy, pose only


def nz(v):
    v = np.asarray(v, np.float64)
    return v / np.linalg.norm(v)


def frame(at, forward, up):
    f = nz(forward)
    u = np.asarray(up, np.float64) - f * np.dot(up, f)
    u = nz(u)
    return np.asarray(at, np.float64), f, u, np.cross(f, u)


class Placed(np.ndarray):
    """Placed shield triangles carrying extras.layer in world space: (disc count, the back's centre,
    the face's normal)."""
    layer = None

    def __array_finalize__(self, obj):
        self.layer = getattr(obj, 'layer', None)


def place_shield(tris, extras, at, forward, up, side='left', spin=0.0, flying=False):
    """Model -> world exactly as the mod does (see the module docstring)."""
    out = _place(tris, extras, at, forward, up, side, spin, flying, tris).view(Placed)
    if extras.get('layer'):
        L = extras['layer']
        c = np.array(L[1:4], float)
        pts = _place(np.array([[c, c + np.array(L[4:7], float) * .01, c]]), extras, at, forward, up, side, spin, flying, tris)[0]
        out.layer = (int(L[0]), pts[0], nz(pts[1] - pts[0]))
    return out


def _place(t, extras, at, forward, up, side, spin, flying, whole):
    t = np.array(t, np.float64)
    whole = np.array(whole, np.float64)
    if side == 'right':
        px, py, pz = extras['pivot']
        t[..., 0], t[..., 1] = px - t[..., 0], py - t[..., 1]
        whole[..., 0], whole[..., 1] = px - whole[..., 0], py - whole[..., 1]
    if flying:  # about the disc's centre (the bounding box's middle), spun about the face normal
        lo, hi = whole.reshape(-1, 3).min(0), whole.reshape(-1, 3).max(0)
        t = t - (lo + hi) / 2
    if spin:
        c, s = np.cos(spin), np.sin(spin)
        x, y = t[..., 0].copy(), t[..., 1].copy()
        t[..., 0], t[..., 1] = c * x - s * y, s * x + c * y
    o, f, u, r = frame(at, forward, up)
    return o + t[..., :1] * r + t[..., 1:2] * u - t[..., 2:3] * f


def prism(a, b, ra, rb=None, n=8, col=SKIN):
    """A closed n-sided tube from a to b, wound outward."""
    rb = ra if rb is None else rb
    a, b = np.asarray(a, float), np.asarray(b, float)
    ax = nz(b - a)
    p = nz(np.cross(ax, [0, 0, 1] if abs(ax[2]) < .9 else [1, 0, 0]))
    q = np.cross(ax, p)
    ang = np.linspace(0, 2 * np.pi, n, endpoint=False)
    ring = lambda c, r: [c + r * (np.cos(t) * p + np.sin(t) * q) for t in ang]
    A, B = ring(a, ra), ring(b, rb)
    tris = []
    for i in range(n):
        j = (i + 1) % n
        tris += [[A[i], A[j], B[j]], [A[i], B[j], B[i]], [a, A[j], A[i]], [b, B[i], B[j]]]
    tris = np.array(tris)
    ctr = (a + b) / 2
    nrm = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    flip = (nrm * (tris.mean(1) - ctr)).sum(1) < 0
    tris[flip] = tris[flip][:, [0, 2, 1]]
    return tris, np.tile(col, (len(tris), 1))


def ball(c, r, col=SKIN, n=10):
    tris = []
    th = np.linspace(0, np.pi, n // 2 + 1)
    ph = np.linspace(0, 2 * np.pi, n, endpoint=False)
    P = lambda i, j: np.asarray(c) + r * np.array([np.sin(th[i]) * np.cos(ph[j % n]), np.cos(th[i]), np.sin(th[i]) * np.sin(ph[j % n])])
    for i in range(len(th) - 1):
        for j in range(n):
            tris += [[P(i, j), P(i + 1, j), P(i + 1, j + 1)], [P(i, j), P(i + 1, j + 1), P(i, j + 1)]]
    tris = np.array(tris)
    nrm = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    flip = (nrm * (tris.mean(1) - np.asarray(c))).sum(1) < 0
    tris[flip] = tris[flip][:, [0, 2, 1]]
    return tris, np.tile(col, (len(tris), 1))


def body(j, head=True):
    """A stand-in body from joints (facing -z, right = +x)."""
    parts = [prism(j['hips'], j['chest'], .15, .19, 10)] + ([ball(j['head'], .11), prism(j['chest'], j['head'] - [0, .1, 0], .06)] if head else []) + [
             prism(j['hips'] + [-.1, 0, 0], j['lknee'], .08, .065), prism(j['lknee'], j['lfoot'], .065, .05),
             prism(j['hips'] + [.1, 0, 0], j['rknee'], .08, .065), prism(j['rknee'], j['rfoot'], .065, .05)]
    for s in 'lr':
        parts += [prism(j[s + 'shoulder'], j[s + 'elbow'], .055, .045), prism(j[s + 'elbow'], j[s + 'hand'], .04, .028),
                  ball(j[s + 'hand'], .03, SKIN * .8, 8), ball(j[s + 'shoulder'], .065, SKIN, 8)]
    return parts


def joints(**over):
    j = dict(hips=np.array([0, 1.0, 0.]), chest=np.array([0, 1.45, 0.]), head=np.array([0, 1.73, 0.]),
             lshoulder=np.array([-.21, 1.47, 0]), rshoulder=np.array([.21, 1.47, 0]),
             lelbow=np.array([-.26, 1.18, .02]), relbow=np.array([.26, 1.18, .02]),
             lhand=np.array([-.27, .92, -.02]), rhand=np.array([.27, .92, -.02]),
             lknee=np.array([-.11, .52, -.03]), rknee=np.array([.11, .52, -.03]),
             lfoot=np.array([-.12, .06, 0]), rfoot=np.array([.12, .06, 0]))
    for k, v in over.items():
        j[k] = np.asarray(v, float)
    return j


def shift(parts, d):
    out = []
    for t, c in parts:
        m = t + np.asarray(d, float)
        if getattr(t, 'layer', None):
            m.layer = (t.layer[0], t.layer[1] + np.asarray(d, float), t.layer[2])
        out.append((m, c))
    return out


def render(parts, eye, target, title='', sub='', size=SIZE, fov=40, notes=()):
    eye, target = np.asarray(eye, float), np.asarray(target, float)
    # the shield's layers (the mod's drawer paints them strictly; one painter for the whole scene
    # here, so the layer behind the eye's side of the shield is pushed back in the sort instead)
    push = []
    for t, _ in parts:
        b = np.zeros(len(t))
        L = getattr(t, 'layer', None)
        if L:
            front = np.dot(eye - L[1], L[2]) > 0
            b[L[0]:] = .5 if front else 0
            b[:L[0]] = -.3 if front else .5  # the face over the forearm in its straps
        push.append(b)
    push = np.concatenate(push)
    tris = np.asarray(np.concatenate([np.asarray(t) for t, _ in parts]))
    cols = np.concatenate([c for _, c in parts])
    f = nz(target - eye)
    r = nz(np.cross(f, [0, 1, 0]))
    u = np.cross(r, f)
    rel = tris - eye
    x, y, z = rel @ r, rel @ u, rel @ f
    keep = (z > .05).all(1)
    tris, cols, x, y, z, rel, push = tris[keep], cols[keep], x[keep], y[keep], z[keep], rel[keep], push[keep]
    foc = size[1] / 2 / np.tan(np.radians(fov) / 2)
    sx, sy = size[0] / 2 + foc * x / z, size[1] / 2 - foc * y / z
    n = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    nl = np.linalg.norm(n, axis=1)
    ok = nl > 1e-12
    n[ok] /= nl[ok, None]
    away = (n * (tris.mean(1) - eye)).sum(1) > 0  # facing away from the eye
    dist = np.linalg.norm(tris.mean(1) - eye, axis=1) + push
    order = np.lexsort((-dist, ~away))  # the turned-away faces first, then far to near
    shade = .32 + .68 * np.abs(n @ LIGHT)
    rgb = (lin2srgb(cols * shade[:, None]) * 255).astype(int)
    im = Image.new('RGB', size, (36, 36, 44))
    d = ImageDraw.Draw(im)
    # a floor grid for depth
    for gx in np.arange(-3, 3.01, .5):
        for a0, b0 in (((gx, 0, -3), (gx, 0, 3)), ((-3, 0, gx), (3, 0, gx))):
            pa, pb = np.asarray(a0) - eye, np.asarray(b0) - eye
            za, zb = pa @ f, pb @ f
            if za > .1 and zb > .1:
                d.line([(size[0] / 2 + foc * (pa @ r) / za, size[1] / 2 - foc * (pa @ u) / za),
                        (size[0] / 2 + foc * (pb @ r) / zb, size[1] / 2 - foc * (pb @ u) / zb)], fill=(52, 52, 62))
    for i in order:
        if ok[i]:
            d.polygon(list(zip(sx[i], sy[i])), fill=tuple(rgb[i]))
    font = ImageFont.load_default()
    d.text((14, 10), title, fill=(150, 215, 255), font=font)
    for k, line in enumerate([sub] + list(notes)):
        d.text((14, 28 + 16 * k), line, fill=(235, 235, 235), font=font)
    proj = lambda p: (size[0] / 2 + foc * ((np.asarray(p) - eye) @ r) / ((np.asarray(p) - eye) @ f),
                      size[1] / 2 - foc * ((np.asarray(p) - eye) @ u) / ((np.asarray(p) - eye) @ f))
    return im, d, proj


def sheet(images, cols=2):
    rows = (len(images) + cols - 1) // cols
    out = Image.new('RGB', (SIZE[0] * cols, SIZE[1] * rows), (20, 20, 24))
    for k, im in enumerate(images):
        out.paste(im, ((k % cols) * SIZE[0], (k // cols) * SIZE[1]))
    return out


def worn_left(j, tris, ex, side='left'):
    """The guard: the shield's grip in the hand, up = the forearm (elbow -> hand), the face out
    (the forearm's perpendicular nearest the body's forward)."""
    s = 'l' if side == 'left' else 'r'
    up = nz(j[s + 'hand'] - j[s + 'elbow'])
    fwd = np.array([0, 0, -1.])
    return place_shield(tris, ex, j[s + 'hand'], fwd - up * np.dot(fwd, up), up, side)


GUARD = dict(lelbow=[-.33, 1.20, -.22], lhand=[.05, 1.25, -.31])            # the left forearm across the chest
GUARD_R = dict(relbow=[.33, 1.20, -.22], rhand=[-.05, 1.25, -.31])


def back_holster(j, tris, ex, side='left'):
    """Between the shoulder blades, the face out (backward), the star upright: the item's up is the
    body's left so the star's top (the model's -x = the item's -right) points up; the grip strap ends
    up on the shield arm's side, where that hand reaches over the shoulder."""
    up = np.array([-1., 0, 0]) if side == 'left' else np.array([1., 0, 0])
    centre = np.array([0, 1.28, .17])
    # the disc's centre in the grip frame (for the RIGHT arm, the turned model's): the grip sits
    # -cy along up from the centre
    t = place_shield(tris, ex, [0, 0, 0], [0, 0, -1.], [0, 1., 0], side)
    lo, hi = t.reshape(-1, 3).min(0), t.reshape(-1, 3).max(0)
    cy = (lo[1] + hi[1]) / 2
    return place_shield(tris, ex, centre - up * cy, [0, 0, 1.], up, side)


def mirror_joints(j):
    """The same pose on the other side: x negated, left and right swapped."""
    sw = {'l': 'r', 'r': 'l'}
    return {(sw[k[0]] + k[1:] if k[0] in sw else k): v * [-1, 1, 1] for k, v in j.items()}


def throw_phase(k, tris, ex, side='right'):
    """The backhand (frisbee) throw (posed for the LEFT arm, mirrored for the RIGHT): the coil across the
    body (the shield flat, the face up, by the other side's ribs), the uncoil (the arm sweeping flat), the
    release (the arm out to its own side-forward, the wrist snapped: the shield leaves spinning flat)."""
    if k == 0:
        j = joints(lelbow=[-.02, 1.22, -.22], lhand=[.26, 1.12, -.08], chest=[.04, 1.45, .02], head=[.06, 1.72, .02],
                   rhand=[.33, 1.02, .1], relbow=[.3, 1.2, .1])
    elif k == 1:
        j = joints(lelbow=[-.25, 1.30, -.28], lhand=[-.05, 1.28, -.55], rhand=[.32, 1.0, .05])
    else:
        j = joints(lelbow=[-.42, 1.34, -.25], lhand=[-.70, 1.33, -.48], chest=[-.04, 1.45, -.02], head=[-.06, 1.72, -.03],
                   rhand=[.35, 1.05, .12], relbow=[.32, 1.22, .08])
    if side == 'right':
        j = mirror_joints(j)
    s = side[0]
    up = nz(j[s + 'hand'] - j[s + 'elbow'])
    sh = place_shield(tris, ex, j[s + 'hand'], [0, 1., 0], up, side)
    return j, sh


def main(models, suit_src, out):
    os.makedirs(out, exist_ok=True)
    tris, cols, ex = read_glb(os.path.join(models, 'Shield', 'scene.glb'))
    font = ImageFont.load_default()
    # 1. the shield as converted
    shots = []
    for title, eye in [('SHIELD front (the face)', [0, 0, -1.2]), ('SHIELD back (straps)', [0, 0, 1.2]),
                       ('SHIELD three-quarter', [.8, .45, -.9]), ('SHIELD edge-on (the dome)', [1.2, .05, -.02])]:
        sh = place_shield(tris, ex, [0, 0, 0], [0, 0, -1.], [0, 1., 0])
        lo, hi = sh.reshape(-1, 3).min(0), sh.reshape(-1, 3).max(0)
        im, _, _ = render([(sh, cols)], np.asarray(eye) + (lo + hi) / 2, (lo + hi) / 2, title,
                          '%d triangles (the overlay budget is 700), 0.61 m across, colours baked from the texture' % len(tris),
                          fov=34)
        shots.append(im)
    sheet(shots).save(os.path.join(out, 'cap_shield_model.png'))
    # 2. the forearm guard (third person, first person, the inside) on the RIGHT arm (SHIELD ARM's
    #    default since 26 Oct) and on the LEFT one (the setting), one sheet each
    arms = {'right': joints(**GUARD_R), 'left': joints(**GUARD)}
    for arm, other in (('right', 'left'), ('left', 'right')):
        ja, jo = arms[arm], arms[other]
        sx = 1 if arm == 'right' else -1  # the camera on the shield arm's side
        sh = worn_left(ja, tris, ex, arm)
        sho = worn_left(jo, tris, ex, other)
        a, d, p = render(body(ja) + [(sh, cols)], [-1.5 * sx, 1.7, -2.2], [0, 1.2, 0],
                         'SHIELD ON THE %s FOREARM (%s)' % (arm.upper(), 'default: shield arm RIGHT' if arm == 'right' else 'setting: shield arm LEFT'),
                         'the hand in the grip strap, the forearm through the other; the face out, the star upright',
                         notes=['stand-in grey body: the pose only, not the suit'] +
                               (['the RIGHT arm takes the model turned 180 degrees about the face: the star stays upright'] if arm == 'right' else []))
        b, _, _ = render(body(ja, head=False)[1:] + [(sh, cols)], [0.0, 1.72, -.05], [0.0, 1.2, -.9], 'FIRST PERSON (what the headset shows)',
                         'the %s controller is the grip: raise the arm and the shield guards the face' % arm, fov=80)
        c, _, _ = render(body(jo) + [(sho, cols)], [1.5 * sx, 1.7, -2.2], [0, 1.2, 0],
                         'shield arm %s (%s)' % (other.upper(), 'the default' if other == 'right' else 'setting, live'),
                         'toggle SHIELD ARM: it moves to the other forearm and only that hand draws it')
        e, _, _ = render(body(ja) + [(sh, cols)], [1.2 * sx, 1.4, .4], [0, 1.25, -.3], '%s forearm, the inside' % arm.upper(),
                         'the straps on the forearm (the source model\'s leather straps and buckles, decimated)')
        sheet([a, b, c, e]).save(os.path.join(out, 'cap_shield_%s_forearm.png' % arm))
    # 3. the back holster (SHIELD ARM RIGHT, the default: the grip strap on the right, drawn over the
    #    right shoulder)
    j = joints()
    sh = back_holster(j, tris, ex, 'right')
    a, d, p = render(body(j) + [(sh, cols)], [-1.0, 1.8, 2.3], [0, 1.2, 0], 'SHIELD ON THE BACK HOLSTER (shield arm RIGHT)',
                     'between the shoulder blades, the face out, the star upright', notes=[
                         'to draw: reach the shield arm\'s hand (RIGHT by default) over the shoulder, grip, pull (haptic ticks)',
                         'to stow: put it back there and let go of the grip (the shared holster: no web-zip / climb clash)',
                         'a right hand on a web or a wall never draws it; the left hand never takes it'])
    hand = j['rshoulder'] + [-0.02, .12, .12]
    d.ellipse([p(hand)[0] - 9, p(hand)[1] - 9, p(hand)[0] + 9, p(hand)[1] + 9], outline=(255, 210, 60), width=3)
    d.text((p(hand)[0] + 12, p(hand)[1] - 6), 'grab zone (right hand)', fill=(255, 210, 60), font=font)
    b, _, _ = render(body(j) + [(sh, cols)], [-2.4, 1.5, .3], [0, 1.2, 0], 'BACK HOLSTER, side',
                     'it sits flat on the back, the dome out')
    jd = mirror_joints(joints(lelbow=[-.25, 1.62, .05], lhand=[-.12, 1.55, .2]))
    c, _, _ = render(body(jd) + [(back_holster(jd, tris, ex, 'right'), cols)], [-1.4, 1.9, 2.0], [0, 1.3, 0], 'THE DRAW',
                     'the right hand over the right shoulder into the grip strap, then pull forward')
    sheet([a, b, c]).save(os.path.join(out, 'cap_shield_back_holster.png'))
    # 4. the throw (RIGHT arm, the default: mirrored, the phases run left to right)
    parts = []
    labels = ['1 COIL: across the body, flat, face up', '2 UNCOIL: the arm sweeps flat', '3 RELEASE: the wrist snaps, flat spin']
    for k in range(3):
        jj, sh = throw_phase(k, tris, ex, 'right')
        parts += shift(body(jj) + [(sh, cols)], [(k - 1) * 1.6, 0, 0])
    jj, _ = throw_phase(2, tris, ex, 'right')
    rel = jj['rhand'] + [1.6, 0, 0] + nz([1, 0, -.9]) * .55
    fl = place_shield(tris, ex, rel, [0, 1., 0], nz([1, 0, -.9]), 'right', spin=-1.1, flying=True)
    parts += [(fl, cols)]
    a, d, p = render(parts, [-0.3, 3.2, -4.6], [0, 1.0, 0], 'THE BACKHAND (FRISBEE) THROW, RIGHT ARM (the default)',
                     'how he throws it: coil across the chest, uncoil with a flat arm, snap the wrist for spin',
                     size=(SIZE[0] * 2, SIZE[1]), fov=46,
                     notes=['in the mod: pull the trigger while the arm sweeps; fast sweeps send it along the sweep, slow ones use the aim pick',
                            'SHIELD ARM LEFT: the same throw, mirrored'])
    for k in range(3):
        q = p(np.array([(k - 1) * 1.6, 0.0, -.2]))
        d.text((q[0] - 110, q[1] + 8), labels[k], fill=(255, 210, 60), font=font)
    q = p(rel)
    d.text((q[0] - 150, q[1] + 34), 'spins flat (clockwise from above for a right-hand backhand)', fill=(150, 215, 255), font=font)
    a.save(os.path.join(out, 'cap_throw_pose.png'))
    # 5. the suit's source model
    g, parts_s = gltf_io.load(suit_src)
    st = np.concatenate([q['pos'] for q in parts_s]).astype(np.float64)
    mats = [g['materials'][q['material']] for q in parts_s]

    texc = {}

    def colours(q, m):  # the base-colour texture at the corners and the centre (a preview only)
        ti = m.get('pbrMetallicRoughness', {}).get('baseColorTexture', {}).get('index')
        if ti is None:
            return np.tile([.5, .5, .5], (len(q['pos']), 1))
        if ti not in texc:
            texc[ti] = np.asarray(Image.open(os.path.join(suit_src, g['images'][g['textures'][ti]['source']]['uri'])).convert('RGB'),
                                  np.float64) / 255
        tx = texc[ti]
        uv = np.concatenate([q['uv'], q['uv'].mean(1, keepdims=True)], 1)
        u = np.clip((np.mod(uv[..., 0], 1) * tx.shape[1]).astype(int), 0, tx.shape[1] - 1)
        v = np.clip((np.mod(uv[..., 1], 1) * tx.shape[0]).astype(int), 0, tx.shape[0] - 1)
        c = tx[v, u]
        return np.where(c <= .04045, c / 12.92, ((c + .055) / 1.055) ** 2.4).mean(1)
    sc = np.concatenate([colours(q, m) for q, m in zip(parts_s, mats)])
    # the mesh data is Y-up facing +z (glTF's convention; the file's root node turns it for Z-up
    # tools): turn it 180 degrees about y to face the previews' -z
    st = st * np.array([-1, 1, -1])
    pts = st.reshape(-1, 3)
    lo, hi = pts.min(0), pts.max(0)
    k = 1.88 / (hi[1] - lo[1])
    st = (st - [(lo[0] + hi[0]) / 2, lo[1], (lo[2] + hi[2]) / 2]) * k
    shots = []
    for title, eye in [('front', [0, 1.0, -3.6]), ('back', [0, 1.0, 3.6]), ('side', [3.6, 1.0, 0]), ('three-quarter', [2.4, 1.6, -2.6])]:
        for sgn in (1,):
            im, _, _ = render([(st, sc)], eye, [0, .95, 0], 'SUIT SOURCE MODEL (%s), as downloaded' % title,
                              '%d triangles, %d meshes, %d materials, no skeleton / skin; shown at 1.88 m' % (len(st), len(parts_s), len(g['materials'])),
                              notes=['one texture colour a triangle (preview); the game swap is done on the PC (docs/CAP-SUIT-PIPELINE.md)'])
            shots.append(im)
    sheet(shots).save(os.path.join(out, 'cap_suit_source.png'))


if __name__ == '__main__':
    main(*sys.argv[1:4])
