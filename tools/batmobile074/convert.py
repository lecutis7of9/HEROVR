# HERO VR 0.0.74 test 2 BATMOBILE converter (Made by Agent Mulder). Blender 4.2 headless.
#   blender -b --python convert.py -- <id|cockpit> <outdir>
# Writes the game GLBs: body (~6 m, game frame) and, test 2 (owner t1171u), its CABIN cut: every shell face that
# reaches into the cockpit volume (dash front edge to past the headrest, floor to roof line, cockpit width) is
# DELETED from the body below the beltline; at or above the beltline (canopy, glass, roof, pillars over the cabin,
# full car width) the faces move to <car>/Roof/model.glb, an opaque exterior-only piece the game draws only while
# you are outside the car (INSIDE VIEW CULL). Nothing translucent. And for
# "cockpit" the shared cockpit (no wheel) and the steering wheel (hub at origin, rim in xy, +z to the driver,
# +y 12 o'clock). Prints LAYOUT json (game frame: x right, y up, -z forward, origin = ground under the middle).
import bpy, bmesh, sys, os, math, json, glob
from mathutils import Vector as V, Matrix, Euler
argv = sys.argv[sys.argv.index('--') + 1:]
which, outdir = argv[0], argv[1]
BM = '/workspace/hero-0.0.72/batmobile'
SPECS = {
 '01': dict(src='dl/dreamloft/Bat-Mobile_(1989).obj', tex='', s=1, r=(0,0,0), hide=[], glass=[], glassify=[], cut=[],
            L=(0,0.07,0.1), k=0.92, tris=110000, folder='1989', colors=False,
            restore=dict(glass=['Windows'], head=0.22, wheel=0.26)),
 '07': dict(src='dl/07_odami/scene.gltf', tex='dl/07_odami', s=0.01, r=(0,0,0), hide=['Material__90','Material__53','Material__54'],
            glass=[], glassify=[], cut=[], L=(0,0,0.03), k=1.0, tris=110000, folder='ODAMI', colors=False),
 '09': dict(src='dl/09_sorrow/scene.gltf', tex='', s=0.001, r=(0,0,0), hide=['Object_2','Object_3','Object_4','Object_5','Object_18','Object_68','Object_67','Object_65'],
            glass=[], glassify=[], cut=[], L=(0,-0.55,0.6), k=0.74, tris=110000, folder='FOREVER', colors=True),
 '10': dict(src='dl/10_syork/scene.gltf', tex='dl/10_syork', s=4, r=(0,0,90), hide=[], glass=[], glassify=[], cut=[],
            L=(0,0.1,0.16), k=0.84, tris=110000, folder='FOREVER2', colors=False),
}
# Test 2 paint (owner: satin black, subtle metallic): material -> (base colour linear, metallic, roughness).
PAINT = {'01': {'Base_Color': (0.03, 0.40, 0.42), 'Back_Things': (0.06, 0.35, 0.45), 'Wheels': (0.012, 0.2, 0.55)},
         '07': {'Material__26': (None, 0.35, 0.45), 'Material__27': (None, 0.35, 0.45), 'Material__28': (None, 0.35, 0.45), 'Material__29': (None, 0.35, 0.45)}}
# Test 2 cockpit volume (cockpit-local metres: x across, y negative forward, z up from the cockpit floor):
# x +-0.76 (doors 0.70 + 0.06), y from the dash front edge (-1.06) less 0.06 to 0.06 behind the headrest (0.524+0.06),
# z from 0.05 under the floor to the roof line (everything above, the car's full height).
CAB = dict(x=0.76, y0=-1.12, y1=0.60, z0=-0.05)
OUTSIDE_WHOLE = True   # test 2b: the cut cabin faces go to Roof (outside only), not the bin
# test 3 (owner, item 4): the 1989's roof, canopy frame and pillars go BACK into the body (drawn seated too); only the
# glass (material 'Windows', so the windshield stays see-through seated) and whatever comes within `head` m of the
# eye or `wheel` m of the steering hub stays in the exterior-only Roof piece. RESTORE_ROOF=False = test 2b.
RESTORE_ROOF = True
COCKPIT_PAINT = {'BC_Carbon': (0.03, 0.45, 0.38)}   # base (linear), metallic, roughness: satin black metallic
DASH_TRIM = 0.03   # the dash top 0.82 -> 0.79 (the passenger screen moves down with it)
BELT = 0.80   # beltline (cockpit-local z): the dash top is 0.82, the door trim 0.80
EYE = (0.37, 0.25, 1.14)   # test 2: 0.12 higher and 0.07 further back than test 1 (0.37, 0.18, 1.02)
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

