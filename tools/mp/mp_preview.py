#!/usr/bin/env python3
"""0.0.73 MP SOLO TEST: desktop previews of the remote player (PLAYER 2) and its nameplate, from the game's
own code (spidy_mp_preview: the real packets, mp::partner, the suit_wear / mesh_model suit, the soft figure and
the nameplate), rasterised from the player's eye 3 m behind the ghost.

  python3 tools/mp/mp_preview.py [--models DIR] [--preview build/spidy_mp_preview] [--out DIR]
Writes <out>/mp_scene_<k>.png, <out>/mp-preview-all.png and <out>/scenes.json.
"""
import argparse, json, os, shutil, struct, subprocess, sys, tempfile
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', '..')
sys.path.insert(0, os.path.join(ROOT, 'tools', 'models072'))
import model_check as mc  # noqa: E402

EYE = np.array([0, 1.65, 0])
W, H, FOV = 900, 900, 62


def run(preview, models, work):
    cache, out = os.path.join(work, 'cache') + '/', os.path.join(work, 'out') + '/'
    os.makedirs(cache, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    for _ in range(3):
        r = subprocess.run([preview, models.rstrip('/') + '/', cache, out], capture_output=True, text=True)
        if r.returncode == 0:
            return out, json.load(open(out + 'scenes.json'))
        if r.returncode != 3:
            break
        for f in os.listdir(cache):
            if f.endswith('.enc') and not os.path.exists(cache + f[:-4] + '.rgba'):
                im = Image.open(cache + f).convert('RGBA')
                with open(cache + f[:-4] + '.rgba', 'wb') as o:
                    o.write(struct.pack('<II', *im.size))
                    o.write(im.tobytes())
    raise SystemExit('spidy_mp_preview failed: %s %s' % (r.returncode, r.stderr[-800:]))


def camera():
    f = np.array([0, -.08, -1.0])
    f /= np.linalg.norm(f)
    x = np.cross(f, [0, 1, 0])
    x /= np.linalg.norm(x)
    y = np.cross(x, f)
    return np.stack([x, y, -f])


def project(pos, R):
    p = (pos - EYE) @ R.T
    k = 1 / np.tan(np.radians(FOV) / 2)
    sx = (p[:, 0] / -p[:, 2] * k * .5 + .5) * W
    sy = (-p[:, 1] / -p[:, 2] * k * .5 + .5) * H
    return np.c_[sx, sy, p[:, 2]]


def rasterise(img, zb, tris, cols, alphas=None, tex=None):
    """tris (T,3,3) screen (x, y, z<0 toward -inf far); cols (T,3,3) linear; alphas (T,3) or None (opaque, z-tested)."""
    for t in range(len(tris)):
        q = tris[t]
        if (q[:, 2] > -.05).any():
            continue
        x0, y0 = np.floor(q[:, :2].min(0)).astype(int)
        x1, y1 = np.ceil(q[:, :2].max(0)).astype(int)
        x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, W - 1), min(y1, H - 1)
        if x1 < x0 or y1 < y0:
            continue
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + .5, np.arange(y0, y1 + 1) + .5)
        (ax, ay, _), (bx, by, _), (cx, cy, _) = q
        den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(den) < 1e-12:
            continue
        l0 = ((by - cy) * (xs - cx) + (cx - bx) * (ys - cy)) / den
        l1 = ((cy - ay) * (xs - cx) + (ax - cx) * (ys - cy)) / den
        l2 = 1 - l0 - l1
        m = (l0 >= -1e-6) & (l1 >= -1e-6) & (l2 >= -1e-6)
        if not m.any():
            continue
        z = l0 * q[0, 2] + l1 * q[1, 2] + l2 * q[2, 2]
        sub = zb[y0:y1 + 1, x0:x1 + 1]
        a, b, c = l0[m], l1[m], l2[m]
        col = a[:, None] * cols[t, 0] + b[:, None] * cols[t, 1] + c[:, None] * cols[t, 2]
        if alphas is None:
            m &= z > sub
            a, b, c = l0[m], l1[m], l2[m]
            if not m.any():
                continue
            col = a[:, None] * cols[t, 0] + b[:, None] * cols[t, 1] + c[:, None] * cols[t, 2]
            if tex is not None:
                col = tex(t, a, b, c, col)
                if col.shape[1] == 4:
                    keep = col[:, 3] >= .5
                    idx = np.nonzero(m)
                    m2 = np.zeros_like(m)
                    m2[idx[0][keep], idx[1][keep]] = True
                    m, col = m2, col[keep, :3]
            img[y0:y1 + 1, x0:x1 + 1][m] = col
            sub[m] = z[m]
        else:
            al = np.clip(a * alphas[t, 0] + b * alphas[t, 1] + c * alphas[t, 2], 0, 1)[:, None]
            reg = img[y0:y1 + 1, x0:x1 + 1]
            reg[m] = reg[m] * (1 - al) + col * al


