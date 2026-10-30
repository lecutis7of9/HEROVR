# HERO VR 0.0.74 test 3 previews (Made by Agent Mulder). SIMULATED in Blender EEVEE from the shipped GLBs placed by
# layout.json - NOT game capture. blender -b --python preview3.py -- <root> <folder> <outdir> [samples]
import bpy, sys, os, math, json
from mathutils import Vector as V, Matrix
a = sys.argv[sys.argv.index('--') + 1:]
root, folder, outdir = a[0], a[1], a[2]; samples = int(a[3]) if len(a) > 3 else 16
lay = json.load(open(os.path.join(root, folder, 'layout.json')))
bpy.ops.wm.read_factory_settings(use_empty=True); sc = bpy.context.scene
def conv(g): return V((g[0], -g[2], g[1]))
def imp(path, M):
    before = set(sc.objects); bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in sc.objects if o not in before]
    e = bpy.data.objects.new('grp', None); sc.collection.objects.link(e)
    for o in new:
        if o.parent is None: o.parent = e
    e.matrix_world = M; return [o for o in new if o.type == 'MESH']
mats_before = set(bpy.data.materials)
body = imp(os.path.join(root, folder, 'model.glb'), Matrix.Identity(4))
roof = imp(os.path.join(root, folder, 'Roof', 'model.glb'), Matrix.Identity(4)) if os.path.exists(os.path.join(root, folder, 'Roof', 'model.glb')) else []
pads = [o for o in body if any(m and 'HERO_handle_pad' in m.name for m in o.data.materials)]
car_mats = set(bpy.data.materials)
ck = imp(os.path.join(root, 'Cockpit', 'model.glb'), Matrix.Translation(conv(lay['cockpit'])))
ax = V(lay['axis']).normalized(); up0 = (V((0, 1, 0)) - ax * ax.y).normalized()
cx, cy, cz = conv((1, 0, 0)), -conv(tuple(ax)), conv(tuple(up0))
R = Matrix(((cx.x, cy.x, cz.x, 0), (cx.y, cy.y, cz.y, 0), (cx.z, cy.z, cz.z, 0), (0, 0, 0, 1)))
wh = imp(os.path.join(root, 'SteeringWheel', 'model.glb'), Matrix.Translation(conv(lay['hub'])) @ R)
# COCKPIT GLOW as the game's cabin_look074 (dark .52, lamps unlit x .92, tips + .42 glow, cool above / amber below)
cab_mats = [m for m in bpy.data.materials if m not in car_mats and m.use_nodes]
glow_nodes = []
for m in cab_mats:
    nt = m.node_tree; b = [n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED']
    if not b: continue
    b = b[0]; sock = b.inputs['Base Color']
    lamp = 'BC_Gauges' in m.name or 'BC_Screen' in m.name; tip = 'BC_RedCap' in m.name
    src = sock.links[0].from_socket if sock.links else None
    mix = nt.nodes.new('ShaderNodeMix'); mix.data_type = 'RGBA'; mix.blend_type = 'MULTIPLY'
    if src: nt.links.new(src, mix.inputs[6])
    else: mix.inputs[6].default_value = sock.default_value[:]
    mix.inputs[7].default_value = (0.52, 0.52, 0.52, 1); nt.links.new(mix.outputs[2], sock)
    if lamp or tip:
        nt.links.new(src if src else mix.outputs[2], b.inputs['Emission Color']) if src else None
        if not src: b.inputs['Emission Color'].default_value = sock.default_value[:]
    glow_nodes.append((m, mix, b, lamp, tip))
def set_glow(on):
    for m, mix, b, lamp, tip in glow_nodes:
        mix.inputs[0].default_value = 1.0 if (on and not lamp) else 0.0
        b.inputs['Emission Strength'].default_value = (1.1 if lamp else 0.55 if tip else 0.0) if on else (0.35 if lamp else 0.0)
    for l in accents: l.hide_render = not on
eye = conv(lay['eye']); cpt = conv(lay['cockpit'])
def light(n, kind, loc, rot, energy, col, size=0.5):
    L = bpy.data.objects.new(n, bpy.data.lights.new(n, kind)); L.data.energy = energy; L.data.color = col
    if kind == 'AREA': L.data.size = size
    L.location = loc; L.rotation_euler = rot; sc.collection.objects.link(L); return L
accents = [light('cool', 'AREA', eye + V((0.37, 0.1, 0.25)), (0, 0, 0), 3.0, (0.35, 0.55, 1.0), 0.9),
           light('amber', 'AREA', cpt + V((0, -0.5, 0.12)), (math.pi, 0, 0), 2.0, (1.0, 0.55, 0.15), 0.8)]
cab = light('cab', 'POINT', eye + V((0.35, -0.3, 0.15)), (0, 0, 0), 6, (1, 1, 1))
# street (as render_game.py)
def mat(n, c, e=0):
    m = bpy.data.materials.new(n); m.use_nodes = True; b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = c + (1,)
    if e: b.inputs['Emission Color'].default_value = c + (1,); b.inputs['Emission Strength'].default_value = e
    return m
def boxo(n, loc, size, m):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc); o = bpy.context.object; o.scale = size; o.data.materials.append(m); o.name = n; return o
asph, line, kerb, walk = mat('asph', (0.06, 0.06, 0.065)), mat('line', (0.9, 0.9, 0.85), 0.6), mat('kerb', (0.35, 0.35, 0.35)), mat('walk', (0.22, 0.21, 0.2))
boxo('road', (1.75, 150, -0.01), (14, 340, 0.02), asph)
for y in range(-20, 300, 9): boxo('d1', (3.5, y, 0.002), (0.15, 3.0, 0.004), line); boxo('d2', (-1.75, y, 0.002), (0.15, 3.0, 0.004), line)
import random; random.seed(3)
bmt = mat('bld', (0.09, 0.085, 0.08)); win = mat('win', (1.0, 0.75, 0.4), 2.0)
for sx in (-1, 1):
    boxo('kerb', (1.75 + sx * 7.0, 150, 0.08), (0.25, 340, 0.16), kerb); boxo('walk', (1.75 + sx * 9.5, 150, 0.07), (4.8, 340, 0.14), walk)
    y = -10
    while y < 300:
        d = random.uniform(10, 22); h = random.uniform(12, 45)
        boxo('b', (1.75 + sx * 18, y + d / 2, h / 2), (12, d - 1, h), bmt)
        for k in range(int(h / 3.5)):
            if random.random() < 0.6: boxo('w', (1.75 + sx * 11.95, y + d / 2, 2.5 + k * 3.5), (0.05, d - 3, 1.2), win)
        y += d