def meshes(): return [o for o in sc.objects if o.type == 'MESH']
def tris_of(objs):
    n = 0
    for o in objs:
        o.data.calc_loop_triangles(); n += len(o.data.loop_triangles)
    return n

def upstream_image(sock, seen=None):
    seen = seen or set()
    for l in sock.links:
        n = l.from_node
        if id(n) in seen: continue
        seen.add(id(n))
        if n.type == 'TEX_IMAGE' and n.image: return n.image
        for i in n.inputs:
            im = upstream_image(i, seen)
            if im: return im
    return None

def simplify(m, keep_vcol=False, size=1024, factor=None, paint=None):
    """Base colour (image or factor) only: the game reads nothing else. Opaque."""
    if not m.use_nodes:
        m.use_nodes = True
    nt = m.node_tree
    b = [n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED']
    col = tuple(b[0].inputs['Base Color'].default_value) if b else (0.5, 0.5, 0.5, 1)
    im = upstream_image(b[0].inputs['Base Color']) if b else None
    if factor is not None: col = tuple(factor)
    if paint and paint[0] is not None: col = (paint[0], paint[0], paint[0], 1)
    for n in list(nt.nodes): nt.nodes.remove(n)
    out = nt.nodes.new('ShaderNodeOutputMaterial'); p = nt.nodes.new('ShaderNodeBsdfPrincipled')
    nt.links.new(p.outputs[0], out.inputs[0]); p.inputs['Roughness'].default_value = 0.45
    if paint: p.inputs['Metallic'].default_value = paint[1]; p.inputs['Roughness'].default_value = paint[2]
    if im:
        if max(im.size) > size:
            w, h = im.size; f = size / max(w, h); im.scale(max(1, int(w * f)), max(1, int(h * f)))
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = im; nt.links.new(t.outputs[0], p.inputs['Base Color'])
    else:
        p.inputs['Base Color'].default_value = col
    if keep_vcol:
        a = nt.nodes.new('ShaderNodeVertexColor')
        mul = nt.nodes.new('ShaderNodeMix'); mul.data_type = 'RGBA'; mul.blend_type = 'MULTIPLY'; mul.inputs[0].default_value = 1
        mul.inputs[6].default_value = col
        nt.links.new(a.outputs[0], mul.inputs[7]); nt.links.new(mul.outputs[2], p.inputs['Base Color'])
    m.blend_method = 'OPAQUE'

def export(path, objs, colors=False):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.hide_set(False); o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=True, export_apply=True, export_yup=True,
        export_texcoords=True, export_normals=True, export_tangents=False, export_materials='EXPORT', export_image_format='JPEG',
        export_jpeg_quality=85, export_vertex_color='ACTIVE' if colors else 'NONE', export_animations=False, export_skins=False,
        export_morph=False, export_cameras=False, export_lights=False, export_extras=False)
    print('EXPORT', path, os.path.getsize(path), 'TRIS', tris_of(objs))

def flatten(root):
    """Applies every parent transform into the meshes (each object at identity)."""
    bpy.context.view_layer.update()
    for o in meshes():
        if o.data.users > 1: o.data = o.data.copy()
        mw = o.matrix_world.copy(); o.parent = None; o.matrix_world = Matrix.Identity(4)
        o.data.transform(mw)
    for o in list(sc.objects):
        if o.type != 'MESH': bpy.data.objects.remove(o)

def turn_game(objs):
    # Blender front -Y -> rotate 180 about Z: front +Y = glTF -Z = game forward
    R = Matrix.Rotation(math.pi, 4, 'Z')
    for o in objs: o.data.transform(R)

def pad(name, centre, size, colour):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts: v.co = V((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2])) + V(centre)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    m = bpy.data.materials.new(name + '_mat'); m.use_nodes = True
    m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = colour
    me.materials.append(m)
    o = bpy.data.objects.new(name, me); sc.collection.objects.link(o); return o

