"""HERO VR 0.0.78 PUNISHER ARSENAL: the Blender half of convert_guns_078.py (run by it, under Blender's Python:
blender -b -P dump_gun_078.py -- <in.glb> <out.npz>, or bpy's own python). Imports one Sketchfab .glb, evaluates
it (skins posed), triangulates, and writes every triangle's corners - Blender's frame (z up, metres or the file's
units), corner normals, uv 0, the source material's index and its part label (the object's name, or for a skinned
mesh its rigid bone) - to an .npz. No geometry is changed here."""
import json, struct, sys
import bpy
import numpy as np

src, out = sys.argv[-2], sys.argv[-1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
blob = open(src, 'rb').read()
gl = json.loads(blob[20:20 + struct.unpack('<I', blob[12:16])[0]])
matIndex = {m.get('name', ''): i for i, m in enumerate(gl.get('materials', []))}
dg = bpy.context.evaluated_depsgraph_get()
P, N, U, M, L, labels = [], [], [], [], [], []
for o in bpy.context.scene.objects:
    if o.type != 'MESH' or o.name.startswith('Icosphere'):  # the 870's bone shape, not the gun
        continue
    e = o.evaluated_get(dg)
    me = e.to_mesh()
    me.calc_loop_triangles()
    mw = np.array(e.matrix_world)
    nw = np.linalg.inv(mw[:3, :3]).T
    co = np.zeros(len(me.vertices) * 3)
    me.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3) @ mw[:3, :3].T + mw[:3, 3]
    cn = np.zeros(len(me.loops) * 3)
    me.corner_normals.foreach_get('vector', cn)
    cn = cn.reshape(-1, 3) @ nw.T
    cn /= np.maximum(np.linalg.norm(cn, axis=1, keepdims=True), 1e-12)
    uv = np.zeros(len(me.loops) * 2)
    me.uv_layers[0].data.foreach_get('uv', uv)
    uv = uv.reshape(-1, 2)
    uv[:, 1] = 1 - uv[:, 1]  # back to glTF's v (down the image)
    lt = np.zeros(len(me.loop_triangles) * 3, np.int64)
    me.loop_triangles.foreach_get('loops', lt)
    vt = np.zeros(len(me.loop_triangles) * 3, np.int64)
    me.loop_triangles.foreach_get('vertices', vt)
    mi = np.zeros(len(me.loop_triangles), np.int64)
    me.loop_triangles.foreach_get('material_index', mi)
    slotMat = [matIndex.get(s.material.name if s.material else '', 0) for s in o.material_slots] or [0]
    if o.vertex_groups and o.find_armature():  # a rigid skin: each triangle the bone most of its corners follow
        W = np.zeros((len(o.data.vertices), len(o.vertex_groups)))
        for v in o.data.vertices:
            for g in v.groups:
                W[v.index, g.group] = g.weight
        dom = W.argmax(1)
        names = [g.name for g in o.vertex_groups]
        tv = dom[vt.reshape(-1, 3)]
        lab = [names[np.bincount(t).argmax()] for t in tv]
    else:
        lab = [o.name] * len(mi)
    P.append(co[vt]); N.append(cn[lt]); U.append(uv[lt])
    M.append(np.array([slotMat[min(k, len(slotMat) - 1)] for k in mi]))
    L += lab
    e.to_mesh_clear()
names = sorted(set(L))
np.savez_compressed(out, P=np.concatenate(P).astype(np.float64), N=np.concatenate(N).astype(np.float32),
                    U=np.concatenate(U).astype(np.float32), M=np.concatenate(M).astype(np.int32),
                    L=np.array([names.index(x) for x in L], np.int32), names=np.array(names))
print('DUMPED', out, len(L), 'triangles', {n: L.count(n) for n in names})
