"""Build box only (numpy + Pillow): turn each hero's SM2 suit (Models/Suits/<suit>/scene.glb) into the
SM1 suit pack the launcher installs (Models/SM1Suits/<id>.hsm1, see hsm1.py).

    python3 tools/sm1_suits/prepack.py <Models/Suits dir> <out dir>

Per pack: the suit's primitives merged by texture (SM1's suit model has a fixed number of meshes), its
vertices in the canonical frame, its 16-bone weights, its 19 ends, and each texture as a BC1 mip chain
(at most 1024 px, enough for a body seen in VR, and the game's own suits use block compression too)."""
import io
import json
import os
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hsm1  # noqa: E402
from heroes import SM1_SUITS  # noqa: E402

MAX_TEX = 1024


def read_glb(path):
    b = open(path, 'rb').read()
    jl = struct.unpack('<I', b[12:16])[0]
    g = json.loads(b[20:20 + jl])
    binb = b[20 + jl + 8:]

    def acc(i):
        a = g['accessors'][i]
        v = g['bufferViews'][a['bufferView']]
        dt = {5126: np.float32, 5125: np.uint32, 5123: np.uint16, 5121: np.uint8}[a['componentType']]
        n = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
        off = v.get('byteOffset', 0) + a.get('byteOffset', 0)
        return np.frombuffer(binb, dt, a['count'] * n, off).reshape(a['count'], n) if n > 1 else \
            np.frombuffer(binb, dt, a['count'], off)

    def image(ti):
        im = g['images'][g['textures'][ti]['source']]
        v = g['bufferViews'][im['bufferView']]
        return Image.open(io.BytesIO(binb[v.get('byteOffset', 0):v.get('byteOffset', 0) + v['byteLength']])).convert('RGB')
    return g, acc, image