if which == 'cockpit':
    bpy.ops.wm.open_mainfile(filepath=os.path.join(outdir, '..', 'cockpit_game.blend') if os.path.exists(os.path.join(outdir, '..', 'cockpit_game.blend')) else '/workspace/hero-0.0.74/batconv/out/cockpit_game.blend')
    sc = bpy.context.scene
    # test 3 (owner, item 4): satin black metallic instead of the carbon zigzag texture; the dash top 0.03 lower
    for m in bpy.data.materials: simplify(m, size=512, paint=COCKPIT_PAINT.get(m.name))
    for m in bpy.data.materials:
        if m.name in COCKPIT_PAINT:
            for n in [n for n in m.node_tree.nodes if n.type == 'TEX_IMAGE']: m.node_tree.nodes.remove(n)
    for o in meshes():
        if o.name == 'Dash':   # squash the top of the dash body: z .66 .. .82 -> .66 .. .82-DASH_TRIM
            for v in o.data.vertices:
                if v.co.z > 0.66: v.co.z = 0.66 + (v.co.z - 0.66) * (0.16 - DASH_TRIM) / 0.16
        elif o.name in ('DashTop', 'ScreenBezel_P', 'Screen_Passenger'):
            for v in o.data.vertices: v.co.z -= DASH_TRIM
    flatten(None)
    wheelnames = ('SteeringRim', 'SteeringHub', 'SteeringHubCap', 'SteeringSpoke_')
    wheel = [o for o in meshes() if o.name.startswith(wheelnames)]
    rest = [o for o in meshes() if o not in wheel]
    C = Matrix.Translation(V((0.36, -0.33, 0.74))) @ Euler((math.radians(-70), 0, 0)).to_matrix().to_4x4()
    Mw = Matrix.Rotation(math.radians(90), 4, 'X') @ Matrix.Rotation(math.pi, 4, 'Z') @ C.inverted()
    for o in wheel: o.data.transform(Mw)
    turn_game(rest)
    os.makedirs(outdir + '/Cockpit', exist_ok=True); os.makedirs(outdir + '/SteeringWheel', exist_ok=True)
    export(outdir + '/Cockpit/model.glb', rest)
    export(outdir + '/SteeringWheel/model.glb', wheel)
    sys.exit(0)

S = SPECS[which]
src = os.path.join(BM, S['src'])
if src.endswith('.gltf'): bpy.ops.import_scene.gltf(filepath=src)
else: bpy.ops.wm.obj_import(filepath=src)
for o in list(sc.objects):
    if any(h == o.name or (o.type == 'MESH' and h in [m.name for m in o.data.materials]) for h in S['hide']):
        bpy.data.objects.remove(o)
# fix missing textures (as combo.py)
if S['tex']:
    idx = {}
    for p in glob.glob(os.path.join(BM, S['tex'], '**', '*'), recursive=True):
        idx[os.path.basename(p).lower()] = p; idx[os.path.splitext(os.path.basename(p))[0].lower()] = p
    for im in bpy.data.images:
        if im.source != 'FILE': continue
        p = bpy.path.abspath(im.filepath)
        if not os.path.exists(p):
            b = os.path.basename(p.replace('\\', '/')).lower(); k = b if b in idx else os.path.splitext(b)[0]
            if k in idx: im.filepath = idx[k]; im.reload()
root = bpy.data.objects.new('root', None); sc.collection.objects.link(root)
for o in sc.objects:
    if o.parent is None and o is not root: o.parent = root
root.scale = (S['s'],) * 3; root.rotation_euler = [math.radians(a) for a in S['r']]
flatten(root)
def bbox(objs):
    mn = V((1e9,) * 3); mx = -mn
    for o in objs:
        for v in o.data.vertices: mn = V(map(min, mn, v.co)); mx = V(map(max, mx, v.co))
    return mn, mx
mn, mx = bbox(meshes())
T = Matrix.Translation(V((-(mn.x + mx.x) / 2, -(mn.y + mx.y) / 2, -mn.z)))
for o in meshes(): o.data.transform(T)
# cut boxes (pre-k coordinates, as combo.py)
for (x0, x1, y0, y1, z0, z1) in S['cut']:
    for o in meshes():
        bm = bmesh.new(); bm.from_mesh(o.data)
        kill = [f for f in bm.faces if (lambda c: x0 < c.x < x1 and y0 < c.y < y1 and z0 < c.z < z1)(f.calc_center_median())]
        bmesh.ops.delete(bm, geom=kill, context='FACES'); bm.to_mesh(o.data); bm.free(); print('CUT', o.name, len(kill))
