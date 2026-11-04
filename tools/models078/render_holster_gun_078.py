"""HERO VR 0.0.78 PUNISHER ARSENAL: an OFFLINE Cycles render of the shipped suppressed pistol (Models/Punisher078/Pistol,
its slide and magazine) in the right-leg holster pose the game uses (punisher078::legHolsterPose: the grip point
legHolsterBelow - 5 cm under the eyes, legHolsterSide to the right, legHolsterAhead; the barrel down, its top forward)
beside the Punisher model (Suit_punisher_1's source, standing, eyes 1.73 m up). Not an in-game capture.
  <blender python> render_holster_gun_078.py <punisher model.glb> <Models dir> <out dir>"""
import os, sys
import bpy
from mathutils import Matrix, Vector

src, models, out = sys.argv[-3], sys.argv[-2], sys.argv[-1]
BELOW, SIDE, AHEAD = .85, .24, -.06  # holster.hpp
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
body = [o for o in bpy.context.scene.objects if o.type == 'MESH']
pts = [o.matrix_world @ Vector(c) for o in body for c in o.bound_box]
lo = Vector([min(p[i] for p in pts) for i in range(3)])
head = Vector((0, 0, lo.z + 1.73))
f, r, down = Vector((0, -1, 0)), Vector((-1, 0, 0)), Vector((0, 0, -1))  # the model faces -y; its right is -x
at = head + down * (BELOW - .05) + r * SIDE + f * AHEAD
gF = (down + f * .04).normalized()
gU = f
gR = gF.cross(gU).normalized()
A = Matrix((tuple(gR) + (at.x,), tuple(gU) + (at.y,), tuple(-gF) + (at.z,), (0, 0, 0, 1)))
A = Matrix([[gR.x, gU.x, -gF.x, at.x], [gR.y, gU.y, -gF.y, at.y], [gR.z, gU.z, -gF.z, at.z], [0, 0, 0, 1]])
imp = Matrix([[1, 0, 0, 0], [0, 0, -1, 0], [0, 1, 0, 0], [0, 0, 0, 1]])  # glTF file -> Blender: (x, y, z) -> (x, -z, y)
for part in ('Pistol', 'Pistol/Slide', 'Pistol/Mag'):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(models, 'Punisher078', *part.split('/'), 'scene.glb'))
    for o in set(bpy.data.objects) - before:
        if o.parent is None:
            o.matrix_world = A @ imp.inverted() @ o.matrix_world
sc = bpy.context.scene
sc.render.engine = 'CYCLES'; sc.cycles.samples = 96; sc.cycles.use_denoising = True; sc.cycles.device = 'CPU'
sc.view_settings.view_transform = 'AgX'
w = bpy.data.worlds.new('w'); sc.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs[0].default_value = (.05, .055, .06, 1)
w.node_tree.nodes['Background'].inputs[1].default_value = .5


def light(name, energy, loc, size, color=(1, 1, 1)):
    L = bpy.data.lights.new(name, 'AREA'); L.energy = energy; L.color = color; L.size = size
    o = bpy.data.objects.new(name, L); sc.collection.objects.link(o); o.location = loc
    o.rotation_euler = (at - o.location).to_track_quat('-Z', 'Y').to_euler()


light('key', 500, at + Vector((-1.4, -2.0, 1.0)), 1.5, (1, .96, .9))
light('fill', 150, at + Vector((1.5, -1.8, .3)), 2.0, (.85, .9, 1))
light('rim', 300, at + Vector((-.8, 2.0, 1.0)), 1.2)
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); sc.collection.objects.link(cam); sc.camera = cam
cam.data.lens = 50
os.makedirs(out, exist_ok=True)
for name, frm, tgt, rx, ry in (('punisher078_pistol_holster_offline', at + Vector((-.75, -1.05, .05)), at + Vector((0, 0, -.08)), 1200, 1200),
                               ('punisher078_pistol_holster_full_offline', Vector((-.9, -4.6, .1)), Vector((0, 0, lo.z + .95)), 900, 1400)):
    sc.render.resolution_x, sc.render.resolution_y = rx, ry
    cam.location = frm
    cam.rotation_euler = (tgt - frm).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(out, name + '.png')
    bpy.ops.render.render(write_still=True)
    print('RENDERED', sc.render.filepath)
