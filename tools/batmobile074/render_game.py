# Renders the SHIPPED Batmobile GLBs as the game places them (layout.json): exterior, driver's eye
# (road ahead through the windscreen), dash. Blender 4.2 headless. SIMULATED, NOT GAME CAPTURE.
#   blender -b --python render_game.py -- <Models/Vehicle/Batmobile dir> <folder> <outdir> [screen.png] [samples]
import bpy, sys, os, math, json
from mathutils import Vector as V, Matrix
a = sys.argv[sys.argv.index('--') + 1:]
root, folder, outdir = a[0], a[1], a[2]
screen_png = a[3] if len(a) > 3 and a[3] != '-' else ''
samples = int(a[4]) if len(a) > 4 else 32
lay = json.load(open(os.path.join(root, folder, 'layout.json')))
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
def conv(g): return V((g[0], -g[2], g[1]))           # game (x right, y up, -z forward) -> Blender (z up, +y forward)
def imp(path, M):
    before = set(sc.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in sc.objects if o not in before]
    e = bpy.data.objects.new('grp', None); sc.collection.objects.link(e)
    for o in new:
        if o.parent is None: o.parent = e
    e.matrix_world = M
    return new
body = imp(os.path.join(root, folder, 'model.glb'), Matrix.Identity(4))
glass = []
gp = os.path.join(root, folder, 'Glass', 'model.glb')
if os.path.exists(gp):
    glass = imp(gp, Matrix.Identity(4))
    gm = bpy.data.materials.new('glass_game'); gm.use_nodes = True
    b = gm.node_tree.nodes['Principled BSDF']; b.inputs['Base Color'].default_value = (0.55, 0.62, 0.70, 1)
    b.inputs['Alpha'].default_value = 0.25; b.inputs['Roughness'].default_value = 0.05
    gm.blend_method = 'BLEND'; gm.surface_render_method = 'BLENDED'; gm.use_backface_culling = False
    for o in glass:
        if o.type == 'MESH':
            o.data.materials.clear(); o.data.materials.append(gm)
ck = imp(os.path.join(root, 'Cockpit', 'model.glb'), Matrix.Translation(conv(lay['cockpit'])))
ax = V(lay['axis']).normalized(); up0 = (V((0, 1, 0)) - ax * ax.y).normalized()
R = Matrix(((1, 0, 0), (0, 0, 0), (0, 0, 0)))
cx, cy, cz = conv((1, 0, 0)), -conv(tuple(ax)), conv(tuple(up0))
R = Matrix(((cx.x, cy.x, cz.x, 0), (cx.y, cy.y, cz.y, 0), (cx.z, cy.z, cz.z, 0), (0, 0, 0, 1)))
wh = imp(os.path.join(root, 'SteeringWheel', 'model.glb'), Matrix.Translation(conv(lay['hub'])) @ R)
# the live dash screen (the game's van_screen Batman page, drawn by spidy_van_screen_preview) on its mount
if screen_png and os.path.exists(screen_png):
    W, H = 0.28, 0.17
    bpy.ops.mesh.primitive_plane_add(size=1)
    p = bpy.context.object; p.scale = (W, H, 1)
    p.rotation_euler = (math.radians(90), 0, math.radians(180))   # face toward the driver (Blender -y)
    p.location = conv(lay['screen'])
    m = bpy.data.materials.new('screen'); m.use_nodes = True; nt = m.node_tree; pb = nt.nodes['Principled BSDF']
    t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(screen_png)
    nt.links.new(t.outputs[0], pb.inputs['Base Color']); nt.links.new(t.outputs[0], pb.inputs['Emission Color'])
    pb.inputs['Emission Strength'].default_value = 1.6
    p.data.materials.append(m)
# the street: asphalt, lane lines, kerbs, pavements, building blocks (dusk)
def mat(n, c, e=0):
    m = bpy.data.materials.new(n); m.use_nodes = True; b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = c + (1,)
    if e: b.inputs['Emission Color'].default_value = c + (1,); b.inputs['Emission Strength'].default_value = e
    return m
