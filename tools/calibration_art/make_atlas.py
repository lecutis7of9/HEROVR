"""Builds the calibration panel's art atlas, assets/calibration_art.qoi.

In it: the night street (city.py), the crouching Spider-Man the player chose
(hero.png, supplied by the player for their own copy; not Spidy's drawing)
with a warm gold halo made from its outline, the two titles set in Orbitron
(SIL OFL 1.1, title.py; the font itself is not shipped) and the health
ring's spider emblem drawn here. The panel's live text is not in it:
it is drawn as geometry with Spidy's pixel font (src/pixel_font.cpp).

    python3 tools/calibration_art/make_atlas.py [--work DIR] [--ss N]

The slow render (the city, about 2 minutes) is cached as .npy in
the work directory. The layout must match include/spidy/calibration_art.hpp;
the test CalibrationArt checks the decoded size and a few texels.
"""
import argparse, os, struct, subprocess, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
W, H = 2048, 1280
# name: (x, y, w, h); kept in step with include/spidy/calibration_art.hpp.
LAYOUT = {
    'city': (0, 0, 1024, 512),
    'figure': (1024, 0, 1024, 1024),   # hero.png, aspect kept, centred
    'figureGlow': (0, 512, 512, 512),  # its gold halo, the same framing
    'emblem': (512, 512, 512, 512),
    'titleCalibration': (0, 1024, 1024, 128),
    'titleCalibrated': (0, 1152, 1024, 128),
    'white': (1024, 1024, 8, 8),
}


def to_srgb(x):
    x = np.clip(x, 0, 1)
    return np.where(x <= .0031308, x * 12.92, 1.055 * x ** (1 / 2.4) - .055)


def blur(a, sigma):
    """Separable gaussian (own, so scipy is not needed)."""
    r = int(3 * sigma) + 1
    k = np.exp(-np.arange(-r, r + 1) ** 2 / (2 * sigma * sigma))
    k /= k.sum()
    pad = [(r, r), (r, r)] + [(0, 0)] * (a.ndim - 2)
    p = np.pad(a, pad, mode='edge')
    p = sum(k[i] * p[i:i + a.shape[0]] for i in range(2 * r + 1))
    return sum(k[i] * p[:, i:i + a.shape[1]] for i in range(2 * r + 1))


def bleed(rgb, a):
    """Gives transparent texels their neighbours' colour, so filtering and
    mips do not pull black fringes into the edges."""
    out = rgb.copy()
    weight = a.copy()
    acc = rgb * a[..., None]
    for sigma in (2, 6, 18, 48):
        w = blur(weight, sigma)
        c = blur(acc, sigma) / np.maximum(w, 1e-6)[..., None]
        empty = a <= 0
        out[empty] = np.where(w[empty, None] > 1e-4, c[empty], out[empty])
    return out


