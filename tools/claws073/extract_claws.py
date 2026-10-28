#!/usr/bin/env python3
"""Version 0.0.73 (fix 10) RETRACTABLE CLAWS: the JACKET AND JEANS Wolverine's own claws for every suit.

Reads Models/Suits/Suit_wolverine_3/scene.glb ("Wolverine - Jacket - Jeans (Rigged and Textured)" by Light.k,
CC BY 4.0) and writes, with the artist's real geometry, normals, UVs and texture (nothing made up):
  Models/Claws/Left/scene.glb, Models/Claws/Right/scene.glb - each hand's three claws, in the claw frame the game
      draws them in: metres, the claws' base (between the knuckles) at the origin, -z along the claws, +y the back of
      the hand, x = right (only moved and turned, never scaled or reshaped);
  the suit itself again without those claw triangles (so a worn suit never shows two sets: the claws come out of
      the knuckles only when the grip pops them).
The claws are the separate pieces of the suit's 4th primitive past the knuckles (|x| >= 0.62 m in the suit's rest
pose: its left at +x). Usage: extract_claws.py <in Suit_wolverine_3/scene.glb> <out Models dir>
"""
import json
import struct
import sys
from pathlib import Path

import numpy as np

CLAW_PRIM = 3
CLAW_X = 0.62


def load(path):
    b = Path(path).read_bytes()
    assert b[:4] == b'glTF'
    n = struct.unpack('<I', b[12:16])[0]
    j = json.loads(b[20:20 + n])
    off = 20 + n
    bl = struct.unpack('<I', b[off:off + 4])[0]
    return j, bytearray(b[off + 8:off + 8 + bl])


def acc(j, bin_, i):
    a = j['accessors'][i]
    bv = j['bufferViews'][a['bufferView']]
    n = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
    dt = {5126: np.float32, 5123: np.uint16, 5125: np.uint32, 5121: np.uint8}[a['componentType']]
    o = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    isz = np.dtype(dt).itemsize * n
    st = bv.get('byteStride', 0)
    if st and st != isz:
        raw = np.frombuffer(bytes(bin_), np.uint8, a['count'] * st, o).reshape(-1, st)[:, :isz].copy()
        return raw.view(dt).reshape(-1, n)
    return np.frombuffer(bytes(bin_), dt, a['count'] * n, o).reshape(-1, n).copy()


def components(tris, count):
    par = np.arange(count)

    def find(x):
        while par[x] != x:
            par[x] = par[par[x]]
            x = par[x]
        return x
    for a, b, c in tris:
        for u, v in ((a, b), (b, c)):
            ru, rv = find(u), find(v)
            if ru != rv:
                par[ru] = rv
    return np.array([find(i) for i in range(count)])


def write_glb(path, P, N, UV, tris, image_bytes, mime, extras):
    P = P.astype(np.float32); N = N.astype(np.float32); UV = UV.astype(np.float32)
    I = tris.astype(np.uint32).reshape(-1)
    blobs, views, accs = [], [], []
    off = 0
    def add(arr, target, typ, ctype, minmax=False):
        nonlocal off
        data = arr.tobytes()
        views.append({'buffer': 0, 'byteOffset': off, 'byteLength': len(data), **({'target': target} if target else {})})
        a = {'bufferView': len(views) - 1, 'componentType': ctype, 'count': int(arr.shape[0]), 'type': typ}
        if minmax:
            a['min'] = [float(x) for x in arr.min(0)]; a['max'] = [float(x) for x in arr.max(0)]
        accs.append(a)
        pad = (-len(data)) % 4
        blobs.append(data + b'\0' * pad)
        off += len(data) + pad
        return len(accs) - 1
    ip = add(P, 34962, 'VEC3', 5126, True)
    inn = add(N, 34962, 'VEC3', 5126)
    iu = add(UV, 34962, 'VEC2', 5126)
    ii = add(I, 34963, 'SCALAR', 5125)
    views.append({'buffer': 0, 'byteOffset': off, 'byteLength': len(image_bytes)})
    blobs.append(bytes(image_bytes) + b'\0' * ((-len(image_bytes)) % 4))
    off += len(image_bytes) + ((-len(image_bytes)) % 4)
    j = {'asset': {'version': '2.0', 'generator': 'HERO VR 0.0.73 tools/claws073', 'extras': extras},
         'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0}],
         'meshes': [{'primitives': [{'attributes': {'POSITION': ip, 'NORMAL': inn, 'TEXCOORD_0': iu}, 'indices': ii, 'material': 0}]}],
         'materials': [{'name': 'claws', 'pbrMetallicRoughness': {'baseColorFactor': [1, 1, 1, 1], 'metallicFactor': 0,
                                                                   'baseColorTexture': {'index': 0}}, 'doubleSided': True}],
         'textures': [{'source': 0, 'sampler': 0}], 'samplers': [{}],
         'images': [{'bufferView': len(views) - 1, 'mimeType': mime}],
         'accessors': accs, 'bufferViews': views, 'buffers': [{'byteLength': off}]}
    js = json.dumps(j, separators=(',', ':')).encode()
    js += b' ' * ((-len(js)) % 4)
    bin_ = b''.join(blobs)
    out = b'glTF' + struct.pack('<II', 2, 12 + 8 + len(js) + 8 + len(bin_))
    out += struct.pack('<I', len(js)) + b'JSON' + js + struct.pack('<I', len(bin_)) + b'BIN\0' + bin_
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_bytes(out)


