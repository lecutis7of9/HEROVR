"""0.0.74 HIDEOUTS: builds a hideout's shipped folder (Models\\Hideout\\<NAME>\\) from its sources.

    python make_hideout.py batcave <batcave-room.glb> <batcave-room.json> <batcave-collision.glb> <out folder>
    python make_hideout.py safehouse <safehouse.glb> <safehouse.json> <safehouse-collision.glb> <out folder>

Writes:
  scene.glb  the room, as the mod's glTF reader takes it: KHR_lights_punctual (an extensionsRequired the reader
             refuses; the lighting is baked into the textures) stripped, nothing else changed.
  room.txt   what include/spidy/hideout074.hpp reads: the spawn, the interact points (each radius reaching the
             walkable floor), the playable bounds, kill_y, and the walk grid (each cell's floor height in cm, or
             blocked) made from the collision model: floor under the cell, no wall / prop between knee and head
             height, the prop_blockers, one cell of body radius, the part reachable from the spawn only.
  room.json  the source description, copied for reference.
  walk.png   a top view of the walk grid (a check, not shipped).
The spawn yaw (as the sources give it): degrees about up from +x (Blender), counter-clockwise; glTF forward
(cos, 0, -sin).
Coordinates in room.txt: glTF (x right, y up, z toward the viewer), metres, floor 0. Made by Agent Mulder.
"""
import json
import math
import os
import shutil
import sys
from collections import deque

import numpy as np

import glb

CELL = .25
KNEE, HEAD = .35, 1.8
STEP = .4


def blender_to_gltf(p):
    return [p[0], p[2], -p[1]]


def strip_lights(src, dst):
    js, binary = glb.read(src)
    for key in ('extensionsUsed', 'extensionsRequired'):
        if key in js:
            js[key] = [e for e in js[key] if e != 'KHR_lights_punctual']
            if not js[key]:
                del js[key]
    if 'extensions' in js:
        js['extensions'].pop('KHR_lights_punctual', None)
        if not js['extensions']:
            del js['extensions']
    for n in js.get('nodes', []):
        if 'extensions' in n:
            n['extensions'].pop('KHR_lights_punctual', None)
            if not n['extensions']:
                del n['extensions']
    glb.write(dst, js, binary)


def samples(tris, spacing):
    """points over each triangle, about `spacing` apart"""
    out = []
    for a, b, c in tris:
        n = int(min(200, max(2, math.ceil(max(np.linalg.norm(b - a), np.linalg.norm(c - a), np.linalg.norm(c - b)) / spacing))))
        u, v = np.meshgrid(np.arange(n + 1), np.arange(n + 1))
        keep = u + v <= n
        u, v = u[keep] / n, v[keep] / n
        out.append(a + np.outer(u, b - a) + np.outer(v, c - a))
    return np.concatenate(out) if out else np.zeros((0, 3))


def floor_heights(floor, x0, z0, cols, rows):
    h = np.full((rows, cols), np.nan)
    for a, b, c in floor:
        lo = np.minimum(np.minimum(a, b), c)
        hi = np.maximum(np.maximum(a, b), c)
        i0, i1 = max(0, int((lo[0] - x0) / CELL)), min(cols - 1, int((hi[0] - x0) / CELL))
        j0, j1 = max(0, int((lo[2] - z0) / CELL)), min(rows - 1, int((hi[2] - z0) / CELL))
        if i1 < i0 or j1 < j0:
            continue
        # the up-facing triangle's plane at each covered cell centre (barycentric in xz)
        d = (b[2] - c[2]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[2] - c[2])
        if abs(d) < 1e-9:
            continue
        for j in range(j0, j1 + 1):
            for i in range(i0, i1 + 1):
                px, pz = x0 + (i + .5) * CELL, z0 + (j + .5) * CELL
                l1 = ((b[2] - c[2]) * (px - c[0]) + (c[0] - b[0]) * (pz - c[2])) / d
                l2 = ((c[2] - a[2]) * (px - c[0]) + (a[0] - c[0]) * (pz - c[2])) / d
                l3 = 1 - l1 - l2
                if min(l1, l2, l3) < -1e-6:
                    continue
                y = l1 * a[1] + l2 * b[1] + l3 * c[1]
                if y < 2.5 and (np.isnan(h[j, i]) or y > h[j, i]):
                    h[j, i] = y
    return h


