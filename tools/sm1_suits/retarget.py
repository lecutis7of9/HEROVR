"""HERO VR SM1 suits: fit a pack's mesh (tools/sm1_suits/hsm1.py) onto the game's own suit model.

Pure Python (the launcher's embedded Python has no numpy). Three steps:
1. find the game model's frame from its own vertices (which way is up and which way it faces), so the
   rest works in the packs' canonical frame (Y up, floor at 0, facing +Z, its left at +X);
2. find the game body's 16 joints from its shape (the same rules as tools/suits/suit_skeleton.shape_joints)
   and move each of our vertices bone by bone from our skeleton onto the game's (bone lengths from the
   game, girth scaled by the height ratio, blended by our own skin weights);
3. give each of our vertices the game's own skin weights: those of the nearest game vertex that belongs to
   the same body part (so an arm next to the hip still follows the arm).
Everything here is CANDIDATE until the launcher's dump step has been run on the player's game files."""
import math

JOINTS = ['head', 'neck', 'chest', 'pelvis', 'shoulderL', 'elbowL', 'wristL', 'shoulderR', 'elbowR', 'wristR',
          'hipL', 'kneeL', 'footL', 'hipR', 'kneeR', 'footR']
HEAD_TOP, TIP_L, TIP_R = 16, 17, 18
BONES = ['head', 'neck', 'chest', 'spine', 'clavL', 'upperL', 'foreL', 'handL', 'clavR', 'upperR', 'foreR', 'handR',
         'thighL', 'shinL', 'thighR', 'shinR']
BONE_ENDS = [(0, HEAD_TOP), (1, 0), (2, 1), (3, 2), (1, 4), (4, 5), (5, 6), (6, TIP_L), (1, 7), (7, 8), (8, 9), (9, TIP_R),
             (10, 11), (11, 12), (13, 14), (14, 15)]


# --- small vector helpers (tuples) ---
def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def mul(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def norm(a):
    return math.sqrt(dot(a, a))


def unit(a, fallback=(0.0, 1.0, 0.0)):
    n = norm(a)
    return fallback if n < 1e-12 else (a[0] / n, a[1] / n, a[2] / n)


def median(xs):
    s = sorted(xs)
    n = len(s)
    if n == 0:
        return 0.0
    return s[n // 2] if n % 2 else 0.5 * (s[n // 2 - 1] + s[n // 2])


def mat_vec(m, v):
    return (dot(m[0], v), dot(m[1], v), dot(m[2], v))


def rot_between(u, v):
    """The smallest rotation taking unit u onto unit v, as a 3x3 matrix (rows)."""
    c = dot(u, v)
    ax = cross(u, v)
    s = norm(ax)
    if s < 1e-9:
        if c > 0:
            return ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))
        # opposite: turn half way round any axis perpendicular to u
        p = (1.0, 0.0, 0.0) if abs(u[0]) < 0.9 else (0.0, 1.0, 0.0)
        ax = unit(cross(u, p))
        x, y, z = ax
        return ((2 * x * x - 1, 2 * x * y, 2 * x * z), (2 * x * y, 2 * y * y - 1, 2 * y * z), (2 * x * z, 2 * y * z, 2 * z * z - 1))
    x, y, z = ax[0] / s, ax[1] / s, ax[2] / s
    t = 1 - c
    return ((t * x * x + c, t * x * y - s * z, t * x * z + s * y),
            (t * x * y + s * z, t * y * y + c, t * y * z - s * x),
            (t * x * z - s * y, t * y * z + s * x, t * z * z + c))


# --- 1. the game model's frame ---
def _clusters(vals, gap):
    """How many runs the sorted values make when split at gaps wider than gap (runs of 3+ points count)."""
    if not vals:
        return 0
    s = sorted(vals)
    runs, size = 0, 1
    for i in range(1, len(s)):
        if s[i] - s[i - 1] > gap:
            if size >= 3:
                runs += 1
            size = 1
        else:
            size += 1
    if size >= 3:
        runs += 1
    return runs


