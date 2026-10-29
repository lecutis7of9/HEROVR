#!/usr/bin/env python3
"""0.0.73 HERO HANDS: rasterises spidy_hero_hands_preview's hands.tri (x y z u v image r g b per corner, float32;
image -1: the rgb as is, else that texture (img_<k>.bin beside it) sampled at the triangle's middle uv times the
rgb tint) into a sheet: each hand from the player's eye, from the back of the hand, from the palm side and from
the front (where the repulsor fires). The green rod is the REAL palm's way (OpenXR grip +X left / -X right), the
blue one the controller's aim (grip -Y).

  render_hands.py <scene dir>[,<scene dir>...] <out png> [--labels=A,B,...] [--hands=both|left|right] [--near=.3]
  (--near: the radius kept round each grip, metres; .5 takes in Wolverine's claws)
"""
import io
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'dd_baton073'))
from render_dd_baton import render  # noqa: E402


def load(folder):
    a = np.fromfile(os.path.join(folder, 'hands.tri'), dtype=np.float32).reshape(-1, 3, 9)
    tris = a[:, :, :3].astype(np.float64)
    uv = a[:, :, 3:5].mean(axis=1)
    img = a[:, 0, 5].astype(int)
    rgb = a[:, 0, 6:9].astype(np.float64)
    cols = rgb.copy()
    cache = {}
    for k in sorted(set(img.tolist())):
        if k < 0:
            continue
        path = os.path.join(folder, f'img_{k}.bin')
        if not os.path.exists(path):
            continue
        try:
            im = np.asarray(Image.open(io.BytesIO(open(path, 'rb').read())).convert('RGB'), dtype=np.float64) / 255.0
        except Exception:
            continue
        cache[k] = im
        sel = img == k
        h, w = im.shape[:2]
        u = np.mod(uv[sel, 0], 1.0)
        v = np.mod(uv[sel, 1], 1.0)
        px = np.clip((u * w).astype(int), 0, w - 1)
        py = np.clip((v * h).astype(int), 0, h - 1)
        cols[sel] = im[py, px] * rgb[sel]
    return tris, np.clip(cols, 0, 1)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    opts = dict(a[2:].split('=', 1) for a in sys.argv[1:] if a.startswith('--') and '=' in a)
    folders, out = args[0].split(','), args[1]
    labels = opts.get('labels', ','.join(os.path.basename(f.rstrip('/')) for f in folders)).split(',')
    hand = opts.get('hands', 'both')
    eye = np.array([0, 1.65, 0])
    grips = {'left': np.array([-.2, 1.22, -.36]), 'right': np.array([.2, 1.22, -.36])}
    hands = ['left', 'right'] if hand == 'both' else [hand]
    rows = []
    for folder, lab in zip(folders, labels):
        tris, cols = load(folder)
        gfile = os.path.join(folder, 'grip.txt')
        palms = {}
        if os.path.exists(gfile):
            rows_g = [list(map(float, l.split())) for l in open(gfile) if l.strip()]
            grips = {'left': np.array(rows_g[0][:3]), 'right': np.array(rows_g[1][:3])}
            palms = {'left': (np.array(rows_g[0][3:6]), np.array(rows_g[0][6:9])), 'right': (np.array(rows_g[1][3:6]), np.array(rows_g[1][6:9]))}
        tiles = []
        for hd in hands:
            g = grips[hd]
            side = 1 if hd == 'right' else -1
            views = [(eye + (g - eye) * .35, g + np.array([0, .04, 0]), 'EYE'),
                     (g + np.array([side * .42, .08, .05]), g, 'OUTSIDE (BACK)'),
                     (g + np.array([-side * .42, .02, .02]), g, 'INSIDE (PALM)'),
                     (g + np.array([0, .05, -.45]), g + np.array([0, .03, 0]), 'FRONT')]
            near = np.linalg.norm(tris.mean(axis=1) - g, axis=1) < float(opts.get('near', '.3'))
            if hd in palms:  # views by the real palm: from the eye, the palm side, the back, the thumb side
                pv, av = palms[hd]
                way = av - pv * (av @ pv)
                way /= np.linalg.norm(way)
                thumb = np.cross(pv, way) * (1 if hd == 'right' else -1)
                c = g + way * .05
                views = [(eye + (g - eye) * .35, g + np.array([0, .04, 0]), 'EYE'),
                         (c + pv * .4 + np.array([0, .03, .09]), c, 'PALM SIDE'),
                         (c - pv * .4 + np.array([0, .03, .09]), c, 'BACK SIDE'),
                         (c + thumb * .4 + np.array([0, .03, .09]), c, 'THUMB SIDE')]
            for e, at, label in views:
                im = render(tris[near], cols[near], e, at, size=(320, 320), fov=40)
                ImageDraw.Draw(im).text((6, 5), f'{lab}  {hd.upper()}  {label}', fill=(20, 20, 20))
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
