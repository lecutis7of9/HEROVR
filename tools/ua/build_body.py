"""ULTIMATE ALLIANCE EXPANSION - the player's body in UA places, Blender half (run by convert_ua.py, headless):

  blender -b --factory-startup --python build_body.py -- <skin.igb> <skeleton/anim.igb> <out.npz>

MUA1's own hero skin (Spider-Man's 0901 from the player's disc) imported with Kaiko's igb-blender (MUA1 PS2) on its
own skeleton, in its bind pose. Writes out.npz: corners (T*3, 3) game units z up, normals (T*3, 3), uv (T*3, 2)
(v down), image (T,), img<i> RGBA, bone names / parents / heads (game units), and each corner's bone weights
(corner_bones (T*3, 4) indices into the bone list, corner_weights (T*3, 4)). tools/ua/ua_body.py makes the suit.
"""
import bpy, sys, os, importlib, addon_utils
import numpy as np

argv = sys.argv[sys.argv.index('--') + 1:]
skin_igb, anim_igb, out = argv[0], argv[1], argv[2]
bpy.ops.wm.read_factory_settings(use_empty=True)
addon_utils.enable('igb_blender', default_set=True)
ai = importlib.import_module('igb_blender.actor.actor_import')
ref, _s, _a = ai.import_actor(bpy.context, anim_igb, None,
                              options={'game_preset': 'mua1_ps2', 'import_skins': False, 'import_animations': False})
arm, skins, _ = ai.import_actor(bpy.context, skin_igb, [('skin', skin_igb)],
                                options={'game_preset': 'mua1_ps2', 'import_animations': False})
skins = [x for t in skins for x in (t if isinstance(t, (tuple, list)) else [t]) if getattr(x, 'type', '') == 'MESH']
if arm is None:
    arm = next((o for o in bpy.data.objects if o.type == 'ARMATURE'), None)
names = [b.name for b in arm.data.bones]
parents = [names.index(b.parent.name) if b.parent else -1 for b in arm.data.bones]
heads = [tuple(arm.matrix_world @ b.head_local) for b in arm.data.bones]


def image_of(mat):
    if mat and mat.use_nodes:
        for n in mat.node_tree.nodes:
            if n.type == 'TEX_IMAGE' and n.image is not None:
                return n.image
    return None


images, img_ix = [], {}
corners, normals, uv, img_of_tri, cb, cw = [], [], [], [], [], []
dg = bpy.context.evaluated_depsgraph_get()
for s in skins:
    m = s.data
    m.calc_loop_triangles()
    uvl = m.uv_layers.active
    groups = {g.index: g.name for g in s.vertex_groups}
    e = s.evaluated_get(dg)
    em = e.to_mesh()
    mw = e.matrix_world
    co = [mw @ v.co for v in em.vertices]
    nm = mw.to_3x3().inverted().transposed()
    vn = [(nm @ v.normal).normalized() for v in em.vertices]
    for t in m.loop_triangles:
        im = image_of(m.materials[t.material_index] if t.material_index < len(m.materials) else None)
        if im is not None and im.name not in img_ix:
            img_ix[im.name] = len(images)
            images.append(im)
        img_of_tri.append(img_ix[im.name] if im is not None else -1)
        for vi, li in zip(t.vertices, t.loops):
            corners.append(tuple(co[vi]))
            normals.append(tuple(vn[vi]))
            u, v = uvl.data[li].uv if uvl else (0.0, 0.0)
            uv.append((u, 1.0 - v))
            ws = sorted(((g.weight, groups.get(g.group, '')) for g in m.vertices[vi].groups), reverse=True)[:4]
            bi = [names.index(n) if n in names else -1 for _w, n in ws] + [-1] * (4 - len(ws))
            wv = [w for w, _n in ws] + [0.0] * (4 - len(ws))
            cb.append(bi)
            cw.append(wv)
    e.to_mesh_clear()


def pixels(img):
    w, h = img.size
    a = np.asarray(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1]
    return (np.clip(a, 0, 1) * 255 + .5).astype(np.uint8)


np.savez_compressed(out, corners=np.asarray(corners, np.float64), normals=np.asarray(normals, np.float64),
                    uv=np.asarray(uv, np.float32), image=np.asarray(img_of_tri, np.int32),
                    bone_names=np.asarray(names), bone_parents=np.asarray(parents, np.int32),
                    bone_heads=np.asarray(heads, np.float64), corner_bones=np.asarray(cb, np.int32),
                    corner_weights=np.asarray(cw, np.float32),
                    **{f'img{i}': pixels(im) for i, im in enumerate(images)})
print(f'body: {len(img_of_tri)} triangles, {len(names)} bones, {len(images)} images, skins {len(skins)}', flush=True)
