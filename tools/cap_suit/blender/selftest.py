"""Headless self-test of fit_suit_to_rig.py + transfer_weights.py on a STAND-IN rig (an A-pose
skeleton with Spider-Man-like proportions and a skinned stand-in body; not the game's rig, which is only
on the user's PC). Checks: the suit is scaled and stood on the body; the rig's arms are posed onto the
suit's T-pose; every suit vertex gets weights (at most --limit, summing to 1); the hands' vertices go to
the hand bones; the suit ends in the rig's rest (A) pose; posing the rig back to the T-pose returns the
suit to its fitted shape.

  python selftest.py <CapSuit folder>      (the bpy module, Blender 5.x)   or
  blender -b --python selftest.py -- <CapSuit folder>"""
import math, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import bpy
from mathutils import Vector

ARM_DOWN = math.radians(45)


def build_rig():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    arm = bpy.data.armatures.new('standin_rig')
    rig = bpy.data.objects.new('standin_rig', arm)
    bpy.context.collection.objects.link(rig)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode='EDIT')
    eb = arm.edit_bones

    def bone(name, h, t, parent=None, deform=True):
        b = eb.new(name)
        b.head, b.tail = Vector(h), Vector(t)
        b.use_deform = deform
        if parent:
            b.parent = eb[parent]
        return b
    bone('root', (0, 0, 0), (0, .2, 0), deform=False)
    bone('pelvis', (0, 0, 1.0), (0, 0, 1.15), 'root')
    bone('spine', (0, 0, 1.15), (0, 0, 1.45), 'pelvis')
    bone('neck', (0, 0, 1.45), (0, 0, 1.6), 'spine')
    bone('head', (0, 0, 1.6), (0, 0, 1.8), 'neck')
    for s, x in (('l', 1), ('r', -1)):
        bone('clavicle_' + s, (x * .02, 0, 1.45), (x * .17, 0, 1.47), 'spine')
        sh = Vector((x * .18, 0, 1.46))
        d = Vector((x * math.cos(ARM_DOWN), 0, -math.sin(ARM_DOWN)))
        bone('upperarm_' + s, sh, sh + d * .29, 'clavicle_' + s)
        bone('upperarm_twist_' + s, sh + d * .1, sh + d * .2, 'upperarm_' + s)
        bone('forearm_' + s, sh + d * .29, sh + d * .55, 'upperarm_' + s)
        bone('hand_' + s, sh + d * .55, sh + d * .64, 'forearm_' + s)
        bone('thigh_' + s, (x * .1, 0, 1.0), (x * .1, 0, .55), 'pelvis')
        bone('calf_' + s, (x * .1, 0, .55), (x * .1, 0, .1), 'thigh_' + s)
        bone('foot_' + s, (x * .1, 0, .1), (x * .1, -.14, .02), 'calf_' + s)
    bpy.ops.object.mode_set(mode='OBJECT')
    # a stand-in body: a tube along each deform bone, weighted rigidly to it
    parts = []
    radius = dict(pelvis=.16, spine=.17, neck=.06, head=.1, clavicle=.05, upperarm=.05, forearm=.04, hand=.035,
                  thigh=.08, calf=.06, foot=.045)
    for b in arm.bones:
        key = b.name.split('_')[0]
        if not b.use_deform or key not in radius or 'twist' in b.name:
            continue
        v = b.tail_local - b.head_local
        bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=radius[key], depth=v.length,
                                            location=(b.head_local + b.tail_local) / 2)
        ob = bpy.context.active_object
        ob.rotation_mode = 'QUATERNION'
        ob.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(v.normalized())
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        parts.append(ob)
    for o in bpy.context.selected_objects:
        o.select_set(False)
    for p in parts:
        p.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    body = bpy.context.active_object
    body.name = 'standin_body'
    # smooth skin weights: the 3 nearest deform bones, falling off with the distance to each
    segs = [(b.name, b.head_local, b.tail_local) for b in arm.bones if b.use_deform]
    groups = {n: body.vertex_groups.new(name=n) for n, _, _ in segs}
    for v in body.data.vertices:
        ds = []
        for n, h, t in segs:
            d = t - h
            u = max(0.0, min(1.0, (v.co - h).dot(d) / d.length_squared))
            ds.append(((v.co - (h + d * u)).length, n))
        ds.sort()
        w = [math.exp(-(d - ds[0][0]) / .03) for d, _ in ds[:3]]
        for (d, n), x in zip(ds[:3], w):
            groups[n].add([v.index], x / sum(w), 'REPLACE')
    body.parent = rig
    m = body.modifiers.new('Armature', 'ARMATURE')
    m.object = rig
    return rig, body