class Frame(object):
    """rows: the canonical X (its left), Y (up), Z (forward) as unit vectors in the model's own axes; floor:
    the model's height of the floor along up. to_canon / from_canon move points (and directions with
    point=False) between the two."""

    def __init__(self, rows, floor, notes):
        self.rows = rows
        self.floor = floor
        self.notes = notes

    def to_canon(self, p, point=True):
        c = mat_vec(self.rows, p)
        return (c[0], c[1] - self.floor, c[2]) if point else c

    def from_canon(self, c, point=True):
        if point:
            c = (c[0], c[1] + self.floor, c[2])
        r = self.rows
        return (r[0][0] * c[0] + r[1][0] * c[1] + r[2][0] * c[2],
                r[0][1] * c[0] + r[1][1] * c[1] + r[2][1] * c[2],
                r[0][2] * c[0] + r[1][2] * c[1] + r[2][2] * c[2])

    def describe(self):
        names = 'XYZ'

        def ax(v):
            i = max(range(3), key=lambda k: abs(v[k]))
            return ('+' if v[i] > 0 else '-') + names[i]
        return {'left': ax(self.rows[0]), 'up': ax(self.rows[1]), 'forward': ax(self.rows[2]),
                'floor': round(self.floor, 5), 'notes': self.notes}


def detect_frame(P):
    """P: the game body's vertices in its own axes -> Frame. Up: of the two longer axes, the one whose low end
    has two blobs (the feet) and whose high end has one (the head). Forward: the shorter axis, signed the way
    the toes point (the lowest slice sits further forward than the shins)."""
    notes = []
    lo = [min(p[k] for p in P) for k in range(3)]
    hi = [max(p[k] for p in P) for k in range(3)]
    ext = [hi[k] - lo[k] for k in range(3)]
    order = sorted(range(3), key=lambda k: ext[k])
    depth = order[0]
    best = None
    for up in order[1:]:
        lat = [k for k in order[1:] if k != up][0]
        H = ext[up]
        for sign in (1, -1):
            def h(p):
                return (p[up] - lo[up]) if sign > 0 else (hi[up] - p[up])
            bottom = [p[lat] for p in P if h(p) < 0.03 * H]
            top = [p[lat] for p in P if h(p) > 0.97 * H]
            nb, nt = _clusters(bottom, 0.03 * H), _clusters(top, 0.03 * H)
            score = (2 if nb == 2 else 0) + (1 if nt == 1 else 0) + (0.5 if up == 1 and sign > 0 else 0)
            if best is None or score > best[0]:
                best = (score, up, sign, lat, nb, nt)
    score, up, sign, lat, nb, nt = best
    notes.append('feet blobs %d, head blobs %d (want 2 and 1)' % (nb, nt))
    if score < 3:
        notes.append('LOW CONFIDENCE in which way is up')
    H = ext[up]

    def h(p):
        return (p[up] - lo[up]) if sign > 0 else (hi[up] - p[up])
    toes = [p[depth] for p in P if h(p) < 0.04 * H]
    shins = [p[depth] for p in P if 0.07 * H < h(p) < 0.14 * H]
    fsign = 1
    if toes and shins:
        d = sum(toes) / len(toes) - sum(shins) / len(shins)
        fsign = 1 if d >= 0 else -1
        if abs(d) < 0.005 * H:
            notes.append('LOW CONFIDENCE in which way it faces')
    else:
        notes.append('no feet found: facing assumed +')
    upv = [0.0, 0.0, 0.0]
    upv[up] = float(sign)
    fwd = [0.0, 0.0, 0.0]
    fwd[depth] = float(fsign)
    upv, fwd = tuple(upv), tuple(fwd)
    left = cross(upv, fwd)
    floor = lo[up] if sign > 0 else -hi[up]
    return Frame((left, upv, fwd), floor, notes)


