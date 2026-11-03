# HERO VR 0.0.76 HARLEYS converter (Made by Agent Mulder). Blender 4.2 headless:
#   blender -b --python tools/bike076/convert.py -- --preset cap|wolverine --src <Sketchfab zip | scene.gltf | .glb>
#           --out <Models dir> [--preview <dir>] [--length m] [--rake deg] [--axis-offset m] [--steer-back m]
# Any motorcycle model in, the game's two parts and their layout out:
#   <out>/<folder>/model.glb             the bike without its steer part (<= BODY_TRIANGLES)
#   <out>/<folder>/Handlebars/model.glb  the steer part: bars, risers, fork, front wheel and what rides on them,
#                                        its origin the hub (the steer axis at the grips' height) (<= BARS_TRIANGLES)
#   <out>/<folder>/layout.json           bike075::parseLayout's numbers (size, eye, grips, steer axis, screen, tracker)
#   license.txt beside each model        the CC BY 4.0 credit and what was changed
# The frame: metres; in Blender +X right, +Y forward, +Z up (the glTF: x right, y up, -z forward), the ground under
# the axles' middle at the origin. Every step is MEASURED on the model; a model it cannot read (no two tyres, grips
# off the bars' circle, over budget after decimation) stops the conversion with the reason - nothing is guessed.
import sys, os, json, math, re, shutil, tempfile, zipfile, argparse
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy, bmesh
import numpy as np
from mathutils import Vector as V, Matrix
from mathutils.bvhtree import BVHTree
import presets as PR

ap = argparse.ArgumentParser()
ap.add_argument('--preset', required=True, choices=sorted(PR.PRESETS))
ap.add_argument('--src', required=True)
ap.add_argument('--out', required=True)
ap.add_argument('--preview')
ap.add_argument('--length', type=float)
ap.add_argument('--rake', type=float)
ap.add_argument('--axis-offset', type=float)
ap.add_argument('--steer-back', type=float)
ap.add_argument('--inspect', action='store_true', help='orient, split and preview only; write no models')
A = ap.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
P = dict(PR.PRESETS[A.preset])
for k, a in (('length', A.length), ('rake_deg', A.rake), ('axis_offset', A.axis_offset), ('steer_back', A.steer_back)):
    if a is not None: P[k] = a
P.setdefault('axis_offset', .05); P.setdefault('steer_back', .12)

def die(why):
    print('CONVERT FAILED: ' + why); sys.stdout.flush(); os._exit(2)

# ---------------------------------------------------------------- load
def find_scene(src):
    if src.lower().endswith('.zip'):
        d = tempfile.mkdtemp(prefix='bike076_')
        with zipfile.ZipFile(src) as z:
            for n in z.namelist():
                if os.path.isabs(n) or '..' in n.replace('\\', '/').split('/'): die('unsafe path in the zip: ' + n)
            z.extractall(d)
        src = d
    if os.path.isdir(src):
        found = []
        for r, _, fs in os.walk(src):
            for f in fs:
                if f.lower().endswith(('.gltf', '.glb')): found.append(os.path.join(r, f))
        if not found: die('no .gltf or .glb in ' + src)
        found.sort(key=lambda p: (p.count(os.sep), p))
        return found[0], os.path.dirname(found[0])
    return src, os.path.dirname(src)

scene_path, scene_dir = find_scene(A.src)
print('SOURCE', scene_path)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=scene_path)
bpy.context.view_layer.update()
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
if not meshes: die('no meshes')
for o in meshes:
    mw = o.matrix_world.copy(); o.parent = None; o.matrix_world = mw
    if o.data.users > 1: o.data = o.data.copy()
for o in [o for o in bpy.context.scene.objects if o.type != 'MESH']:
    bpy.data.objects.remove(o)
bpy.ops.object.select_all(action='DESELECT')
for o in meshes:
    o.select_set(True)
    o.modifiers.clear()
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
for o in meshes:
    o.data.transform(Matrix.Identity(4))

def tri_count(objs):
    return sum(sum(len(pl.vertices) - 2 for pl in o.data.polygons) for o in objs)

SRC_TRIS = tri_count(meshes)
print('SOURCE TRIANGLES', SRC_TRIS, 'OBJECTS', len(meshes))

def all_pts():
    return np.vstack([np.array([v.co[:] for v in o.data.vertices]) for o in meshes if len(o.data.vertices)])

def apply_all(M):
    for o in meshes: o.data.transform(M)
    bpy.context.view_layer.update()

