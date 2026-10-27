"""CAPTAIN AMERICA suit: weight transfer and re-pose (Version 0.0.7; docs/CAP-SUIT-PIPELINE.md
step C). Run after fit_suit_to_rig.py, in the same Blender session or on its saved .blend:

  blender <fitted>.blend --python transfer_weights.py -- [--armature NAME] [--limit 4] [--save out.blend]

With the rig still in the fit's T-pose it (1) copies Peter's skin weights onto the suit from the game
meshes as they stand posed (nearest face, interpolated), (2) normalises them and keeps the --limit
largest a vertex (CANDIDATE: 4; the game's own limit is not documented on Luna Engine IO's page),
(3) carries the suit from the T-pose to the rig's rest pose (a copy of the rig, rest = the T-pose,
posed back onto the real rest), (4) clears the fit pose and binds the suit to the rig (an Armature
modifier, parented). Peter's meshes are left as they are; hide or delete them yourself before the
Luna export (see the doc). Prints a weight report."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
import cap_common as cc


def ctx(obj, mode=None):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    if mode and obj.mode != mode:
        bpy.ops.object.mode_set(mode=mode)


def weight_source(body):
    """One temporary mesh: the game meshes evaluated (posed), vertex groups kept."""
    dg = bpy.context.evaluated_depsgraph_get()
    parts = []
    for o in body:
        e = o.evaluated_get(dg)
        me = bpy.data.meshes.new_from_object(e, preserve_all_data_layers=True, depsgraph=dg)
        ob = bpy.data.objects.new('cap_weight_src_part', me)
        bpy.context.collection.objects.link(ob)
        ob.matrix_world = o.matrix_world
        for vg in o.vertex_groups:
            ob.vertex_groups.new(name=vg.name)
        parts.append(ob)
    ctx(parts[0])
    for p in parts[1:]:
        p.select_set(True)
    if len(parts) > 1:
        bpy.ops.object.join()
    src = bpy.context.view_layer.objects.active
    src.name = 'cap_weight_src'
    return src


def main():
    def extra(p):
        p.add_argument('--limit', type=int, default=4, help='weights a vertex (CANDIDATE default 4)')
        p.add_argument('--save', help='save the .blend here when done')
    a = cc.args(__doc__, extra)
    rig = cc.armature(a.armature)
    if not rig.get('cap_fit_pose'):
        raise SystemExit('run fit_suit_to_rig.py first (the rig is not in the fit pose)')
    suit = cc.suit_objects()
    body = cc.body_meshes(rig)
    deform = [b.name for b in rig.data.bones if b.use_deform]
    src = weight_source(body)
    report = dict(rig=rig.name, deform_bones=len(deform), suit={})
    for o in suit:
        for n in deform:
            if n not in o.vertex_groups:
                o.vertex_groups.new(name=n)
        m = o.modifiers.new('cap_weights', 'DATA_TRANSFER')
        m.object = src
        m.use_vert_data = True
        m.data_types_verts = {'VGROUP_WEIGHTS'}
        m.vert_mapping = 'POLYINTERP_NEAREST'
        m.layers_vgroup_select_src = 'ALL'
        m.layers_vgroup_select_dst = 'NAME'
        m.use_object_transform = True
        ctx(o, 'OBJECT')
        bpy.ops.object.modifier_apply(modifier=m.name)
        ctx(o, 'WEIGHT_PAINT')
        bpy.ops.object.vertex_group_limit_total(group_select_mode='ALL', limit=a.limit)
        bpy.ops.object.vertex_group_normalize_all(group_select_mode='ALL', lock_active=False)
        bpy.ops.object.mode_set(mode='OBJECT')
    bpy.data.objects.remove(src, do_unlink=True)
    # the T-pose -> the rest pose: a copy whose rest is the fit pose, posed onto the real rest
    tmp = rig.copy()
    tmp.data = rig.data.copy()
    tmp.name = 'cap_tpose_rig'
    tmp[cc.SUIT_TAG] = 1
    bpy.context.collection.objects.link(tmp)
    ctx(tmp, 'POSE')
    bpy.ops.pose.select_all(action='SELECT')
    bpy.ops.pose.armature_apply(selected=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    for pb in rig.pose.bones:
        pb.matrix_basis.identity()
    bpy.context.view_layer.update()
    for pb in tmp.pose.bones:
        c = pb.constraints.new('COPY_TRANSFORMS')
        c.target = rig
        c.subtarget = pb.name
    bpy.context.view_layer.update()
    for o in suit:
        m = o.modifiers.new('cap_repose', 'ARMATURE')
        m.object = tmp
        ctx(o, 'OBJECT')
        bpy.ops.object.modifier_apply(modifier=m.name)
    bpy.data.objects.remove(tmp, do_unlink=True)
    del rig['cap_fit_pose']
    for o in suit:
        mw = o.matrix_world.copy()
        o.parent = rig
        o.matrix_parent_inverse = rig.matrix_world.inverted()
        o.matrix_world = mw
        m = o.modifiers.new('Armature', 'ARMATURE')
        m.object = rig
        n = len(o.data.vertices)
        unweighted = sum(1 for v in o.data.vertices if not v.groups)
        most = max((len(v.groups) for v in o.data.vertices), default=0)
        used = sorted({o.vertex_groups[g.group].name for v in o.data.vertices for g in v.groups if g.weight > 0})
        report['suit'][o.name] = dict(vertices=n, unweighted=unweighted, most_weights_a_vertex=most, bones_used=len(used))
    print('CAP WEIGHT REPORT', report)
    if a.save:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(a.save))
    return report


if __name__ == '__main__':
    main()