def run(script, argv):
    sys.argv = ['blender', '--'] + argv
    ns = {'__name__': 'cap_selftest', '__file__': os.path.join(HERE, script)}
    exec(compile(open(os.path.join(HERE, script)).read(), script, 'exec'), ns)
    return ns['main']()


def coords(o):
    dg = bpy.context.evaluated_depsgraph_get()
    e = o.evaluated_get(dg)
    me = e.to_mesh()
    out = [e.matrix_world @ v.co for v in me.vertices]
    e.to_mesh_clear()
    return out


def main(capsuit):
    rig, body = build_rig()
    fit = run('fit_suit_to_rig.py', ['--suit', os.path.join(capsuit, 'cap_suit.gltf')])
    import cap_common as cc
    suit = cc.suit_objects()
    fails = []
    check = lambda ok, what: (print(('PASS ' if ok else 'FAIL ') + what), ok or fails.append(what))
    lo, hi = cc.world_bounds(suit, evaluated=False)
    blo, bhi = cc.world_bounds([body])
    check(abs((hi.z - lo.z) - (bhi.z - blo.z)) < .005 and abs(lo.z - blo.z) < .005, 'suit scaled to the body and on its feet (%.3f m)' % (hi.z - lo.z))
    check(fit['arm_bones'] == {'l': ('upperarm_l', 'forearm_l', 'hand_l'), 'r': ('upperarm_r', 'forearm_r', 'hand_r')},
          'arm bones found by shape %s' % fit['arm_bones'])
    for s in 'lr':
        t = Vector(fit['arms'][s]['turned_to'])
        w = Vector(fit['arms'][s]['target_dir'])
        check(t.angle(w) < math.radians(2), 'the %s arm posed onto the suit line (%.2f deg)' % (s, math.degrees(t.angle(w))))
    tpose = {o.name: [v.co.copy() for v in o.data.vertices] for o in suit}
    hand_pose = {s: rig.pose.bones['hand_' + s].matrix.copy() for s in 'lr'}
    rep = run('transfer_weights.py', ['--limit', '4'])
    for name, r in rep['suit'].items():
        check(r['unweighted'] == 0 and r['most_weights_a_vertex'] <= 4, '%s: every vertex weighted, at most 4 (%s)' % (name, r))
    sums = [sum(g.weight for g in v.groups) for o in suit for v in o.data.vertices]
    check(all(abs(x - 1) < 1e-3 for x in sums), 'weights normalised')
    # the far left of the T-pose (the left hand) now hangs in the rest (A) pose by hand_l
    for s, x in (('l', 1), ('r', -1)):
        o, i = max(((o, i) for o in suit for i, c in enumerate(tpose[o.name])), key=lambda p: x * p[0].matrix_world.__matmul__(tpose[p[0].name][p[1]]).x)
        v = o.data.vertices[i]
        top = max(v.groups, key=lambda g: g.weight)
        check(o.vertex_groups[top.group].name == 'hand_' + s, 'the %s fingertip is weighted to hand_%s (%s)' % (s, s, o.vertex_groups[top.group].name))
        sh = rig.data.bones['upperarm_' + s].head_local
        rest = rig.data.bones['hand_' + s].tail_local - sh
        p = o.matrix_world @ v.co
        want = rig.data.bones['hand_' + s].matrix_local @ hand_pose[s].inverted() @ (o.matrix_world @ tpose[o.name][i])
        check((p - want).length < .02, 'the %s fingertip carried to the rest (A) pose with its hand (%.4f m off)' % (s, (p - want).length))
        reach = (p - sh).length / rest.length
        check(abs(reach - 1) < .12, 'the %s arm fitted to the rig\'s length (fingertip at %.2f of the rest reach)' % (s, reach))
    # posing the rig back to the T-pose returns the suit to its fitted shape
    rig['cap_fit_pose'] = 0
    mw = rig.matrix_world
    for s in 'lr':
        up, fore, hand = (rig.pose.bones[n] for n in fit['arm_bones'][s])
        want = Vector(fit['arms'][s]['target_dir'])
        for pb in (up, fore):
            have = ((mw @ hand.head) - (mw @ pb.head)).normalized()
            cc.set_pose_world_rotation(rig, pb, have.rotation_difference(want).to_matrix())
    worst, mean, n = 0, 0, 0
    for o in suit:
        now = coords(o)
        for a, b in zip(now, tpose[o.name]):
            d = (a - b).length
            worst, mean, n = max(worst, d), mean + d, n + 1
    check(mean / n < .01, 'T-pose round trip: mean %.4f m, worst %.4f m' % (mean / n, worst))
    print('SELFTEST', 'PASS' if not fails else 'FAIL %d' % len(fails))
    return not fails


if __name__ == '__main__':
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
    sys.exit(0 if main(argv[0]) else 1)
