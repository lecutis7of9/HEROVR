"""HERO VR 0.0.72 MODEL CHECK: every shipped model drawn as the game draws it, next to its source.

Runs spidy_model_dump (the game's own glTF reader, texture packing and model drawers, into the textured
model sink), rasterises the dumped overlay vertices with the dumped texture-array layers (u, v, layer
per vertex; the colour x the sRGB texture, as the overlay's textured pass), renders the source glTF
with its own textures and the same light, and writes <slot>.png side by side. The GPU itself is not run:
this is the overlay's pipeline in numpy (orthographic, z-buffered).

  python3 tools/models072/model_check.py [Models/] [sources/] [out/]
Returns non-zero when a check fails (see check())."""
import json, os, struct, subprocess, sys, tempfile, shutil
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(REPO, 'tools', 'models007'))
sys.path.insert(0, HERE)
import gltf_io  # noqa: E402

DEFAULT_MODELS = os.path.join(REPO, 'build', 'h7out', 'Models')
DEFAULT_SOURCES = '/workspace/hero-0.0.7-models'
DEFAULT_OUT = '/workspace/hero-0.0.72/model-check'
DUMP = os.path.join(REPO, 'build', 'gate', 'spidy_model_dump')

# slot -> (source folder, kind). The sawed-off's two parts are cut from one source model.
SOURCES = {
    'Batarang/EIRA': ('batarang', 'weapon'),
    'Batarang/CLOONEY': ('batarang_clooney', 'weapon'),
    'Batarang/JOKER': ('batarang_joker', 'weapon'),
    'Pistol': ('9mm_pistol', 'weapon'),
    'SawedOff/Frame': ('sawed-off_shotgun', 'weapon'),  # parts of one source: no shape match (PART)
    'SawedOff/Barrels': ('sawed-off_shotgun', 'weapon'),
    'Baton/Left': ('daredevils_baton', 'weapon'),
    'Baton/Right': ('daredevils_baton', 'weapon'),
    'Shield': ('captain_america_shield', 'shield'),
    'GrappleGun': ('grapple_gun', 'weapon'),
}
# The least a shipped model may have (it may keep fewer only when its source has fewer), by kind, and
# never under MIN_SHARE of its source.
FLOORS = {'weapon': 3000, 'prop': 3000, 'shield': 5000, 'vehicle': 20000, 'vehicle_part': 1500, 'body': 10000}
MIN_SHARE = .30
# slots cut from part of their source (their shape is not the whole source's)
PART = {'SawedOff/Frame', 'SawedOff/Barrels', 'Baton/Left', 'Baton/Right'}
# a whole model's shape (the area-weighted spread along its own axes, as ratios to the longest, so the
# scale and the frame do not matter) may differ from its source's by at most this: a decimation that
# tore the surface (the 0.0.72 shield's first conversion kept a quarter disc) fails here
SHAPE_TOLERANCE = .12
SIZE = 512
LIGHT = np.array([.3, 1, .25]) / np.linalg.norm([.3, 1, .25])


def s2l(c):
    c = np.asarray(c, np.float64)
    return np.where(c <= .04045, c / 12.92, ((c + .055) / 1.055) ** 2.4)


def l2s(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= .0031308, 12.92 * c, 1.055 * c ** (1 / 2.4) - .055)


