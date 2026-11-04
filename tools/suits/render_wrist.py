#!/usr/bin/env python3
"""0.0.78 WRIST TWIST: renders apps/wrist_twist_preview's poses (<out>/<suit>_<on|off>_<deg>/hands.tri, arm.txt) into
one sheet: a row per roll (-90, 0, +90, 180 degrees), columns the right forearm from outside and from above with the
twist chain OFF and ON, then the left forearm from outside with it ON. The cross-section numbers are the harness's.

  render_wrist.py <out dir> <suit> <png> [--side=right|left]
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'hero_hands073'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'dd_baton073'))
from render_hands import load  # noqa: E402
from render_dd_baton import render  # noqa: E402


def views(folder, hand):
    a = [list(map(float, l.split())) for l in open(os.path.join(folder, 'arm.txt')) if l.strip()]
    e, w = np.array(a[hand][:3]), np.array(a[hand][3:6])
    ax = (w - e) / np.linalg.norm(w - e)
    side = 1 if hand else -1
    out = np.array([side, 0, 0.]) - ax * ax[0] * side
    out /= np.linalg.norm(out)
    up = np.cross(ax, out) * side
    if up[1] < 0:
        up = -up
    c = e + (w - e) * .85
    return [(c + out * .5, c, 'OUTSIDE'), (c + up * .5 + ax * 1e-3, c, 'ABOVE')]


def main():
    out_dir, suit, png = sys.argv[1:4]
    meas = {}  # the harness's lines: (on|off, deg, hand) -> the hand's min %
    mf = os.path.join(out_dir, 'measure.txt')
    if os.path.exists(mf):
        import re
        for l in open(mf):
            m = re.match(r'(\S+)\s+twist (ON|OFF)\s+([+-]\d+)\s+L.*?min\s+(\d+)%\s+R.*?min\s+(\d+)%', l)
            if m and m[1] == suit:
                meas[(m[2].lower(), int(m[3]), 0)] = int(m[4])
                meas[(m[2].lower(), int(m[3]), 1)] = int(m[5])
    rows = []
    for deg in (-90, 0, 90, 180):
        tiles = []
        for flag, hand, pick in (('off', 1, (0, 1)), ('on', 1, (0, 1)), ('on', 0, (0,))):
            f = os.path.join(out_dir, f'{suit}_{flag}_{deg}')
            tris, cols = load(f)
            cols = np.clip(cols ** .45 * 1.15, 0, 1)  # the black suit lifted so its folds show
            vs = views(f, hand)
            near = np.linalg.norm(tris.mean(axis=1) - vs[0][1], axis=1) < .2
            for i in pick:
                eye, at, lab = vs[i]
                im = render(tris[near], cols[near], eye, at, size=(300, 300), fov=40)
                d = ImageDraw.Draw(im)
                d.text((6, 5), f'{deg:+d} deg  TWIST {flag.upper()}  {"R" if hand else "L"} {lab}', fill=(200, 20, 20))
                m = meas.get((flag, deg, hand))
                if m:
                    d.text((6, 284), f'wrist cross-section min {m}% of rest', fill=(20, 20, 200))
                tiles.append(im)
        rows.append(tiles)
    tw, th = rows[0][0].size
    sheet = Image.new('RGB', (tw * len(rows[0]), th * len(rows)), 'white')
    for j, r in enumerate(rows):
        for i, im in enumerate(r):
            sheet.paste(im, (i * tw, j * th))
    sheet.save(png)
    print(png, sheet.size)


if __name__ == '__main__':
    main()
