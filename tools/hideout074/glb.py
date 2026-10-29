"""0.0.74 HIDEOUTS: a tiny glTF 2.0 (.glb) reader for the hideout tools: each mesh node's triangles in
world (glTF, Y-up) space, by node name. numpy only. Made by Agent Mulder."""
import json
import struct

import numpy as np

COMPONENT = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
WIDTH = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


def read(path):
    data = open(path, 'rb').read()
    magic, version, length = struct.unpack_from('<III', data, 0)
    if magic != 0x46546C67 or version != 2:
        raise ValueError(f'{path}: not a glb 2.0')
    at, js, binary = 12, None, b''
    while at < length:
        size, kind = struct.unpack_from('<II', data, at)
        chunk = data[at + 8:at + 8 + size]
        if kind == 0x4E4F534A:
            js = json.loads(chunk)
        elif kind == 0x004E4942:
            binary = chunk
        at += 8 + size
    return js, binary


def write(path, js, binary):
    text = json.dumps(js, separators=(',', ':')).encode()
    text += b' ' * (-len(text) % 4)
    binary = binary + b'\0' * (-len(binary) % 4)
    out = struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(text) + (8 + len(binary) if binary else 0))
    out += struct.pack('<II', len(text), 0x4E4F534A) + text
    if binary:
        out += struct.pack('<II', len(binary), 0x004E4942) + binary
    open(path, 'wb').write(out)


def accessor(js, binary, index):
    a = js['accessors'][index]
    view = js['bufferViews'][a['bufferView']]
    dtype = np.dtype(COMPONENT[a['componentType']])
    width = WIDTH[a['type']]
    offset = view.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = view.get('byteStride', 0) or dtype.itemsize * width
    count = a['count']
    raw = np.frombuffer(binary, dtype=np.uint8, count=stride * (count - 1) + dtype.itemsize * width, offset=offset)
    rows = np.lib.stride_tricks.as_strided(raw, shape=(count, dtype.itemsize * width), strides=(stride, 1))
    return np.frombuffer(rows.copy().tobytes(), dtype=dtype).reshape(count, width)


def matrix(node):
    if 'matrix' in node:
        return np.array(node['matrix'], dtype=np.float64).reshape(4, 4).T
    t = np.array(node.get('translation', [0, 0, 0]), dtype=np.float64)
    x, y, z, w = node.get('rotation', [0, 0, 0, 1])
    r = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    s = np.array(node.get('scale', [1, 1, 1]), dtype=np.float64)
    m = np.eye(4)
    m[:3, :3] = r * s
    m[:3, 3] = t
    return m


def triangles(path):
    """{node name: (n, 3, 3) float64 triangles in world space}"""
    js, binary = read(path)
    out = {}

    def walk(i, parent):
        node = js['nodes'][i]
        m = parent @ matrix(node)
        if 'mesh' in node:
            tris = []
            for p in js['meshes'][node['mesh']]['primitives']:
                if p.get('mode', 4) != 4:
                    continue
                pos = accessor(js, binary, p['attributes']['POSITION']).astype(np.float64)
                idx = accessor(js, binary, p['indices']).reshape(-1) if 'indices' in p else np.arange(len(pos))
                w = pos @ m[:3, :3].T + m[:3, 3]
                tris.append(w[idx.reshape(-1, 3)])
            if tris:
                out.setdefault(node.get('name', f'node{i}'), []).append(np.concatenate(tris))
        for c in node.get('children', []):
            walk(c, m)

    scene = js['scenes'][js.get('scene', 0)]
    for r in scene['nodes']:
        walk(r, np.eye(4))
    return {k: np.concatenate(v) for k, v in out.items()}