# glass split: material names + glassify boxes
gm = bpy.data.materials.new('HERO_glass'); gm.use_nodes = True
gm.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.55, 0.62, 0.70, 1)
glassobjs = []
for o in list(meshes()):
    me = o.data
    isg = [any(g.lower() in (me.materials[p.material_index].name.lower() if me.materials and me.materials[p.material_index] else '') for g in S['glass'])
           for p in me.polygons]
    for (x0, x1, y0, y1, z0, z1) in S['glassify']:
        for i, p in enumerate(me.polygons):
            c = p.center
            if x0 < c.x < x1 and y0 < c.y < y1 and z0 < c.z < z1: isg[i] = True
    if not any(isg): continue
    g = o.copy(); g.data = me.copy(); g.name = 'GLASS_' + o.name; sc.collection.objects.link(g)
    for keepglass, ob in ((True, g), (False, o)):
        bm = bmesh.new(); bm.from_mesh(ob.data); bm.faces.ensure_lookup_table()
        kill = [f for f in bm.faces if isg[f.index] != keepglass]
        bmesh.ops.delete(bm, geom=kill, context='FACES'); bm.to_mesh(ob.data); bm.free()
    g.data.materials.clear(); g.data.materials.append(gm)
    for a in list(g.data.color_attributes): g.data.color_attributes.remove(a)
    while g.data.uv_layers: g.data.uv_layers.remove(g.data.uv_layers[0])
    glassobjs.append(g)
    if not len(o.data.polygons): bpy.data.objects.remove(o)
body = [o for o in meshes() if o not in glassobjs]
print('GLASS faces', sum(len(g.data.polygons) for g in glassobjs))
# scale k (about the origin: x/y centred, ground z=0 stays)
k = S['k']
for o in meshes(): o.data.transform(Matrix.Scale(k, 4))
L = V(S['L']) * k
# materials: base colour only (09: specGloss diffuseFactor x COLOR_0)
fac = {}
if S['colors']:
    g = json.load(open(src))
    for m in g['materials']:
        d = m.get('extensions', {}).get('KHR_materials_pbrSpecularGlossiness', {}).get('diffuseFactor')
        if d: fac[m['name']] = d
for m in bpy.data.materials:
    if m is gm: continue
    simplify(m, keep_vcol=S['colors'], size=1024, factor=fac.get(m.name), paint=PAINT.get(which, {}).get(m.name))
if S['colors']:
    for o in body:
        ca = o.data.color_attributes
        if len(ca): ca.active_color = ca[0]
else:
    for o in body:
        for a in list(o.data.color_attributes): o.data.color_attributes.remove(a)
# the driver-side outer handle: a ray from outside at the cockpit's sill height
dg = bpy.context.evaluated_depsgraph_get()
eye_b = L + V(EYE)
hy, hz = L.y - 0.10, L.z + 0.80
hit, loc, *_ = sc.ray_cast(dg, V((4.0, hy, hz)), V((-1, 0, 0)))
side = loc.x if hit else (mx.x - mn.x) * k / 2
side = max(side, L.x + CAB['x'] + 0.02)   # test 2: outside the cockpit volume (the cut opens the cabin sides)
handle_out_b = V((side + 0.05, hy, hz))
pads = [pad('HERO_handle_pad', (side + 0.012, hy, hz), (0.024, 0.18, 0.05), (0.15, 0.55, 1.0, 1))]
body += pads
# decimate the body under the cap (collapse keeps UVs and colours)
n = tris_of(body)
if n > S['tris']:
    ratio = S['tris'] / n * 0.98
    for o in body:
        if len(o.data.polygons) < 50: continue
        md = o.modifiers.new('dec', 'DECIMATE'); md.ratio = ratio; md.use_collapse_triangulate = True
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    for o in body:
        if o.modifiers:
            bpy.context.view_layer.objects.active = o
            bpy.ops.object.modifier_apply(modifier='dec')
print('BODY TRIS', n, '->', tris_of(body))
# glass under its cap (8000)
ng = tris_of(glassobjs)
if ng > 8000:
    for o in glassobjs:
        md = o.modifiers.new('dec', 'DECIMATE'); md.ratio = 8000 / ng * 0.98
        bpy.context.view_layer.objects.active = o; bpy.ops.object.modifier_apply(modifier='dec')
