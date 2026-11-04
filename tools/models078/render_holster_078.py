"""HERO VR 0.0.78 PUNISHER ARSENAL preview: the new Punisher model's own right-leg holster (the one the suppressed
pistol goes in; no holster is modelled or drawn by the mod), lit (key, fill, rim; Cycles).
  /workspace/bpy-venv/bin/python render_holster_078.py <model.glb> <out folder>"""
import math, sys
import bpy, mathutils

src, out = sys.argv[-2], sys.argv[-1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
pts = [o.matrix_world @ mathutils.Vector(c) for o in meshes for c in o.bound_box]
lo = [min(p[i] for p in pts) for i in range(3)]
hi = [max(p[i] for p in pts) for i in range(3)]
h = hi[2] - lo[2]
print('BOUNDS', lo, hi)
sc = bpy.context.scene
sc.render.engine = 'CYCLES'
sc.cycles.samples = 64
sc.cycles.use_denoising = True
sc.cycles.device = 'CPU'
sc.view_settings.view_transform = 'AgX'
w = bpy.data.worlds.new('w'); sc.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs[0].default_value = (.05, .055, .06, 1)
w.node_tree.nodes['Background'].inputs[1].default_value = .4


def light(name, kind, energy, loc, size=1.0, color=(1, 1, 1)):
    L = bpy.data.lights.new(name, kind); L.energy = energy; L.color = color
    if kind == 'AREA':
        L.size = size
    o = bpy.data.objects.new(name, L); sc.collection.objects.link(o); o.location = loc
    d = mathutils.Vector(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2] + h * .55)) - o.location
    o.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()


cx, cy = (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2
light('key', 'AREA', 900, (cx + 2.2, cy - 3.0, lo[2] + h * 1.1), 2.0, (1, .96, .9))
light('fill', 'AREA', 250, (cx - 3.0, cy - 2.0, lo[2] + h * .6), 3.0, (.85, .9, 1))
light('rim', 'AREA', 600, (cx + .5, cy + 3.0, lo[2] + h * 1.2), 1.5)
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); sc.collection.objects.link(cam); sc.camera = cam
cam.data.lens = 50


def shot(name, at, frm, rx, ry):
    sc.render.resolution_x, sc.render.resolution_y = rx, ry
    cam.location = frm
    cam.rotation_euler = (mathutils.Vector(at) - mathutils.Vector(frm)).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = '%s/%s.png' % (out, name)
    bpy.ops.render.render(write_still=True)


shot('punisher078_model_lit_front', (cx, cy, lo[2] + h * .5), (cx + .6, cy - 5.2, lo[2] + h * .55), 900, 1400)
thigh = (cx - (hi[0] - lo[0]) * .12, cy, lo[2] + h * .5)
shot('punisher078_right_leg_holster_lit', thigh, (thigh[0] - .35, thigh[1] - 1.25, thigh[2] + .1), 1200, 1200)