# ---------------------------------------------------------------- islands (welded by position: UV seams split verts)
class Islands:
    def __init__(self):
        self.obj, self.poly, self.label = [], [], []
        allv, offs = [], [0]
        polys = []
        for oi, o in enumerate(meshes):
            co = np.array([v.co[:] for v in o.data.vertices]).reshape(-1, 3)
            allv.append(co); offs.append(offs[-1] + len(co))
            for p in o.data.polygons:
                polys.append((oi, p.index, [offs[oi] + i for i in p.vertices]))
        allv = np.vstack(allv)
        key = np.round(allv / 1e-4).astype(np.int64)
        _, weld = np.unique(key, axis=0, return_inverse=True)
        weld = weld.reshape(-1)
        parent = np.arange(weld.max() + 1)
        def find(a):
            r = a
            while parent[r] != r: r = parent[r]
            while parent[a] != r: parent[a], a = r, parent[a]
            return r
        for _, _, vs in polys:
            r0 = find(weld[vs[0]])
            for v in vs[1:]:
                r = find(weld[v])
                if r != r0: parent[r] = r0
        roots = np.array([find(weld[vs[0]]) for _, _, vs in polys])
        _, lab = np.unique(roots, return_inverse=True)
        self.n = int(lab.max()) + 1
        self.face_obj = np.array([p[0] for p in polys]); self.face_poly = np.array([p[1] for p in polys])
        self.face_label = lab.reshape(-1)
        self.face_verts = [p[2] for p in polys]
        self.allv = allv
        self.weld = weld
        # per island: bbox, centroid (area weighted), triangles
        cent = np.array([allv[vs].mean(0) for vs in self.face_verts])
        ntri = np.array([len(vs) - 2 for vs in self.face_verts])
        self.face_cent, self.face_tris = cent, ntri
        self.lo = np.full((self.n, 3), np.inf); self.hi = np.full((self.n, 3), -np.inf)
        for f, vs in enumerate(self.face_verts):
            l = lab[f]; q = allv[vs]
            self.lo[l] = np.minimum(self.lo[l], q.min(0)); self.hi[l] = np.maximum(self.hi[l], q.max(0))
        self.tris = np.bincount(lab, weights=ntri, minlength=self.n)
        w = np.bincount(lab, minlength=self.n).astype(float)
        self.cent = np.stack([np.bincount(lab, weights=cent[:, k], minlength=self.n) / np.maximum(w, 1) for k in range(3)], 1)

# ---------------------------------------------------------------- orient: length along Y, forward the bars' side, Z up
pts = all_pts()
lo, hi = pts.min(0), pts.max(0)
if hi[0] - lo[0] > hi[1] - lo[1]:
    apply_all(Matrix.Rotation(math.pi / 2, 4, 'Z'))
    pts = all_pts(); lo, hi = pts.min(0), pts.max(0)
c = (lo + hi) / 2
high = pts[pts[:, 2] > lo[2] + .7 * (hi[2] - lo[2])]
wf = np.ptp(high[high[:, 1] > c[1], 0]) if (high[:, 1] > c[1]).any() else 0
wr = np.ptp(high[high[:, 1] < c[1], 0]) if (high[:, 1] < c[1]).any() else 0
if P.get('forward') in ('-y',) or (P.get('forward') is None and wr > wf):
    apply_all(Matrix.Rotation(math.pi, 4, 'Z'))
print('ORIENT: the bars (%.2f wide up high) are at +Y (the other half %.2f)' % (max(wf, wr), min(wf, wr)))

# ---------------------------------------------------------------- upright (a preset's upright=True: a model posed on its stand)
def tyre_axles():
    """The two tyres' axles (unit, +X), by PCA of each tyre's points: the axle is the least spread direction."""
    isl = Islands(); pts = isl.allv; lo, hi = pts.min(0), pts.max(0); H = hi[2] - lo[2]; size = isl.hi - isl.lo
    big = [i for i in range(isl.n) if max(size[i, 1], size[i, 2]) > .25 * H and min(size[i, 1], size[i, 2]) > .8 * max(size[i, 1], size[i, 2])
           and size[i, 0] < .5 * max(size[i, 1], size[i, 2])]
    cy = (lo[1] + hi[1]) / 2; out = []
    vl = np.empty(len(pts), int)
    for f, vs in enumerate(isl.face_verts): vl[vs] = isl.face_label[f]
    for side in (1, -1):
        c = [i for i in big if (isl.cent[i, 1] - cy) * side > 0]
        if not c: die('upright: no %s tyre' % ('front' if side > 0 else 'rear'))
        t = max(c, key=lambda i: isl.tris[i]); q = pts[vl == t]; q = q - q.mean(0)
        w, v = np.linalg.eigh(q.T @ q); a = v[:, 0]; a = a if a[0] > 0 else -a
        out.append(a)
    return out
if P.get('upright'):
    for _ in range(3):
        aF, aR = tyre_axles()
        yaw = math.atan2(aR[1], aR[0]); lean = math.atan2(aR[2], math.hypot(aR[0], aR[1]))
        print('UPRIGHT: rear axle yaw %.2f deg, lean %.2f deg; front axle %s' % (math.degrees(yaw), math.degrees(lean), np.round(aF, 3)))
        if abs(yaw) < 1e-4 and abs(lean) < 1e-4: break
        apply_all(Matrix.Rotation(lean, 4, 'Y') @ Matrix.Rotation(-yaw, 4, 'Z'))

# ---------------------------------------------------------------- the tyres: round in Y-Z, thin in X, on the ground
isl = Islands()
pts = isl.allv; lo, hi = pts.min(0), pts.max(0); H = hi[2] - lo[2]
size = isl.hi - isl.lo
tyre = [i for i in range(isl.n) if size[i, 2] > .25 * H and abs(size[i, 1] - size[i, 2]) < .12 * size[i, 2]
        and size[i, 0] < .45 * size[i, 2] and isl.lo[i, 2] < lo[2] + .03 * H]
cy = (lo[1] + hi[1]) / 2
front = [i for i in tyre if isl.cent[i, 1] > cy]; rear = [i for i in tyre if isl.cent[i, 1] < cy]
if not front or not rear: die('could not find two tyres (round, thin, on the ground): %d front, %d rear' % (len(front), len(rear)))
tf = max(front, key=lambda i: size[i, 2]); tr = max(rear, key=lambda i: size[i, 2])
hubF = (isl.lo[tf] + isl.hi[tf]) / 2; hubR = (isl.lo[tr] + isl.hi[tr]) / 2
radF = size[tf, 2] / 2; radR = size[tr, 2] / 2
ground = min(isl.lo[tf, 2], isl.lo[tr, 2])
k = P['length'] / (hi[1] - lo[1])
mid = np.array([(hubF[0] + hubR[0]) / 2, (hubF[1] + hubR[1]) / 2, ground])
apply_all(Matrix.Scale(k, 4) @ Matrix.Translation(V(tuple(-mid))))
hubF = (hubF - mid) * k; hubR = (hubR - mid) * k; radF *= k; radR *= k
wheelBase = float(hubF[1] - hubR[1])
print('TYRES: front %.3f m across at y %.3f, rear %.3f at y %.3f; wheelbase %.3f m; scale %.4f'
      % (2 * radF, hubF[1], 2 * radR, hubR[1], wheelBase, k))
