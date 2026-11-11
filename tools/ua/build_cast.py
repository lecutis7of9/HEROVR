"""ULTIMATE ALLIANCE EXPANSION - Blender half of the hub's characters (run by convert_ua.py, headless):

  blender -b --factory-startup --python build_cast.py -- <plan.json> <out dir>

plan.json: {"scale": metres a game unit, "looks": [{"name", "skin_igb", "anim_igb", "anim", "frames"}]}. For each
look: the skin imported with Kaiko's igb-blender (MUA1 PS2), its own skeleton driven by the map's zone performance
(<anim> of actors/zone_<map>.igb, rebased as the add-on's briefing loader does, root kept on its mark), sampled at
<frames> even steps of the loop; the feet put on y 0. Writes <out>/<name>.npz: frames (F, T*3, 3) glTF metres in
the character's own space (facing +x, as the map's yaw turns it), uv (T*3, 2) (v down), image (T,), images, seconds.
"""
import bpy, sys, json, os, importlib, addon_utils
import numpy as np

argv = sys.argv[sys.argv.index('--') + 1:]
plan = json.load(open(argv[0]))
out_dir = argv[1]
os.makedirs(out_dir, exist_ok=True)
S = float(plan['scale'])


def fresh():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    addon_utils.enable('igb_blender', default_set=True)


fresh()
ai = importlib.import_module('igb_blender.actor.actor_import')
ab = importlib.import_module('igb_blender.actor.animation_builder')
from igb_blender.igb_format.igb_reader import IGBReader
from igb_blender.actor.sg_skeleton import extract_skeleton
from igb_blender.actor.sg_animation import extract_animations

perf_cache = {}


def performances(path):
    if path not in perf_cache:
        r = IGBReader(path); r.read()
        perf_cache[path] = {p.name: p for p in extract_animations(r, extract_skeleton(r))}
    return perf_cache[path]


def image_of(mat):
    if mat and mat.use_nodes:
        for n in mat.node_tree.nodes:
            if n.type == 'TEX_IMAGE' and n.image is not None:
                return n.image
    return None


def pixels(img):
    w, h = img.size
    a = np.asarray(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1]
    return (np.clip(a, 0, 1) * 255 + .5).astype(np.uint8)


report = []
for look in plan['looks']:
    fresh()
    ref, _s, _a = ai.import_actor(bpy.context, look['anim_igb'], None,
                                  options={'game_preset': 'mua1_ps2', 'import_skins': False, 'import_animations': False})
    arm, skins, _ = ai.import_actor(bpy.context, look['skin_igb'], [('skin', look['skin_igb'])],
                                    options={'game_preset': 'mua1_ps2', 'import_animations': False})
    skins = [x for t in skins for x in (t if isinstance(t, (tuple, list)) else [t]) if getattr(x, 'type', '') == 'MESH']
    pa = performances(look['anim_igb']).get(look['anim'])
    sc = bpy.context.scene
    fps = sc.render.fps / max(sc.render.fps_base, 1e-6)
    f0 = f1 = 0.0
    if pa is not None:
        act = ab.build_action(arm, pa, bind_pose=None, fps=None, anim_armature=ref, rebase_unanimated_parents=True,
                              relative_translation=True, root_motion_mode='vertical')
        if arm.animation_data is None:
            arm.animation_data_create()
        arm.animation_data.action = act
        f0, f1 = act.frame_range
    n = max(1, int(look.get('frames', 1))) if f1 > f0 else 1
    # the triangles once: uv, image per triangle (the posed frames keep this order)
    images, img_ix, uv, img_of_tri = [], {}, [], []
    for s in skins:
        m = s.data
        m.calc_loop_triangles()
        uvl = m.uv_layers.active
        for t in m.loop_triangles:
            im = image_of(m.materials[t.material_index] if t.material_index < len(m.materials) else None)
            if im is not None and im.name not in img_ix:
                img_ix[im.name] = len(images)
                images.append(im)
            img_of_tri.append(img_ix[im.name] if im is not None else -1)
            for li in t.loops:
                u, v = uvl.data[li].uv if uvl else (0.0, 0.0)
                uv.append((u, 1.0 - v))
    frames = []
    for k in range(n):
        t = f0 + (f1 - f0) * k / n
        sc.frame_set(int(t), subframe=t - int(t))
        dg = bpy.context.evaluated_depsgraph_get()
        corners = []
        for s in skins:
            e = s.evaluated_get(dg)
            m = e.to_mesh()
            m.calc_loop_triangles()
            mw = e.matrix_world
            co = [mw @ v.co for v in m.vertices]
            for tri in m.loop_triangles:
                for vi in tri.vertices:
                    corners.append(tuple(co[vi]))
            e.to_mesh_clear()
        frames.append(np.asarray(corners, dtype=np.float64))
    if any(len(f) != len(uv) for f in frames):
        raise SystemExit(f"{look['name']}: the posed triangles differ from the rest mesh's")
    F = np.stack(frames)                    # (n, T*3, 3) game units, z up, facing +x
    F[:, :, 2] -= F[:, :, 2].min()          # the feet on the floor (over the whole loop)
    G = np.empty_like(F)
    G[:, :, 0], G[:, :, 1], G[:, :, 2] = F[:, :, 0] * S, F[:, :, 2] * S, -F[:, :, 1] * S
    np.savez_compressed(os.path.join(out_dir, look['name'] + '.npz'), frames=G.astype(np.float32),
                        uv=np.asarray(uv, dtype=np.float32), image=np.asarray(img_of_tri, dtype=np.int32),
                        seconds=np.float32((f1 - f0) / fps if f1 > f0 else 0.0),
                        **{f'img{i}': pixels(im) for i, im in enumerate(images)})
    h = G[0, :, 1].max()
    report.append(f"{look['name']}: {len(uv) // 3} triangles, {n} frames over {(f1 - f0) / fps:.2f} s, "
                  f"{len(images)} images, {h:.2f} m tall" + ('' if pa is not None else f" (no {look['anim']}: rest pose)"))
    print(report[-1], flush=True)
open(os.path.join(out_dir, 'cast_report.txt'), 'w').write('\n'.join(report) + '\n')
