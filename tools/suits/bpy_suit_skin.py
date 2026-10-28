"""Build 9 SUITS, step 2 (run under the bpy venv): decimate a suit (Blender's collapse, the UV map and
seams kept, as tools/models072/bpy_decimate.py) and skin it to the 16-bone suit skeleton.

  <bpy python> bpy_suit_skin.py in.npz out.npz

in.npz: tris (T,3,3) Y-up metres; uv (T,3,2); mat (T,); target (int); joints (16,3) the rest joints
        (suit_wear.hpp's order); bones (16,2) each bone's two ends as indices into ends (E,3);
        skin (T,3,16) the artist's weights mapped to the 16 bones, or an empty array (unrigged).
The mesh is welded (1e-6) so it decimates as one surface; the artist's weights ride the decimation
as vertex groups (Blender interpolates them). Unrigged: Blender's automatic (bone heat) weights from
an armature built on the 16 bones; any vertex heat leaves unweighted gets its nearest bone.
Normals: smooth, with edges over 40 degrees kept sharp (armour stays crisp, cloth stays soft).
out.npz: tris, uv, mat, nrm (T,3,3), skin (T,3,16) normalised weights."""
import os
import sys
import numpy as np
import bpy, bmesh
from mathutils import Vector


def to_b(p):  # Y-up -> Blender Z-up
    p = np.asarray(p, np.float64)
    return np.stack([p[..., 0], -p[..., 2], p[..., 1]], -1)


def from_b(p):
    p = np.asarray(p, np.float64)
    return np.stack([p[..., 0], p[..., 2], -p[..., 1]], -1)


def seg_dist(p, a, b):
    ab = b - a
    t = np.clip(((p - a) @ ab) / max(ab @ ab, 1e-12), 0, 1)
    return np.linalg.norm(p - (a + t[:, None] * ab), axis=1)