isl = Islands()
pts = isl.allv; lo, hi = pts.min(0), pts.max(0)
length, width, height = float(hi[1] - lo[1]), float(hi[0] - lo[0]), float(hi[2])
print('SIZE %.3f long, %.3f wide, %.3f high' % (length, width, height))

# ---------------------------------------------------------------- previews (numpy z-buffer of surface samples)
def surface_samples(objs_faces, per_m2=250000):
    """objs_faces: list of (points Nx3 triangles as (T,3,3) arrays, rgb). Returns samples, normals, colours."""
    S, N, C = [], [], []
    rng = np.random.default_rng(1)
    for tri, rgb in objs_faces:
        if not len(tri): continue
        e1, e2 = tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]
        cr = np.cross(e1, e2); area = np.linalg.norm(cr, axis=1) / 2
        n = cr / np.maximum(2 * area, 1e-12)[:, None]
        cnt = np.minimum(np.maximum((area * per_m2).astype(int), 1), 400)
        idx = np.repeat(np.arange(len(tri)), cnt)
        u, v = rng.random(len(idx)), rng.random(len(idx))
        f = u + v > 1; u[f], v[f] = 1 - u[f], 1 - v[f]
        S.append(tri[idx, 0] + e1[idx] * u[:, None] + e2[idx] * v[:, None]); N.append(n[idx])
        C.append(np.tile(np.array(rgb, float), (len(idx), 1)))
    return np.vstack(S), np.vstack(N), np.vstack(C)

def previews(S, N, C, views):
    job = os.path.join(A.preview, A.preset + '_job.npz')
    np.savez_compressed(job, S=S.astype(np.float32), N=N.astype(np.float32), C=C.astype(np.uint8), views=json.dumps(views))
    import subprocess
    subprocess.run(['python3', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'preview.py'), job], check=True,
                   env={k: v for k, v in os.environ.items() if not k.startswith('PYTHON')})
    os.remove(job)

# ---------------------------------------------------------------- the steer axis and the steer part
r = math.radians(P['rake_deg'])
axisDir = np.array([0., -math.sin(r), math.cos(r)])      # up and back
axisN = np.array([0., math.cos(r), math.sin(r)])         # across the axis, forward
axisP = hubF - axisN * P['axis_offset']                  # the axle sits the fork's offset ahead of the axis

def ahead(p):   # metres ahead of the steer axis, across it
    return (np.asarray(p) - axisP) @ axisN

def fan(vs):
    return [(vs[0], vs[i], vs[i + 1]) for i in range(1, len(vs) - 1)]

tri_v = np.array([t for vs in isl.face_verts for t in fan(vs)])
tri_face = np.repeat(np.arange(len(isl.face_verts)), isl.face_tris)
TRI = isl.allv[tri_v]

# An island turns with the bars when it is one of:
#  - the fork and what it carries: wholly within steer_back behind the steer axis and centred ahead of it
#  - the front wheel and its fender: inside the wheel's circle grown by .12 m (and no further back than that)
#  - the bars, grips, levers and mirrors: wholly above the tank's top (bars_min_z) and centred less than .5 m behind
#  - the risers, clamps and switch housings: centred within .1 m under that, reaching no more than .3 m behind,
#    and reaching to within .1 m of the axis or out beside the tank (the tank's filler caps sit there too)
#  less the preset's static_boxes (by centre): parts there stay on the frame
vl = np.empty(len(isl.allv), int)                     # every vertex's island
for f, vs in enumerate(isl.face_verts): vl[vs] = isl.face_label[f]
va = ahead(isl.allv)
def per_island(vals, how):
    out = np.full(isl.n, np.inf if how is np.minimum else -np.inf); how.at(out, vl, vals); return out
far_back = per_island(va, np.minimum); far_fwd = per_island(va, np.maximum)
cent_ahead = ahead(isl.cent)
fork = (cent_ahead > 0) & (far_back > -P['steer_back'])
wheel_far = per_island(np.hypot(isl.allv[:, 1] - hubF[1], isl.allv[:, 2] - hubF[2]), np.maximum)
wheel = (wheel_far < radF + .12) & (np.maximum(np.abs(isl.lo[:, 0]), np.abs(isl.hi[:, 0])) < .25) & (far_back > -(radF + .15))
if P.get('bars_min_z') is None:
    tank = (far_back > -.8) & (far_fwd < -.03) & (isl.lo[:, 2] > .5) & (isl.tris > 500) & (isl.hi[:, 0] - isl.lo[:, 0] > .25)
    if not tank.any(): die('could not find the tank (pass bars_min_z)')
    P['bars_min_z'] = float(isl.hi[tank, 2].max()) + .01   # the tank: the widest shell behind the head
bars = (isl.lo[:, 2] > P['bars_min_z']) & (cent_ahead > -.5)
head = ((isl.cent[:, 2] > P['bars_min_z'] - .1) & (far_back > -.3) & (cent_ahead > -.25)   # risers, clamps, levers;
        & ((far_fwd > -.1) | (np.abs(isl.cent[:, 0]) > .2)))                                 # not the tank's caps
