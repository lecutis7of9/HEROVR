"""Preview renders of the hub's characters as HERO VR places them (no game needed):
  blender -b --factory-startup --python render_cast_preview.py -- <Models/UA/StarkTower> <out_prefix> [pair]
Reads what the game reads - scene.glb (the room), Cast/scene.glb + cast.txt (the characters: each look's first
frame, placed at its npc line, turned by its yaw as ua_cast.hpp does) - and writes <prefix>cast_hub.png (the hub
with its characters, as he walks in), <prefix>cast_floor.png (the floor from above) and <prefix>cast_raw_closeup.png (the first character you can talk to, at arm's length;
convert_ua.py lays the subtitle mock over it as cast_closeup.png). `pair`: the office workers' pair shown
(default pair05; in VR one pair a visit, picked at random as the map's rand_npcs.py does)."""
import sys, os, math, json, struct, tempfile, bpy
import numpy as np

argv = sys.argv[sys.argv.index('--') + 1:]
folder, prefix = argv[0], argv[1]
pair = argv[2] if len(argv) > 2 else 'pair05'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=os.path.join(folder, 'scene.glb'))
sc = bpy.context.scene
sc.render.engine = 'CYCLES'; sc.cycles.samples = 32; sc.cycles.device = 'CPU'; sc.cycles.use_denoising = False
sc.view_settings.view_transform = 'Standard'
sc.render.resolution_x, sc.render.resolution_y = 1280, 720
w = bpy.data.worlds.new('w'); sc.world = w; w.use_nodes = True
bg = w.node_tree.nodes['Background']; bg.inputs[0].default_value = (1, 1, 1, 1); bg.inputs[1].default_value = 1.0

# ---- Cast/scene.glb read the way mesh_model's parser does: scene nodes, primitives in order, 3 corners a triangle
data = open(os.path.join(folder, 'Cast', 'scene.glb'), 'rb').read()
jl = struct.unpack_from('<I', data, 12)[0]
gl = json.loads(data[20:20 + jl])
binoff = 20 + jl + 8
def acc(i, comps):
    a = gl['accessors'][i]; v = gl['bufferViews'][a['bufferView']]
    o = binoff + v.get('byteOffset', 0) + a.get('byteOffset', 0)
    return np.frombuffer(data, dtype='<f4', count=a['count'] * comps, offset=o).reshape(-1, comps)
