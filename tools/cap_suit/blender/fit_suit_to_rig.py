"""CAPTAIN AMERICA suit: auto-fit (Version 0.0.7; docs/CAP-SUIT-PIPELINE.md step B).

Open Blender 5.0+ with the game model imported by Luna Engine IO (hero_spiderman_advanced.model: the
rig plus Peter's meshes), then run:

  blender <scene>.blend --python fit_suit_to_rig.py -- --suit <CapSuit>/cap_suit.gltf [--armature NAME] [--bones bone_map.json]

It (1) imports the prepped suit (prep_suit.py's cap_suit.gltf, tagged 'cap_suit'), (2) scales it to the
game body's height and stands it on the body's feet, centred on it, (3) poses the rig's arms straight
out along the suit's T-pose arms (the pose is temporary: transfer_weights.py uses it, then clears it),
and prints a fit report (heights, arm lengths, the arm bones it used). Nothing is exported."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
from mathutils import Vector
import cap_common as cc


def import_suit(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == 'MESH']
    for o in new:
        if o.type != 'MESH':
            continue
        o[cc.SUIT_TAG] = 1
    # flatten any import parents (the glTF importer can add an empty), keeping the world transform
    for o in meshes:
        mw = o.matrix_world.copy()
        o.parent = None
        o.matrix_world = mw
    for o in new:
        if o.type != 'MESH':
            bpy.data.objects.remove(o, do_unlink=True)
    return meshes


def suit_arm_dir(meshes, side):
    """The suit's T-pose arm direction (shoulder -> wrist) on one side, from its vertices: the
    centroids of slices along X between 45 % and 85 % of the half span, fitted by a line."""
    pts = []
    for o in meshes:
        mw = o.matrix_world
        pts += [mw @ v.co for v in o.data.vertices]
    sign = 1 if side == 'l' else -1
    half = max(sign * p.x for p in pts)
    cents = []
    for k in range(8):
        a, b = half * (.45 + .05 * k), half * (.5 + .05 * k)
        sl = [p for p in pts if a <= sign * p.x < b]
        if sl:
            cents.append(sum(sl, Vector()) / len(sl))
    d = cents[-1] - cents[0]
    return d.normalized(), cents[0]


def main():
    def extra(p):
        p.add_argument('--suit', required=True, help="prep_suit.py's cap_suit.gltf")
        p.add_argument('--no-arm-fit', action='store_true', help="keep the suit's own arm length")
    a = cc.args(__doc__, extra)
    rig = cc.armature(a.armature)
    body = cc.body_meshes(rig)
    if not body:
        raise SystemExit('no mesh is deformed by %s: import the model with its meshes (Luna Engine IO)' % rig.name)
    # the rig at rest for the measure
    for pb in rig.pose.bones:
        pb.matrix_basis.identity()
    bpy.context.view_layer.update()
    blo, bhi = cc.world_bounds(body)
    suit = import_suit(a.suit)
    slo, shi = cc.world_bounds(suit, evaluated=False)
    k = (bhi.z - blo.z) / (shi.z - slo.z)
    for o in suit:
        o.matrix_world = o.matrix_world.copy()
        o.scale = o.scale * k
        o.location = o.location * k
    bpy.context.view_layer.update()
    slo, shi = cc.world_bounds(suit, evaluated=False)
    move = Vector(((blo.x + bhi.x) / 2 - (slo.x + shi.x) / 2, (blo.y + bhi.y) / 2 - (slo.y + shi.y) / 2, blo.z - slo.z))
    for o in suit:
        o.location += move
    bpy.context.view_layer.update()
    for o in suit:  # bake the transform into the vertices (the export wants identity objects)
        o.data.transform(o.matrix_world)
        o.matrix_world.identity()
    bones = cc.arm_bones(rig, a.bones)
    report = dict(rig=rig.name, body_meshes=[o.name for o in body], suit=[o.name for o in suit],
                  body_height_m=round(bhi.z - blo.z, 4), suit_scale=round(k, 5), arm_bones=bones, arms={})
    mw = rig.matrix_world
    inv = mw.inverted()
    for s in 'lr':
        want, _ = suit_arm_dir(suit, s)
        up, fore, hand = (rig.pose.bones[n] for n in bones[s])
        for pb in (up, fore):  # the whole arm onto the suit's line, then the elbow straight
            have = ((mw @ hand.head) - (mw @ pb.head)).normalized()
            rot = (inv.to_3x3() @ have).rotation_difference(inv.to_3x3() @ want)
            cc.set_pose_world_rotation(rig, pb, rot.to_matrix())
        shoulder, wrist = mw @ up.head, mw @ hand.head
        report['arms'][s] = dict(target_dir=[round(c, 3) for c in want], rig_arm_m=round((wrist - shoulder).length, 4),
                                 turned_to=[round(c, 3) for c in (wrist - shoulder).normalized()])
    # the arm length: Peter's posed arms reach (the body meshes in the fit pose) vs the suit's; the
    # suit's arm (beyond the shoulder, eased in over the armpit) is stretched along X to match
    bpy.context.view_layer.update()
    plo, phi = cc.world_bounds(body)
    ulo, uhi = cc.world_bounds(suit, evaluated=False)
    for s, sign in (('l', 1), ('r', -1)):
        sh = sign * (mw @ rig.pose.bones[bones[s][0]].head).x
        reach_body, reach_suit = (phi.x if sign > 0 else -plo.x), (uhi.x if sign > 0 else -ulo.x)
        r = (reach_body - sh) / max(reach_suit - sh, 1e-6)
        report['arms'][s].update(shoulder_x_m=round(sh, 4), body_reach_m=round(reach_body, 4), suit_reach_m=round(reach_suit, 4),
                                 stretch=1.0 if a.no_arm_fit else round(r, 4))
        if a.no_arm_fit or abs(r - 1) < .005:
            continue
        ease0 = .7 * sh
        for o in suit:
            for v in o.data.vertices:
                x = sign * v.co.x
                if x <= ease0:
                    continue
                if x >= sh:
                    nx = sh + (x - sh) * r
                else:  # inside the armpit: no stretch yet, a smooth start
                    nx = x
                v.co.x = sign * nx
        for o in suit:
            o.data.update()
    rig['cap_fit_pose'] = 1
    print('CAP FIT REPORT', report)
    return report


if __name__ == '__main__':
    main()