lamps = []
for y in range(10, 300, 30):
    for sx in (-1, 1):
        boxo('pole', (1.75 + sx * 7.6, y, 3.5), (0.12, 0.12, 7), kerb)
        lamps.append(light('lamp', 'POINT', (1.75 + sx * 6.8, y, 7.0), (0, 0, 0), 1500, (1, 0.8, 0.55)))
w = bpy.data.worlds.new('w'); sc.world = w; w.use_nodes = True; bg = w.node_tree.nodes['Background']
sun = light('sun', 'SUN', (0, 0, 0), (math.radians(50), 0, math.radians(-150)), 1.5, (1, 0.9, 0.8))
def set_time(day):
    bg.inputs[0].default_value = (0.55, 0.68, 0.9, 1) if day else (0.012, 0.016, 0.03, 1); bg.inputs[1].default_value = 1.0
    sun.data.energy = 4.0 if day else 0.0
    for l in lamps: l.hide_render = day
    win.node_tree.nodes['Principled BSDF'].inputs['Emission Strength'].default_value = 0.0 if day else 2.0
    sc.view_settings.exposure = 0.0 if day else 0.6
# extras: the dash finger ray (item 7) and the handle marker (item 8), hidden unless a shot needs them
def emis(n, c, e):
    m = bpy.data.materials.new(n); m.use_nodes = True; b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = c + (1,); b.inputs['Emission Color'].default_value = c + (1,); b.inputs['Emission Strength'].default_value = e; return m
def cyl_between(n, p0, p1, r, m):
    p0, p1 = V(p0), V(p1); d = p1 - p0
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=d.length, location=(p0 + p1) / 2, vertices=12)
    o = bpy.context.object; o.rotation_euler = d.to_track_quat('Z', 'Y').to_euler(); o.data.materials.append(m); o.name = n; return o
