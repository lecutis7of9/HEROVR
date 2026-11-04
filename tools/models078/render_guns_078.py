"""HERO VR 0.0.78 PUNISHER ARSENAL: OFFLINE Cycles renders of the converted guns (Models/Punisher078), their own PBR maps
(base colour, normal, ORM), studio lit. Not an in-game capture.

  <blender python> tools/models078/render_guns_078.py <Models dir> <out dir> [samples]
Writes <out>/punisher078_<gun>_<view>_offline.png (gun: pistol, pump; view: right, three, open)."""
import math, os, sys
import bpy
from mathutils import Vector

models, out = sys.argv[-2], sys.argv[-1]
if sys.argv[-1].isdigit():
    models, out = sys.argv[-3], sys.argv[-2]
SAMPLES = int(sys.argv[-1]) if sys.argv[-1].isdigit() else 96
os.makedirs(out, exist_ok=True)
ROOT = os.path.join(models, 'Punisher078')
GUNS = {'pistol': (['Pistol', 'Pistol/Slide', 'Pistol/Mag'], .035, 'Pistol/Slide'),
        'pump': (['Pump', 'Pump/Forend', 'Pump/Shell'], .0692, 'Pump/Forend')}


def scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    s = bpy.context.scene
    s.render.engine = 'CYCLES'
    s.cycles.device = 'CPU'
    s.cycles.samples = SAMPLES
    s.cycles.use_denoising = True
    s.render.resolution_x, s.render.resolution_y = 1600, 900
    s.view_settings.view_transform = 'AgX'
    w = bpy.data.worlds.new('w')
    s.world = w
    w.use_nodes = True
    bg = w.node_tree.nodes['Background']
    bg.inputs[0].default_value = (.045, .05, .058, 1)
    bg.inputs[1].default_value = 1.
    return s


def light(name, loc, energy, size, colour=(1, 1, 1)):
    d = bpy.data.lights.new(name, 'AREA')
    d.energy, d.size, d.color = energy, size, colour
    o = bpy.data.objects.new(name, d)
    bpy.context.scene.collection.objects.link(o)
    o.location = loc
    o.rotation_euler = (Vector((0, 0, 0)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()


def load(part, offset=(0, 0, 0)):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, *part.split('/'), 'scene.glb'))
    for o in set(bpy.data.objects) - before:
        o.location = Vector(o.location) + Vector(offset)
    return [o for o in set(bpy.data.objects) - before if o.type == 'MESH']


def floor():
    bpy.ops.mesh.primitive_plane_add(size=6, location=(0, 0, -.12))
    m = bpy.data.materials.new('floor')
    m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (.02, .022, .025, 1)
    b.inputs['Roughness'].default_value = .55
    bpy.context.object.data.materials.append(m)


def camera(target, at, lens=50):
    c = bpy.data.cameras.new('c')
    c.lens = lens
    c.clip_start = .01
    o = bpy.data.objects.new('c', c)
    bpy.context.scene.collection.objects.link(o)
    o.location = at
    o.rotation_euler = (Vector(target) - Vector(at)).to_track_quat('-Z', 'Y').to_euler()
    bpy.context.scene.camera = o


for gun, (parts, travel, slide) in GUNS.items():
    length = .5 if gun == 'pistol' else .6
    for view in ('right', 'three', 'open'):
        s = scene()
        floor()
        objs = []
        for p in parts:
            off = (0, 0, 0)
            # Blender's import: file -z (the barrel) is +y, +y up is +z, +x right stays +x
            if view == 'open' and p == slide:
                off = (0, -travel, 0)
            loose = p.endswith('Shell') or (p.endswith('Mag') and view == 'open')
            if loose:
                continue
            objs += load(p, off)
        for p in parts:  # the loose magazine / shell: on the floor beside the gun, toward the camera
            if p.endswith('Shell') or (p.endswith('Mag') and view == 'open'):
                lo0 = min((o.matrix_world @ Vector(b)).z for o in objs for b in o.bound_box)
                got = load(p)
                for o in got:
                    o.rotation_euler = (0, 0, 0) if p.endswith('Mag') else (0, math.pi / 2, 0)
                bpy.context.view_layer.update()
                gl = min((o.matrix_world @ Vector(b)).z for o in got for b in o.bound_box)
                for o in got:
                    o.location = Vector(o.location) + Vector((.07, .05 if gun == 'pistol' else .12, lo0 - gl))
                bpy.context.view_layer.update()
                objs += got
        lo = Vector((min(v[i] for o in objs for v in [o.matrix_world @ Vector(b) for b in o.bound_box]) for i in range(3)))
        hi = Vector((max(v[i] for o in objs for v in [o.matrix_world @ Vector(b) for b in o.bound_box]) for i in range(3)))
        mid = (lo + hi) / 2
        bpy.data.objects['Plane'].location.z = lo.z - .002
        L = (hi - lo).length
        light('key', mid + Vector((.9, -.4, 1.1)) * L * 2, 60 * L * L * 4, L * 1.2, (1, .97, .92))
        light('fill', mid + Vector((-1.0, .3, .5)) * L * 2, 18 * L * L * 4, L * 1.5, (.85, .9, 1))
        light('rim', mid + Vector((.1, 1.2, .6)) * L * 2, 45 * L * L * 4, L * .8)
        light('top', mid + Vector((0, 0, 1.6)) * L * 2, 20 * L * L * 4, L * 2)
        if view == 'right':
            camera(mid, mid + Vector((1.0, 0, .08)) * L * 1.55, 50)
        elif view == 'three':
            camera(mid, mid + Vector((.75, .55, .35)) * L * 1.5, 50)
        else:
            camera(mid, mid + Vector((.85, -.35, .45)) * L * 1.55, 50)
        s.render.filepath = os.path.join(out, 'punisher078_%s_%s_offline.png' % (gun, view))
        bpy.ops.render.render(write_still=True)
        print('RENDERED', s.render.filepath)
