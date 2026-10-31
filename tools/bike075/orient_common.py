# HERO VR 0.0.75 MOTORCYCLE: the shared orient step (Made by Agent Mulder). Blender 4.2 headless.
# Imports the Low-Poly Sci-Fi Motorcycle (Mert Tetik, CC BY 4.0) and puts it in the vehicles' frame (Blender
# axes: +X right, +Y forward, +Z up; exported glTF: x right, y up, -z forward), the ground under the axles' middle
# at the origin, scaled so the tyres are TYRE_D metres across. Its frame is MEASURED from the model: the two tyres
# (the Wheels mesh's two 1110-vertex rings) give the axle (their thinnest direction), the line between their
# centres gives forward (the rear tyre -> the front one, the front the Front mesh's side), up is the axle x
# forward turned toward the seat.
import bpy, bmesh
import numpy as np
from mathutils import Vector as V, Matrix

TYRE_D = 0.64   # metres: a sport bike's tyre (CANDIDATE look; sets the scale)

def comps_of(o):
    bm = bmesh.new(); bm.from_mesh(o.data); bm.verts.ensure_lookup_table()
    seen = set(); out = []
    for v in bm.verts:
        if v.index in seen: continue
        st = [v]; c = []; seen.add(v.index)
        while st:
            x = st.pop(); c.append(x.index)
            for e in x.link_edges:
                y = e.other_vert(x)
                if y.index not in seen: seen.add(y.index); st.append(y)
        out.append(c)
    bm.free()
    return out

def world_pts(o):
    return np.array([tuple(o.matrix_world @ v.co) for v in o.data.vertices])

def load(src):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=src)
    bpy.context.view_layer.update()
    obs = {o.data.materials[0].name: o for o in bpy.context.scene.objects if o.type == 'MESH'}
    W = obs['Wheels']; P = world_pts(W)
    rings = [c for c in comps_of(W) if len(c) == 1110]
    assert len(rings) == 2, 'expected two tyre rings'
    cs, ax, rad = [], None, 0
    for c in rings:
        q = P[c]; m = q.mean(0); _, _, vt = np.linalg.svd(q - m); a = vt[2]
        d = q - m; r = np.linalg.norm(d - np.outer(d @ a, a), axis=1).max()
        cs.append(m); ax = a; rad = r
    front_c = world_pts(obs['Front']).mean(0)
    f_i = 0 if np.linalg.norm(cs[0] - front_c) < np.linalg.norm(cs[1] - front_c) else 1
    cf, cr = cs[f_i], cs[1 - f_i]
    fwd = (cf - cr) / np.linalg.norm(cf - cr)
    up = np.cross(ax, fwd); up /= np.linalg.norm(up)
    seat_c = world_pts(obs['Seat']).mean(0)
    if (seat_c - (cf + cr) / 2) @ up < 0: up = -up
    right = np.cross(fwd, up)
    k = TYRE_D / (2 * rad)
    mid = (cf + cr) / 2 - up * rad
    R = Matrix(((*right, 0), (*fwd, 0), (*up, 0), (0, 0, 0, 1)))
    M = Matrix.Scale(k, 4) @ R @ Matrix.Translation(V(tuple(-mid)))
    for o in bpy.context.scene.objects:
        if o.type == 'MESH':
            mw = o.matrix_world.copy()
            o.parent = None
            o.matrix_world = M @ mw
    for o in [o for o in bpy.context.scene.objects if o.type != 'MESH']:
        bpy.data.objects.remove(o)
    bpy.context.view_layer.update()
    for o in obs.values():
        bpy.context.view_layer.objects.active = o
        o.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    info = dict(scale=k, wheelbase=float(np.linalg.norm(cf - cr) * k), tyre=TYRE_D)
    return obs, info
