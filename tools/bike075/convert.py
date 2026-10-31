# HERO VR 0.0.75 MOTORCYCLE converter (Made by Agent Mulder). Blender 4.2 headless:
#   blender -b --python tools/bike075/convert.py -- <scene.gltf> <out Models dir>
# The source: "Low-Poly Sci-Fi Motorcycle - Cyberpunk Theme" by Mert Tetik (CC BY 4.0,
# https://sketchfab.com/3d-models/low-poly-sci-fi-motorcycle-cyberpunk-theme-50c38758e5134a0b8ffa49780e7fe228).
# Writes <out>/Vehicle/Motorcycle/model.glb (the bike without its handlebar yoke), <out>/Vehicle/Motorcycle/
# Handlebars/model.glb (the yoke: the steering pivot at the origin, x right, y up, -z forward, the grips at +z),
# layout.json (the numbers bike075.cpp carries) and the artist's licence beside each. The frame (orient_common):
# metres, x right, y up, -z forward, the ground under the axles' middle at the origin. Every number here is
# MEASURED on the model (CONFIRMED on the model, CANDIDATE in game); the pivot's place along the yoke and the
# seated eye's height over the seat are choices (CANDIDATE), marked below.
import sys, os, json, math, shutil
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy, bmesh
import numpy as np
from mathutils import Vector as V
from orient_common import load, world_pts, comps_of

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
SRC = argv[0] if argv else '/tmp/bikes/low-poly_sci-fi_motorcycle_-_cyberpunk_theme/scene.gltf'
OUT = argv[1] if len(argv) > 1 else '/workspace/hero-0.0.75/bikeconv/Models'
YOKE_SIZES = (289, 105)      # the Front mesh's two yoke pieces (vertex counts; rendered and checked)
PIVOT_FWD = 0.40             # CANDIDATE: the steering pivot this far ahead of the axles' middle (the yoke meets the cowl)
EYE_OVER_SEAT = 0.73         # CANDIDATE: a seated rider's eye over the seat's top
EYE_BACK = 0.30              # CANDIDATE: the eye this far behind the axles' middle (over the seat's front half)
TEX = 1024

def to_game(p):              # Blender (+X right, +Y forward, +Z up) -> the vehicle frame (x right, y up, -z forward)
    return [round(float(p[0]), 4), round(float(p[2]), 4), round(float(-p[1]), 4)]

obs, info = load(SRC)
print('ORIENT', info)

# ---- split the yoke out of the Front mesh ----
F = obs['Front']
comps = comps_of(F)
yoke = sorted(set(sum([c for c in comps if len(c) in YOKE_SIZES], [])))
assert len(yoke) == sum(YOKE_SIZES), 'yoke pieces not found (%d)' % len(yoke)
P = world_pts(F)
Q = P[yoke]
grips = []
for side in (-1, 1):
    g = Q[(Q[:, 0] * side > 0.15) & (Q[:, 1] < -0.03)]
    grips.append(g.mean(0))
gripZ = float(np.mean([g[2] for g in grips]))
pivot = np.array([0.0, PIVOT_FWD, gripZ])
bars = F.copy(); bars.data = F.data.copy(); bpy.context.scene.collection.objects.link(bars); bars.name = 'Handlebars'
keep = set(yoke)
for o, drop_yoke in ((F, True), (bars, False)):
    bm = bmesh.new(); bm.from_mesh(o.data); bm.verts.ensure_lookup_table()
    gone = [v for v in bm.verts if (v.index in keep) == drop_yoke]
    bmesh.ops.delete(bm, geom=gone, context='VERTS'); bm.to_mesh(o.data); bm.free()
bars.data.transform(__import__('mathutils').Matrix.Translation(V(tuple(-pivot))))

# ---- materials: the base colour texture only (no normal / metallic maps; the overlay reads base colour) ----
def upstream_image(sock):
    for l in sock.links:
        n = l.from_node
        if n.type == 'TEX_IMAGE': return n.image
        for s in n.inputs:
            im = upstream_image(s)
            if im: return im
    return None