steer = fork | wheel | bars | head
for lo_, hi_ in P.get('static_boxes', []):      # what the model fixes to the frame there (a tank console's gauges)
    steer &= ~np.all((isl.cent >= lo_) & (isl.cent <= hi_), 1)
print('STEER: rake %.1f deg, the axle %.3f m ahead of the axis, bars above z %.3f' % (P['rake_deg'], P['axis_offset'], P['bars_min_z']))
_first = np.unique(isl.face_label, return_index=True)
isl_obj = {int(l): meshes[isl.face_obj[f]].name for l, f in zip(*_first)}
if P.get('static_objects'):   # the model's own part names: what is bolted to the frame (a tank's dash, the neck)
    for l, nm in isl_obj.items():
        if re.search(P['static_objects'], nm): steer[l] = False
for i in P.get('steer_add', []): steer[i] = True
for i in P.get('steer_drop', []): steer[i] = False
face_steer = steer[isl.face_label]
if A.inspect:   # every island and why it turns, for tuning a preset's static_boxes / steer_add / steer_drop
    json.dump(dict(axisP=axisP.tolist(), axisDir=axisDir.tolist(), bars_min_z=P['bars_min_z'], islands=[dict(
        i=int(i), tris=int(isl.tris[i]), cent=np.round(isl.cent[i], 4).tolist(), lo=np.round(isl.lo[i], 4).tolist(),
        hi=np.round(isl.hi[i], 4).tolist(), ahead=round(float(cent_ahead[i]), 4), back=round(float(far_back[i]), 4),
        obj=isl_obj[i], steer=bool(steer[i]), why=[n for n, f in (('fork', fork), ('wheel', wheel), ('bars', bars), ('head', head)) if f[i]])
        for i in range(isl.n)]), open(os.path.join(A.preview, A.preset + '_islands.json'), 'w'))

def rotate_about_axis(p, deg):
    """The steer part turned by deg about the steer axis (Rodrigues), as the game turns it."""
    a = math.radians(deg); k = axisDir
    q = p - axisP
    return axisP + q * math.cos(a) + np.cross(k, q) * math.sin(a) + np.outer(q @ k, k) * (1 - math.cos(a))

# ---------------------------------------------------------------- what can be seen (orthographic z-buffers of tri ids)
def visibility(TRI, px=1400, elevations=(-8, 10, 30, 55, 80), azimuths=24, occluder=None):
    """Per triangle: how many of the views see it (any pixel). The views ring the bike from just under the
    horizon (a crouched look under the tank) to straight down; 1.7 mm pixels at the bike's length."""
    rng = np.random.default_rng(7)
    e1, e2 = TRI[:, 1] - TRI[:, 0], TRI[:, 2] - TRI[:, 0]
    area = np.linalg.norm(np.cross(e1, e2), axis=1) / 2
    cnt = np.minimum(np.maximum((area * 400000).astype(int), 3), 300)
    if occluder is not None: cnt[~occluder] = 0   # glass dropped from the model: it hides nothing
    tid = np.repeat(np.arange(len(TRI)), cnt)
    u, v = rng.random(len(tid)), rng.random(len(tid)); f = u + v > 1; u[f], v[f] = 1 - u[f], 1 - v[f]
    Sm = TRI[tid, 0] + e1[tid] * u[:, None] + e2[tid] * v[:, None]
    seen = np.zeros(len(TRI), np.int32)
    span = np.ptp(Sm, 0).max() * 1.05; c0 = Sm.mean(0)
    views = [(el, az) for el in elevations for az in np.linspace(0, 360, azimuths if el < 80 else 6, endpoint=False)]
    for el, az in views:
        d = np.array([math.cos(math.radians(el)) * math.cos(math.radians(az)), math.cos(math.radians(el)) * math.sin(math.radians(az)), math.sin(math.radians(el))])
        r = np.cross(d, [0, 0, 1.]) if abs(d[2]) < .99 else np.array([1., 0, 0]); r /= np.linalg.norm(r); up = np.cross(r, d)
        q = Sm - c0
        x = ((q @ r) / span * .5 + .5) * px; y = ((q @ up) / span * .5 + .5) * px; z = -(q @ d)   # small z: nearest
        ok = (x >= 0) & (x < px) & (y >= 0) & (y < px)
        pix = (y[ok].astype(np.int64) * px + x[ok].astype(np.int64))
        zz, tt = z[ok], tid[ok]
        o = np.lexsort((zz, pix))
        pix_s = pix[o]; first = np.ones(len(o), bool); first[1:] = pix_s[1:] != pix_s[:-1]
        seen[np.unique(tt[o][first])] += 1
    return seen, len(views)

if A.preview:
    os.makedirs(A.preview, exist_ok=True)
    ts = face_steer[tri_face]
    S, N, C = surface_samples([(TRI[~ts], (200, 200, 200)), (TRI[ts], (230, 60, 50))])
    am = [[list(map(float, axisP + axisDir * t)), [0, 0, 255]] for t in np.linspace(-.3, 1.2, 16)]
    views = [{'view': vw, 'path': os.path.join(A.preview, A.preset + '_split_' + vw + '.png'), 'marks': am}
             for vw in ('side', 'top', 'front')]
    previews(S, N, C, views)
    turned = TRI.copy(); sel = np.where(ts)[0]
    turned[sel] = rotate_about_axis(TRI[sel].reshape(-1, 3), 30).reshape(-1, 3, 3)
    S, N, C = surface_samples([(turned[~ts], (200, 200, 200)), (turned[ts], (230, 60, 50))])
    previews(S, N, C, [{'view': vw, 'path': os.path.join(A.preview, A.preset + '_turned30_' + vw + '.png'), 'marks': am}
                       for vw in ('side', 'top')])