skin = mat('skin', (0.05, 0.05, 0.06))
scr = conv(lay['screen']); hit = scr + V((0.06, -0.012, 0.03))
knuckle = eye + V((0.05, 0.40, -0.30)); tip = knuckle + (hit - knuckle).normalized() * 0.075
bpy.ops.mesh.primitive_uv_sphere_add(radius=0.045, location=knuckle - (hit - knuckle).normalized() * 0.05); palm = bpy.context.object; palm.scale = (1.0, 1.3, 0.6); palm.data.materials.append(skin)
finger = cyl_between('finger', knuckle, tip, 0.009, skin)
ray = cyl_between('ray', tip, hit, 0.0015, emis('ray', (0.55, 0.8, 1.0), 4.0))
bpy.ops.mesh.primitive_uv_sphere_add(radius=0.007, location=hit); dot = bpy.context.object; dot.data.materials.append(emis('dot', (0.55, 0.8, 1.0), 6.0))
ray_set = [palm, finger, ray, dot]
ho = conv(lay['handleOut']); side = -1 if ho.x < 0 else 1
bpy.ops.mesh.primitive_torus_add(major_radius=0.018, minor_radius=0.0025, location=ho, rotation=(0, math.pi / 2, 0)); ring = bpy.context.object
ring.data.materials.append(emis('ring', (0.55, 0.8, 1.0), 1.5))
bpy.ops.mesh.primitive_uv_sphere_add(radius=0.05, location=ho + V((side * 0.16, -0.12, 0.08))); hand2 = bpy.context.object; hand2.scale = (0.7, 1.3, 0.6); hand2.data.materials.append(skin)
mark_set = [ring, hand2]
def show(objs, on):
    for o in objs: o.hide_render = not on
sc.render.engine = 'BLENDER_EEVEE_NEXT'; sc.eevee.taa_render_samples = samples
sc.render.resolution_x, sc.render.resolution_y = 1280, 800; sc.view_settings.view_transform = 'AgX'
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); sc.collection.objects.link(cam); sc.camera = cam
cam.data.clip_start = 0.02; cam.data.clip_end = 600
os.makedirs(outdir, exist_ok=True); tag = folder.lower()
def shot(name, loc, look, fov):
    cam.location = V(loc); d = V(look) - V(loc); cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler(); cam.data.angle = math.radians(fov)
    sc.render.filepath = os.path.join(outdir, tag + '_' + name + '.png'); bpy.ops.render.render(write_still=True); print('WROTE', sc.render.filepath)
side = -1 if conv(lay['handleOut']).x < 0 else 1   # the driver's door side (Blender x)
views = dict(forward=eye + V((0, 20, -1.6)), up=eye + V((0, 4, 9)), left=eye + V((side * 10, 1.5, -1)), right=eye + V((-side * 10, 1.5, -1)))
# seated: Roof (exterior only) hidden, the baked handle pad hidden (DASH INSIDE on: role marker skipped)
show(ray_set, False); show(mark_set, False)
for day in (False, True):
    set_time(day); show(roof, False); show(pads, False)
    for on in (True, False):
        set_glow(on)
        for v, look in views.items():
            if not on and v in ('up', 'left', 'right'): continue
            shot('driver_%s_%s_glow%s' % (v, 'day' if day else 'night', 'ON' if on else 'OFF'), eye, look, 95)
    set_glow(True)
    show(ray_set, True); shot('finger_ray_%s' % ('day' if day else 'night'), eye, hit + V((0, 0, -0.05)), 70); show(ray_set, False)
# outside: Roof drawn; no dash panel; the marker ring only with a hand near (item 8 shot)
set_time(True); set_glow(True); show(roof, True); show(pads, False)
shot('exterior_frontleft34', (-3.3, 5.8, 1.5), (0, 0.3, 0.5), 50)   # owner screenshot: front-left 3/4, ~5 m, low
shot('exterior_frontleft34_low', (-2.9, 4.6, 0.8), (0, 0.4, 0.55), 60)
show(mark_set, True); shot('handle_marker_hand_near', ho + V((side * 0.9, -0.6, 0.25)), ho, 35); show(mark_set, False)
shot('handle_marker_no_hand', ho + V((side * 0.9, -0.6, 0.25)), ho, 35)
