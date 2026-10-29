#!/usr/bin/env python3
"""0.0.73 HERO HANDS: where a worn IRON MAN gauntlet's palm repulsor disc faces, against the REAL palm.

Reads a spidy_hero_hands_preview scene (hands.tri + grip.txt), finds the bright flat disc (the repulsor node:
light, not reddish, near the grip), fits its plane (the smallest spread of its corners) and prints, per hand, the
angle between the disc's outward normal (away from the hand's middle) and the real palm (OpenXR grip +X left / -X
right). 0 deg: the drawn palm faces where the real palm faces; 180: the hand is drawn back to front.

  palm_disc.py <scene dir> [...]
"""
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from render_hands import load  # noqa: E402


def grips(folder):
    rows = [list(map(float, l.split())) for l in open(os.path.join(folder, 'grip.txt')) if l.strip()]
    return [(np.array(r[:3]), np.array(r[3:6]), np.array(r[6:9])) for r in rows]


def measure(folder):
    tris, cols = load(folder)
    m = tris.mean(axis=1)
    area = np.linalg.norm(np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0]), axis=1) / 2
    out = []
    for side, (g, palm, aim) in enumerate(grips(folder)):
        near = np.linalg.norm(m - g, axis=1) < .12
        bright = near & (cols.mean(1) > .6) & (cols[:, 2] >= cols[:, 0] - .05) & (np.abs(cols[:, 1] - .85) > .05)
        if not bright.any():
            out.append(None)
            continue
        hc = (m[near] * area[near, None]).sum(0) / area[near].sum()
        rc = (m[bright] * area[bright, None]).sum(0) / area[bright].sum()
        pts = tris[bright].reshape(-1, 3)
        w, v = np.linalg.eigh((pts - pts.mean(0)).T @ (pts - pts.mean(0)))
        n = v[:, 0] if v[:, 0] @ (rc - hc) >= 0 else -v[:, 0]
        out.append(math.degrees(math.acos(max(-1, min(1, n @ palm)))))
    return out


if __name__ == '__main__':
    for f in sys.argv[1:]:
        r = measure(f)
        print(os.path.basename(f.rstrip('/')), ' '.join(f'{"LR"[i]}: {"no disc" if a is None else f"{a:.0f} deg off the real palm"}'
                                                         for i, a in enumerate(r)))
