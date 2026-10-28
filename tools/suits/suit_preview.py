#!/usr/bin/env python3
"""Build 9 SUITS: worn-suit previews and the per-frame cost, from the game's own code (spidy_suit_bench).

For every suit: what the player sees wearing it - the first-person view (SUIT BODY = ARMS: the suit's arms and
gloves on the tracked hands) and the mirror 1.4 m ahead (the whole suit reflected) - rendered from the eye
(perspective, the game's textures), plus the timing of estimate / moves / skin / draw per frame.

  python3 tools/suits/suit_preview.py [--models build/h7out/Models] [--out DIR] [--frames 300]
Writes <out>/worn_<suit>.png, <out>/worn-all.png (a contact sheet) and <out>/suit-perf.json.
"""
import argparse, json, os, struct, subprocess, sys, tempfile
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', '..')
sys.path.insert(0, os.path.join(ROOT, 'tools', 'models072'))
import model_check as mc  # noqa: E402

FOLDERS = mc.SUIT_FOLDERS
EYE = np.array([0, 1.65, 0])


def run_bench(bench, models, work, frames):
    cache, out = os.path.join(work, 'cache') + '/', os.path.join(work, 'out') + '/'
    os.makedirs(cache, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    for _ in range(3):
        r = subprocess.run([bench, models.rstrip('/') + '/', cache, out, str(frames), '512'], capture_output=True, text=True)
        misses = [f[:-4] for f in os.listdir(cache) if f.endswith('.enc') and not os.path.exists(cache + f[:-4] + '.rgba')]
        if r.returncode == 0:
            return out, json.load(open(out + 'bench.json'))
        for h in misses:
            im = Image.open(cache + h + '.enc').convert('RGBA')
            with open(cache + h + '.rgba', 'wb') as f:
                f.write(struct.pack('<II', *im.size))
                f.write(im.tobytes())
    raise SystemExit('spidy_suit_bench failed: %s %s' % (r.returncode, r.stderr[-500:]))


def look(forward, up=(0, 1, 0)):
    f = np.asarray(forward, float) / np.linalg.norm(forward)
    x = np.cross(f, up)
    x /= np.linalg.norm(x)
    y = np.cross(x, f)
    return np.stack([x, y, -f])  # rows: camera x, y, z (z toward the eye)


def render(v, layers, R, fov, size=512):
    if not len(v):
        return np.zeros((size, size, 3))
    pos = v[:, 0:3].astype(np.float64)
    C = v[:, 3:6].astype(np.float64).reshape(-1, 3, 3)
    S = v[:, 7:11].astype(np.float64).reshape(-1, 3, 4)
    p = (pos - EYE) @ R.T
    keep = (p[:, 2].reshape(-1, 3) < -.05).all(1)
    p = p.reshape(-1, 3, 3)[keep]
    C, S = C[keep], S[keep]
    k = 1 / np.tan(np.radians(fov) / 2)
    T = np.empty_like(p)
    T[..., 0] = p[..., 0] / -p[..., 2] * k
    T[..., 1] = p[..., 1] / -p[..., 2] * k
    T[..., 2] = p[..., 2]

    def shade(t, l0, l1, l2):
        lin = l0[:, None] * C[t, 0] + l1[:, None] * C[t, 1] + l2[:, None] * C[t, 2]
        layer = int(round(S[t, 0, 2]))
        if S[t, 0, 3] == 1 and 0 <= layer < len(layers):
            uv = l0[:, None] * S[t, 0, :2] + l1[:, None] * S[t, 1, :2] + l2[:, None] * S[t, 2, :2]
            tx = mc.sample(layers[layer], uv)
            return np.c_[lin * mc.s2l(tx[:, :3]), tx[:, 3]]
        return lin

    on = (np.abs(T[..., :2]) < 1.5).any(2).any(1)
    img, _ = mc.raster(T[on], lambda t, a, b, c: shade(np.nonzero(on)[0][t], a, b, c), size=size)
    return img


def to8(img):
    return Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--models', default=os.path.join(ROOT, 'build', 'h7out', 'Models'))
    ap.add_argument('--bench', default=os.path.join(ROOT, 'build', 'gate', 'spidy_suit_bench'))
    ap.add_argument('--out', default='/workspace/hero-0.0.72/suit-preview')
    ap.add_argument('--frames', type=int, default=300)
    ap.add_argument('--only', default='')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    work = tempfile.mkdtemp(prefix='suitbench')
    d, bench = run_bench(a.bench, a.models, work, a.frames)
    layers = []
    for l in range(bench['layers']):
        raw = open(d + 'layer_%d.rgba' % l, 'rb').read()
        w, h = struct.unpack('<II', raw[:8])
        layers.append(np.frombuffer(raw[8:8 + w * h * 4], np.uint8).reshape(h, w, 4))
    fp = look((0, -.55, -1))      # first person: looking ahead and down at the hands
    mirror = look((0, -.18, -1))  # at the mirror 1.4 m ahead (the reflection 2.8 m away)
    tiles = []
    for k, f in enumerate(FOLDERS):
        if a.only and f not in a.only.split(','):
            continue
        load = lambda n: np.fromfile(d + '%s_%d.bin' % (n, k), dtype=np.float32).reshape(-1, 11)
        b = bench['suits'][f]
        A = to8(render(load('arms'), layers, fp, 95))
        M = to8(render(load('mirror'), layers, mirror, 55))
        im = Image.new('RGB', (1024, 560), (20, 20, 24))
        im.paste(A, (0, 24))
        im.paste(M, (512, 24))
        g = ImageDraw.Draw(im)
        g.text((8, 6), '%s  FIRST PERSON (SUIT BODY: ARMS)' % f, fill=(255, 220, 120))
        g.text((520, 6), 'MIRROR 1.4 m ahead (the whole suit, reflected)', fill=(255, 220, 120))
        g.text((8, 540), '%d triangles  per frame: estimate+moves %.0f us, skin %.0f us, draw arms %.0f us, mirror %.0f us = %.0f us%s' % (
            b['triangles'], b['estimateUs'] + b['movesUs'], b['skinUs'], b['drawArmsUs'], b['drawMirrorUs'],
            b['frameArmsMirrorUs'], '' if b['ok'] else '  NOT WORN'), fill=(230, 230, 230))
        im.save(os.path.join(a.out, 'worn_%s.png' % f))
        tiles.append(im)
    if tiles:
        cols = 3
        sheet = Image.new('RGB', (cols * 512, ((len(tiles) + cols - 1) // cols) * 280), (0, 0, 0))
        for i, t in enumerate(tiles):
            sheet.paste(t.resize((512, 280)), ((i % cols) * 512, (i // cols) * 280))
        sheet.save(os.path.join(a.out, 'worn-all.png'))
    s = bench['suits']
    tot = [s[f]['frameArmsMirrorUs'] for f in FOLDERS]
    summary = dict(frames=bench['frames'], cpu=open('/proc/cpuinfo').read().split('model name')[1].split('\n')[0].strip(': ')
                   if os.path.exists('/proc/cpuinfo') else '', meanUs=float(np.mean(tot)), maxUs=float(np.max(tot)),
                   worst=max(FOLDERS, key=lambda f: s[f]['frameArmsMirrorUs']), suits=s)
    json.dump(summary, open(os.path.join(a.out, 'suit-perf.json'), 'w'), indent=1)
    print('suits worn: %d of %d  per frame (arms + mirror): mean %.0f us, max %.0f us (%s)' % (
        sum(s[f]['ok'] for f in FOLDERS), len(FOLDERS), summary['meanUs'], summary['maxUs'], summary['worst']))


if __name__ == '__main__':
    main()
