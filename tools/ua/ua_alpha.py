#!/usr/bin/env python3
"""ULTIMATE ALLIANCE EXPANSION fix 3 - the room's OPAQUE materials marked opaque.

The IGB import marks every material BLEND, so HERO VR cut every texel under half alpha (mesh_model: cut-out
alpha) - on solid furniture, walls and floors a few stray clear texels (the texture atlases' padding, a mip's
edge) became holes that showed the black box and shimmered as the head moved. A material whose texture is
really solid (under CUT_SHARE of its texels below half alpha) is written OPAQUE here (its alpha then ignored);
real cut-outs (the plants, leaves, rails, the glass, the soft shadows) keep BLEND.

  python ua_alpha.py <scene.glb> [<out.glb>]   (in place without <out.glb>)
"""
import io
import json
import struct
import sys
from pathlib import Path

CUT_SHARE = 0.02  # a material is a real cut-out when over 2 % of its texels are under half alpha


def read_glb(path):
    data = Path(path).read_bytes()
    magic, version, length = struct.unpack_from('<III', data, 0)
    if magic != 0x46546C67 or version != 2:
        raise ValueError(f'{path}: not a glTF 2 binary')
    at, doc, blob = 12, None, b''
    while at < length:
        size, kind = struct.unpack_from('<II', data, at)
        chunk = data[at + 8:at + 8 + size]
        if kind == 0x4E4F534A:
            doc = json.loads(chunk.decode('utf-8'))
        elif kind == 0x004E4942:
            blob = chunk
        at += 8 + size
    if doc is None:
        raise ValueError(f'{path}: no JSON chunk')
    return doc, blob


def write_glb(path, doc, blob):
    text = json.dumps(doc, separators=(',', ':')).encode('utf-8')
    text += b' ' * (-len(text) % 4)
    blob = blob + b'\0' * (-len(blob) % 4)
    chunks = struct.pack('<II', len(text), 0x4E4F534A) + text
    if blob:
        chunks += struct.pack('<II', len(blob), 0x004E4942) + blob
    Path(path).write_bytes(struct.pack('<III', 0x46546C67, 2, 12 + len(chunks)) + chunks)


def image_bytes(doc, blob, index, folder):
    im = doc.get('images', [])[index]
    if 'bufferView' in im:
        v = doc['bufferViews'][im['bufferView']]
        o = v.get('byteOffset', 0)
        return blob[o:o + v['byteLength']]
    if 'uri' in im and not im['uri'].startswith('data:'):
        return (Path(folder) / im['uri']).read_bytes()
    return None


def cut_share(png):
    """The share of texels under half alpha (0 without an alpha channel)."""
    from PIL import Image
    import numpy as np
    pic = Image.open(io.BytesIO(png))
    if pic.mode not in ('RGBA', 'LA', 'PA') and not (pic.mode == 'P' and 'transparency' in pic.info):
        return 0.0
    a = np.asarray(pic.convert('RGBA'))[..., 3]
    return float((a < 128).mean()) if a.size else 0.0


def solidify(src, dst=None, share=CUT_SHARE):
    """Marks the solid materials OPAQUE; returns (opaque, kept) material names."""
    doc, blob = read_glb(src)
    folder = Path(src).parent
    opaque, kept = [], []
    shares = {}
    for m in doc.get('materials', []):
        name = m.get('name', '?')
        if m.get('alphaMode', 'OPAQUE') == 'OPAQUE':
            continue
        ref = m.get('pbrMetallicRoughness', {}).get('baseColorTexture')
        if not ref:
            kept.append(name)
            continue
        tex = doc['textures'][ref['index']]
        src_index = tex.get('source')
        if src_index is None:
            kept.append(name)
            continue
        if src_index not in shares:
            png = image_bytes(doc, blob, src_index, folder)
            try:
                shares[src_index] = cut_share(png) if png else 1.0
            except Exception:
                shares[src_index] = 1.0  # unreadable: left as it was
        f = m.get('pbrMetallicRoughness', {}).get('baseColorFactor', [1, 1, 1, 1])
        if shares[src_index] <= share and (len(f) < 4 or f[3] >= .999):
            m['alphaMode'] = 'OPAQUE'
            m.pop('alphaCutoff', None)
            opaque.append(name)
        else:
            kept.append(name)
    write_glb(dst or src, doc, blob)
    return opaque, kept


if __name__ == '__main__':
    if len(sys.argv) not in (2, 3):
        raise SystemExit(__doc__)
    o, k = solidify(sys.argv[1], sys.argv[2] if len(sys.argv) == 3 else None)
    print(f'opaque {len(o)}, cut-out kept {len(k)}: {", ".join(k)}')