print('GLASS TRIS', ng, '->', tris_of(glassobjs))
# ---- test 2 CABIN cut (owner t1171u): nothing of the shell inside the cockpit volume
def tri_box(a, b, c, lo, hi):
    """Triangle / axis-aligned box overlap (separating axes)."""
    ctr = (lo + hi) * 0.5; h = (hi - lo) * 0.5
    v = [a - ctr, b - ctr, c - ctr]
    for i in range(3):
        if min(p[i] for p in v) > h[i] or max(p[i] for p in v) < -h[i]: return False
    e = [v[1] - v[0], v[2] - v[1], v[0] - v[2]]
    n = e[0].cross(v[2] - v[0])
    r = h.x * abs(n.x) + h.y * abs(n.y) + h.z * abs(n.z)
    if abs(n.dot(v[0])) > r: return False
    for ed in e:
        for ax in (V((1, 0, 0)), V((0, 1, 0)), V((0, 0, 1))):
            a_ = ax.cross(ed)
            if a_.length < 1e-12: continue
            pr = [a_.dot(p) for p in v]
            rr = h.x * abs(a_.x) + h.y * abs(a_.y) + h.z * abs(a_.z)
            if min(pr) > rr or max(pr) < -rr: return False
    return True
TOP = 50.0
cab_lo = L + V((-CAB['x'], CAB['y0'], CAB['z0'])); cab_hi = L + V((CAB['x'], CAB['y1'], TOP))
belt = L.z + BELT
# above the beltline over the cabin's length, the car's full width (side windows, canopy sides, pillars)
roof_lo = V((-50, L.y + CAB['y0'], belt)); roof_hi = V((50, L.y + CAB['y1'], TOP))
roofobjs = []; deleted = moved = 0; restored = [0]
from mathutils.geometry import closest_point_on_tri
def closest(p, a, b, c): return (closest_point_on_tri(p, a, b, c) - p).length
for o in [o for o in body if not o.name.startswith('HERO_handle_pad')]:
    bm = bmesh.new(); bm.from_mesh(o.data); bmesh.ops.triangulate(bm, faces=bm.faces[:])
    kill, roof = [], []
    for f in bm.faces:
        a, b, c = (l.vert.co.copy() for l in f.loops)
        inroof = tri_box(a, b, c, roof_lo, roof_hi)
        incab = tri_box(a, b, c, cab_lo, cab_hi)
        if incab and not inroof: kill.append(f)
        elif inroof: roof.append(f)
        elif incab: kill.append(f)
    # a face in the cabin that also reaches over the beltline: in the roof only when it lies wholly above the beltline
    # inside the cockpit width; otherwise (it reaches down into the cockpit) deleted
    keep_roof = []
    for f in roof:
        zs = [l.vert.co.z for l in f.loops]
        if tri_box(*(l.vert.co.copy() for l in f.loops), cab_lo, V((cab_hi.x, cab_hi.y, belt))): kill.append(f)
        else: keep_roof.append(f)
    # test 2b (owner pt1208u screenshot: a black hole and missing panels from outside): the cut faces are not deleted
    # any more - they go to the exterior-only Roof piece too, so from outside the car is whole (body + Roof = the
    # source car) and seated the cockpit volume is still clear (Roof is not drawn while inside).
    back = []
    R = S.get('restore') if RESTORE_ROOF else None
    if R:
        hub_b = L + V((0.36, -0.33, 0.74))
        mats = o.data.materials
        for f in keep_roof:
            mn_ = mats[f.material_index].name if len(mats) and mats[f.material_index] else ''
            if any(g.lower() in mn_.lower() for g in R['glass']): continue
            pts = [l.vert.co.copy() for l in f.loops]
            if closest(eye_b, *pts) < R['head'] or closest(hub_b, *pts) < R['wheel']: continue
            back.append(f)
        bk = set(f.index for f in back); keep_roof = [f for f in keep_roof if f.index not in bk]
        restored[0] += len(back)
    if OUTSIDE_WHOLE: keep_roof += kill; kill = []
    deleted += len(kill); moved += len(keep_roof)
    if keep_roof:
        g = o.copy(); g.data = o.data.copy(); g.name = 'ROOF_' + o.name; sc.collection.objects.link(g)
        bm2 = bm.copy(); bm2.faces.ensure_lookup_table()
        ids = set(f.index for f in keep_roof)
        bmesh.ops.delete(bm2, geom=[f for f in bm2.faces if f.index not in ids], context='FACES'); bm2.to_mesh(g.data); bm2.free()
        roofobjs.append(g)
    gone = set(f.index for f in kill) | set(f.index for f in keep_roof)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index in gone], context='FACES'); bm.to_mesh(o.data); bm.free()
