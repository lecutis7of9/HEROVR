"""Build 9 SUITS: the 16-joint / 16-bone suit skeleton (the same as include/spidy/suit_wear.hpp), and
where its joints are on a downloaded suit: from the artist's rig by bone name (3ds Max Biped, Mixamo)
or, unrigged, from the body's own shape (a standing A or T pose, facing +Z, its left at +X)."""
import re
import numpy as np

JOINTS = ['head', 'neck', 'chest', 'pelvis', 'shoulderL', 'elbowL', 'wristL', 'shoulderR', 'elbowR', 'wristR',
          'hipL', 'kneeL', 'footL', 'hipR', 'kneeR', 'footR']  # native_body::SuitJoint's order
# ends: the 16 joints then the head's top and each hand's fingertips
HEAD_TOP, TIP_L, TIP_R = 16, 17, 18
BONES = ['head', 'neck', 'chest', 'spine', 'clavL', 'upperL', 'foreL', 'handL', 'clavR', 'upperR', 'foreR', 'handR',
         'thighL', 'shinL', 'thighR', 'shinR']
BONE_ENDS = np.array([[0, HEAD_TOP], [1, 0], [2, 1], [3, 2], [1, 4], [4, 5], [5, 6], [6, TIP_L], [1, 7], [7, 8], [8, 9], [9, TIP_R],
                      [10, 11], [11, 12], [13, 14], [14, 15]])
HEAD_BONE = 0


def core(name):
    n = re.sub(r'[\s-]+', '_', name.lower().split(':')[-1])
    n = re.sub(r'(_\d+)+$', '', n)
    for p in ('bip01_', 'bip001_', 'bip01 ', 'bip001 '):
        if n.startswith(p):
            n = n[len(p):]
    return n.replace(' ', '_')


def bone_role(name):
    """-> one of BONES' indices, or None (decided by the parent)."""
    n = core(name)
    for side, s in (('l', 'L'), ('r', 'R')):
        full = 'left' if side == 'l' else 'right'
        b = lambda k: BONES.index(k + s)
        if n.startswith(side + '_clavicle') or n == full + 'shoulder':
            return b('clav')
        if n.startswith(side + '_upperarm') or n == full + 'arm':
            return b('upper')
        if n.startswith(side + '_fore') or n.startswith(full + 'forearm'):
            return b('fore')
        if n.startswith(side + '_hand') or n.startswith(side + '_finger') or n.startswith(full + 'hand'):
            return b('hand')
        if n.startswith(side + '_thigh') or n.startswith(full + 'upleg'):
            return b('thigh')
        if n.startswith(side + '_calf') or n.startswith(side + '_foot') or n.startswith(side + '_toe') or \
                n.startswith(full + 'leg') or n.startswith(full + 'foot') or n.startswith(full + 'toe'):
            return b('shin')
    if n.startswith('head'):
        return 0
    if n.startswith('neck'):
        return 1
    if n in ('spine2', 'spine3', 'spine4'):
        return 2
    if n in ('spine', 'spine1', 'pelvis', 'hips'):
        return 3
    return None


def rig_roles(bones):
    """bones: [{name, parent}] -> each bone's role (its own, or its nearest ancestor's; spine by default)."""
    by = {b['name']: b for b in bones}
    out = []
    for b in bones:
        r, x, k = None, b, 0
        while x is not None and k < 200:
            r = bone_role(x['name'])
            if r is not None:
                break
            x = by.get(x['parent']) if x['parent'] else None
            k += 1
        out.append(3 if r is None else r)
    return out


def rig_joints(bones):
    """Joint positions from a Biped / Mixamo rig (None when a needed bone is not there)."""
    want = {'head': ['head'], 'neck': ['neck'], 'pelvis': ['pelvis', 'hips'],
            'shoulderL': ['l_upperarm', 'leftarm'], 'elbowL': ['l_forearm', 'leftforearm'], 'wristL': ['l_hand', 'lefthand'],
            'shoulderR': ['r_upperarm', 'rightarm'], 'elbowR': ['r_forearm', 'rightforearm'], 'wristR': ['r_hand', 'righthand'],
            'hipL': ['l_thigh', 'leftupleg'], 'kneeL': ['l_calf', 'leftleg'], 'footL': ['l_foot', 'leftfoot'],
            'hipR': ['r_thigh', 'rightupleg'], 'kneeR': ['r_calf', 'rightleg'], 'footR': ['r_foot', 'rightfoot']}
    first = {}
    for b in bones:
        first.setdefault(core(b['name']), np.array(b['head'], np.float64))
    J = {}
    for k, names in want.items():
        for n in names:
            if n in first:
                J[k] = first[n]
                break
        else:
            return None
    J['chest'] = J['pelvis'] + .62 * (J['neck'] - J['pelvis'])
    return np.array([J[k] for k in JOINTS])


