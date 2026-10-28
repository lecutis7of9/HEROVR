"""Version 0.0.7 model tools: read a Sketchfab glTF (scene.gltf + scene.bin) into flat
triangle lists, sample base-colour textures, write a compact GLB the mod's loader reads
(src/mesh_model.cpp: POSITION + COLOR_0 + indices, uncompressed). The source folder is only read."""
import json, os, struct
import numpy as np

COMP = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
NCOMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


def accessor(g, bin_, i):
    a = g['accessors'][i]
    v = g['bufferViews'][a['bufferView']]
    dt = np.dtype(COMP[a['componentType']])
    n, k = a['count'], NCOMP[a['type']]
    off = v.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = v.get('byteStride', 0) or dt.itemsize * k
    raw = np.frombuffer(bin_, dtype=np.uint8, count=stride * (n - 1) + dt.itemsize * k, offset=off)
    out = np.lib.stride_tricks.as_strided(raw, shape=(n, dt.itemsize * k), strides=(stride, 1)).copy()
    out = out.view(dt).reshape(n, k)
    if a.get('normalized') and dt != np.float32:
        out = out.astype(np.float32) / np.iinfo(dt).max
    return out


def node_matrix(n):
    if 'matrix' in n:
        return np.array(n['matrix'], dtype=np.float64).reshape(4, 4).T
    t = np.eye(4)
    if 'translation' in n:
        t[:3, 3] = n['translation']
    r = np.eye(4)
    if 'rotation' in n:
        x, y, z, w = n['rotation']
        r[:3, :3] = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
    s = np.eye(4)
    if 'scale' in n:
        s[0, 0], s[1, 1], s[2, 2] = n['scale']
    return t @ r @ s


def load(folder):
    """-> list of parts: dict(mesh=index, material=index, pos=(T,3,3) world, uv=(T,3,2) or None), and the glTF."""
    name = 'scene.gltf'
    if not os.path.exists(os.path.join(folder, name)):  # build 9: a zip with its own name (Superman.gltf)
        named = sorted(f for f in os.listdir(folder) if f.lower().endswith('.gltf'))
        name = named[0] if len(named) == 1 else name
    g = json.load(open(os.path.join(folder, name), encoding='utf-8'))
    bin_ = open(os.path.join(folder, g['buffers'][0]['uri']), 'rb').read()
    parts = []

    def walk(ni, m):
        n = g['nodes'][ni]
        m = m @ node_matrix(n)
        if 'mesh' in n:
            for p in g['meshes'][n['mesh']]['primitives']:
                if p.get('mode', 4) != 4:
                    continue
                pos = accessor(g, bin_, p['attributes']['POSITION']).astype(np.float64)
                pos = (np.c_[pos, np.ones(len(pos))] @ m.T)[:, :3]
                idx = accessor(g, bin_, p['indices']).ravel() if 'indices' in p else np.arange(len(pos))
                idx = idx.reshape(-1, 3)
                uv = accessor(g, bin_, p['attributes']['TEXCOORD_0']) if 'TEXCOORD_0' in p['attributes'] else None
                parts.append(dict(mesh=n['mesh'], material=p.get('material'), pos=pos[idx],
                                  uv=None if uv is None else uv[idx]))
        for c in n.get('children', []):
            walk(c, m)

    for ni in g['scenes'][g.get('scene', 0)]['nodes']:
        walk(ni, np.eye(4))
    return g, parts


def write_glb(path, tris, colors, extras=None):
    """tris (T,3,3) metres, colors (T,3) linear 0..1. Unindexed corners, one colour per corner."""
    tris = np.asarray(tris, np.float32).reshape(-1, 3)
    col = np.repeat(np.asarray(colors, np.float32), 3, axis=0)
    idx = np.arange(len(tris), dtype=np.uint32)
    pb, cb, ib = tris.tobytes(), col.tobytes(), idx.tobytes()
    binb = pb + cb + ib
    lo, hi = tris.min(0).tolist(), tris.max(0).tolist()
    asset = {'version': '2.0', 'generator': 'HERO VR 0.0.7 tools/models007'}
    if extras:
        asset['extras'] = extras
    g = {'asset': asset, 'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0}],
         'meshes': [{'primitives': [{'attributes': {'POSITION': 0, 'COLOR_0': 1}, 'indices': 2}]}],
         'buffers': [{'byteLength': len(binb)}],
         'bufferViews': [{'buffer': 0, 'byteOffset': 0, 'byteLength': len(pb)},
                         {'buffer': 0, 'byteOffset': len(pb), 'byteLength': len(cb)},
                         {'buffer': 0, 'byteOffset': len(pb) + len(cb), 'byteLength': len(ib)}],
         'accessors': [{'bufferView': 0, 'componentType': 5126, 'count': len(tris), 'type': 'VEC3', 'min': lo, 'max': hi},
                       {'bufferView': 1, 'componentType': 5126, 'count': len(col), 'type': 'VEC3'},
                       {'bufferView': 2, 'componentType': 5125, 'count': len(idx), 'type': 'SCALAR'}]}
    j = json.dumps(g, separators=(',', ':')).encode()
    j += b' ' * (-len(j) % 4)
    binb += b'\0' * (-len(binb) % 4)
    body = struct.pack('<II', len(j), 0x4E4F534A) + j + struct.pack('<II', len(binb), 0x004E4942) + binb
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', 0x46546C67, 2, 12 + len(body)) + body)