# --- 2. the 16 joints from the body's shape (canonical frame) ---
def shape_joints(P):
    """The same rules as tools/suits/suit_skeleton.shape_joints, without numpy. P: canonical points."""
    H = max(p[1] for p in P)
    band = [p[0] for p in P if .55 * H < p[1] < .75 * H]
    cx = median(band) if band else 0.0

    def centre(y, xlo, xhi, hh=.02):
        s = [p for p in P if abs(p[1] - y) < hh * H and xlo < p[0] < xhi]
        if not s:
            x = (xlo + xhi) / 2 if math.isfinite(xlo + xhi) else cx
            return (x, y, 0.0)
        xs = [p[0] for p in s]
        zs = [p[2] for p in s]
        return ((min(xs) + max(xs)) / 2, y, (min(zs) + max(zs)) / 2)

    def slice_mid(pt, axd, half, radius):
        cs = []
        for p in P:
            d = sub(p, pt)
            al = dot(d, axd)
            if abs(al) < half and norm(sub(d, mul(axd, al))) < radius:
                cs.append(p)
        if len(cs) <= 5:
            return None
        return tuple((min(p[k] for p in cs) + max(p[k] for p in cs)) / 2 for k in range(3))

    J = {}
    J['head'] = centre(.885 * H, cx - .06 * H, cx + .06 * H)
    J['neck'] = centre(.845 * H, cx - .05 * H, cx + .05 * H)
    J['pelvis'] = centre(.53 * H, cx - .1 * H, cx + .1 * H)
    J['chest'] = add(J['pelvis'], mul(sub(J['neck'], J['pelvis']), .62))
    for s, sign in (('L', 1), ('R', -1)):
        side = [p for p in P if .3 * H < p[1] < .9 * H and (p[0] - cx) * sign > .16 * H]
        sh = (cx + sign * .1 * H, .815 * H, J['neck'][2] - .01 * H)
        if side:
            tip = max(side, key=lambda p: (p[0] - cx) * sign)
        else:
            tip = (cx + sign * .45 * H, .55 * H, sh[2])
        ax = unit(sub(tip, sh), (float(sign), 0.0, 0.0))
        reach = norm(sub(tip, sh))
        wrist = add(sh, mul(ax, reach - .105 * H))
        for _ in range(2):
            m = slice_mid(wrist, ax, .012 * H, .05 * H)
            if m:
                wrist = m
        elbow = add(sh, mul(sub(wrist, sh), .54))
        for _ in range(2):
            m = slice_mid(elbow, unit(sub(wrist, sh), ax), .012 * H, .06 * H)
            if m:
                elbow = m
        J['shoulder' + s], J['elbow' + s], J['wrist' + s] = sh, elbow, wrist
        lo, hi = (cx + .005 * H, float('inf')) if sign > 0 else (float('-inf'), cx - .005 * H)
        knee = centre(.285 * H, lo, hi)
        foot = centre(.05 * H, lo, hi, hh=.015)
        hx = cx + sign * .052 * H
        hx = (hx + centre(.44 * H, lo, hi)[0]) / 2
        J['hip' + s], J['knee' + s], J['foot' + s] = (hx, .5 * H, J['pelvis'][2]), knee, foot
    return [J[k] for k in JOINTS]


def ends_of(joints, P):
    """The 19 ends: the joints, the head's top and each hand's fingertips (the hand's farthest point)."""
    H = max(p[1] for p in P)
    E = list(joints) + [None, None, None]
    E[HEAD_TOP] = (joints[0][0], H, joints[0][2])
    for w, e, k in ((6, 5, TIP_L), (9, 8, TIP_R)):
        ax = unit(sub(joints[w], joints[e]), (1.0, 0.0, 0.0))
        reach = None
        for p in P:
            d = sub(p, joints[w])
            al = dot(d, ax)
            if 0 < al < .16 * H and norm(sub(d, mul(ax, al))) < .07 * H:
                reach = al if reach is None or al > reach else reach
        if reach is None:
            reach = .1 * H
        E[k] = add(joints[w], mul(ax, max(reach, .05 * H)))
    return E