def main(src, dst):
    d = np.load(src)
    tris, uv, mat, target = d['tris'], d['uv'], d['mat'], int(d['target'])
    ends, bones, skin = d['ends'], d['bones'], d['skin']
    rigged = skin.size > 0
    pts = to_b(tris.reshape(-1, 3))
    key = np.round(pts / 1e-6).astype(np.int64)
    uniq, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    inv = inv.ravel()
    verts = pts[first]
    faces = inv.reshape(-1, 3)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    me = bpy.data.meshes.new('suit')
    me.from_pydata(verts.tolist(), [], faces.tolist())
    me.update()
    lay = me.uv_layers.new(name='UV')
    lay.data.foreach_set('uv', uv.reshape(-1).astype(np.float32))
    me.polygons.foreach_set('material_index', mat.astype(np.int32))
    for _ in range(int(mat.max()) + 1):
        me.materials.append(bpy.data.materials.new('m'))
    ob = bpy.data.objects.new('suit', me)
    bpy.context.scene.collection.objects.link(ob)
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    groups = [ob.vertex_groups.new(name='b%d' % b) for b in range(16)]
    if rigged:
        vw = np.zeros((len(verts), 16))
        np.add.at(vw, inv, skin.reshape(-1, 16))
        cnt = np.bincount(inv, minlength=len(verts))[:, None]
        vw /= np.maximum(cnt, 1)
        for b in range(16):
            for i in np.nonzero(vw[:, b] > 1e-3)[0]:
                groups[b].add([int(i)], float(vw[i, b]), 'REPLACE')
    if len(faces) > target:
        mod = ob.modifiers.new('dec', 'DECIMATE')
        mod.decimate_type = 'COLLAPSE'
        mod.ratio = target / len(faces)
        mod.use_collapse_triangulate = True
        mod.use_symmetry = False
        bpy.ops.object.modifier_apply(modifier=mod.name)
    me = ob.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    for f in bm.faces:
        f.smooth = True
    import math
    for e in bm.edges:
        e.smooth = not (len(e.link_faces) == 2 and e.calc_face_angle(0) > math.radians(40))
    bm.to_mesh(me)
    bm.free()
    co = np.zeros(len(me.vertices) * 3)
    me.vertices.foreach_get('co', co)
    co = co.reshape(-1, 3)
    E = to_b(ends)
    if not rigged:
        arm = bpy.data.armatures.new('rig')
        ao = bpy.data.objects.new('rig', arm)
        bpy.context.scene.collection.objects.link(ao)
        bpy.context.view_layer.objects.active = ao
        bpy.ops.object.mode_set(mode='EDIT')
        for b in range(16):
            eb = arm.edit_bones.new('b%d' % b)
            h, t = E[bones[b, 0]], E[bones[b, 1]]
            if np.linalg.norm(t - h) < 1e-4:
                t = h + np.array([0, 0, .05])
            eb.head, eb.tail = Vector(h.tolist()), Vector(t.tolist())
        bpy.ops.object.mode_set(mode='OBJECT')
        for g in list(ob.vertex_groups):
            ob.vertex_groups.remove(g)
        bpy.ops.object.select_all(action='DESELECT')
        ob.select_set(True)
        ao.select_set(True)
        bpy.context.view_layer.objects.active = ao
        try:
            if os.environ.get('SUITS_NOHEAT') == '1':
                raise RuntimeError('SUITS_NOHEAT: the distance weights')
            bpy.ops.object.parent_set(type='ARMATURE_AUTO')
        except Exception as e:
            print('heat weights failed:', e)
    vw = np.zeros((len(me.vertices), 16))
    gi = {g.index: int(g.name[1:]) for g in ob.vertex_groups if g.name.startswith('b')}
    for v in me.vertices:
        for g in v.groups:
            if g.group in gi:
                vw[v.index, gi[g.group]] = g.weight
    lost = vw.sum(1) < 1e-4
    if lost.any():  # heat found no path (a closed armour piece away from its bone): the nearest bone
        P = co[lost]
        dist = np.stack([seg_dist(P, E[bones[b, 0]], E[bones[b, 1]]) for b in range(16)], 1)
        # the nearest bone, blended into the bones that share a joint with it over a few cm (no hard seam)
        near = dist.argmin(1)
        adjacent = np.array([[a == b or bool(set(bones[a]) & set(bones[b])) for b in range(16)] for a in range(16)])
        w = np.exp(-(dist - dist.min(1, keepdims=True)) / .02) * adjacent[near]
        vw[np.nonzero(lost)[0]] = w / w.sum(1, keepdims=True)
        # then smoothed along the surface (each a few rounds toward its neighbours' mean): no seams at the joints
        ev = np.zeros(len(me.edges) * 2, np.int64)
        me.edges.foreach_get('vertices', ev)
        ev = ev.reshape(-1, 2)
        idx = np.nonzero(lost)[0]
        for _ in range(6):
            acc = np.zeros_like(vw)
            cnt = np.zeros(len(vw))
            np.add.at(acc, ev[:, 0], vw[ev[:, 1]])
            np.add.at(acc, ev[:, 1], vw[ev[:, 0]])
            np.add.at(cnt, ev.ravel(), 1)
            mean = acc / np.maximum(cnt, 1)[:, None]
            vw[idx] = .5 * vw[idx] + .5 * np.where(cnt[idx, None] > 0, mean[idx], vw[idx])
    print('weights: %d vertices, %d by nearest bone' % (len(vw), int(lost.sum())))
    vw /= vw.sum(1, keepdims=True)
    me.calc_loop_triangles()
    lt = me.loop_triangles
    n = len(lt)
    li = np.zeros(n * 3, np.int64); lt.foreach_get('loops', li)
    vi = np.zeros(n * 3, np.int64); lt.foreach_get('vertices', vi)
    mi = np.zeros(n, np.int64); lt.foreach_get('material_index', mi)
    luv = np.zeros(len(me.loops) * 2, np.float32)
    me.uv_layers[0].data.foreach_get('uv', luv)
    cn = np.zeros(len(me.loops) * 3, np.float32)
    me.corner_normals.foreach_get('vector', cn)
    np.savez(dst, tris=from_b(co[vi]).reshape(-1, 3, 3), uv=luv.reshape(-1, 2)[li].reshape(-1, 3, 2), mat=mi,
             nrm=from_b(cn.reshape(-1, 3)[li]).reshape(-1, 3, 3), skin=vw[vi].reshape(-1, 3, 16), lost=int(lost.sum()))


if __name__ == '__main__':
    main(*sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:3])