def area_down(img, f):
    h, w = img.shape[:2]
    return img.reshape(h // f, f, w // f, f, *img.shape[2:]).mean((1, 3))


def cached(work, name, script, args):
    path = os.path.join(work, name)
    if not os.path.exists(path):
        print('rendering', name, '(slow)')
        subprocess.run([sys.executable, os.path.join(HERE, script)] + args, cwd=work, check=True)
    return np.load(path)


def hero_layers(path, size=1024, fit=.86):
    """hero.png fitted (aspect kept) into a size x size square with a margin
    for the halo, straight alpha, linear; and its halo at half size: the
    outline grown and blurred, warm gold."""
    im = Image.open(path).convert('RGBA')
    k = fit * size / max(im.size)
    w, h = round(im.width * k), round(im.height * k)
    src = np.asarray(im, float) / 255
    lin = np.concatenate([np.where(src[..., :3] <= .04045, src[..., :3] / 12.92,
                                   ((src[..., :3] + .055) / 1.055) ** 2.4), src[..., 3:]], -1)
    pre = np.concatenate([lin[..., :3] * lin[..., 3:], lin[..., 3:]], -1)
    chans = [np.asarray(Image.fromarray(pre[..., c].astype(np.float32), 'F').resize((w, h), Image.LANCZOS))
             for c in range(4)]
    pre = np.clip(np.stack(chans, -1), 0, None)
    a = np.clip(pre[..., 3], 0, 1)
    rgb = np.clip(pre[..., :3] / np.maximum(a, 1e-6)[..., None], 0, 1)
    out = np.zeros((size, size, 4))
    x0, y0 = (size - w) // 2, (size - h) // 2
    out[y0:y0 + h, x0:x0 + w, :3] = rgb
    out[y0:y0 + h, x0:x0 + w, 3] = a
    out[..., 3][out[..., 3] < 1 / 255] = 0
    # The halo: half size, the outline grown, then soft.
    m = area_down(out[..., 3], 2)
    grown = np.clip(blur(m, 3) * 4, 0, 1)
    halo = np.clip(blur(grown, 9) * 1.15, 0, 1)
    gold = np.array([1.0, .55, .08])
    glow = np.concatenate([np.broadcast_to(gold, halo.shape + (3,)), halo[..., None]], -1)
    return out, glow


def emblem(size=512, ss=4):
    """Spidy's own spider for the health ring's centre: a black, angular
    spider (8 legs, two body parts), not any published logo."""
    n = size * ss
    im = Image.new('L', (n, n), 0)
    d = ImageDraw.Draw(im)
    c = n / 2
    s = n / 512

    def P(x, y):
        return (c + x * s, c + y * s)
    # body: small head, long abdomen
    d.ellipse([*P(-30, -118), *P(30, -52)], fill=255)
    d.polygon([P(0, -62), P(44, -20), P(30, 92), P(0, 128), P(-30, 92), P(-44, -20)], fill=255)
    # legs, each three straight segments, mirrored
    legs = [
        [(18, -88), (70, -150), (118, -196), (142, -250)],
        [(30, -60), (110, -96), (178, -120), (226, -168)],
        [(34, -20), (114, 12), (184, 54), (230, 112)],
        [(24, 10), (78, 92), (124, 168), (138, 238)],
    ]
    for leg in legs:
        for sign in (1, -1):
            pts = [P(sign * x, y) for x, y in leg]
            for (p, q), w in zip(zip(pts, pts[1:]), (18, 14, 9)):
                d.line([p, q], fill=255, width=int(w * s))
                d.ellipse([p[0] - w * s / 2, p[1] - w * s / 2, p[0] + w * s / 2, p[1] + w * s / 2], fill=255)
    a = np.asarray(im.resize((size, size), Image.LANCZOS), float) / 255
    rgb = np.zeros((size, size, 3)) + .015
    return np.concatenate([rgb, a[..., None]], -1)


def qoi_encode(img):
    """QOI (qoiformat.org, public domain format), RGBA, sRGB channels."""
    h, w = img.shape[:2]
    px = img.reshape(-1, 4)
    out = bytearray(b'qoif' + struct.pack('>II', w, h) + bytes([4, 0]))
    index = [(0, 0, 0, 0)] * 64
    prev = (0, 0, 0, 255)
    run = 0
    n = len(px)
    rows = [tuple(int(v) for v in p) for p in px]
    for i, p in enumerate(rows):
        if p == prev:
            run += 1
            if run == 62 or i == n - 1:
                out.append(0xc0 | (run - 1))
                run = 0
            continue
        if run:
            out.append(0xc0 | (run - 1))
            run = 0
        r, g, b, a = p
        h6 = (r * 3 + g * 5 + b * 7 + a * 11) % 64
        if index[h6] == p:
            out.append(h6)
        else:
            index[h6] = p
            if a == prev[3]:
                dr = (r - prev[0] + 128) % 256 - 128
                dg = (g - prev[1] + 128) % 256 - 128
                db = (b - prev[2] + 128) % 256 - 128
                drg, dbg = dr - dg, db - dg
                if -2 <= dr <= 1 and -2 <= dg <= 1 and -2 <= db <= 1:
                    out.append(0x40 | (dr + 2) << 4 | (dg + 2) << 2 | (db + 2))
                elif -32 <= dg <= 31 and -8 <= drg <= 7 and -8 <= dbg <= 7:
                    out += bytes([0x80 | (dg + 32), (drg + 8) << 4 | (dbg + 8)])
                else:
                    out += bytes([0xfe, r, g, b])
            else:
                out += bytes([0xff, r, g, b, a])
        prev = p
    out += bytes([0] * 7 + [1])
    return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--work', default=os.path.join(ROOT, 'build', 'calibration-art'))
    ap.add_argument('--out', default=os.path.join(ROOT, 'assets', 'calibration_art.qoi'))
    ap.add_argument('--png', default=None, help='also write a PNG of the atlas')
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)
    sys.path.insert(0, HERE)
    import title as titles

    atlas = np.zeros((H, W, 4))  # sRGB, straight alpha, 0..1

    def put(name, srgb_rgba):
        x, y, w, h = LAYOUT[name]
        assert srgb_rgba.shape[:2] == (h, w), (name, srgb_rgba.shape)
        atlas[y:y + h, x:x + w] = srgb_rgba

    def put_linear(name, rgba):
        rgb = bleed(rgba[..., :3], rgba[..., 3])
        put(name, np.concatenate([to_srgb(rgb), rgba[..., 3:]], -1))

    city = cached(a.work, 'city_lin.npy', 'city.py', ['2', 'city.png'])
    put('city', np.concatenate([to_srgb(city), np.ones(city.shape[:2] + (1,))], -1))
    hero, glow = hero_layers(os.path.join(HERE, 'hero.png'))
    put_linear('figure', hero)
    put_linear('figureGlow', glow)
    put_linear('emblem', emblem())
    for name, text in (('titleCalibration', 'BODY CALIBRATION'), ('titleCalibrated', 'CALIBRATED')):
        t = titles.title(text)  # sRGB colours, straight alpha
        rgb = bleed(t[..., :3], t[..., 3])
        put(name, np.concatenate([rgb, t[..., 3:]], -1))
    put('white', np.ones((8, 8, 4)))
    data = (np.clip(atlas, 0, 1) * 255 + .5).astype(np.uint8)
    blob = qoi_encode(data)
    with open(a.out, 'wb') as f:
        f.write(blob)
    print(a.out, len(blob), 'bytes')
    if a.png:
        Image.fromarray(data, 'RGBA').save(a.png)


if __name__ == '__main__':
    main()
