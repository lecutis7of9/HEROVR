#!/usr/bin/env python3
"""ULTIMATE ALLIANCE EXPANSION v5 - put the converted places (Stark Tower and Iron Man's Lab) into ONE room.

build_stark_tower.py builds each MUA1 map into its own folder (scene.glb, walk.glb, room.json, npcs.txt) with `--offset`
set, so the lab already stands far from the tower in the same room space (each place has its own city and sky box; the
offset keeps them apart, and the room's black sphere hides what is past 190 m). This script joins the folders: the
glTFs (no re-export: the buffers, materials and textures are appended as they are), the points and landings, the
bounds and the NPC hooks. The teleports between the places are the points' `to` landings (make_hideout.py).

  python combine_places.py <out> <place_dir> <place_dir> ...
"""
import copy
import json
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'hideout074'))
import glb  # noqa: E402


def _shift_textures(node, k, n_tex):
    """Add n_tex to every texture index below a material (baseColorTexture, normalTexture, extensions' too)."""
    if isinstance(node, dict):
        for key, val in node.items():
            if key.lower().endswith('texture') and isinstance(val, dict) and 'index' in val:
                val['index'] += n_tex
            else:
                _shift_textures(val, key, n_tex)
    elif isinstance(node, list):
        for v in node:
            _shift_textures(v, k, n_tex)


def merge_glb(paths, out):
    """Append each glb after the first: its buffer, views, accessors, images, textures, materials, meshes, nodes."""
    js, binary = glb.read(paths[0])
    binary = bytes(binary)
    for path in paths[1:]:
        j2, b2 = glb.read(path)
        pad = -len(binary) % 4
        binary += b'\0' * pad
        off = len(binary)
        binary += bytes(b2)
        n = {k: len(js.get(k, [])) for k in ('bufferViews', 'accessors', 'images', 'textures', 'samplers', 'materials', 'meshes', 'nodes')}
        for bv in j2.get('bufferViews', []):
            bv = copy.deepcopy(bv)
            bv['byteOffset'] = bv.get('byteOffset', 0) + off
            bv['buffer'] = 0
            js.setdefault('bufferViews', []).append(bv)
        for a in j2.get('accessors', []):
            a = copy.deepcopy(a)
            if 'bufferView' in a:
                a['bufferView'] += n['bufferViews']
            js.setdefault('accessors', []).append(a)
        for im in j2.get('images', []):
            im = copy.deepcopy(im)
            if 'bufferView' in im:
                im['bufferView'] += n['bufferViews']
            js.setdefault('images', []).append(im)
        js.setdefault('samplers', []).extend(copy.deepcopy(j2.get('samplers', [])))
        for t in j2.get('textures', []):
            t = copy.deepcopy(t)
            if 'source' in t:
                t['source'] += n['images']
            if 'sampler' in t:
                t['sampler'] += n['samplers']
            js.setdefault('textures', []).append(t)
        for m in j2.get('materials', []):
            m = copy.deepcopy(m)
            _shift_textures(m, '', n['textures'])
            js.setdefault('materials', []).append(m)
        for me in j2.get('meshes', []):
            me = copy.deepcopy(me)
            for p in me['primitives']:
                p['attributes'] = {k: v + n['accessors'] for k, v in p['attributes'].items()}
                if 'indices' in p:
                    p['indices'] += n['accessors']
                if 'material' in p:
                    p['material'] += n['materials']
            js.setdefault('meshes', []).append(me)
        for nd in j2.get('nodes', []):
            nd = copy.deepcopy(nd)
            if 'mesh' in nd:
                nd['mesh'] += n['meshes']
            if 'children' in nd:
                nd['children'] = [c + n['nodes'] for c in nd['children']]
            js.setdefault('nodes', []).append(nd)
        sc = j2['scenes'][j2.get('scene', 0)]
        js['scenes'][js.get('scene', 0)]['nodes'] += [c + n['nodes'] for c in sc['nodes']]
        js['extensionsUsed'] = sorted(set(js.get('extensionsUsed', [])) | set(j2.get('extensionsUsed', [])))
        if not js['extensionsUsed']:
            del js['extensionsUsed']
    if 'samplers' in js and not js['samplers']:
        del js['samplers']
    js['buffers'] = [{'byteLength': len(binary) + (-len(binary) % 4)}]
    glb.write(out, js, binary)


def combine(out, places):
    """places: the built folders (the first is the main place: its spawn is the room's). Writes into out."""
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    places = [Path(p) for p in places]
    merge_glb([p / 'scene.glb' for p in places], out / 'scene.glb')
    merge_glb([p / 'walk.glb' for p in places], out / 'walk.glb')
    rooms = [json.loads((p / 'room.json').read_text()) for p in places]
    room = copy.deepcopy(rooms[0])
    room['name'] = ' + '.join(r['name'] for r in rooms)
    room['places'] = [{'name': r['name'], 'asset': r.get('asset'), 'offset_xz': r.get('offset_xz', [0.0, 0.0])} for r in rooms]
    lo, hi = room['bounds']['playable_min'][:], room['bounds']['playable_max'][:]
    kill = room['bounds'].get('kill_z', -3.0)
    seen = {p['id'] for p in room['points']}
    for r in rooms[1:]:
        for i in range(2):
            lo[i] = min(lo[i], r['bounds']['playable_min'][i])
            hi[i] = max(hi[i], r['bounds']['playable_max'][i])
        kill = min(kill, r['bounds'].get('kill_z', kill))
        for p in r['points']:
            if p['type'] == 'spawn':  # one spawn: the main place's (the others are reached by a landing)
                continue
            if p['id'] in seen:
                raise SystemExit(f"point {p['id']} is in two places")
            seen.add(p['id'])
            room['points'].append(p)
    room['bounds'] = {'playable_min': lo, 'playable_max': hi, 'kill_z': kill}
    for p in room['points']:  # every teleport names a landing that exists
        if p.get('to') is not None and not any(q['id'] == p['to'] and q['type'] == 'landing' for q in room['points']):
            raise SystemExit(f"point {p['id']}: no landing {p['to']}")
    (out / 'room.json').write_text(json.dumps(room, indent=1))
    lines = []
    for p in places:
        for line in (p / 'npcs.txt').read_text().splitlines():
            if line.startswith('npc ') or not lines:
                lines.append(line)
    (out / 'npcs.txt').write_text('\n'.join(lines) + '\n')
    with open(out / 'report.txt', 'w') as f:
        for p in places:
            f.write(f'== {p.name} ==\n' + (p / 'report.txt').read_text())
    return room


if __name__ == '__main__':
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    combine(sys.argv[1], sys.argv[2:])