def main(src, models):
    j, bin_ = load(src)
    prims = j['meshes'][0]['primitives']
    p = prims[CLAW_PRIM]
    P = acc(j, bin_, p['attributes']['POSITION'])
    N = acc(j, bin_, p['attributes']['NORMAL'])
    UV = acc(j, bin_, p['attributes']['TEXCOORD_0'])
    T = acc(j, bin_, p['indices']).reshape(-1, 3).astype(np.int64)
    roots = components(T, len(P))
    far = {}
    for r in np.unique(roots):
        vs = np.nonzero(roots == r)[0]
        if np.abs(P[vs, 0]).min() >= CLAW_X:
            far[r] = vs
    clawVert = np.zeros(len(P), bool)
    for vs in far.values():
        clawVert[vs] = True
    clawTri = clawVert[T].all(1)
    assert clawTri.sum() > 50, 'no claws found'
    # The body (skin) near each claw base: which side the back of the hand is.
    body = acc(j, bin_, prims[1]['attributes']['POSITION'])
    img = j['images'][j['textures'][j['materials'][p['material']]['pbrMetallicRoughness']['baseColorTexture']['index']]['source']]
    bv = j['bufferViews'][img['bufferView']]
    image = bin_[bv.get('byteOffset', 0):bv.get('byteOffset', 0) + bv['byteLength']]
    report = {}
    for side, name in ((1.0, 'Left'), (-1.0, 'Right')):  # the suit's left at +x
        tris = T[clawTri & (np.sign(P[T[:, 0], 0]) == side)]
        used = np.unique(tris)
        Q = P[used]
        reach = Q[:, 0] * side  # out along the arm
        base = Q[reach <= np.quantile(reach, .1)].mean(0)
        tip = Q[reach >= np.quantile(reach, .9)].mean(0)
        f = tip - base; length = float(np.linalg.norm(f)); f /= length
        near = body[np.linalg.norm(body - base, axis=1) < .08]
        hand = near.mean(0) if len(near) else base - np.array([0, .03, 0])
        u = base - hand; u -= f * u.dot(f); u /= np.linalg.norm(u)
        r = np.cross(f, u)
        # model coords: x = r, y = u, z = -f
        R = np.stack([r, u, -f])
        remap = -np.ones(len(P), np.int64); remap[used] = np.arange(len(used))
        LP = (P[used] - base) @ R.T
        LN = N[used] @ R.T
        write_glb(Path(models) / 'Claws' / name / 'scene.glb', LP, LN, UV[used], remap[tris], image, img.get('mimeType', 'image/jpeg'),
                  {'claws': {'from': 'Suits/Suit_wolverine_3', 'length': round(length, 4)}})
        lic = Path(src).parent / 'license.txt'
        note = ('Version 0.0.73 RETRACTABLE CLAWS: the claws of the model below, cut out of it unchanged (only moved and turned '
                'to the knuckles) by tools/claws073/extract_claws.py.\n\n')
        (Path(models) / 'Claws' / name / 'license.txt').write_text(note + (lic.read_text(errors='replace') if lic.is_file() else
            'Wolverine - Jacket - Jeans (Rigged and Textured) by Light.k (https://sketchfab.com/Light.k), CC-BY-4.0.\n'))
        report[name] = {'triangles': int(len(tris)), 'length_m': round(length, 3)}
    # The suit without its claws: the 4th primitive's indices rewritten (vertices left as they are).
    keep = T[~clawTri].astype(np.uint32).reshape(-1)
    ia = j['accessors'][p['indices']]
    data = keep.tobytes()
    off = len(bin_); pad = (-off) % 4
    bin_ += b'\0' * pad; off += pad
    bin_ += data
    j['bufferViews'].append({'buffer': 0, 'byteOffset': off, 'byteLength': len(data), 'target': 34963})
    ia.update({'bufferView': len(j['bufferViews']) - 1, 'byteOffset': 0, 'componentType': 5125, 'count': int(len(keep))})
    bin_ += b'\0' * ((-len(bin_)) % 4)
    j['buffers'][0]['byteLength'] = len(bin_)
    j['asset'].setdefault('extras', {})['clawsRemoved'] = 'Version 0.0.73: the claws are Models/Claws (retractable)'
    js = json.dumps(j, separators=(',', ':')).encode(); js += b' ' * ((-len(js)) % 4)
    out = b'glTF' + struct.pack('<II', 2, 12 + 8 + len(js) + 8 + len(bin_))
    out += struct.pack('<I', len(js)) + b'JSON' + js + struct.pack('<I', len(bin_)) + b'BIN\0' + bytes(bin_)
    dst = Path(models) / 'Suits' / 'Suit_wolverine_3' / 'scene.glb'
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(out)
    if (Path(src).parent / 'license.txt').is_file() and not (dst.parent / 'license.txt').is_file():
        (dst.parent / 'license.txt').write_bytes((Path(src).parent / 'license.txt').read_bytes())
    report['suit_triangles_removed'] = int(clawTri.sum())
    print(json.dumps(report))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