def build(kind, room_glb, room_json, col_glb, out):
    os.makedirs(out, exist_ok=True)
    meta = json.load(open(room_json))
    col = glb.triangles(col_glb)
    floor = np.concatenate([v for k, v in col.items() if 'floor' in k.lower()])
    solid = [v for k, v in col.items() if 'floor' not in k.lower()]
    solid = np.concatenate(solid) if solid else np.zeros((0, 3, 3))
    b = meta['bounds']
    lo = blender_to_gltf(b['playable_min'])
    hi = blender_to_gltf(b['playable_max'])
    x0, x1 = min(lo[0], hi[0]) - CELL, max(lo[0], hi[0]) + CELL
    z0, z1 = min(lo[2], hi[2]) - CELL, max(lo[2], hi[2]) + CELL
    cols, rows = int(math.ceil((x1 - x0) / CELL)), int(math.ceil((z1 - z0) / CELL))
    h = floor_heights(floor, x0, z0, cols, rows)
    blocked = np.isnan(h)
    pts = samples(solid, .08)
    for p in pts:
        i, j = int((p[0] - x0) / CELL), int((p[2] - z0) / CELL)
        if 0 <= i < cols and 0 <= j < rows and not np.isnan(h[j, i]) and h[j, i] + KNEE < p[1] < h[j, i] + HEAD:
            blocked[j, i] = True
    for box in meta.get('collision', {}).get('prop_blockers', []):
        c = blender_to_gltf(box['center'])
        sx, sy, sz = box['size']  # blender x, y, z
        yaw = math.radians(box.get('yaw_deg', 0))  # about blender z (glTF y)
        for j in range(rows):
            for i in range(cols):
                px, pz = x0 + (i + .5) * CELL, z0 + (j + .5) * CELL
                # into the box frame: blender (x, y) = glTF (x, -z)
                dx, dy = px - c[0], -(pz - c[2])
                lx = dx * math.cos(yaw) + dy * math.sin(yaw)
                ly = -dx * math.sin(yaw) + dy * math.cos(yaw)
                if abs(lx) <= sx / 2 and abs(ly) <= sy / 2:
                    floor_y = 0 if np.isnan(h[j, i]) else h[j, i]
                    if c[1] - sz / 2 < floor_y + HEAD and c[1] + sz / 2 > floor_y + KNEE:
                        blocked[j, i] = True
    # one cell of body radius
    grown = blocked.copy()
    grown[1:, :] |= blocked[:-1, :]
    grown[:-1, :] |= blocked[1:, :]
    grown[:, 1:] |= blocked[:, :-1]
    grown[:, :-1] |= blocked[:, 1:]
    blocked = grown
    # the spawn and what it reaches (steps up to STEP)
    spawn = next(p for p in meta['points'] if p['type'] == 'spawn')
    sp = spawn['pos_gltf']
    si, sj = int((sp[0] - x0) / CELL), int((sp[2] - z0) / CELL)
    if not (0 <= si < cols and 0 <= sj < rows) or blocked[sj, si]:
        free = np.argwhere(~blocked)
        if not len(free):
            raise SystemExit('no walkable floor')
        d = (free[:, 0] - sj) ** 2 + (free[:, 1] - si) ** 2
        sj, si = free[int(np.argmin(d))]
        print(f'spawn moved to the nearest walkable cell ({si}, {sj})')
    reach = np.zeros_like(blocked)
    reach[sj, si] = True
    q = deque([(sj, si)])
    while q:
        j, i = q.popleft()
        for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nj, ni = j + dj, i + di
            if 0 <= nj < rows and 0 <= ni < cols and not reach[nj, ni] and not blocked[nj, ni] and abs(h[nj, ni] - h[j, i]) <= STEP:
                reach[nj, ni] = True
                q.append((nj, ni))
    walk = np.argwhere(reach)
    centres = np.stack([x0 + (walk[:, 1] + .5) * CELL, z0 + (walk[:, 0] + .5) * CELL], 1)
    lines = ['HEROVR-HIDEOUT 1', f'name {meta.get("name", kind.upper())}',
             f'spawn {x0 + (si + .5) * CELL:.3f} {h[sj, si]:.3f} {z0 + (sj + .5) * CELL:.3f} {spawn.get("yaw_deg", 0):.2f}']
    notes = []
    for p in meta['points']:
        if p['type'] != 'interact':
            continue
        x, y, z = p['pos_gltf']
        near = float(np.min(np.hypot(centres[:, 0] - x, centres[:, 1] - z)))
        r = float(p.get('radius', 1.5))
        if near + .35 > r:
            notes.append(f"{p['id']}: radius {r} -> {near + .6:.2f} (the floor's nearest {near:.2f} m)")
            r = near + .6
        prompt = p.get('prompt', p['id']).replace('\n', ' ')
        lines.append(f"point {p['id']} {p.get('action', 'none')} {x:.3f} {y:.3f} {z:.3f} {r:.2f} {prompt}")
    killy = b.get('kill_z', -3.0)
    lines.append(f'bounds {x0:.3f} {z0:.3f} {x1:.3f} {z1:.3f}')
    lines.append(f'killy {killy:.2f}')
    lines.append(f'grid {x0:.3f} {z0:.3f} {CELL:.3f} {cols} {rows}')
    for j in range(rows):
        lines.append('row ' + ' '.join(str(int(round(h[j, i] * 100))) if reach[j, i] else '.' for i in range(cols)))
    open(os.path.join(out, 'room.txt'), 'w', newline='\n').write('\n'.join(lines) + '\n')
    strip_lights(room_glb, os.path.join(out, 'scene.glb'))
    shutil.copyfile(room_json, os.path.join(out, 'room.json'))
    try:
        from PIL import Image
        img = np.zeros((rows, cols, 3), np.uint8)
        img[blocked] = (90, 20, 20)
        img[reach] = (40, 160, 60)
        img[sj, si] = (255, 255, 0)
        for p in meta['points']:
            x, _, z = p['pos_gltf']
            i, j = int((x - x0) / CELL), int((z - z0) / CELL)
            if 0 <= i < cols and 0 <= j < rows:
                img[j, i] = (80, 160, 255)
        Image.fromarray(img).resize((cols * 4, rows * 4), Image.NEAREST).save(os.path.join(out, 'walk.png'))
    except ImportError:
        pass
    print(f'{kind}: {int(reach.sum())} walkable cells of {cols}x{rows} ({reach.sum() * CELL * CELL:.0f} m2)')
    for n in notes:
        print('  ' + n)


if __name__ == '__main__':
    if len(sys.argv) != 6:
        raise SystemExit(__doc__)
    build(*sys.argv[1:])