# --- 3. moving our vertices onto the game skeleton ---
class BoneMap(object):
    """Per bone: our segment (a, u, length) -> the game's (A, rotation, length); girth scaled by g."""

    def __init__(self, ours, game):
        self.g = (game[HEAD_TOP][1] - 0.0) / max(1e-6, ours[HEAD_TOP][1])
        self.bones = []
        for i0, i1 in BONE_ENDS:
            a, b = ours[i0], ours[i1]
            A, B = game[i0], game[i1]
            d, D = sub(b, a), sub(B, A)
            L, LD = norm(d), norm(D)
            u = unit(d)
            U = unit(D, u)
            R = rot_between(u, U)
            s = LD / L if L > 1e-6 else self.g
            self.bones.append((a, u, s, A, R))

    def point(self, bi, p):
        a, u, s, A, R = self.bones[bi]
        d = sub(p, a)
        al = dot(d, u)
        q = add(mul(u, al * s), mul(sub(d, mul(u, al)), self.g))
        return add(A, mat_vec(R, q))

    def direction(self, bi, n):
        return mat_vec(self.bones[bi][4], n)

    def move(self, p, n, bones, weights):
        """bones/weights: up to 4 of our bones and their weights (any scale)."""
        tw = float(sum(weights))
        if tw <= 0:
            bones, weights, tw = [3], [1.0], 1.0
        P, N = (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)
        for b, w in zip(bones, weights):
            if w <= 0:
                continue
            f = w / tw
            P = add(P, mul(self.point(b, p), f))
            N = add(N, mul(self.direction(b, n), f))
        return P, unit(N, n)


def seg_dist2(p, a, b):
    ab = sub(b, a)
    L2 = dot(ab, ab)
    t = 0.0 if L2 < 1e-12 else max(0.0, min(1.0, dot(sub(p, a), ab) / L2))
    q = add(a, mul(ab, t))
    d = sub(p, q)
    return dot(d, d)


def bone_of(p, ends):
    """Which of the 16 bones a canonical point is nearest to."""
    best, bi = None, 3
    for i, (i0, i1) in enumerate(BONE_ENDS):
        d = seg_dist2(p, ends[i0], ends[i1])
        if best is None or d < best:
            best, bi = d, i
    return bi


class Grid(object):
    """Nearest-point lookup over a fixed point set (a uniform hash grid, searched ring by ring)."""

    def __init__(self, pts, cell):
        self.pts = pts
        self.cell = cell
        self.cells = {}
        for i, p in enumerate(pts):
            self.cells.setdefault(self._key(p), []).append(i)

    def _key(self, p):
        c = self.cell
        return (int(math.floor(p[0] / c)), int(math.floor(p[1] / c)), int(math.floor(p[2] / c)))

    def nearest(self, p, max_ring=64):
        if not self.pts:
            return -1
        kx, ky, kz = self._key(p)
        best, bi = None, -1
        for r in range(max_ring + 1):
            for dx in range(-r, r + 1):
                for dy in range(-r, r + 1):
                    for dz in range(-r, r + 1):
                        if max(abs(dx), abs(dy), abs(dz)) != r:
                            continue
                        for i in self.cells.get((kx + dx, ky + dy, kz + dz), ()):
                            d = sub(self.pts[i], p)
                            d2 = dot(d, d)
                            if best is None or d2 < best:
                                best, bi = d2, i
            # anything in a later ring is at least r * cell away
            if best is not None and best <= (r * self.cell) ** 2:
                break
        if bi < 0:  # very far away: brute force
            for i, q in enumerate(self.pts):
                d = sub(q, p)
                d2 = dot(d, d)
                if best is None or d2 < best:
                    best, bi = d2, i
        return bi