body = [o for o in body if len(o.data.polygons)]
print('CABIN CUT deleted %d faces, roof (exterior only) %d faces, restored to the body %d' % (deleted, moved, restored[0]))
# ---- verification on the body as it ships (the roof hidden, as the game draws it seated)
def tris_world(objs):
    for o in objs:
        me = o.data; me.calc_loop_triangles()
        for t in me.loop_triangles: yield [me.vertices[i].co.copy() for i in t.vertices]
def closest(p, a, b, c):
    from mathutils.geometry import closest_point_on_tri
    return (closest_point_on_tri(p, a, b, c) - p).length
in_cab = sum(1 for t in tris_world([o for o in body if not o.name.startswith('HERO_handle_pad')]) if tri_box(*t, cab_lo, V((cab_hi.x, cab_hi.y, belt))))
in_head = 0; nearest = 9.0
for t in tris_world(body):
    d = closest(eye_b, *t); nearest = min(nearest, d)
    if d < 0.15: in_head += 1
print('CHECK body triangles in the cockpit volume below the beltline: %d; within 0.15 m of the eye: %d; nearest %.3f m' % (in_cab, in_head, nearest))
assert in_cab == 0 and in_head == 0
for o in roofobjs: o.hide_set(True); o.hide_viewport = True
bpy.context.view_layer.update(); dg = bpy.context.evaluated_depsgraph_get()
def rays(yaws, pitches, floor):
    blocked = total = 0
    for yaw in yaws:
        for pitch in pitches:
            d = V((math.sin(math.radians(-yaw)) * math.cos(math.radians(pitch)), -math.cos(math.radians(yaw)) * math.cos(math.radians(pitch)), math.sin(math.radians(pitch))))
            h, p, *_ = sc.ray_cast(dg, eye_b, d.normalized(), distance=8.0)
            total += 1
            if h and p.z > floor: blocked += 1
    return blocked, total
fwd = rays(range(-40, 41, 5), range(-14, 11, 3), L.z + 0.84)      # windscreen: above the dash top
left = rays(range(55, 116, 5), range(-10, 26, 5), belt)          # driver's window side: above the beltline
right = rays(range(-115, -54, 5), range(-10, 26, 5), belt)
up = rays(range(-40, 41, 20), range(35, 81, 15), belt)
print('VIEW blocked: forward %d/%d, left %d/%d, right %d/%d, up %d/%d (body only, roof hidden)' % (fwd + left + right + up))
for o in roofobjs: o.hide_viewport = False; o.hide_set(False)
mn2, mx2 = bbox(body + roofobjs)
turn_game(meshes())
os.makedirs(outdir + '/' + S['folder'], exist_ok=True)
assert not glassobjs
export(outdir + '/' + S['folder'] + '/model.glb', body, colors=S['colors'])
if roofobjs:
    os.makedirs(outdir + '/' + S['folder'] + '/Roof', exist_ok=True)
    export(outdir + '/' + S['folder'] + '/Roof/model.glb', roofobjs, colors=S['colors'])
def g3(b): return [round(-b.x, 4), round(b.z, 4), round(b.y, 4)]   # pre-turn Blender -> game
lay = dict(id=which, folder=S['folder'], length=round(mx2.y - mn2.y, 3), width=round(mx2.x - mn2.x, 3), height=round(mx2.z - mn2.z, 3),
           cockpit=g3(L), eye=g3(eye_b), hub=g3(L + V((0.36, -0.33, 0.74))), axis=[0, 0.342, 0.940],
           handleIn=g3(L + V((0.665, -0.22, 0.60))), handleOut=g3(handle_out_b),
           exit=[round(-(side + 0.80), 3), 0.0, round(eye_b.y, 3)], screen=g3(L + V((0, -0.5895 + 0.009, 0.69))),
           cabinLow=g3(V((cab_hi.x, cab_lo.y, cab_lo.z))), cabinHigh=g3(V((cab_lo.x, cab_hi.y, mx2.z))),
           blocked=list(fwd), side=[left[0] + right[0], left[1] + right[1]], up=list(up), deleted=deleted, roof=moved, restored=restored[0],
           inCab=in_cab, inHead=in_head, nearest=round(nearest, 3))
print('LAYOUT ' + json.dumps(lay))
json.dump(lay, open(outdir + '/' + S['folder'] + '/layout.json', 'w'), indent=1)