P, UV, MAT = [], [], []
for prim in gl['meshes'][0]['primitives']:
    p = acc(prim['attributes']['POSITION'], 3); t = acc(prim['attributes']['TEXCOORD_0'], 2)
    P.append(p); UV.append(t); MAT += [prim.get('material', 0)] * (len(p) // 3)
P, UV, MAT = np.concatenate(P), np.concatenate(UV), np.asarray(MAT)
tmp = tempfile.mkdtemp()
mats = []
for i, im in enumerate(gl['images']):
    v = gl['bufferViews'][im['bufferView']]
    path = os.path.join(tmp, f'atlas{i}.png')
    open(path, 'wb').write(data[binoff + v.get('byteOffset', 0):binoff + v.get('byteOffset', 0) + v['byteLength']])
    img = bpy.data.images.load(path)
    m = bpy.data.materials.new(f'cast{i}'); m.use_nodes = True
    nt = m.node_tree; nt.nodes.clear()
    tex = nt.nodes.new('ShaderNodeTexImage'); tex.image = img; tex.interpolation = 'Closest'
    em = nt.nodes.new('ShaderNodeEmission'); tr = nt.nodes.new('ShaderNodeBsdfTransparent')
    mix = nt.nodes.new('ShaderNodeMixShader'); out = nt.nodes.new('ShaderNodeOutputMaterial')
    nt.links.new(tex.outputs['Color'], em.inputs['Color']); em.inputs['Strength'].default_value = .92
    gt = nt.nodes.new('ShaderNodeMath'); gt.operation = 'GREATER_THAN'; gt.inputs[1].default_value = .5
    nt.links.new(tex.outputs['Alpha'], gt.inputs[0])
    nt.links.new(gt.outputs[0], mix.inputs['Fac']); nt.links.new(tr.outputs[0], mix.inputs[1])
    nt.links.new(em.outputs[0], mix.inputs[2]); nt.links.new(mix.outputs[0], out.inputs['Surface'])
    mats.append(m)

looks, npcs = {}, []
for line in open(os.path.join(folder, 'cast.txt')):
    f = line.split()
    if f and f[0] == 'look':
        looks[f[1]] = (int(f[2]), int(f[3]), int(f[4]))
    elif f and f[0] == 'npc':
        npcs.append({'id': f[1], 'look': f[2], 'at': [float(x) for x in f[3:6]], 'yaw': float(f[6]), 'group': f[7],
                     'talk': f[8], 'label': ' '.join(f[9:])})
shown = [n for n in npcs if n['group'] in ('-', pair)]
for n in shown:
    first, tris, _frames = looks[n['look']]
    c = P[first * 3:(first + tris) * 3]; t = UV[first * 3:(first + tris) * 3]; mt = MAT[first:first + tris]
    me = bpy.data.meshes.new(n['id'])
    me.from_pydata([(x, -z, y) for x, y, z in c.tolist()], [], [(3 * i, 3 * i + 1, 3 * i + 2) for i in range(tris)])
    uvl = me.uv_layers.new()
    for li, lp in enumerate(me.loops):
        u, v = t[lp.vertex_index]; uvl.data[li].uv = (u, 1 - v)
    for m in mats:
        me.materials.append(m)
    for i, poly in enumerate(me.polygons):
        poly.material_index = int(mt[i])
    ob = bpy.data.objects.new(n['id'], me); sc.collection.objects.link(ob)
    x, y, z = n['at']
    ob.location = (x, -z, y); ob.rotation_euler = (0, 0, math.radians(n['yaw']))


def shoot(name, eye, look_at, lens):
    cam = bpy.data.cameras.new(name); cam.lens = lens; cam.clip_start = .05; cam.clip_end = 2000
    ob = bpy.data.objects.new(name, cam); sc.collection.objects.link(ob)
    ob.location = (eye[0], -eye[2], eye[1])
    d = np.subtract(look_at, eye)
    yaw = math.atan2(-d[2], d[0]); pitch = math.atan2(d[1], math.hypot(d[0], d[2]))
    ob.rotation_euler = (math.pi / 2 + pitch, 0, yaw - math.pi / 2)
    sc.camera = ob
    sc.render.filepath = f'{prefix}{name}.png'
    bpy.ops.render.render(write_still=True)
    print('WROTE', sc.render.filepath)


talkers = [n for n in shown if n['talk'] != '-']
main = next((n for n in talkers if 'fury' in n['id']), talkers[0] if talkers else shown[0])
mx, my, mz = main['at']
yaw = math.radians(main['yaw'])
fx, fz = math.cos(yaw), -math.sin(yaw)               # where he faces (room space)
# whom he talks to near the first: the group the hub opens on (Fury and the Widow at the conference table)
near = [n for n in talkers if math.hypot(n['at'][0] - mx, n['at'][2] - mz) < 4] or [main]
cx = float(np.mean([n['at'][0] for n in near]))
cz = float(np.mean([n['at'][2] for n in near]))
# the hub as he walks in: from the spawn's side, eye height, the table's group ahead
dx, dz = cx - 0.0, cz - 0.0
dl = math.hypot(dx, dz) or 1.0
shoot('cast_hub', (cx - dx / dl * 4.2 + dz / dl * 1.2, 1.9, cz - dz / dl * 4.2 - dx / dl * 1.2), (cx, .95, cz), 15)
# the whole floor from above the far side: everyone always there in view
far = [n for n in talkers if n not in near]
if far:
    ax = float(np.mean([n['at'][0] for n in talkers])); az = float(np.mean([n['at'][2] for n in talkers]))
    shoot('cast_floor', (cx + 3.5, 4.2, cz + 3.5), (ax, .6, az), 13)
shoot('cast_raw_closeup', (mx + fx * 1.35, 1.62, mz + fz * 1.35), (mx, 1.38, mz), 24)
open(f'{prefix}cast_preview.json', 'w').write(json.dumps({'closeup': main['id'], 'talk': main['talk'],
                                                          'label': main['label'], 'shown': [n['id'] for n in shown]}))