for m in bpy.data.materials:
    if not m.use_nodes: continue
    nt = m.node_tree
    b = [n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED']
    im = upstream_image(b[0].inputs['Base Color']) if b else None
    col = tuple(b[0].inputs['Base Color'].default_value) if b else (.5, .5, .5, 1)
    for n in list(nt.nodes): nt.nodes.remove(n)
    out = nt.nodes.new('ShaderNodeOutputMaterial'); p = nt.nodes.new('ShaderNodeBsdfPrincipled')
    nt.links.new(p.outputs[0], out.inputs[0]); p.inputs['Roughness'].default_value = .5
    if im:
        if max(im.size) > TEX:
            w, h = im.size; f = TEX / max(w, h); im.scale(max(1, int(w * f)), max(1, int(h * f)))
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = im; nt.links.new(t.outputs[0], p.inputs['Base Color'])
    else:
        p.inputs['Base Color'].default_value = col
    m.blend_method = 'OPAQUE'

def tris(objs):
    return sum(sum(len(pl.vertices) - 2 for pl in o.data.polygons) for o in objs)

def export(path, objs):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=True, export_apply=True, export_yup=True,
        export_texcoords=True, export_normals=True, export_tangents=False, export_materials='EXPORT', export_image_format='JPEG',
        export_jpeg_quality=85, export_vertex_color='NONE', export_animations=False, export_skins=False,
        export_morph=False, export_cameras=False, export_lights=False, export_extras=False)
    print('EXPORT', path, os.path.getsize(path), 'TRIS', tris(objs))

body = [obs['Core'], F, obs['Seat'], obs['Wheels']]
bpy.context.view_layer.update()

# ---- measure (the body; ray casts straight down for the seat's and the cowl's tops) ----
dg = bpy.context.evaluated_depsgraph_get()
sc = bpy.context.scene
bars.hide_set(True)
def top_at(x, y, z0=3.0):
    h, p, *_ = sc.ray_cast(dg, V((x, y, z0)), V((0, 0, -1)), distance=5.0)
    return float(p.z) if h else None
seat_tops = [top_at(0, y) for y in np.linspace(-0.45, -0.05, 9)]
seat_top = max(t for t in seat_tops if t is not None and t < 0.8)
bars.hide_set(False)
allp = np.vstack([world_pts(o) for o in body] + [world_pts(bars) + pivot])
lo, hi = allp.min(0), allp.max(0)
length, width, height = float(hi[1] - lo[1]), float(hi[0] - lo[0]), float(hi[2])
eye = np.array([0, -EYE_BACK, seat_top + EYE_OVER_SEAT])
gl, gr = grips[0], grips[1]
rho = float(math.hypot(gr[0] - pivot[0], gr[1] - pivot[1]))
# the screen: over the yoke's middle, 4 cm above the bars' top there (CANDIDATE place; not inside the cowl)
yk = Q[(abs(Q[:, 0]) < .05)]
screen = np.array([0, float(np.clip(pivot[1] - .25, yk[:, 1].min(), yk[:, 1].max())), float(yk[:, 2].max()) + .04])
lay = dict(
    source='Low-Poly Sci-Fi Motorcycle - Cyberpunk Theme by Mert Tetik (CC BY 4.0)',
    scale=info['scale'], tyre=info['tyre'], wheelBase=round(info['wheelbase'], 4),
    length=round(length, 4), width=round(width, 4), height=round(height, 4),
    seatTop=round(seat_top, 4), seatTops=[None if t is None else round(t, 4) for t in seat_tops],
    hub=to_game(pivot), gripLeft=to_game(gl), gripRight=to_game(gr), gripRadius=round(rho, 4),
    eye=to_game(eye), handleOut=to_game(gl),
    handleIn=to_game([-(width / 2), -EYE_BACK - .10, seat_top + .05]),
    exit=to_game([-(width / 2 + .65), -EYE_BACK, 0]),
    screen=to_game(screen),
    trisBody=tris(body), trisBars=tris([bars]))
print('LAYOUT ' + json.dumps(lay))
d = os.path.join(OUT, 'Vehicle', 'Motorcycle')
export(os.path.join(d, 'model.glb'), body)
export(os.path.join(d, 'Handlebars', 'model.glb'), [bars])
json.dump(lay, open(os.path.join(d, 'layout.json'), 'w'), indent=1)
lic = os.path.join(os.path.dirname(SRC), 'license.txt')
for t in (d, os.path.join(d, 'Handlebars')):
    if os.path.exists(lic): shutil.copy(lic, os.path.join(t, 'license.txt'))
