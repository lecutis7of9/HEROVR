#!/usr/bin/env python3
"""0.0.73 test 5 CLAW LOOK: Wolverine's fists with their claws out, close (spidy_hero_hands_preview scenes made with
HANDS_CLAWS=old|steel). Per scene, one hand from the player's eye, from outside (the back of the hand), from above
and from in front (the punch's way).

  render_claws.py <scene dir>[,<scene dir>...] <out png> [--labels=A,B,...] [--hand=right|left]
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'dd_baton073'))
from render_dd_baton import render  # noqa: E402
from render_hands import load  # noqa: E402


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    opts = dict(a[2:].split('=', 1) for a in sys.argv[1:] if a.startswith('--') and '=' in a)
    folders, out = args[0].split(','), args[1]
    labels = opts.get('labels', ','.join(os.path.basename(f.rstrip('/')) for f in folders)).split(',')
    hand = opts.get('hand', 'right')
    s = 1 if hand == 'right' else -1
    eye = np.array([0, 1.65, 0])
    g = np.array([.2 * s, 1.22, -.36])
    ahead = g + np.array([0, 0, -.13])
    views = [(eye + (g - eye) * .2, ahead, 'EYE'), (g + np.array([s * .6, .05, -.1]), ahead, 'OUTSIDE (BACK OF HAND)'),
             (g + np.array([0, .6, .05]), ahead, 'ABOVE'), (g + np.array([s * .12, .12, -.75]), g + np.array([0, 0, -.05]), 'FRONT')]
    size = 400
    sheet = Image.new('RGB', (size * len(views), size * len(folders)), 'white')
    for j, (folder, lab) in enumerate(zip(folders, labels)):
        tris, cols = load(folder)
        near = np.linalg.norm(tris.mean(axis=1) - g, axis=1) < .6
        for i, (e, at, name) in enumerate(views):
            im = render(tris[near], cols[near], e, at, size=(size, size), fov=50)
            ImageDraw.Draw(im).text((6, 5), f'{lab}  {hand.upper()}  {name}', fill=(20, 20, 20))
            sheet.paste(im, (i * size, j * size))
    sheet.save(out)
    print(out, sheet.size)


if __name__ == '__main__':
    main()