# ---- the dump -----------------------------------------------------------------------------------------
def run_dump(models, work, size=SIZE, dump=DUMP):
    cache = os.path.join(work, 'cache') + '/'
    out = os.path.join(work, 'dump') + '/'
    os.makedirs(cache, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    root = models.rstrip('/') + '/'
    for _ in range(3):
        subprocess.run([dump, root, cache, out, str(size)], check=True, capture_output=True)
        summary = json.load(open(out + 'summary.json'))
        if not summary['misses']:
            return out, summary
        for h in summary['misses']:  # PNG / JPEG as WIC would (straight RGBA8, rows from the top)
            im = Image.open(cache + h + '.enc').convert('RGBA')
            with open(cache + h + '.rgba', 'wb') as f:
                f.write(struct.pack('<II', *im.size))
                f.write(im.tobytes())
    return out, summary


def load_dump(out, slot_index, summary):
    v = np.fromfile(out + 'slot_%d.bin' % slot_index, dtype=np.float32).reshape(-1, 11)
    layers = []
    for l in range(summary['layers']):
        raw = open(out + 'layer_%d.rgba' % l, 'rb').read()
        w, h = struct.unpack('<II', raw[:8])
        layers.append(np.frombuffer(raw[8:8 + w * h * 4], np.uint8).reshape(h, w, 4))
    return v, layers


# ---- the source -----------------------------------------------------------------------------------------
def load_source(folder):
    g, parts = gltf_io.load(folder)
    mats = []
    for m in g.get('materials', []):
        p = m.get('pbrMetallicRoughness', {})
        t = p.get('baseColorTexture')
        img = None
        if t is not None:
            path = os.path.join(folder, g['images'][g['textures'][t['index']]['source']]['uri'])
            try:
                im = Image.open(path).convert('RGBA')
                if max(im.size) > 1024:
                    im = im.resize((1024, 1024), Image.BOX)
                img = np.asarray(im)
            except Exception:
                img = None
        mats.append(dict(factor=np.array(p.get('baseColorFactor', [1, 1, 1, 1]), np.float64), image=img,
                         cutout=m.get('alphaMode', 'OPAQUE') != 'OPAQUE'))
    tris = np.concatenate([p['pos'] for p in parts])
    uv = np.concatenate([p['uv'] if p['uv'] is not None else np.zeros((len(p['pos']), 3, 2)) for p in parts])
    mat = np.concatenate([np.full(len(p['pos']), -1 if p['material'] is None else p['material']) for p in parts])
    return tris, uv, mat, mats


# ---- drawing ----------------------------------------------------------------------------------------------
def frame_of(points):
    """A shared view frame: the centre and principal axes (the longest first), signs fixed by skew."""
    c = points.mean(0)
    d = points - c
    w, vec = np.linalg.eigh(np.cov(d.T))
    vec = vec[:, ::-1]
    for k in range(3):
        if np.mean((d @ vec[:, k]) ** 3) < 0:
            vec[:, k] = -vec[:, k]
    if np.linalg.det(vec) < 0:
        vec[:, 2] = -vec[:, 2]
    return c, vec


def view_matrix():
    # a 3/4 view: turned 35 degrees about the second axis, tipped 25 about the first
    a, b = np.radians(35), np.radians(-25)
    ry = np.array([[np.cos(a), 0, np.sin(a)], [0, 1, 0], [-np.sin(a), 0, np.cos(a)]])
    rx = np.array([[1, 0, 0], [0, np.cos(b), -np.sin(b)], [0, np.sin(b), np.cos(b)]])
    return rx @ ry


def raster(tris, shade, size=SIZE, bg=(.09, .1, .12)):
    """tris (T,3,3) in view space (x right, y up, z toward the eye, all within [-1,1]); shade(t, l0, l1, l2)
    -> (n,3) linear colour, or (n,4) with alpha for a cut-out. Orthographic, z-buffered."""
    img = np.empty((size, size, 3))
    img[:] = s2l(bg)
    zb = np.full((size, size), -np.inf)
    P = np.empty((len(tris), 3, 3))
    P[..., 0] = (tris[..., 0] * .5 + .5) * size
    P[..., 1] = (-tris[..., 1] * .5 + .5) * size
    P[..., 2] = tris[..., 2]
    covered = 0
    for t in range(len(P)):
        q = P[t]
        x0, y0 = np.floor(q[:, :2].min(0)).astype(int)
        x1, y1 = np.ceil(q[:, :2].max(0)).astype(int)
        x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, size - 1), min(y1, size - 1)
        if x1 < x0 or y1 < y0:
            continue
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + .5, np.arange(y0, y1 + 1) + .5)
        (ax, ay, _), (bx, by, _), (cx, cy, _) = q
        den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(den) < 1e-12:
            continue
        l0 = ((by - cy) * (xs - cx) + (cx - bx) * (ys - cy)) / den
        l1 = ((cy - ay) * (xs - cx) + (ax - cx) * (ys - cy)) / den
        l2 = 1 - l0 - l1
        m = (l0 >= -1e-6) & (l1 >= -1e-6) & (l2 >= -1e-6)
        if not m.any():
            continue
        z = l0 * q[0, 2] + l1 * q[1, 2] + l2 * q[2, 2]
        sub = zb[y0:y1 + 1, x0:x1 + 1]
        m &= z > sub
        if not m.any():
            continue
        col = shade(t, l0[m], l1[m], l2[m])
        if col.shape[1] == 4:
            keep = col[:, 3] >= .5
            idx = np.nonzero(m)
            m2 = np.zeros_like(m)
            m2[idx[0][keep], idx[1][keep]] = True
            m, col = m2, col[keep, :3]
            if not m.any():
                continue
        sub[m] = z[m]
        img[y0:y1 + 1, x0:x1 + 1][m] = col
        covered += 1
    return l2s(img), np.isfinite(zb)