def scene(d, k, layers, R, title, plate, info):
    img = np.empty((H, W, 3))
    # a floor and sky like an evening rooftop, so the figure reads
    yy = np.linspace(0, 1, H)[:, None, None]
    img[:] = mc.s2l((.09, .1, .13)) * (1 - yy) + mc.s2l((.16, .15, .14)) * yy
    zb = np.full((H, W), -np.inf)
    # floor grid (opaque lines drawn as thin quads would be heavy: dots per metre instead)
    g = ImageDraw.Draw(Image.new('RGB', (1, 1)))
    del g
    mv = np.fromfile(d + 'model_%d.bin' % k, dtype=np.float32).reshape(-1, 11)
    if len(mv):
        T = project(mv[:, :3].astype(np.float64), R).reshape(-1, 3, 3)
        C = mv[:, 3:6].astype(np.float64).reshape(-1, 3, 3)
        S = mv[:, 7:11].astype(np.float64).reshape(-1, 3, 4)

        def tex(t, a, b, c, lin):
            layer = int(round(S[t, 0, 2]))
            if S[t, 0, 3] == 1 and 0 <= layer < len(layers):
                uv = a[:, None] * S[t, 0, :2] + b[:, None] * S[t, 1, :2] + c[:, None] * S[t, 2, :2]
                tx = mc.sample(layers[layer], uv)
                return np.c_[lin * mc.s2l(tx[:, :3]), tx[:, 3]]
            return lin
        rasterise(img, zb, T, C, None, tex)
    sv = np.fromfile(d + 'soft_%d.bin' % k, dtype=np.float32).reshape(-1, 11)
    if len(sv):
        T = project(sv[:, :3].astype(np.float64), R).reshape(-1, 3, 3)
        C = sv[:, 3:6].astype(np.float64).reshape(-1, 3, 3)
        A = sv[:, 6].astype(np.float64).reshape(-1, 3)
        rasterise(img, zb, T, C, A)
    out = Image.fromarray((np.clip(mc.l2s(img) if hasattr(mc, 'l2s') else img ** (1 / 2.2), 0, 1) * 255).astype(np.uint8))
    canvas = Image.new('RGB', (W, H + 64), (14, 14, 18))
    canvas.paste(out, (0, 40))
    dr = ImageDraw.Draw(canvas)
    dr.text((10, 8), title, fill=(255, 220, 120))
    dr.text((10, 24), 'nameplate: "%s"   packet %d bytes   %s' % (plate, info['packetBytes'],
            ('suit model: ' + info['suit']) if info['suitDrawn'] else
            'no suit model: PLAYER 2 BODY' if info.get('bodyDrawn') else 'no suit model: soft figure'), fill=(200, 200, 200))
    dr.text((10, H + 46), 'HERO VR 0.0.73 MP SOLO TEST - view from your eyes, PLAYER 2 about 3 m ahead (desktop preview, game code)',
            fill=(150, 150, 160))
    return canvas


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--models', default='/workspace/r72work/e2e72/runs/live-reup/inst/HERO-VR-Launcher-0.0.72/Models')
    ap.add_argument('--preview', default=os.path.join(ROOT, 'build', 'spidy_mp_preview'))
    ap.add_argument('--out', default='/workspace/hero-0.0.73/mp-solo/previews')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    work = tempfile.mkdtemp(prefix='mppreview')
    d, js = run(a.preview, a.models, work)
    layers = []
    for l in range(js['layers']):
        raw = open(d + 'layer_%d.rgba' % l, 'rb').read()
        w, h = struct.unpack('<II', raw[:8])
        layers.append(np.frombuffer(raw[8:8 + w * h * 4], np.uint8).reshape(h, w, 4))
    R = camera()
    tiles = []
    for k, s in enumerate(js['scenes']):
        im = scene(d, k, layers, R, s['title'], s['plate'], s)
        im.save(os.path.join(a.out, 'mp_scene_%d.png' % k))
        tiles.append(im)
    sheet = Image.new('RGB', (len(tiles) * 600, int(600 * (H + 64) / W)), (0, 0, 0))
    for i, t in enumerate(tiles):
        sheet.paste(t.resize((600, int(600 * (H + 64) / W))), (i * 600, 0))
    sheet.save(os.path.join(a.out, 'mp-preview-all.png'))
    shutil.copy(d + 'scenes.json', os.path.join(a.out, 'scenes.json'))
    print(json.dumps(js, indent=1))


if __name__ == '__main__':
    main()
