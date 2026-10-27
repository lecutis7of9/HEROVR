"""Version 0.0.72 REAL MODELS: Blender's collapse decimation, which keeps the UV map (seams and all),
for the converter (convert_models_072.py runs this under the bpy venv):

  <bpy python> bpy_decimate.py in.npz out.npz

in.npz: tris (T,3,3) float64, uv (T,3,2), mat (T,) int, target (int). Positions are welded (1e-6 m)
so the surface is one connected mesh; the UVs stay per corner (loops), so a seam is never smeared.
out.npz: the same three arrays for the decimated mesh (triangulated)."""
import sys
import numpy as np
import bpy, bmesh


def main(src, dst):
    d = np.load(src)
    tris, uv, mat, target = d['tris'], d['uv'], d['mat'], int(d['target'])
    pts = tris.reshape(-1, 3)
    key = np.round(pts / 1e-6).astype(np.int64)
    uniq, inv = np.unique(key, axis=0, return_inverse=True)
    inv = inv.ravel()
    verts = np.zeros((len(uniq), 3))
    np.add.at(verts, inv, pts)
    verts /= np.bincount(inv)[:, None]
    faces = inv.reshape(-1, 3)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    me = bpy.data.meshes.new('m')
    me.from_pydata(verts.tolist(), [], faces.tolist())
    me.update()
    lay = me.uv_layers.new(name='UV')
    lay.data.foreach_set('uv', uv.reshape(-1).astype(np.float32))
    me.polygons.foreach_set('material_index', mat.astype(np.int32))
    for _ in range(int(mat.max()) + 1):
        me.materials.append(bpy.data.materials.new('m'))
    ob = bpy.data.objects.new('o', me)
    bpy.context.scene.collection.objects.link(ob)
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    if len(faces) > target:
        mod = ob.modifiers.new('dec', 'DECIMATE')
        mod.decimate_type = 'COLLAPSE'
        mod.ratio = target / len(faces)
        mod.use_collapse_triangulate = True
        bpy.ops.object.modifier_apply(modifier=mod.name)
    me = ob.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.to_mesh(me)
    bm.free()
    me.calc_loop_triangles()
    lt = me.loop_triangles
    n = len(lt)
    li = np.zeros(n * 3, np.int64)
    lt.foreach_get('loops', li)
    vi = np.zeros(n * 3, np.int64)
    lt.foreach_get('vertices', vi)
    mi = np.zeros(n, np.int64)
    lt.foreach_get('material_index', mi)
    co = np.zeros(len(me.vertices) * 3)
    me.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3)
    luv = np.zeros(len(me.loops) * 2, np.float32)
    me.uv_layers[0].data.foreach_get('uv', luv)
    luv = luv.reshape(-1, 2)
    np.savez(dst, tris=co[vi].reshape(-1, 3, 3), uv=luv[li].reshape(-1, 3, 2), mat=mi)


if __name__ == '__main__':
    main(*sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:3])