class Fitter(object):
    """Built once per game model: its frame, its joints and its vertices sorted by body part."""

    def __init__(self, game_points, game_skin, joint_side=None):
        """game_points: the game body's skinned vertices (its own axes); game_skin: each one's
        [(joint index, weight 0..256), ...] as decoded from its skin data; joint_side: joint index -> +1
        (a left-side joint by its name) or -1 (right), used to tell the body's left from its right (the
        shape alone cannot: a body and its mirror image look the same)."""
        self.frame = detect_frame(game_points)
        self.side_check = 'no left/right joint names: assumed right-handed axes'
        if joint_side:
            sums = {1: [0.0, 0], -1: [0.0, 0]}
            for p, sk in zip(game_points, game_skin):
                if not sk:
                    continue
                dom = max(sk, key=lambda t: t[1])[0]
                sd = joint_side.get(dom)
                if sd in sums:
                    sums[sd][0] += self.frame.to_canon(p)[0]
                    sums[sd][1] += 1
            if sums[1][1] >= 20 and sums[-1][1] >= 20:
                lx, rx = sums[1][0] / sums[1][1], sums[-1][0] / sums[-1][1]
                if lx < rx:
                    r = self.frame.rows
                    self.frame.rows = [mul(r[0], -1.0), r[1], r[2]]
                    self.frame.notes = list(self.frame.notes) + ['left/right flipped by joint names']
                self.side_check = 'left joints at canonical x %.3f, right at %.3f (%d / %d vertices)%s' % (
                    max(lx, rx) if lx < rx else lx, min(lx, rx) if lx < rx else rx, sums[1][1], sums[-1][1],
                    ' after flipping' if lx < rx else '')
        r = self.frame.rows
        self.mirrored = dot(r[0], cross(r[1], r[2])) < 0
        C = [self.frame.to_canon(p) for p in game_points]
        self.canon = C
        self.H = max(p[1] for p in C)
        self.joints = shape_joints(C)
        self.ends = ends_of(self.joints, C)
        self.skin = game_skin
        by_bone = {}
        for i, p in enumerate(C):
            by_bone.setdefault(bone_of(p, self.ends), []).append(i)
        self.parts = {}
        cell = max(1e-3, 0.02 * self.H)
        for b, idx in by_bone.items():
            self.parts[b] = (idx, Grid([C[i] for i in idx], cell))
        self.all = (list(range(len(C))), Grid(C, cell))

    def report(self):
        return {'frame': self.frame.describe(), 'height': round(self.H, 5), 'sides': self.side_check, 'mirrored': self.mirrored,
                'joints': {JOINTS[i]: [round(v, 4) for v in self.joints[i]] for i in range(16)},
                'parts': {BONES[b]: len(v[0]) for b, v in sorted(self.parts.items())}}

    def fit(self, ours_ends, pos, nrm, jnt, wgt):
        """Our canonical vertices -> (positions and normals in the game model's axes, skin per vertex)."""
        bm = BoneMap(ours_ends, self.ends)
        out_p, out_n, out_s = [], [], []
        for i in range(len(pos)):
            bones = jnt[i]
            weights = wgt[i]
            cp, cn = bm.move(pos[i], nrm[i], bones, weights)
            dom = bones[max(range(len(bones)), key=lambda k: weights[k])] if bones else 3
            idx, grid = self.parts.get(dom, self.all)
            k = grid.nearest(cp)
            gi = idx[k] if k >= 0 else 0
            out_p.append(self.frame.from_canon(cp))
            out_n.append(unit(self.frame.from_canon(cn, point=False), (0.0, 1.0, 0.0)))
            out_s.append(self.skin[gi])
        return out_p, out_n, out_s


_LEFT = __import__('re').compile(r'(^|[^a-z])(l|lf|lft|left)([^a-z]|$)|^left', __import__('re').I)
_RIGHT = __import__('re').compile(r'(^|[^a-z])(r|rt|rgt|right)([^a-z]|$)|^right', __import__('re').I)


def joint_sides(names):
    """joint index -> +1 / -1 for joint names that say left / right (e.g. 'l_hand', 'LeftArm', 'arm_r')."""
    out = {}
    for i, n in enumerate(names or []):
        if not n:
            continue
        lft, rgt = bool(_LEFT.search(n)), bool(_RIGHT.search(n))
        if lft != rgt:
            out[i] = 1 if lft else -1
    return out