def sample(image, uv):
    """Nearest sample, wrapping, image rows from the top (glTF v down) -> (n,4) floats 0..1 sRGB."""
    h, w = image.shape[:2]
    x = np.floor(np.mod(uv[:, 0], 1) * w).astype(int).clip(0, w - 1)
    y = np.floor(np.mod(uv[:, 1], 1) * h).astype(int).clip(0, h - 1)
    return image[y, x].astype(np.float64) / 255


def fit_view(points, c, axes):
    R = view_matrix()
    p = (points.reshape(-1, 3) - c) @ axes @ R.T
    span = np.abs(p[:, :2]).max() * 1.08 or 1
    return lambda pts: ((pts.reshape(-1, 3) - c) @ axes @ R.T / span * np.array([1, 1, 1])).reshape(pts.shape)


def render_ours(v, layers):
    pos = v[:, 0:3].astype(np.float64)
    col = v[:, 3:6].astype(np.float64)
    shape = v[:, 7:11].astype(np.float64)
    tris = pos.reshape(-1, 3, 3)
    c, axes = frame_of(pos)
    to = fit_view(pos, c, axes)
    T = to(tris)
    C = col.reshape(-1, 3, 3)
    S = shape.reshape(-1, 3, 4)

    def shade(t, l0, l1, l2):
        lin = l0[:, None] * C[t, 0] + l1[:, None] * C[t, 1] + l2[:, None] * C[t, 2]
        layer = int(round(S[t, 0, 2]))
        if S[t, 0, 3] == 1 and 0 <= layer < len(layers):
            uv = l0[:, None] * S[t, 0, :2] + l1[:, None] * S[t, 1, :2] + l2[:, None] * S[t, 2, :2]
            tx = sample(layers[layer], uv)
            return np.c_[lin * s2l(tx[:, :3]), tx[:, 3]]
        return lin

    return raster(T, shade)


def render_source(tris, uv, mat, mats):
    c, axes = frame_of(tris.reshape(-1, 3))
    to = fit_view(tris, c, axes)
    T = to(tris)
    n = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    lit = .32 + .68 * np.abs(n @ LIGHT)

    def shade(t, l0, l1, l2):
        m = mats[mat[t]] if 0 <= mat[t] < len(mats) else dict(factor=np.array([.5, .5, .5, 1.]), image=None, cutout=False)
        base = np.tile(m['factor'][:3] * lit[t], (len(l0), 1))
        if m['image'] is not None:
            q = l0[:, None] * uv[t, 0] + l1[:, None] * uv[t, 1] + l2[:, None] * uv[t, 2]
            tx = sample(m['image'], q)
            out = base * s2l(tx[:, :3])
            return np.c_[out, tx[:, 3]] if m['cutout'] else out
        return base

    return raster(T, shade)


def side_by_side(path, ours, source, title, lines):
    a = Image.fromarray((ours * 255).astype(np.uint8))
    b = Image.fromarray((source * 255).astype(np.uint8))
    W = SIZE * 2 + 30
    img = Image.new('RGB', (W, SIZE + 40 + 18 * len(lines)), (16, 20, 24))
    img.paste(a, (10, 30))
    img.paste(b, (SIZE + 20, 30))
    d = ImageDraw.Draw(img)
    d.text((10, 8), 'OURS (game reader + drawer, overlay pipeline): ' + title, fill=(245, 197, 24))
    d.text((SIZE + 20, 8), 'SOURCE (its glTF and textures)', fill=(245, 197, 24))
    for i, s in enumerate(lines):
        d.text((10, SIZE + 36 + 18 * i), s, fill=(220, 220, 220) if not s.startswith('FAIL') else (255, 90, 90))
    img.save(path)