if A.inspect:
    print('INSPECT: no models written'); sys.stdout.flush(); os._exit(0)

# ---------------------------------------------------------------- the source's materials (read from its own JSON)
def source_json(path):
    if path.lower().endswith('.glb'):
        with open(path, 'rb') as f:
            head = f.read(20)
            n = int.from_bytes(head[12:16], 'little')
            return json.loads(f.read(n))
    with open(path, encoding='utf-8') as f:
        return json.load(f)

G = source_json(scene_path)
def image_ref(tex):
    if tex is None: return None
    src = G['textures'][tex['index']].get('source')
    if src is None: return None
    im = G['images'][src]
    if 'uri' in im and not im['uri'].startswith('data:'):
        return {'file': os.path.join(scene_dir, im['uri'].replace('%20', ' '))}
    return {'glb': scene_path, 'image': src}

def lum(c): return .2126 * c[0] + .7152 * c[1] + .0722 * c[2]
def sat(c): return (max(c) - min(c)) / max(max(c), 1e-6)

MATS, DROP = {}, set()
for m in G.get('materials', []):
    name = m.get('name', '')
    alpha = m.get('alphaMode', 'OPAQUE')
    sg = m.get('extensions', {}).get('KHR_materials_pbrSpecularGlossiness')
    if sg is not None:
        # spec-gloss (the GTA-style '__env' / '__spec' / '_trans' materials) -> the game's metal-rough factors
        d = list(sg.get('diffuseFactor', [1, 1, 1, 1])); spec = max(sg.get('specularFactor', [1, 1, 1]))
        gloss = sg.get('glossinessFactor', 1.0); tex = sg.get('diffuseTexture'); env = '_env' in name
        if alpha == 'BLEND' and tex is None and sat(d[:3]) < .3:
            DROP.add(name); continue                       # colourless clear glass: the game draws no glass
        if alpha == 'BLEND' and tex is None: alpha, d[3] = 'OPAQUE', 1.0    # a coloured lens: solid, its colour
        rough = min(max(1 - gloss, .06), 1.0)
        if tex is not None:
            metal = min(max((spec - .2) / .6, 0), 1)
            if env: metal, rough = max(metal, .15), min(rough, .3)
            base = d
        elif spec >= .2:                                    # chrome, polished alloy, the bronze wheels
            metal, rough = (1.0 if spec >= .8 else .85), min(max(rough, .12), .6)
            if sat(d[:3]) > .3 and max(d[:3]) > .005:
                kk = .6 / max(lum(d[:3]), 1e-6); base = [min(c * kk, .95) for c in d[:3]] + [1]
            else:
                g = min(.95, .9 * spec + .05); base = [g, g, g, 1]
        elif env and max(d[:3]) < .1:                       # gloss black paint and powder coat
            base = [max(c, .025) for c in d[:3]] + [1]; metal, rough = .35, max(min(rough, .25), .2)
        else:
            base = d; metal, rough = 0.0, min(max(rough, .3), .95)
        MATS[name] = dict(base=base, metal=metal, rough=rough, alpha=alpha, image=image_ref(tex), mr=None)
    else:
        pb = m.get('pbrMetallicRoughness', {})
        if alpha == 'BLEND' and 'baseColorTexture' not in pb and sat(pb.get('baseColorFactor', [1] * 4)[:3]) < .3 and pb.get('baseColorFactor', [1] * 4)[3] < .6:
            DROP.add(name); continue
        MATS[name] = dict(base=pb.get('baseColorFactor', [1, 1, 1, 1]), metal=pb.get('metallicFactor', 1.0),
                          rough=pb.get('roughnessFactor', 1.0), alpha=alpha, image=image_ref(pb.get('baseColorTexture')),
                          mr=image_ref(pb.get('metallicRoughnessTexture')))
print('MATERIALS: %d (%d clear glass dropped: %s)' % (len(MATS), len(DROP), ', '.join(sorted(DROP)) or '-'))

def mat_key(n):
    return re.sub(r'\.\d{3}$', '', n)
face_mat = np.empty(len(isl.face_verts), object)
for oi, o in enumerate(meshes):
    sel = np.where(isl.face_obj == oi)[0]
    if not len(sel): continue
    mi = np.zeros(len(o.data.polygons), np.int32); o.data.polygons.foreach_get('material_index', mi)
    names = [mat_key(s.material.name) if s.material else '' for s in o.material_slots] or ['']
    face_mat[sel] = [names[min(mi[p], len(names) - 1)] for p in isl.face_poly[sel]]
face_drop = np.array([n in DROP for n in face_mat])

# ---------------------------------------------------------------- remove what cannot be seen
seen, nviews = visibility(TRI, occluder=~face_drop[tri_face])
face_seen = np.zeros(len(isl.face_verts), np.int32); np.maximum.at(face_seen, tri_face, seen)
vis_v = np.zeros(isl.weld.max() + 1, bool)
for f in np.where(face_seen > 0)[0]: vis_v[isl.weld[isl.face_verts[f]]] = True
keep = np.array([vis_v[isl.weld[vs]].any() for vs in isl.face_verts]) & ~face_drop   # seen, or touching what is
print('VISIBLE: %d of %d triangles seen from %d views (ortho, 1.7 mm pixels); kept with their neighbours %d; never seen, removed %d'
      % ((seen > 0).sum(), len(TRI), nviews, isl.face_tris[keep].sum(), isl.face_tris[~keep].sum()))