def bc1_block_encode(rgb):
    """rgb (H, W, 3) uint8, H and W multiples of 4 -> BC1 blocks (bytes), row by row."""
    H, W, _ = rgb.shape
    blk = rgb.reshape(H // 4, 4, W // 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 3).astype(np.float32)
    mean = blk.mean(1, keepdims=True)
    c = blk - mean
    cov = np.einsum('bki,bkj->bij', c, c)
    axis = np.ones((len(blk), 3), np.float32)
    for _ in range(8):  # power iteration: each block's main colour axis
        axis = np.einsum('bij,bj->bi', cov, axis)
        axis /= np.maximum(np.linalg.norm(axis, axis=1, keepdims=True), 1e-6)
    t = np.einsum('bki,bi->bk', c, axis)
    lo = mean[:, 0] + axis * t.min(1, keepdims=True)
    hi = mean[:, 0] + axis * t.max(1, keepdims=True)

    def to565(col):
        col = np.clip(np.rint(col), 0, 255).astype(np.int32)
        return ((col[:, 0] * 31 + 127) // 255 << 11) | ((col[:, 1] * 63 + 127) // 255 << 5) | ((col[:, 2] * 31 + 127) // 255)

    def from565(v):
        r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
        return np.stack([(r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31], 1).astype(np.float32)
    c0, c1 = to565(hi), to565(lo)
    swap = c0 < c1
    c0, c1 = np.where(swap, c1, c0), np.where(swap, c0, c1)
    p0, p1 = from565(c0), from565(c1)
    pal = np.stack([p0, p1, (2 * p0 + p1) / 3, (p0 + 2 * p1) / 3], 1)  # (B, 4, 3)
    d = ((blk[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(3)
    idx = d.argmin(2).astype(np.uint32)
    idx[c0 == c1] = 0
    bits = (idx << (2 * np.arange(16, dtype=np.uint32))).sum(1).astype(np.uint32)
    out = np.zeros(len(blk), dtype=[('c0', '<u2'), ('c1', '<u2'), ('i', '<u4')])
    out['c0'], out['c1'], out['i'] = c0, c1, bits
    return out.tobytes()


def bc1_mips(im):
    w, h = im.size
    s = 1.0
    while max(w, h) * s > MAX_TEX:
        s /= 2
    W = max(4, 1 << int(np.floor(np.log2(max(4, w * s)))))
    H = max(4, 1 << int(np.floor(np.log2(max(4, h * s)))))
    im = im.resize((W, H), Image.LANCZOS)
    data, mips = bytearray(), 0
    cw, ch, cur = W, H, im
    while True:
        a = np.asarray(cur.convert('RGB'), np.uint8)
        pw, ph = max(4, cw), max(4, ch)
        if a.shape[0] != ph or a.shape[1] != pw:
            a = np.asarray(cur.resize((pw, ph), Image.BOX).convert('RGB'), np.uint8)
        data += bc1_block_encode(a)
        mips += 1
        if cw == 1 and ch == 1:
            break
        cw, ch = max(1, cw // 2), max(1, ch // 2)
        cur = im.resize((cw, ch), Image.LANCZOS)
    return W, H, mips, bytes(data)


def prepack(suits_dir, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    report = []
    for s in SM1_SUITS:
        g, acc, image = read_glb(os.path.join(suits_dir, s['suit'], 'scene.glb'))
        extras = g['asset']['extras']['suit']
        ends = np.r_[np.array(extras['joints'], np.float64).reshape(16, 3), np.array(extras['ends'], np.float64).reshape(3, 3)]
        groups = {}
        for p in g['meshes'][0]['primitives']:
            m = g['materials'][p['material']]
            pbr = m.get('pbrMetallicRoughness', {})
            key = ('t', pbr['baseColorTexture']['index']) if 'baseColorTexture' in pbr else \
                ('c', tuple(round(float(x), 3) for x in pbr.get('baseColorFactor', [1, 1, 1, 1])[:3]))
            groups.setdefault(key, []).append(p)
        header = {'hero': s['hero'], 'id': s['id'], 'name': s['name'], 'suit': s['suit'],
                  'ends': [[round(float(v), 6) for v in e] for e in ends], 'groups': []}
        blobs = {}
        for gi, (key, prims) in enumerate(sorted(groups.items(), key=lambda kv: -sum(
                g['accessors'][p['attributes']['POSITION']]['count'] for p in kv[1]))):
            P, N, U, J, Wt, I = [], [], [], [], [], []
            base = 0
            for p in prims:
                a = p['attributes']
                pos = acc(a['POSITION'])
                P.append(pos)
                N.append(acc(a['NORMAL']))
                U.append(acc(a['TEXCOORD_0']))
                J.append(acc(a['JOINTS_0']))
                Wt.append(acc(a['WEIGHTS_0']))
                I.append(acc(p['indices']).astype(np.int64) + base)
                base += len(pos)
            P, N, U = np.concatenate(P).astype(np.float32), np.concatenate(N).astype(np.float32), np.concatenate(U).astype(np.float32)
            J, Wt, I = np.concatenate(J).astype(np.uint8), np.concatenate(Wt).astype(np.uint8), np.concatenate(I).astype(np.uint32)
            if key[0] == 't':
                im = image(key[1])
            else:
                im = Image.new('RGB', (4, 4), tuple(int(round(255 * c)) for c in key[1]))
            w, h, mips, tex = bc1_mips(im)
            n = 'g%d_' % gi
            blobs.update({n + 'pos': P.tobytes(), n + 'nrm': N.tobytes(), n + 'uv': U.tobytes(), n + 'jnt': J.tobytes(),
                          n + 'wgt': Wt.tobytes(), n + 'idx': I.tobytes(), n + 'tex': tex})
            header['groups'].append({'vertices': int(len(P)), 'indices': int(len(I)), 'pos': n + 'pos', 'nrm': n + 'nrm',
                                     'uv': n + 'uv', 'jnt': n + 'jnt', 'wgt': n + 'wgt', 'idx': n + 'idx',
                                     'uv_min': [float(x) for x in U.min(0)], 'uv_max': [float(x) for x in U.max(0)],
                                     'tex': {'width': w, 'height': h, 'mips': mips, 'blob': n + 'tex'}})
        path = os.path.join(out_dir, s['id'] + '.hsm1')
        hsm1.write(path, header, blobs)
        report.append('%s: %d groups, %d vertices, %d triangles, %d bytes' % (
            s['id'], len(header['groups']), sum(x['vertices'] for x in header['groups']),
            sum(x['indices'] for x in header['groups']) // 3, os.path.getsize(path)))
    return report


def write_license(suits_dir, out_dir):
    """Models/SM1Suits/license.txt: which suit each pack comes from, with that suit's own license.txt (same credits)."""
    parts = ['HERO VR SM1 suit packs (made by Agent Mulder with tools/sm1_suits/prepack.py).',
             'Each pack is made from one HERO VR suit model below and keeps its credit and licence. Changes: the mesh is',
             'kept, the textures are resized to at most 1024 px and BC1-compressed; the launcher then fits it to',
             "Spider-Man Remastered's own suit on the player's PC. See THIRD-PARTY-MODELS.md.", '']
    for h in SM1_SUITS:
        lic = os.path.join(suits_dir, h['suit'], 'license.txt')
        text = open(lic, encoding='utf-8', errors='replace').read().strip() if os.path.isfile(lic) else '(license.txt missing)'
        parts += ['== %s.hsm1 (%s) from Models/Suits/%s ==' % (h['id'], h['name'], h['suit']), text, '']
    with open(os.path.join(out_dir, 'license.txt'), 'w', encoding='utf-8', newline='\r\n') as f:
        f.write('\n'.join(parts))


if __name__ == '__main__':
    for line in prepack(sys.argv[1], sys.argv[2]):
        print(line)
    write_license(sys.argv[1], sys.argv[2])
    print('license.txt written')