def boxo(n, loc, size, m):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc); o = bpy.context.object; o.scale = size; o.data.materials.append(m); o.name = n; return o
asph, line, yel, kerb, walk = mat('asph', (0.06, 0.06, 0.065)), mat('line', (0.9, 0.9, 0.85), 0.6), mat('yel', (0.95, 0.7, 0.1), 0.6), mat('kerb', (0.35, 0.35, 0.35)), mat('walk', (0.22, 0.21, 0.2))
boxo('road', (1.75, 150, -0.01), (14, 340, 0.02), asph)
for y in range(-20, 300, 9): boxo('dash', (-1.75 + 1.75 + 1.75, y, 0.002), (0.15, 3.0, 0.004), line)   # lane dashes right of the car
for y in range(-20, 300, 9): boxo('dash2', (-1.75, y, 0.002), (0.15, 3.0, 0.004), line)
boxo('yellow', (-5.25 + 0.3, 150, 0.002), (0.12, 340, 0.004), yel); boxo('yellow2', (-5.25 + 0.5, 150, 0.002), (0.12, 340, 0.004), yel)
for sx in (-1, 1):
    boxo('kerb', (1.75 + sx * 7.0, 150, 0.08), (0.25, 340, 0.16), kerb)
    boxo('walk', (1.75 + sx * 9.5, 150, 0.07), (4.8, 340, 0.14), walk)
import random; random.seed(3)
bm = mat('bld', (0.09, 0.085, 0.08)); win = mat('win', (1.0, 0.75, 0.4), 2.0)
for sx in (-1, 1):
    y = -10
    while y < 300:
        d = random.uniform(10, 22); h = random.uniform(12, 45)
        boxo('b', (1.75 + sx * (12 + 6), y + d / 2, h / 2), (12, d - 1, h), bm)
        for k in range(int(h / 3.5)):
            if random.random() < 0.6: boxo('w', (1.75 + sx * 11.95, y + d / 2, 2.5 + k * 3.5), (0.05, d - 3, 1.2), win)
        y += d
for y in range(10, 300, 30):
    for sx in (-1, 1):
        boxo('pole', (1.75 + sx * 7.6, y, 3.5), (0.12, 0.12, 7), kerb)
        L = bpy.data.objects.new('lamp', bpy.data.lights.new('lamp', 'POINT')); L.data.energy = 1500; L.data.color = (1, 0.8, 0.55)
        L.location = (1.75 + sx * 6.8, y, 7.0); sc.collection.objects.link(L)
w = bpy.data.worlds.new('w'); sc.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs[0].default_value = (0.20, 0.25, 0.38, 1); w.node_tree.nodes['Background'].inputs[1].default_value = 1.0
s = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN')); s.data.energy = 1.5; s.data.color = (1, 0.75, 0.55)
s.rotation_euler = (math.radians(70), 0, math.radians(-150)); sc.collection.objects.link(s)
cab = bpy.data.objects.new('cab', bpy.data.lights.new('cab', 'POINT')); cab.data.energy = 6; cab.data.shadow_soft_size = 0.4
cab.location = conv(lay['eye']) + V((0.35, -0.3, 0.15)); sc.collection.objects.link(cab)
sc.render.engine = 'BLENDER_EEVEE_NEXT'; sc.eevee.taa_render_samples = samples
sc.render.resolution_x, sc.render.resolution_y = 1600, 1000
sc.view_settings.view_transform = 'AgX'; sc.view_settings.exposure = 0.2
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); sc.collection.objects.link(cam); sc.camera = cam
cam.data.clip_start = 0.02; cam.data.clip_end = 600
eye = conv(lay['eye'])
def shot(name, loc, look, fov):
    cam.location = V(loc); d = V(look) - V(loc); cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    cam.data.angle = math.radians(fov)
    sc.render.filepath = os.path.join(outdir, name); bpy.ops.render.render(write_still=True); print('WROTE', sc.render.filepath)
os.makedirs(outdir, exist_ok=True)
tag = folder.lower()
shot('%s_driver_eye.png' % tag, eye, eye + V((0, 20, -1.6)), 95)
scr = conv(lay['screen'])
shot('%s_dash_screen.png' % tag, eye, scr + V((0.10, 0, 0.02)), 70)
shot('%s_exterior_front.png' % tag, (4.6, 6.2, 2.2), (0, 0, 0.7), 45)
shot('%s_exterior_rear.png' % tag, (-4.8, -6.0, 2.6), (0, 0, 0.7), 45)
