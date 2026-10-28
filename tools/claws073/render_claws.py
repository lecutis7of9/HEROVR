"""Version 0.0.73 claws previews, no headset: the JACKET AND JEANS suit as shipped in 0.0.72 (its claws in it),
the suit as 0.0.73 ships it (claws taken out), and the two claw models alone. Flat grey, painter's order, lit
.32 + .68 |n.l|. Usage: render_claws.py <0.0.72 Models> <0.0.73 Models> <out folder>"""
import json, os, struct, sys
import numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'models007'))
import gltf_io

L = np.array([.35, .8, .5]); L /= np.linalg.norm(L)

def tris_of(path):
    b = open(path, 'rb').read()
    jl = struct.unpack_from('<I', b, 12)[0]
    g = json.loads(b[20:20 + jl]); binb = b[20 + jl + 8:]
    out = []
    for m in g['meshes']:
        for p in m['primitives']:
            pos = gltf_io.accessor(g, binb, p['attributes']['POSITION']).astype(np.float64)
            idx = gltf_io.accessor(g, binb, p['indices']).ravel() if 'indices' in p else np.arange(len(pos))
            out.append(pos[idx.reshape(-1, 3)])
    return np.concatenate(out)

def render(t, yaw, title, size=(520, 620), hl=None):
    W, H = size
    a = np.radians(yaw); R = np.array([[np.cos(a), 0, np.sin(a)], [0, 1, 0], [-np.sin(a), 0, np.cos(a)]])
    v = t @ R.T
    p = v.reshape(-1, 3); lo, hi = p.min(0), p.max(0)
    s = .85 * min(W / max(hi[0] - lo[0], 1e-6), (H - 60) / max(hi[1] - lo[1], 1e-6)); c = (lo + hi) / 2
    n = np.cross(v[:, 1] - v[:, 0], v[:, 2] - v[:, 0]); n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-12
    shade = .32 + .68 * np.abs(n @ L)
    order = np.argsort(v[:, :, 2].mean(1))
    im = Image.new('RGB', (W, H), (18, 22, 30)); d = ImageDraw.Draw(im)
    for i in order:
        q = [(W / 2 + (x - c[0]) * s, H / 2 + 20 - (y - c[1]) * s) for x, y, _ in v[i]]
        base = np.array(hl[i] if hl is not None else (200, 200, 205))
        d.polygon(q, fill=tuple(int(k) for k in base * shade[i]))
    d.text((10, 8), title, fill=(230, 235, 255))
    return im

def main(old, new, out):
    os.makedirs(out, exist_ok=True)
    s72 = tris_of(os.path.join(old, 'Suits/Suit_wolverine_3/scene.glb'))
    s73 = tris_of(os.path.join(new, 'Suits/Suit_wolverine_3/scene.glb'))
    cl = tris_of(os.path.join(new, 'Claws/Left/scene.glb')); cr = tris_of(os.path.join(new, 'Claws/Right/scene.glb'))
    ims = [render(s72, 20, f'0.0.72 suit (claws in it): {len(s72)} tris'),
           render(s73, 20, f'0.0.73 suit (claws taken out): {len(s73)} tris')]
    Image.fromarray(np.hstack([np.asarray(i) for i in ims])).save(os.path.join(out, 'claws-suit-before-after.png'))
    ims = [render(cl, 70, f'Claws/Left: {len(cl)} tris (its own +z back)'),
           render(cr, 70, f'Claws/Right: {len(cr)} tris')]
    Image.fromarray(np.hstack([np.asarray(i) for i in ims])).save(os.path.join(out, 'claws-models.png'))

if __name__ == '__main__':
    main(*sys.argv[1:4])