# ---- the checks -------------------------------------------------------------------------------------------
def shape(tris):
    """(T,3,3) -> the area-weighted spread along the principal axes, sqrt(eigenvalue) / the largest's: (1, b, c)."""
    tris = np.asarray(tris, np.float64)
    a = np.linalg.norm(np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0]), axis=1) / 2
    if a.sum() <= 0:
        return np.zeros(3)
    # each triangle as its three corners and centroid, weighted by its area
    pts = np.concatenate([tris.reshape(-1, 3), tris.mean(1)])
    w = np.concatenate([np.repeat(a, 3), a * 3])
    c = (pts * w[:, None]).sum(0) / w.sum()
    d = pts - c
    cov = (d * w[:, None]).T @ d / w.sum()
    e = np.sqrt(np.clip(np.linalg.eigvalsh(cov)[::-1], 0, None))
    return e / e[0] if e[0] > 0 else e


def check(slot, info, source_tris, kind, v, has_source_textures, source_shape=None):
    """-> list of failure strings (empty: passes)."""
    fails = []
    if info.get('status') != 'LOADED':
        return ['%s: %s %s' % (slot, info.get('status'), info.get('error', ''))]
    n, src = info['triangles'], source_tris
    floor = min(src, FLOORS[kind])
    if n < floor:
        fails.append('%s: %d triangles, under the %s floor %d' % (slot, n, kind, floor))
    if n < MIN_SHARE * src:
        fails.append('%s: %d triangles, under %d%% of the source %d' % (slot, n, MIN_SHARE * 100, src))
    if not info['hasUvs']:
        fails.append('%s: no UVs' % slot)
    if not info['drawn'] or info['vertices'] == 0:
        fails.append('%s: the game drawer drew nothing' % slot)
    if has_source_textures:
        if not info['textured'] or info['layers'] == 0:
            fails.append('%s: the source has textures, the shipped model is untextured' % slot)
        elif info['texturedVertices'] < .5 * info['vertices']:
            fails.append('%s: only %d of %d vertices textured' % (slot, info['texturedVertices'], info['vertices']))
    if len(v):
        uv = v[:, 7:9][v[:, 10] == 1]
        if len(uv) and (np.ptp(uv[:, 0]) < 1e-3 or np.ptp(uv[:, 1]) < 1e-3):
            fails.append('%s: degenerate UVs (all one point or line)' % slot)
        if source_shape is not None:
            ours = shape(v[:, 0:3].reshape(-1, 3, 3))
            if np.abs(ours - source_shape).max() > SHAPE_TOLERANCE:
                fails.append('%s: its shape %s is not its source\'s %s (a torn or lost surface)' % (
                    slot, np.round(ours, 2).tolist(), np.round(source_shape, 2).tolist()))
        # a model coloured per triangle: every triangle one colour and no texture
        C = v[:, 3:6].reshape(-1, 3, 3)
        if not has_source_textures and len(C) > 50 and np.allclose(C[:, 0], C[:, 1]) and np.allclose(C[:, 0], C[:, 2]) \
                and not info['hasNormals']:
            fails.append('%s: flat per-triangle colour, no normals' % slot)
    return fails


def report_triangles(models):
    p = os.path.join(models, 'conversion-report.json')
    return json.load(open(p)) if os.path.exists(p) else {}


