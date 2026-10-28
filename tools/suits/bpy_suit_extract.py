"""Build 9 SUITS, step 1 (run under the bpy venv): a Sketchfab glTF suit as Blender sees it in its scene pose.

  <bpy python> bpy_suit_extract.py <scene.gltf or .glb> <out.npz> <out.json>

Every mesh object is evaluated (modifiers and armature included) as the
scene shows it (each armature in its imported pose, Sketchfab's standing figure); bone display
shapes are skipped. Written:
out.npz: tris (T,3,3) world metres, Blender Z-up turned back to glTF Y-up (x, z, -y);
         uv (T,3,2) the active UV map (glTF convention: v down from the top, as the images are);
         nrm (T,3,3) corner normals (Y-up); mat (T,) the material index into out.json's materials.
         joints / weights (T,3,4): the artist's skin (bone index into out.json's bones, -1 none) when rigged.
out.json: materials (name, image path or null, base colour factor, alpha mode), bones (name, head,
          tail in the same Y-up frame) of every armature, and the triangle count."""
import json, os, sys
import numpy as np
import bpy


def yup(a):
    a = np.asarray(a, np.float64)
    return np.stack([a[..., 0], a[..., 2], -a[..., 1]], -1)


def material_info(m):
    out = {'name': m.name if m else 'none', 'image': None, 'factor': [1, 1, 1, 1], 'alpha': 'OPAQUE'}
    if not m:
        return out
    try:
        out['alpha'] = {'CLIP': 'MASK', 'HASHED': 'BLEND', 'BLEND': 'BLEND'}.get(getattr(m, 'blend_method', 'OPAQUE'), 'OPAQUE')
    except Exception:
        pass
    if m.use_nodes and m.node_tree:
        for n in m.node_tree.nodes:
            if n.type == 'BSDF_PRINCIPLED':
                bc = n.inputs['Base Color']
                out['factor'] = list(bc.default_value)
                a = n.inputs.get('Alpha')
                if a is not None and a.is_linked:
                    out['alpha'] = 'MASK' if out['alpha'] == 'OPAQUE' else out['alpha']
                stack, seen = [bc], set()
                while stack:
                    s = stack.pop()
                    for l in s.links:
                        f = l.from_node
                        if id(f) in seen:
                            continue
                        seen.add(id(f))
                        if f.type == 'TEX_IMAGE' and f.image:
                            out['image'] = bpy.path.abspath(f.image.filepath_raw or f.image.filepath)
                            if not out['image'] or not os.path.exists(out['image']):
                                p = os.path.join(bpy.app.tempdir, 'img_%d.png' % len(seen))
                                f.image.save_render(p) if False else None
                            stack = []
                            break
                        if f.type == 'MIX' or f.type == 'MIX_RGB':
                            pass
                        for i in f.inputs:
                            stack.append(i)
                    if out['image']:
                        break
                if bc.is_linked and not out['image']:
                    pass
                break
    return out


def main(src, npz, js):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=src)
    arms = [o for o in bpy.data.objects if o.type == 'ARMATURE']
    # the scene as Sketchfab shows it: each armature in its imported POSE (a Sketchfab FBX rig's rest
    # pose is often lying down; the scene pose is the standing figure). Bone display shapes are skipped.
    shapes = {pb.custom_shape.name for a in arms for pb in a.pose.bones if pb.custom_shape}
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    mats, mat_index = [], {}
    T, U, N, M, J, W = [], [], [], [], [], []
    bone_names = [b.name for a in arms for b in a.pose.bones]
    bone_index = {n: i for i, n in enumerate(bone_names)}
    for o in bpy.data.objects:
        if o.type != 'MESH' or o.hide_render or o.name in shapes:
            continue
        ev = o.evaluated_get(dg)
        me = ev.to_mesh()
        me.calc_loop_triangles()
        lt = me.loop_triangles
        n = len(lt)
        if n == 0:
            ev.to_mesh_clear()
            continue
        vi = np.zeros(n * 3, np.int64); lt.foreach_get('vertices', vi)
        li = np.zeros(n * 3, np.int64); lt.foreach_get('loops', li)
        mi = np.zeros(n, np.int64); lt.foreach_get('material_index', mi)
        co = np.zeros(len(me.vertices) * 3); me.vertices.foreach_get('co', co)
        co = co.reshape(-1, 3)
        mw = np.array(o.matrix_world)
        co = co @ mw[:3, :3].T + mw[:3, 3]
        cn = np.zeros(len(me.loops) * 3, np.float32)
        try:
            me.corner_normals.foreach_get('vector', cn)
        except Exception:
            me.loops.foreach_get('normal', cn)
        cn = cn.reshape(-1, 3) @ np.linalg.inv(mw[:3, :3])
        cn /= np.maximum(np.linalg.norm(cn, axis=1, keepdims=True), 1e-12)
        if me.uv_layers.active:
            uv = np.zeros(len(me.loops) * 2, np.float32); me.uv_layers.active.data.foreach_get('uv', uv)
            uv = uv.reshape(-1, 2)
            uv[:, 1] = 1 - uv[:, 1]
        else:
            uv = np.zeros((len(me.loops), 2), np.float32)
        slots = [s.material for s in o.material_slots] or [None]
        gm = []
        for s in slots:
            key = s.name if s else 'none'
            if key not in mat_index:
                mat_index[key] = len(mats)
                mats.append(material_info(s))
            gm.append(mat_index[key])
        gm = np.array(gm)
        # the artist's skin weights (vertex groups named by bones), the 4 largest a vertex, when rigged
        jw = np.zeros((len(me.vertices), 4), np.int32) - 1
        ww = np.zeros((len(me.vertices), 4), np.float32)
        groups = {g.index: bone_index.get(g.name, -1) for g in o.vertex_groups}
        if groups and len(o.data.vertices) == len(me.vertices) and any(m.type == 'ARMATURE' for m in o.modifiers):
            for v in o.data.vertices:
                gs = sorted(((g.weight, groups.get(g.group, -1)) for g in v.groups if groups.get(g.group, -1) >= 0 and g.weight > 0),
                            reverse=True)[:4]
                for k, (w, b) in enumerate(gs):
                    jw[v.index, k] = b
                    ww[v.index, k] = w
        J.append(jw[vi].reshape(-1, 3, 4))
        W.append(ww[vi].reshape(-1, 3, 4))
        T.append(yup(co[vi].reshape(-1, 3, 3)))
        U.append(uv[li].reshape(-1, 3, 2))
        N.append(yup(cn[li].reshape(-1, 3, 3)))
        M.append(gm[np.clip(mi, 0, len(gm) - 1)])
        ev.to_mesh_clear()
    bones = []
    for a in arms:
        mw = np.array(a.matrix_world)
        for b in a.pose.bones:
            h = mw[:3, :3] @ np.array(b.head) + mw[:3, 3]
            t = mw[:3, :3] @ np.array(b.tail) + mw[:3, 3]
            bones.append({'name': b.name, 'parent': b.parent.name if b.parent else None,  # index = the weights' bone index
                          'head': yup(h).tolist(), 'tail': yup(t).tolist()})
    tris = np.concatenate(T)
    np.savez_compressed(npz, tris=tris, uv=np.concatenate(U), nrm=np.concatenate(N), mat=np.concatenate(M),
                        joints=np.concatenate(J), weights=np.concatenate(W))
    json.dump({'materials': mats, 'bones': bones, 'triangles': int(len(tris))}, open(js, 'w'), indent=1)


if __name__ == '__main__':
    a = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
    main(*a[:3])