def _slice(P, axis_pt, axis_dir, half, radius):
    d = P - axis_pt
    along = d @ axis_dir
    perp = np.linalg.norm(d - np.outer(along, axis_dir), axis=1)
    return P[(np.abs(along) < half) & (perp < radius)]


def shape_joints(P):
    """Joints from the shape of a standing body (Y up, floor at 0, facing +Z, left at +X)."""
    H = P[:, 1].max()
    band = P[(P[:, 1] > .55 * H) & (P[:, 1] < .75 * H)]
    cx = np.median(band[:, 0])

    def centre(y, xlo, xhi, h=.02):
        s = P[(np.abs(P[:, 1] - y) < h * H) & (P[:, 0] > xlo) & (P[:, 0] < xhi)]
        if len(s) == 0:
            return np.array([(xlo + xhi) / 2 if np.isfinite(xlo + xhi) else cx, y, 0.])
        return np.array([(s[:, 0].min() + s[:, 0].max()) / 2, y, (s[:, 2].min() + s[:, 2].max()) / 2])

    J = {}
    J['head'] = centre(.885 * H, cx - .06 * H, cx + .06 * H)
    J['neck'] = centre(.845 * H, cx - .05 * H, cx + .05 * H)
    J['pelvis'] = centre(.53 * H, cx - .1 * H, cx + .1 * H)
    J['chest'] = J['pelvis'] + .62 * (J['neck'] - J['pelvis'])
    for s, sign in (('L', 1), ('R', -1)):
        side = P[(P[:, 1] > .3 * H) & (P[:, 1] < .9 * H) & ((P[:, 0] - cx) * sign > .16 * H)]
        tip = side[np.argmax((side[:, 0] - cx) * sign)]
        sh = np.array([cx + sign * .1 * H, .815 * H, J['neck'][2] - .01 * H])
        ax = (tip - sh) / np.linalg.norm(tip - sh)
        reach = np.linalg.norm(tip - sh)
        wrist = sh + ax * (reach - .105 * H)
        # the wrist and elbow on the limb's own centre line (each cross-section's middle)
        for _ in range(2):
            cs = _slice(P, wrist, ax, .012 * H, .05 * H)
            if len(cs) > 5:
                wrist = (cs.min(0) + cs.max(0)) / 2
        elbow = sh + (wrist - sh) * .54
        for _ in range(2):
            cs = _slice(P, elbow, (wrist - sh) / np.linalg.norm(wrist - sh), .012 * H, .06 * H)
            if len(cs) > 5:
                elbow = (cs.min(0) + cs.max(0)) / 2
        J['shoulder' + s], J['elbow' + s], J['wrist' + s] = sh, elbow, wrist
        lo, hi = (cx + .005 * H, np.inf) if sign > 0 else (-np.inf, cx - .005 * H)
        knee = centre(.285 * H, lo, hi)
        foot = centre(.05 * H, lo, hi, h=.015)
        hip = np.array([cx + sign * .052 * H, .5 * H, J['pelvis'][2]])
        hip[0] = (hip[0] + centre(.44 * H, lo, hi)[0]) / 2
        J['hip' + s], J['knee' + s], J['foot' + s] = hip, knee, foot
    return np.array([J[k] for k in JOINTS])


def ends_of(joints, P):
    """The 19 ends: the joints, the head's top and each hand's fingertips (the hand's farthest point)."""
    E = np.zeros((19, 3))
    E[:16] = joints
    H = P[:, 1].max()
    E[HEAD_TOP] = [joints[0][0], H, joints[0][2]]
    for w, e, k in ((6, 5, TIP_L), (9, 8, TIP_R)):
        ax = joints[w] - joints[e]
        ax /= np.linalg.norm(ax)
        d = P - joints[w]
        along = d @ ax
        perp = np.linalg.norm(d - np.outer(along, ax), axis=1)
        hand = (along > 0) & (along < .16 * H) & (perp < .07 * H)
        reach = along[hand].max() if hand.any() else .1 * H
        E[k] = joints[w] + ax * max(reach, .05 * H)
    return E


def hand_back(joints, ends, P, side):
    """The back of the hand at rest (unit): the hand's thinnest axis, signed out and up from the palm."""
    w, k = (6, TIP_L) if side == 'L' else (9, TIP_R)
    ax = ends[k] - joints[w]
    L = np.linalg.norm(ax)
    ax /= L
    d = P - joints[w]
    along = d @ ax
    perp = np.linalg.norm(d - np.outer(along, ax), axis=1)
    pts = P[(along > .15 * L) & (along < .9 * L) & (perp < .6 * L)]
    out = np.array([1. if side == 'L' else -1., 1., 0.])
    out -= ax * (out @ ax)
    if len(pts) > 20:
        c = pts - pts.mean(0)
        c -= np.outer(c @ ax, ax)
        w_, v = np.linalg.eigh(c.T @ c)
        n = v[:, 0]
        n -= ax * (n @ ax)
        n /= np.linalg.norm(n)
        return n if n @ out > 0 else -n
    return out / np.linalg.norm(out)