# ---------------------------------------------------------------- split into the body and the steer part
def delete_faces(o, mask):
    bm = bmesh.new(); bm.from_mesh(o.data); bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.faces[i] for i in np.where(mask)[0]], context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(o.data); bm.free()

body, steerp = [], []
for oi, o in enumerate(meshes):
    n = len(o.data.polygons)
    st = np.zeros(n, bool); kp = np.zeros(n, bool); sn = np.zeros(n, np.int32)
    sel = np.where(isl.face_obj == oi)[0]
    st[isl.face_poly[sel]] = face_steer[sel]; kp[isl.face_poly[sel]] = keep[sel]; sn[isl.face_poly[sel]] = face_seen[sel]
    a = o.data.attributes.new('seen', 'INT', 'FACE'); a.data.foreach_set('value', sn)
    if (kp & st).any():
        s2 = o.copy(); s2.data = o.data.copy(); bpy.context.scene.collection.objects.link(s2)
        delete_faces(s2, ~(kp & st)); steerp.append(s2)
    if (kp & ~st).any():
        delete_faces(o, ~(kp & ~st)); body.append(o)
    else:
        bpy.data.objects.remove(o)

def tris_of(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)

# ---------------------------------------------------------------- decimate to the budget, the rarely seen first
def prepare(o):
    bm = bmesh.new(); bm.from_mesh(o.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bm.to_mesh(o.data); bm.free()
    sn = np.zeros(len(o.data.polygons), np.int32); o.data.attributes['seen'].data.foreach_get('value', sn)
    w = np.zeros(len(o.data.vertices)); 
    for p in o.data.polygons:
        for v in p.vertices: w[v] = max(w[v], sn[p.index])
    vg = o.vertex_groups.new(name='rarely_seen')   # weight 1: seen from few views, decimated first
    wt = 1 - np.clip(w / (.25 * nviews), 0, 1)
    for val in np.unique(np.round(wt, 2)):
        vg.add([int(i) for i in np.where(np.round(wt, 2) == val)[0]], float(val), 'REPLACE')

def decimate(objs, budget, what):
    before = sum(tris_of(o) for o in objs)
    if before <= budget:
        print('DECIMATE %s: %d triangles, within %d - none' % (what, before, budget)); return before
    for o in objs: prepare(o)
    big = [o for o in objs if tris_of(o) > 300]
    small = sum(tris_of(o) for o in objs if o not in big)
    for o in big:
        m = o.modifiers.new('decimate', 'DECIMATE'); m.decimate_type = 'COLLAPSE'; m.use_collapse_triangulate = True
        m.vertex_group = 'rarely_seen'; m.vertex_group_factor = 1.0
    def count(r):
        for o in big: o.modifiers['decimate'].ratio = r
        dg = bpy.context.evaluated_depsgraph_get(); dg.update()
        return small + sum(len(o.evaluated_get(dg).data.polygons) for o in big)
    lo_, hi_ = 0.02, 1.0
    for _ in range(14):
        r = (lo_ + hi_) / 2
        if count(r) <= budget: lo_ = r
        else: hi_ = r
    got = count(lo_)
    for o in big:
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.modifier_apply(modifier='decimate')
    after = sum(tris_of(o) for o in objs)
    print('DECIMATE %s: %d -> %d triangles (collapse ratio %.3f on the %d larger parts, weighted to the rarely seen; budget %d)'
          % (what, before, after, lo_, len(big), budget))
    if after > budget: die('%s over budget after decimation: %d > %d' % (what, after, budget))
    return after

trisBody = decimate(body, PR.BODY_TRIANGLES - 500, 'BODY')
trisBars = decimate(steerp, PR.BARS_TRIANGLES - 300, 'STEER PART')

def smooth(o):
    o.data.shade_smooth()
    if o.data.has_custom_normals:
        bpy.context.view_layer.objects.active = o; o.select_set(True)
        with bpy.context.temp_override(object=o, active_object=o, selected_editable_objects=[o]):
            bpy.ops.mesh.customdata_custom_splitnormals_clear()
    bm = bmesh.new(); bm.from_mesh(o.data)
    for e in bm.edges:
        if len(e.link_faces) == 2 and e.calc_face_angle(0) > math.radians(35): e.smooth = False
    bm.to_mesh(o.data); bm.free()
for o in body + steerp: smooth(o)

# ---------------------------------------------------------------- measure the layout on what ships
def verts_of(objs):   # the vertices the triangles use
    out = []
    for o in objs:
        co = np.array([v.co[:] for v in o.data.vertices]).reshape(-1, 3)
        used = np.unique(np.array([i for p in o.data.polygons for i in p.vertices], int))
        if len(co) and np.ptp(co, 0).max() > 2.4: print('  (loose far vertices in %s: %d of %d used)' % (o.name, len(used), len(co)))
        if len(used): out.append(co[used])
    return np.vstack(out)
BV, SV = verts_of(body), verts_of(steerp)
allP = np.vstack([BV, SV]); lo, hi = allP.min(0), allP.max(0)
length, width, height = float(hi[1] - lo[1]), float(2 * max(abs(lo[0]), abs(hi[0]))), float(hi[2])
# the grips: the ends of the handlebar tube (the steer part's one island reaching from the left of the bars to the
# right, low and long; the mirrors, levers and switch housings are their own islands and are not its ends)
tube = [i for i in range(isl.n) if steer[i] and isl.lo[i, 0] < -.15 and isl.hi[i, 0] > .15 and isl.hi[i, 2] - isl.lo[i, 2] < .3
        and isl.lo[i, 2] > P['bars_min_z'] - .08]
if tube:
    ti = max(tube, key=lambda i: isl.hi[i, 0] - isl.lo[i, 0]); tube = [ti]
else:   # a bar made in two halves (split at its clamp): the one part, by the model's own name, whose islands span it
    parts = {}
    for i in range(isl.n):
        if steer[i] and isl.hi[i, 2] - isl.lo[i, 2] < .3 and isl.lo[i, 2] > P['bars_min_z'] - .08 and isl.hi[i, 0] - isl.lo[i, 0] > .12:
            parts.setdefault(isl_obj[i], []).append(i)
    span = {k: (min(isl.lo[i, 0] for i in v), max(isl.hi[i, 0] for i in v)) for k, v in parts.items()}
    ok = [n for n, (a, b) in span.items() if a < -.15 and b > .15 and len(parts[n]) >= 2]
    if not ok: die('no handlebar tube (one steer-part island, or one part\'s halves, from the left of the bars to the right)')
    tube = parts[max(ok, key=lambda n: span[n][1] - span[n][0])]
TV = isl.allv[np.isin(vl, tube)]
print('BARS: the tube is island(s) %s (%s), %.3f m end to end' % (tube, isl_obj[tube[0]], np.ptp(TV[:, 0])))
def grip(side):
    # the tube's end and its direction there; the grip rides past it: the steer part's farthest point along that
    # line within 3 cm of it (the rubber grip and its end cap, not the lever ahead of it or the mirror's stem)
    end = side * (side * TV[:, 0]).max()
    E = TV[side * TV[:, 0] > side * end - .015].mean(0); I = TV[np.abs(TV[:, 0] - (end - side * .10)) < .015].mean(0)
    D = (E - I) / np.linalg.norm(E - I)
    t = (SV - E) @ D; perp = np.linalg.norm((SV - E) - np.outer(t, D), axis=1)
    on = (t > -.01) & (t < .25) & (perp < .03)
    tmax = float(t[on].max()) if on.any() else 0.0
    g = E + D * max(tmax - .06, 0.0)
    print('GRIP %s: the tube ends at x %.3f, the grip reaches %.3f m past it; held at %s' % ('right' if side > 0 else 'left', end, tmax, np.round(g, 3)))
    return g
gL, gR = grip(-1), grip(1)
gz = (gL[2] + gR[2]) / 2
hubB = axisP + axisDir * ((gz - axisP[2]) / axisDir[2])                    # on the axis at the grips' height
gL[2] = gR[2] = gz
def game(p): return [round(float(p[0]), 4), round(float(p[2]), 4), round(float(-p[1]), 4)]
gripRadius = float(np.mean([math.hypot(g[0] - hubB[0], g[1] - hubB[1]) for g in (gL, gR)]))
# the seat: the top profile's lowest point between the rear axle and the tank
ys = np.arange(hubR[1] + .15, axisP[1] - .45, .02)
prof = []
for y in ys:
    q = BV[(np.abs(BV[:, 0]) < .12) & (np.abs(BV[:, 1] - y) < .01)]
    prof.append(q[:, 2].max() if len(q) else np.inf)
prof = np.array(prof)
if not np.isfinite(prof).any(): die('no seat found')
si = int(np.argmin(prof)); seatY, seatZ = float(ys[si]), float(prof[si])
eyeB = np.array([0, seatY + .05, seatZ + .72])
# the dash screen and the tracker: on the frame (they never turn), centred, the screen upright facing the rider, the
# tracker level over it. Placed as low and as near the bars as they go while clear (1.5 cm) of the body and of the
# steer part swept through its whole turn (bike075: the bars turn half a radian either way; checked to 32 degrees).
VOX = .01
def occupancy(tris_sets):
    S_, _, _ = surface_samples([(t_, (0, 0, 0)) for t_ in tris_sets], per_m2=60000)
    return set(map(tuple, np.floor(S_ / VOX).astype(np.int64)))
def tris_world(objs, shift=np.zeros(3)):
    return np.array([[o.data.vertices[i].co[:] for i in t.vertices] for o in objs for t in o.data.loop_triangles]).reshape(-1, 3, 3) + shift
STRI = tris_world(steerp)
if os.environ.get('SCREEN_DUMP'): np.savez(os.environ['SCREEN_DUMP'], body=tris_world(body), steer=STRI, axisP=axisP, axisDir=axisDir, gz=gz, eye=eyeB, gL=gL, gR=gR)
occ = occupancy([tris_world(body)] + [rotate_about_axis(STRI.reshape(-1, 3), a_).reshape(-1, 3, 3) for a_ in range(-32, 33, 4)])
gx, gz_ = np.meshgrid(np.arange(-.075, .0751, .01), np.arange(-.0475, .0475 + .0625 + .04, .01))
panel = np.stack([gx.ravel(), np.zeros(gx.size), gz_.ravel()], 1)             # the screen and the tracker over it
def clear(c):
    q = np.floor((panel + c) / VOX).astype(np.int64)
    for dx in (-1, 0, 1):
        for dy in (-2, -1, 0, 1):
            for dz in (-1, 0, 1):
                if any((a_ + dx, b_ + dy, c_ + dz) in occ for a_, b_, c_ in q): return False
    return True
screenB, best = None, None
for dz_ in np.arange(-.05, .40, .01):
    for sb in np.arange(-.02, .30, .01):
        z_ = gz + dz_
        c = axisP + axisDir * ((z_ - axisP[2]) / axisDir[2]); c[0] = 0; c[1] -= sb
        score = dz_ + .5 * sb
        if best is not None and score >= best: continue
        if c[1] - .03 > eyeB[1] and clear(c): screenB, best = c, score
if screenB is None: die('no room for the dash screen near the bars')
print('SCREEN: centre %s, %.3f m over the grips, %.3f m behind the steer axis' % (np.round(screenB, 3), screenB[2] - gz,
      (axisP + axisDir * ((screenB[2] - axisP[2]) / axisDir[2]))[1] - screenB[1]))
layout = dict(source='%s by %s (%s)' % (P['title'], P['artist'], PR.LICENCE), scale=float(k), tyre=round(2 * float(radF), 4),
              length=round(length, 4), width=round(width, 4), height=round(height, 4), wheelBase=round(wheelBase, 4),
              seatTop=round(seatZ, 4), hub=game(hubB), steerAxis=[0.0, round(math.cos(r), 5), round(math.sin(r), 5)],
              gripLeft=game(gL), gripRight=game(gR), gripRadius=round(gripRadius, 4), eye=game(eyeB), handleOut=game(gL),
              handleIn=game([-(width / 2 + .05), seatY, seatZ - .1]), exit=game([-(width / 2 + .55), seatY, 0]),
              screen=game(screenB), trisBody=int(trisBody), trisBars=int(trisBars))
layout['tracker'] = [layout['screen'][0], round(layout['screen'][1] + .085 * .5 + .02, 4), layout['screen'][2]]
print('LAYOUT', json.dumps(layout))

# ---------------------------------------------------------------- export (placeholder images: the post-pass puts in the real ones)
def plain_materials():
    for mat in bpy.data.materials:
        key = mat_key(mat.name)
        mat.use_nodes = True; nt = mat.node_tree; nt.nodes.clear()
        out = nt.nodes.new('ShaderNodeOutputMaterial'); bs = nt.nodes.new('ShaderNodeBsdfPrincipled')
        nt.links.new(bs.outputs[0], out.inputs[0])
        im = bpy.data.images.new('m_' + key, 1, 1); im.pixels = [1, 1, 1, 1]
        tx = nt.nodes.new('ShaderNodeTexImage'); tx.image = im
        nt.links.new(tx.outputs['Color'], bs.inputs['Base Color'])
        mat.name = key if key not in [m.name for m in bpy.data.materials if m != mat] else mat.name
plain_materials()

def export(objs, path):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=True, export_yup=True, export_apply=True,
                              export_texcoords=True, export_normals=True, export_materials='EXPORT', export_image_format='AUTO',
                              export_animations=False, export_skins=False, export_morph=False, export_cameras=False, export_lights=False)

