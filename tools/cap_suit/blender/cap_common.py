"""Shared helpers for the CAPTAIN AMERICA suit's Blender scripts (Version 0.0.7).
Blender 5.0+ (Luna Engine IO v2.0.0 Beta needs 5.0+, 5.0.1 recommended); tested headless on 5.2.3 LTS
with tools/cap_suit/blender/selftest.py (a stand-in rig, not the game's: the game rig is only on the PC).

Bone names: the game's skeleton (hero_spiderman_advanced.model, as Luna Engine IO imports it) was not
available on the box, so the arm bones are found by shape (the longest outward bones on each side) and
can be named outright with --bones bone_map.json (see docs/CAP-SUIT-PIPELINE.md). CANDIDATE until the
script's printed bone report is checked against the real rig."""
import argparse, json, sys
import bpy
from mathutils import Matrix, Vector

SUIT_TAG = 'cap_suit'


def args(doc, extra=None):
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    p = argparse.ArgumentParser(description=doc)
    p.add_argument('--armature', help='the rig object (default: the armature with the most bones)')
    p.add_argument('--bones', help='bone_map.json: {"upperarm_l": "...", "forearm_l": "...", "hand_l": "...", (and _r)}')
    if extra:
        extra(p)
    # parse_known_args: both scripts can run in one Blender call sharing one argument list
    return p.parse_known_args(argv)[0]


def armature(name=None):
    if name:
        ob = bpy.data.objects.get(name)
        if not ob or ob.type != 'ARMATURE':
            raise SystemExit('no armature called %r' % name)
        return ob
    arms = [o for o in bpy.data.objects if o.type == 'ARMATURE' and not o.get(SUIT_TAG)]
    if not arms:
        raise SystemExit('no armature in the scene: import hero_spiderman_advanced.model with Luna Engine IO first')
    return max(arms, key=lambda o: len(o.data.bones))


def body_meshes(rig):
    """The game model's meshes: the ones deformed by `rig`, not the suit's."""
    out = []
    for o in bpy.data.objects:
        if o.type != 'MESH' or o.get(SUIT_TAG):
            continue
        if any(m.type == 'ARMATURE' and m.object == rig for m in o.modifiers) or o.parent == rig:
            out.append(o)
    return out


def suit_objects():
    return [o for o in bpy.data.objects if o.type == 'MESH' and o.get(SUIT_TAG)]


def world_bounds(objs, evaluated=True):
    dg = bpy.context.evaluated_depsgraph_get()
    lo, hi = Vector((1e9, 1e9, 1e9)), Vector((-1e9, -1e9, -1e9))
    for o in objs:
        e = o.evaluated_get(dg) if evaluated else o
        me = e.to_mesh()
        mw = e.matrix_world
        for v in me.vertices:
            p = mw @ v.co
            lo = Vector(map(min, lo, p))
            hi = Vector(map(max, hi, p))
        e.to_mesh_clear()
    return lo, hi


def arm_bones(rig, bone_map=None):
    """-> {'l': (upperarm, forearm, hand), 'r': (...)} bone names. The character faces -Y, so its
    left is +X. By shape: on each side, the upper arm is the longest bone starting beyond 6 % of the
    height from the middle and above the hips, pointing outward; the forearm is the longest bone starting
    near its tail; the hand the longest starting near the forearm's tail."""
    if bone_map:
        m = json.load(open(bone_map))
        return {s: tuple(m['%s_%s' % (k, s)] for k in ('upperarm', 'forearm', 'hand')) for s in 'lr'}
    bones = rig.data.bones
    zs = [b.head_local.z for b in bones] + [b.tail_local.z for b in bones]
    h = max(zs) - min(zs)
    z0 = min(zs)
    out = {}
    for s, sign in (('l', 1), ('r', -1)):
        def near(p, exclude):
            c = [b for b in bones if b.name not in exclude and (b.head_local - p).length < .035 * h and b.length > .03 * h]
            return max(c, key=lambda b: b.length) if c else None
        cand = [b for b in bones if sign * b.head_local.x > .06 * h and b.head_local.z > z0 + .55 * h and
                sign * (b.tail_local.x - b.head_local.x) > .5 * b.length and b.length > .1 * h]
        if not cand:
            raise SystemExit('could not find the %s upper arm by shape: write a bone_map.json (--bones)' % s)
        up = max(cand, key=lambda b: (b.length, b.head_local.z))
        fore = near(up.tail_local, {up.name})
        hand = near(fore.tail_local, {up.name, fore.name}) if fore else None
        if not fore or not hand:
            raise SystemExit('could not follow the %s arm past %s: write a bone_map.json (--bones)' % (s, up.name))
        out[s] = (up.name, fore.name, hand.name)
    return out


def set_pose_world_rotation(rig, pb, rot):
    """Turns pose bone `pb` by armature-space rotation `rot` about its head."""
    m = pb.matrix.copy()
    head = m.to_translation()
    pb.matrix = Matrix.Translation(head) @ rot.to_4x4() @ Matrix.Translation(-head) @ m
    bpy.context.view_layer.update()
