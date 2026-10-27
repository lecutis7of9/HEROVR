"""Blender (4.x) side of the ENEMY FACTIONS kit, run on the PC like the suit pipeline:

  blender -b -P blender_prep_factions.py -- <model key> <download folder> <out folder> [--emblem emblem.png]

* imports the download (glTF or FBX), keeps the armature (hand1, hand3, hydra1, hydra2);
* hand2 (not rigged): fitted to hand1's armature and weighted with tools/cap_suit/blender/transfer_weights.py;
* decimates (Decimate COLLAPSE) to the model's `target` triangles (factions.MODELS);
* THE HAND: the outfit texture recoloured with factions.recolor (skin, eyes and metal kept);
* HYDRA: the weapons' objects dropped and our emblem added as two decal planes (chest, left shoulder)
  parented to the chest / shoulder bones;
* exports <out>/<key>.glb for Luna Engine IO / Overstrike, with CREDITS.txt.
The game-side swap of the enemies' models is CANDIDATE (no CONFIRMED enemy gear / spawn hook)."""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import factions  # noqa: E402


def run(key, src, out, emblem_png=None):
    import bpy
    m = factions.MODELS[key]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for root, _, files in os.walk(src):
        for f in files:
            p = os.path.join(root, f)
            if f.lower().endswith(('.gltf', '.glb')):
                bpy.ops.import_scene.gltf(filepath=p)
                break
            if f.lower().endswith('.fbx'):
                bpy.ops.import_scene.fbx(filepath=p)
                break
    meshes = [o for o in bpy.data.objects if o.type == 'MESH']
    if m['faction'] == 'HYDRA':
        for o in list(meshes):
            if any(w in o.name.lower() for w in ('m4', 'aks', 'rifle', 'gun', 'weapon', 'mag')):
                bpy.data.objects.remove(o)
        meshes = [o for o in bpy.data.objects if o.type == 'MESH']
    total = sum(len(o.data.polygons) for o in meshes) or 1
    ratio = min(1.0, m['target'] / float(total))
    for o in meshes:
        if ratio < .99:
            d = o.modifiers.new('decimate', 'DECIMATE')
            d.ratio = ratio
            bpy.context.view_layer.objects.active = o
            bpy.ops.object.modifier_apply(modifier=d.name)
    if m['faction'] == 'THE HAND':
        from PIL import Image
        for img in bpy.data.images:
            n = img.name.lower()
            if any(w in n for w in ('cloth', 'outfit', 'suit', 'body', 'gi', 'diffuse', 'basecolor')) and img.filepath:
                p = bpy.path.abspath(img.filepath)
                if os.path.exists(p):
                    factions.recolor(Image.open(p)).save(p)
                    img.reload()
    elif emblem_png:
        arm = next((o for o in bpy.data.objects if o.type == 'ARMATURE'), None)
        img = bpy.data.images.load(emblem_png)
        mat = bpy.data.materials.new('hydra_emblem_ours')
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get('Principled BSDF')
        tex = mat.node_tree.nodes.new('ShaderNodeTexImage')
        tex.image = img
        mat.node_tree.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
        mat.node_tree.links.new(tex.outputs['Alpha'], bsdf.inputs['Alpha'])
        mat.blend_method = 'CLIP'
        for words, size, side in ((('spine2', 'chest', 'spine_03', 'spine1'), .16, 0.0),
                                  (('leftarm', 'upperarm_l', 'l_upperarm', 'leftshoulder'), .09, 1.0)):
            bone = None
            if arm:
                bone = next((bn for bn in arm.data.bones if any(w in bn.name.lower().replace('_', '').replace('.', '') for w in words)), None)
            if not bone:
                print('EMBLEM: no %s bone, place it by hand' % words[0])
                continue
            head = arm.matrix_world @ bone.head_local
            bpy.ops.mesh.primitive_plane_add(size=size, location=(head.x + side * .03, head.y - .14, head.z))
            plane = bpy.context.active_object
            plane.rotation_euler = (1.5708, 0, 0)  # facing -Y (the model's front in Blender after glTF import)
            plane.data.materials.append(mat)
            plane.parent, plane.parent_type, plane.parent_bone = arm, 'BONE', bone.name
            plane.matrix_parent_inverse = (arm.matrix_world @ bone.matrix_local).inverted()
            print('EMBLEM on %s: CHECK its spot in the viewport (models differ)' % bone.name)
    os.makedirs(out, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(out, key + '.glb'))
    open(os.path.join(out, 'CREDITS.txt'), 'w').write(factions.credits())


if __name__ == '__main__':
    a = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    if len(a) < 3:
        print(__doc__)
        sys.exit(2)
    e = a[a.index('--emblem') + 1] if '--emblem' in a else None
    run(a[0], a[1], a[2], e)