out_dir = os.path.join(A.out, *P['folder'].split('/'))
work = tempfile.mkdtemp(prefix='bike076_out_')
export(body, os.path.join(work, 'body.glb'))
for o in steerp: o.data.transform(Matrix.Translation(V(tuple(-hubB))))   # the steer part's origin: the hub
export(steerp, os.path.join(work, 'bars.glb'))
mats_job = os.path.join(work, 'mats.json')
with open(mats_job, 'w') as f:
    json.dump(dict(mats=MATS, texture_px=PR.TEXTURE_PX, jobs=[[os.path.join(work, 'body.glb'), os.path.join(out_dir, 'model.glb')],
                                                            [os.path.join(work, 'bars.glb'), os.path.join(out_dir, 'Handlebars', 'model.glb')]]), f)
import subprocess
os.makedirs(os.path.join(out_dir, 'Handlebars'), exist_ok=True)
subprocess.run(['python3', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'glbfix.py'), mats_job], check=True,
               env={k_: v_ for k_, v_ in os.environ.items() if not k_.startswith('PYTHON')})
with open(os.path.join(out_dir, 'layout.json'), 'w') as f: json.dump(layout, f, indent=1)
for d in (out_dir, os.path.join(out_dir, 'Handlebars')):
    with open(os.path.join(d, 'license.txt'), 'w', newline='\r\n') as f: f.write(PR.credit_text(P))
