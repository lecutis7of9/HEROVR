#!/usr/bin/env python3
"""Version 0.0.73 test 3 DAREDEVIL BATON: rasterises spidy_dd_baton_preview's scenes (x y z r g b per corner,
float32, triangles) into PNGs: each hand close up from the player's eye, from the back of the hand, from the
palm side and from above, flat shaded with a depth buffer.

  render_dd_baton.py <scene dir> <out png> <scene> [<scene> ...] [--hand left|right|both] [--wide]
"""
import math
import sys

import numpy as np
from PIL import Image, ImageDraw


def load(path):
    a = np.fromfile(path, dtype=np.float32).reshape(-1, 3, 6)
    return a[:, :, :3].astype(np.float64), a[:, 0, 3:].astype(np.float64)


def look(eye, at, up=(0, 1, 0)):
    f = np.asarray(at, float) - eye
    f /= np.linalg.norm(f)
    r = np.cross(f, up)
    r /= np.linalg.norm(r)
    u = np.cross(r, f)
    return np.stack([r, u, -f])


def render(tris, cols, eye, at, size=(420, 420), fov=34.0):
    w, h = size
    R = look(np.asarray(eye, float), at)
    p = (tris - eye) @ R.T  # camera space, -z forward
    z = -p[..., 2]
    keep = (z > .02).all(axis=1)
    p, z, c = p[keep], z[keep], cols[keep]
    tr = tris[keep]
    fpx = (w / 2) / math.tan(math.radians(fov / 2))
    sx = w / 2 + fpx * p[..., 0] / z
    sy = h / 2 - fpx * p[..., 1] / z
    n = np.cross(tr[:, 1] - tr[:, 0], tr[:, 2] - tr[:, 0])
    ln = np.linalg.norm(n, axis=1)
    ok = ln > 1e-14
    n[ok] /= ln[ok, None]
    light = np.array([.3, 1., .45]); light /= np.linalg.norm(light)
    lit = .35 + .65 * np.abs(n @ light)
    shade = np.clip(c * lit[:, None], 0, 1)
    img = np.full((h, w, 3), .93)
    depth = np.full((h, w), np.inf)
    for i in np.nonzero(ok)[0]:
        x0, x1 = int(max(0, math.floor(sx[i].min()))), int(min(w - 1, math.ceil(sx[i].max())))
        y0, y1 = int(max(0, math.floor(sy[i].min()))), int(min(h - 1, math.ceil(sy[i].max())))
        if x0 > x1 or y0 > y1:
            continue
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + .5, np.arange(y0, y1 + 1) + .5)
        (ax, bx, cx), (ay, by, cy) = sx[i], sy[i]
        d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(d) < 1e-9:
            continue
        l0 = ((by - cy) * (xs - cx) + (cx - bx) * (ys - cy)) / d
        l1 = ((cy - ay) * (xs - cx) + (ax - cx) * (ys - cy)) / d
        l2 = 1 - l0 - l1
        m = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
        if not m.any():
            continue
        zz = l0 * z[i, 0] + l1 * z[i, 1] + l2 * z[i, 2]
        sub = depth[y0:y1 + 1, x0:x1 + 1]
        m &= zz < sub
        sub[m] = zz[m]
        img[y0:y1 + 1, x0:x1 + 1][m] = shade[i]
    return Image.fromarray((img * 255).astype(np.uint8))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    hand = 'both'
    for a in sys.argv[1:]:
        if a.startswith('--hand='):
            hand = a.split('=', 1)[1]
    folder, out, scenes = args[0], args[1], args[2:]
    wide = '--wide' in sys.argv
    eye = np.array([0, 1.65, 0])
    grips = {'left': np.array([-.2, 1.22, -.36]), 'right': np.array([.2, 1.22, -.36])}
    hands = ['left', 'right'] if hand == 'both' else [hand]
    rows = []
    for s in scenes:
        tris, cols = load(f'{folder}/dd_{s}.tri')
        tiles = []
        for hd in hands:
            g = grips[hd]
            side = 1 if hd == 'right' else -1
            if wide:
                views = [(eye + np.array([0, .02, .05]), np.array([0, 1.4, -.8]), 'EYE VIEW')]
            else:
                views = [(eye + (g - eye) * .35, g + np.array([0, .04, 0]), 'EYE'),
                         (g + np.array([side * .45, .05, .05]), g, 'BACK OF HAND'),
                         (g + np.array([-side * .25, -.2, -.35]), g, 'FRONT/BELOW'),
                         (g + np.array([.0, .2, .4]), g + np.array([0, .05, 0]), 'BEHIND/ABOVE')]
            for e, at, label in views:
                im = render(tris, cols, e, at, size=(840, 520) if wide else (360, 360), fov=60 if wide else 38)
                ImageDraw.Draw(im).text((8, 6), f'{s.upper()}  {hd.upper()}  {label}', fill=(20, 20, 20))
                tiles.append(im)
        rows.append(tiles)
    tw, th = rows[0][0].size
    sheet = Image.new('RGB', (tw * max(len(r) for r in rows), th * len(rows)), 'white')
    for j, r in enumerate(rows):
        for i, im in enumerate(r):
            sheet.paste(im, (i * tw, j * th))
    sheet.save(out)
    print(out, sheet.size)


if __name__ == '__main__':
    main()