def assembled_sawed_off(models, sources, out, d, summary, names):
    """The whole sawed-off as the game holds it (the frame, the barrels closed at their hinge: extras.pivot),
    beside the whole source model, the same view (SawedOff_Assembled.png). Fails when the two parts
    together are not the source's shape."""
    fv, layers = load_dump(d, names.index('SawedOff/Frame') + 1, summary)
    bv, _ = load_dump(d, names.index('SawedOff/Barrels') + 1, summary)
    pivot = np.zeros(3)
    try:
        raw = open(os.path.join(models, 'SawedOff', 'Barrels', 'scene.glb'), 'rb').read()
        n = struct.unpack('<I', raw[12:16])[0]
        pivot = np.array(json.loads(raw[20:20 + n])['asset']['extras']['pivot'], np.float64)
    except Exception:
        pass
    bv = bv.copy()
    bv[:, 0:3] += pivot.astype(np.float32)
    v = np.concatenate([fv, bv])
    tris, uv, mat, mats = load_source(os.path.join(sources, SOURCES['SawedOff/Frame'][0]))
    ours, _ = render_ours(v, layers)
    source, _ = render_source(tris, uv, mat, mats)
    a, b = shape(v[:, 0:3].astype(np.float64).reshape(-1, 3, 3)), shape(tris)
    dev = float(np.abs(np.array(a) - np.array(b)).max())
    fails = [] if dev <= SHAPE_TOLERANCE else ['SawedOff/Assembled: frame + barrels not the source shape (%.3f)' % dev]
    lines = ['SawedOff assembled (frame %d + barrels %d triangles, source %d)  barrels at their hinge %s  shape off %.3f' % (
        len(fv) // 3, len(bv) // 3, len(tris), pivot.round(4).tolist(), dev)]
    lines += ['FAIL ' + f for f in fails] or ['PASS']
    side_by_side(os.path.join(out, 'SawedOff_Assembled.png'), ours, source, 'SawedOff/Assembled', lines)
    return fails


def run(models=DEFAULT_MODELS, sources=DEFAULT_SOURCES, out=DEFAULT_OUT, dump=DUMP, slots=None, extra=None):
    """-> (failures, rows). extra: {slot: (source folder path, kind)} for slots beyond SOURCES (the van)."""
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix='sm2072_mc_')
    try:
        d, summary = run_dump(models, work, dump=dump)
        names = list(summary['slots'].keys())
        rep = report_triangles(models)
        table = dict(SOURCES)
        for k, (src, kind) in (extra or {}).items():
            table[k] = (src, kind)
        failures, rows = [], []
        for i, slot in enumerate(names, start=1):
            if slots and slot not in slots:
                continue
            if slot not in table:
                failures.append('%s: no source registered for the model check' % slot)
                continue
            src_name, kind = table[slot]
            info = summary['slots'][slot]
            v, layers = load_dump(d, i, summary)
            folder = src_name if os.path.isabs(src_name) else os.path.join(sources, src_name)
            tris, uv, mat, mats = load_source(folder)
            src_tris = rep.get(slot, {}).get('source_triangles') or len(tris)
            has_tex = bool(rep.get(slot, {}).get('textures')) if slot in rep else any(m['image'] is not None for m in mats)
            src_shape = None if slot in PART else shape(tris)
            fails = check(slot, info, src_tris, kind, v, has_tex, src_shape)
            ours, _ = render_ours(v, layers) if len(v) else (np.zeros((SIZE, SIZE, 3)), None)
            source, _ = render_source(tris, uv, mat, mats)
            lines = ['%s triangles %d (source %d, floor %d, %s)  UVs %s  normals %s  textured %s  layers %d  images %d' % (
                slot, info.get('triangles', 0), src_tris, min(src_tris, FLOORS[kind]), kind, info.get('hasUvs'),
                info.get('hasNormals'), info.get('textured'), info.get('layers', 0), info.get('images', 0))]
            lines += ['FAIL ' + f for f in fails] or ['PASS']
            side_by_side(os.path.join(out, slot.replace('/', '_') + '.png'), ours, source, slot, lines)
            rows.append(dict(slot=slot, kind=kind, fails=fails, **info, source=src_tris))
            failures += fails
        if (not slots or 'SawedOff/Frame' in slots) and 'SawedOff/Frame' in names and 'SawedOff/Barrels' in names:
            fails = assembled_sawed_off(models, sources, out, d, summary, names)
            rows.append(dict(slot='SawedOff/Assembled', kind='weapon', fails=fails, triangles=0, source=0))
            failures += fails
        json.dump(dict(rows=rows, failures=failures, notes=summary['notes']), open(os.path.join(out, 'model-check.json'), 'w'), indent=1)
        return failures, rows
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    a = sys.argv[1:] + [None] * 3
    f, rows = run(a[0] or DEFAULT_MODELS, a[1] or DEFAULT_SOURCES, a[2] or DEFAULT_OUT)
    for r in rows:
        print('%-18s %6d / %6d  %s' % (r['slot'], r['triangles'], r['source'], 'FAIL' if r['fails'] else 'PASS'))
    for x in f:
        print('FAIL', x)
    sys.exit(1 if f else 0)
