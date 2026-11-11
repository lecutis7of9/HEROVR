"""The suit GLB writer: a copy of tools/suits/convert_suit.py's write_glb (same format: suit_wear.hpp reads it), kept
in tools/ua so the UA converter needs none of convert_suit's Sketchfab / bpy-venv set-up."""
import io, json, os, struct
import numpy as np
from PIL import Image


def write_glb(path, tris, uv, nrm, mat, mats, skin, extras):
    """skin (T,3,16) -> the 4 largest per corner, uint8 (summing to 255)."""
    w = skin.reshape(-1, 16)
    top = np.argsort(-w, axis=1)[:, :4]
    tw = np.take_along_axis(w, top, 1)
    tw /= np.maximum(tw.sum(1, keepdims=True), 1e-9)
    q = np.floor(tw * 255 + .5).astype(np.int64)
    q[:, 0] += 255 - q.sum(1)
    J = np.where(q > 0, top, 0).astype(np.uint8)
    Wq = np.clip(q, 0, 255).astype(np.uint8)
    binb = bytearray()
    g = {'asset': {'version': '2.0', 'generator': 'HERO VR build 9 tools/suits', 'extras': extras}, 'scene': 0,
         'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0}], 'meshes': [{'primitives': []}], 'buffers': [{}],
         'bufferViews': [], 'accessors': [], 'materials': [], 'textures': [], 'images': [], 'samplers': [{}]}

    def view(data):
        off = len(binb)
        binb.extend(data)
        binb.extend(b'\0' * (-len(binb) % 4))
        g['bufferViews'].append({'buffer': 0, 'byteOffset': off, 'byteLength': len(data)})
        return len(g['bufferViews']) - 1
    images = {}
    P_all = tris.reshape(-1, 3).astype(np.float32)
    N_all = nrm.reshape(-1, 3).astype(np.float32)
    N_all /= np.maximum(np.linalg.norm(N_all, axis=1, keepdims=True), 1e-9)
    U_all = uv.reshape(-1, 2).astype(np.float32)
    cm = np.repeat(mat, 3)
    for mi in sorted(set(int(m) for m in mat)):
        m = mats[mi]
        gm = {'name': m['name'][:120], 'pbrMetallicRoughness': {'baseColorFactor': list(m['factor']), 'metallicFactor': 0},
              'doubleSided': True}
        if m['image']:
            if m['image'] not in images:
                im = Image.open(m['image']).convert('RGB')  # base colour only: JPEG (a third of a PNG's size)
                b = io.BytesIO()
                im.save(b, 'JPEG', quality=90, subsampling=0)
                g['images'].append({'bufferView': view(b.getvalue()), 'mimeType': 'image/jpeg', 'name': os.path.basename(m['image'])})
                g['textures'].append({'source': len(g['images']) - 1, 'sampler': 0})
                images[m['image']] = len(g['textures']) - 1
            gm['pbrMetallicRoughness']['baseColorTexture'] = {'index': images[m['image']]}
            gm['pbrMetallicRoughness']['baseColorFactor'] = [1, 1, 1, 1]
        g['materials'].append(gm)
        sel = cm == mi
        P, N, U, Jm, Wm = P_all[sel], N_all[sel], U_all[sel], J[sel], Wq[sel]
        key = np.c_[np.round(P, 6), np.round(N, 4), np.round(U, 6), Jm, Wm]
        uniq, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
        inv = inv.ravel().astype(np.uint32)
        P, N, U, Jm, Wm = P[first], N[first], U[first], Jm[first], Wm[first]
        acc = len(g['accessors'])
        g['accessors'] += [
            {'bufferView': view(P.tobytes()), 'componentType': 5126, 'count': len(P), 'type': 'VEC3', 'min': P.min(0).tolist(), 'max': P.max(0).tolist()},
            {'bufferView': view(N.tobytes()), 'componentType': 5126, 'count': len(N), 'type': 'VEC3'},
            {'bufferView': view(U.tobytes()), 'componentType': 5126, 'count': len(U), 'type': 'VEC2'},
            {'bufferView': view(Jm.tobytes()), 'componentType': 5121, 'count': len(Jm), 'type': 'VEC4'},
            {'bufferView': view(Wm.tobytes()), 'componentType': 5121, 'normalized': True, 'count': len(Wm), 'type': 'VEC4'},
            {'bufferView': view(inv.tobytes()), 'componentType': 5125, 'count': len(inv), 'type': 'SCALAR'}]
        g['meshes'][0]['primitives'].append({'attributes': {'POSITION': acc, 'NORMAL': acc + 1, 'TEXCOORD_0': acc + 2,
                                                            'JOINTS_0': acc + 3, 'WEIGHTS_0': acc + 4},
                                             'indices': acc + 5, 'material': len(g['materials']) - 1})
    for k in ('textures', 'images'):
        if not g[k]:
            del g[k]
    if 'textures' not in g:
        del g['samplers']
    g['buffers'][0]['byteLength'] = len(binb)
    j = json.dumps(g, separators=(',', ':')).encode()
    j += b' ' * (-len(j) % 4)
    body = struct.pack('<II', len(j), 0x4E4F534A) + j + struct.pack('<II', len(binb), 0x004E4942) + bytes(binb)
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', 0x46546C67, 2, 12 + len(body)) + body)
