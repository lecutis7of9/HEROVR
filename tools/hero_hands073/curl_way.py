#!/usr/bin/env python3
"""0.0.73 HERO HANDS: which way a worn hand's fingers curl. Two spidy_hero_hands_preview scenes of one suit and grip
(an open hand, a fist); per hand, the hand's far part (past 6 cm from the grip along the hand's way) moves toward the
REAL palm (positive cm: the fingers close over the palm, as they should) or away (negative: the knuckles hinge back).

  curl_way.py <open scene> <fist scene>
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from palm_disc import grips  # noqa: E402
from render_hands import load  # noqa: E402


def far_centroid(folder, side):
    tris, _ = load(folder)
    g, palm, aim = grips(folder)[side]
    m = tris.mean(axis=1)
    near = np.linalg.norm(m - g, axis=1) < .2
    return m[near], g, palm, aim


if __name__ == '__main__':
    o, f = sys.argv[1], sys.argv[2]
    for side in (0, 1):
        mo, g, palm, aim = far_centroid(o, side)
        mf, _, _, _ = far_centroid(f, side)
        # the hand's way: the open hand's far points from the grip, mostly
        d = mo - g
        way = d[np.argsort(-np.linalg.norm(d, axis=1))[:40]].mean(0)
        way -= palm * (way @ palm)
        way /= np.linalg.norm(way)
        fo = mo[(mo - g) @ way > .06]
        ff = mf[(mf - g) @ way > .03]
        print(f'{"LR"[side]}: fingers move {100 * ((ff.mean(0) - fo.mean(0)) @ palm):+.1f} cm toward the real palm')
