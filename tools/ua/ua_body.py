"""ULTIMATE ALLIANCE EXPANSION - the player's body in UA places (the pure half; build_body.py is Blender's).

The game's own hero body is part of the game's picture, which a UA place hides; so in a UA place the mod draws the
player's body itself (include/spidy/ua_body.hpp): the roster suit he wears when he wears one (Models/Suits), else
this one - MUA1's own Spider-Man (0901, the classic suit) from the player's disc, made into a HERO VR suit
(include/spidy/suit_wear.hpp's 16-bone skeleton, the format of tools/suits/convert_suit.py) at Models/UA/Body:
skinned to the headset and the hands, its fingers curled by the triggers, its legs stepping with the walk.

  make_suit(npz from build_body.py, out_dir) -> notes
"""
import os, sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'suits'))
import suit_skeleton as sk  # noqa: E402

HEIGHT = 1.8  # metres, as every suit (suit_wear scales the body to the player's own eye height)
FOLDER = 'UA/Body'
BODY_SKIN = '0901'  # MUA1 Spider-Man, classic suit (packages/generated/characters/spiderman_0901.fb)
BODY_PACKAGE = 'packages/generated/characters/spiderman_0901.fb'
BODY_ANIM = '09_spiderman.igb'  # its skeleton (the skin's bones' bind pose)


def to_suit_space(p):
    """MUA1 actor space (z up, facing +x, its left +y) -> the suit's (y up, facing +z, its left +x)."""
    p = np.asarray(p, np.float64)
    return np.stack([p[..., 1], p[..., 2], p[..., 0]], -1)


def hand_back(joints, ends, P, side):
    """The back of the hand at rest (unit): the hand's thinnest axis across it, signed out and up from the palm
    (suit_skeleton.hand_back's rule; the axis along the hand, flattened out first, is never taken)."""
    w, k = (6, sk.TIP_L) if side == 'L' else (9, sk.TIP_R)
    ax = ends[k] - joints[w]
    L = np.linalg.norm(ax)
    ax = ax / L
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
        for i in range(3):  # the smallest spread across the hand (the axis along it has none left)
            n = v[:, i] - ax * (v[:, i] @ ax)
            if np.linalg.norm(n) > .5:
                n /= np.linalg.norm(n)
                return n if n @ out > 0 else -n
    return out / np.linalg.norm(out)


def make_suit(npz, out_dir):
    from ua_suit_glb import write_glb  # the suit GLB writer (a copy of tools/suits/convert_suit.py's)
    d = np.load(npz)
    P = to_suit_space(d['corners'])
    N = to_suit_space(d['normals'])
    heads = to_suit_space(d['bone_heads'])
    lo, hi = P.min(0), P.max(0)
    scale = HEIGHT / (hi[1] - lo[1])
    names = [str(n) for n in d['bone_names']]
    parents = [int(x) for x in d['bone_parents']]
    bones = [{'name': n, 'parent': names[p] if p >= 0 else None, 'head': ((heads[i] - [0, lo[1], 0]) * scale).tolist()}
             for i, (n, p) in enumerate(zip(names, parents))]
    P = (P - [0, lo[1], 0]) * scale
    J = sk.rig_joints(bones)
    notes = []
    if J is None:
        J = sk.shape_joints(P)
        notes.append("joints from the body's shape (the rig's names not found)")
    else:
        notes.append("joints from MUA1's own Biped rig")
    shift = np.array([J[3][0], 0, J[3][2]])  # the pelvis over the origin
    P -= shift
    J = np.asarray(J) - shift
    T = len(P) // 3
    tris = P.reshape(T, 3, 3)
    nrm = N.reshape(T, 3, 3)
    # MUA1's weights (a PS2 skin: mostly one bone a vertex) onto the suit's 16 bones by the bones' roles
    roles = np.array(sk.rig_roles(bones))
    cb, cw = d['corner_bones'].reshape(T, 3, 4), d['corner_weights'].reshape(T, 3, 4)
    skin = np.zeros((T, 3, 16), np.float32)
    for k in range(4):
        jj = cb[..., k]
        ok = jj >= 0
        r = np.where(ok, roles[np.clip(jj, 0, None)], 0)
        np.add.at(skin, (np.nonzero(ok)[0], np.nonzero(ok)[1], r[ok]), cw[..., k][ok])
    none = skin.sum(2) < 1e-6
    skin[none, 3] = 1
    if none.any():
        notes.append('%d corners without weights put on the spine' % int(none.sum()))
    ends = sk.ends_of(J, P)
    hb = [hand_back(J, ends, P, 'L').round(4).tolist(), hand_back(J, ends, P, 'R').round(4).tolist()]
    os.makedirs(out_dir, exist_ok=True)
    images = sorted(k for k in d.files if k.startswith('img'))
    mats = []
    for k in images:
        path = os.path.join(out_dir, '_body_%s.png' % k)
        Image.fromarray(d[k]).convert('RGB').save(path)
        mats.append({'name': 'ua_body_' + k, 'factor': [1, 1, 1, 1], 'image': path})
    if not mats:
        mats = [{'name': 'ua_body', 'factor': [.7, .1, .1, 1], 'image': None}]
    mat = np.clip(d['image'], 0, len(mats) - 1).astype(np.int64)
    uv = d['uv'].reshape(T, 3, 2).astype(np.float64)
    extras = {'suit': {'joints': J.round(5).ravel().tolist(), 'ends': ends[16:].round(5).ravel().tolist(),
                       'handBack': hb[0] + hb[1], 'height': HEIGHT},
              'ua_body': {'source': "Marvel: Ultimate Alliance (PS2) " + BODY_PACKAGE + ", from the player's own disc"}}
    write_glb(os.path.join(out_dir, 'scene.glb'), tris, uv, nrm, mat, mats, skin, extras)
    for m in mats:
        if m['image']:
            os.remove(m['image'])
    notes.append('%d triangles, %d bones -> 16, %.2f m tall' % (T, len(names), HEIGHT))
    return notes, tris, ends, skin


if __name__ == '__main__':
    print('\n'.join(make_suit(sys.argv[1], sys.argv[2])[0]))
