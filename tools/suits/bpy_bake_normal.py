"""0.0.78 PUNISHER HQ (owner, t1889u: keep its look after decimation): a high-to-low normal bake
(run under the bpy venv). The source (every triangle, its own tangent-space normal map on it) is baked onto
the decimated suit in the suit's own UVs, so the combined map carries both the source's painted detail and
the shape the decimation took away (folds, straps, buckles, the face). convert_suit.py then folds that map's
cavity into the base colour (the suit shader draws the base colour only).

  <bpy python> bpy_bake_normal.py high.npz low.npz normal.png out.png [size]

high.npz: tris (T,3,3) Y-up metres, uv (T,3,2) glTF (v down) - convert_suit's *_skin_in.npz;
low.npz: tris, uv, nrm - bpy_suit_skin's *_skin.npz (the same frame)."""
import sys
import numpy as np
import bpy


def to_b(p):
    p = np.asarray(p, np.float64)
    return np.stack([p[..., 0], -p[..., 2], p[..., 1]], -1)


def mesh(name, tris, uv, nrm=None):
    pts = to_b(tris.reshape(-1, 3))
    key = np.round(pts / 1e-6).astype(np.int64)
    if nrm is not None:  # keep the low mesh's own split normals: weld by place and normal
        key = np.concatenate([key, np.round(to_b(nrm.reshape(-1, 3)) * 1000).astype(np.int64)], 1)
    _, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    faces = inv.ravel().reshape(-1, 3)
    ok = (faces[:, 0] != faces[:, 1]) & (faces[:, 1] != faces[:, 2]) & (faces[:, 0] != faces[:, 2])
    me = bpy.data.meshes.new(name)
    me.from_pydata(pts[first].tolist(), [], faces[ok].tolist())
    me.update()
    lay = me.uv_layers.new(name='UV')
    u = uv[ok].reshape(-1, 2).astype(np.float64)
    lay.data.foreach_set('uv', np.stack([u[:, 0], 1 - u[:, 1]], 1).ravel())
    me.polygons.foreach_set('use_smooth', [True] * len(me.polygons))
    if nrm is not None:
        me.normals_split_custom_set(to_b(nrm[ok].reshape(-1, 3)).tolist())
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def main(high, low, normal, out, size=4096):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    h, l = np.load(high), np.load(low)
    hi = mesh('high', h['tris'], h['uv'])
    lu = l['uv'].astype(np.float64)
    span = np.ptp(lu, axis=1).max(1)
    fine = span < .08  # a triangle the decimation stretched over a UV seam paints a swath over other islands: not baked
    print('low triangles not baked (stretched over a UV seam):', int((~fine).sum()), flush=True)
    lo = mesh('low', l['tris'][fine], lu[fine], l['nrm'][fine])
    m = bpy.data.materials.new('high')
    m.use_nodes = True  # noqa (still needed in 5.x)
    nt = m.node_tree
    tex = nt.nodes.new('ShaderNodeTexImage')
    tex.image = bpy.data.images.load(normal)
    tex.image.colorspace_settings.name = 'Non-Color'
    nm = nt.nodes.new('ShaderNodeNormalMap')
    nt.links.new(tex.outputs['Color'], nm.inputs['Color'])
    nt.links.new(nm.outputs['Normal'], nt.nodes['Principled BSDF'].inputs['Normal'])
    hi.data.materials.append(m)
    img = bpy.data.images.new('baked', size, size, float_buffer=False)
    img.colorspace_settings.name = 'Non-Color'
    ml = bpy.data.materials.new('low')
    ml.use_nodes = True
    t2 = ml.node_tree.nodes.new('ShaderNodeTexImage')
    t2.image = img
    ml.node_tree.nodes.active = t2
    lo.data.materials.append(ml)
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'
    sc.cycles.samples = 1
    bk = sc.render.bake
    bk.use_selected_to_active, bk.cage_extrusion, bk.max_ray_distance, bk.margin = True, .012, .03, 8
    bk.normal_space = 'TANGENT'
    bpy.context.view_layer.update()
    hi.select_set(True)
    lo.select_set(True)
    bpy.context.view_layer.objects.active = lo
    print('selected', [o.name for o in bpy.context.selected_objects], 'active', bpy.context.view_layer.objects.active.name, flush=True)
    bpy.ops.object.bake(type='NORMAL')
    img.filepath_raw, img.file_format = out, 'PNG'
    img.save()
    print('baked', out, size, len(hi.data.polygons), '->', len(lo.data.polygons))


if __name__ == '__main__':
    a = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
    main(a[0], a[1], a[2], a[3], int(a[4]) if len(a) > 4 else 4096)
