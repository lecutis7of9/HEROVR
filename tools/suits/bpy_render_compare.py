"""0.0.78 PUNISHER HQ (t1889u): lit close-ups, the source (every triangle, its base colour, normal, roughness and
metallic maps) beside the shipped suit as the game draws it (its decimated mesh and baked base-colour atlases,
lit by its vertex normals only: the suit shader has no normal / roughness / metallic maps).

  <bpy python> bpy_render_compare.py high.npz shipped.glb model_dir out_dir"""
import os
import sys
import math
import numpy as np
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bpy_bake_normal import mesh  # noqa: E402


def img(path, data):
    im = bpy.data.images.load(path)
    if data:
        im.colorspace_settings.name = 'Non-Color'
    return im


def source_material(d):
    m = bpy.data.materials.new('source')
    m.use_nodes = True
    nt = m.node_tree
    p = nt.nodes['Principled BSDF']
    def tex(name, data):
        t = nt.nodes.new('ShaderNodeTexImage')
        t.image = img(os.path.join(d, name), data)
        return t
    nt.links.new(tex('texture_0.png', False).outputs['Color'], p.inputs['Base Color'])
    nt.links.new(tex('texture_0_roughness.png', True).outputs['Color'], p.inputs['Roughness'])
    nt.links.new(tex('texture_0_metallic.png', True).outputs['Color'], p.inputs['Metallic'])
    nm = nt.nodes.new('ShaderNodeNormalMap')
    nt.links.new(tex('texture_0_normal.png', True).outputs['Color'], nm.inputs['Color'])
    nt.links.new(nm.outputs['Normal'], p.inputs['Normal'])
    return m


def main(high, shipped, d, out, mode='compare'):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    h = np.load(high)
    src = mesh('source', h['tris'], h['uv'])
    src.data.materials.append(source_material(d))
    bpy.ops.import_scene.gltf(filepath=shipped)
    ship = [o for o in bpy.context.scene.objects if o.type == 'MESH' and o.name != 'source']
    for o in ship:
        for slot in o.material_slots:
            p = slot.material.node_tree.nodes.get('Principled BSDF')
            if p:
                p.inputs['Roughness'].default_value, p.inputs['Metallic'].default_value = 1, 0
        o.modifiers.clear()
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'
    sc.cycles.samples = 48
    sc.render.resolution_x = sc.render.resolution_y = 640
    sc.world = bpy.data.worlds.new('w')
    sc.world.color = (.25, .25, .27)
    for loc, energy in (((-1.2, -2.0, 2.4), 260), ((1.6, -1.2, 1.4), 90), ((0, 2.0, 2.0), 120)):
        L = bpy.data.lights.new('l', 'AREA')
        L.energy, L.size = energy, 1.2
        o = bpy.data.objects.new('l', L)
        o.location = loc
        o.rotation_euler = (Vector((0, 0, 1.0)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
        sc.collection.objects.link(o)
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.lens = 50
    # Y-up suit frame -> Blender: (x, -z, y); the suit faces +z, so the camera stands at -y in Blender
    shots = {'head': ((0, 1.66, .02), .62), 'torso': ((0, 1.28, .0), 1.25), 'holster': ((-.24, .80, .03), .75)}
    if mode == 'preview':  # the shipped suit whole: front, side (from his right), and the holster
        shots = {'front': ((0, .92, 0), 3.4), 'side': ((0, .92, 0), 3.4), 'holster': ((-.24, .80, .03), .75)}
    for name, (at, dist) in shots.items():
        t = Vector((at[0], -at[2], at[1]))
        cam.location = t + Vector((-.25 * dist, -dist, .08 * dist)) if name == 'holster' else t + Vector((0, -dist, .05 * dist))
        if name == 'side':
            cam.location = t + Vector((-dist, 0, .05 * dist))
        cam.rotation_euler = (t - cam.location).to_track_quat('-Z', 'Y').to_euler()
        for which, show in (('source', [src]), ('shipped', ship)):
            if mode == 'preview' and which == 'source':
                continue
            for o in [src] + ship:
                o.hide_render = o not in show
            sc.render.filepath = os.path.join(out, '%s_%s.png' % (name, which))
            bpy.ops.render.render(write_still=True)
            print('rendered', sc.render.filepath, flush=True)


if __name__ == '__main__':
    a = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
    main(*a[:5])