shutil.rmtree(work)
print('WROTE %s: body %d triangles, steer part %d' % (out_dir, trisBody, trisBars))

if A.preview:
    S, N, C = surface_samples([(np.array([[o.matrix_world @ o.data.vertices[i].co for i in t.vertices] for o in body for t in o.data.loop_triangles] or np.zeros((0, 3, 3))), (200, 200, 200))])
    marks = [[list(map(float, p)), c] for p, c in ((gL, [0, 200, 0]), (gR, [0, 200, 0]), (hubB, [0, 0, 255]), (eyeB, [255, 0, 255]),
             (screenB, [255, 160, 0]), (screenB + np.array([0, 0, .0625]), [255, 255, 0]))]
    S2, N2, C2 = surface_samples([(np.array([[(o.matrix_world @ o.data.vertices[i].co)[:] for i in t.vertices] for o in steerp for t in o.data.loop_triangles]) + hubB, (230, 60, 50))])
    S, N, C = np.vstack([S, S2]), np.vstack([N, N2]), np.vstack([C, C2])
    previews(S, N, C, [{'view': vw, 'path': os.path.join(A.preview, A.preset + '_final_' + vw + '.png'), 'marks': marks} for vw in ('side', 'top', 'front')]
             + [{'view': 'eye', 'eye': list(map(float, eyeB)), 'look': list(map(float, hubB)), 'path': os.path.join(A.preview, A.preset + '_final_eye.png'), 'marks': marks}])
sys.stdout.flush(); os._exit(0)
