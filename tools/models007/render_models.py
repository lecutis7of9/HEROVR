"""Version 0.0.7 model previews, no headset: each source model (read only, flat grey, as
downloaded) next to what the mod draws (the converted GLB, its baked colours), and DAREDEVIL's baton
file with its rope marked red next to the cleaned LEFT / RIGHT pair. Painter's order, flat lit like the
overlay (.32 + .68 |n.l|). Usage: render_models.py <source models folder> <Models folder> <out folder>"""
import json, os, struct, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
sys.path.insert(0, os.path.dirname(__file__))
import gltf_io
from convert_models import components

W, H = 700, 560
LIGHT = np.array([.35, .8, .5]) / np.linalg.norm([.35, .8, .5])


def read_glb(path):
    b = open(path, 'rb').read()
    jl = struct.unpack_from('<I', b, 12)[0]
    g = json.loads(b[20:20 + jl])
    binb = b[20 + jl + 8:]
    p = g['meshes'][0]['primitives'][0]
    pos = gltf_io.accessor(g, binb, p['attributes']['POSITION'])
    col = gltf_io.accessor(g, binb, p['attributes']['COLOR_0'])[:, :3]
    idx = gltf_io.accessor(g, binb, p['indices']).ravel() if 'indices' in p else np.arange(len(pos))
    idx = idx.reshape(-1, 3)
    return pos[idx].astype(np.float64), col[idx].mean(1), g['asset'].get('extras', {})


def lin2srgb(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= .0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - .055)


def render(tris, cols, yaw=35, pitch=20, size=(W, H), title='', sub=''):
    """tris (T,3,3), cols (T,3) linear. Fit to the frame; far to near."""
    # look along the model's thinnest axis (longest across, second up), then the small tilt given
    pts = tris.reshape(-1, 3)
    _, _, vt = np.linalg.svd(pts - pts.mean(0), full_matrices=False)
    if np.linalg.det(vt) < 0:
        vt[2] = -vt[2]
    tris = ((pts - pts.mean(0)) @ vt.T).reshape(-1, 3, 3)
    a, p = np.radians(yaw), np.radians(pitch)
    ry = np.array([[np.cos(a), 0, np.sin(a)], [0, 1, 0], [-np.sin(a), 0, np.cos(a)]])
    rx = np.array([[1, 0, 0], [0, np.cos(p), -np.sin(p)], [0, np.sin(p), np.cos(p)]])
    t = tris.reshape(-1, 3) @ (rx @ ry).T
    t = t.reshape(-1, 3, 3)
    lo, hi = t.reshape(-1, 3).min(0), t.reshape(-1, 3).max(0)
    sc = min((size[0] - 60) / max(hi[0] - lo[0], 1e-9), (size[1] - 130) / max(hi[1] - lo[1], 1e-9))
    mid = (lo + hi) / 2
    n = np.cross(t[:, 1] - t[:, 0], t[:, 2] - t[:, 0])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    shade = .32 + .68 * np.abs(n @ LIGHT)
    rgb = (lin2srgb(cols * shade[:, None]) * 255).astype(int)
    im = Image.new('RGB', size, (36, 36, 44))
    d = ImageDraw.Draw(im)
    order = np.argsort(t[:, :, 2].mean(1))  # far (most negative z towards the viewer at +z) first
    for i in order:
        pts = [((x - mid[0]) * sc + size[0] / 2, size[1] / 2 + 25 - (y - mid[1]) * sc) for x, y, _ in t[i]]
        d.polygon(pts, fill=tuple(rgb[i]))
    f = ImageFont.load_default()
    d.text((14, 10), title, fill=(150, 215, 255), font=f)
    d.text((14, 28), sub, fill=(235, 235, 235), font=f)
    return im


def sheet(images, cols=2):
    rows = (len(images) + cols - 1) // cols
    out = Image.new('RGB', (W * cols, H * rows), (20, 20, 24))
    for k, im in enumerate(images):
        out.paste(im, ((k % cols) * W, (k // cols) * H))
    return out


def source(folder):
    g, parts = gltf_io.load(folder)
    tris = np.concatenate([p['pos'] for p in parts])
    return tris, parts


def main(src, models, out):
    os.makedirs(out, exist_ok=True)
    rep = json.load(open(os.path.join(models, 'conversion-report.json')))
    grey = lambda n: np.full((n, 3), .55)
    shots = []
    for key, sub_src, glbs, yaw, pitch in [
            ('Batarang/EIRA', 'batarang', ['Batarang/EIRA'], 15, 20),
            ('Batarang/CLOONEY', 'batarang_clooney', ['Batarang/CLOONEY'], 15, 20),
            ('Batarang/JOKER', 'batarang_joker', [], 15, 20),
            ('Pistol', '9mm_pistol', ['Pistol'], 15, 10),
            ('SawedOff', 'sawed-off_shotgun', ['SawedOff/Frame', 'SawedOff/Barrels'], 15, 10),
            ('Baton', 'daredevils_baton', ['Baton/Left', 'Baton/Right'], 10, 15)]:
        r = rep[key]
        st, _ = source(os.path.join(src, sub_src))
        shots.append(render(st, grey(len(st)), yaw, pitch, title='%s  SOURCE (as downloaded, untextured here)' % key,
                            sub='%d triangles; textures: %s' % (len(st), ', '.join('%dx%d' % tuple(v) for v in r['source_textures'].values()) or 'none')))
        if glbs:
            ts, cs = [], []
            off = 0
            for gname in glbs:
                t, c, _ = read_glb(os.path.join(models, gname, 'scene.glb'))
                if key == 'Baton':  # the pair side by side (along -z: offset in y)
                    t = t + np.array([0, off, 0]); off += .08
                ts.append(t); cs.append(c)
            t, c = np.concatenate(ts), np.concatenate(cs)
            shots.append(render(t, c, yaw, pitch,
                                title='%s  IN THE MOD (converted GLB, colours baked, no textures)' % key,
                                sub='%d triangles (%s)' % (len(t), ' + '.join('%s %d' % (gname, len(x)) for gname, x in zip(glbs, ts)))))
        else:
            shots.append(render(st, grey(len(st)), yaw, pitch, title='%s  IN THE MOD: UNCHANGED (CC BY-NC-ND)' % key,
                                sub='%d triangles, textures 4096x4096 shipped as downloaded (unused by the overlay)' % len(st)))
    sheet(shots).save(os.path.join(out, 'models_source_vs_mod.png'))
    # the baton: the rope marked, then the cleaned pair
    tris, parts = source(os.path.join(src, 'daredevils_baton'))
    comp = np.zeros(len(tris), int); base = 0; s0 = 0
    for p in parts:
        r = components(p['pos']); comp[s0:s0 + len(r)] = r + base; base += r.max() + 1; s0 += len(r)
    rope = np.zeros(len(tris), bool)
    for k in np.unique(comp):
        sel = comp == k; pts = tris[sel].reshape(-1, 3); size = pts.max(0) - pts.min(0)
        if size[1] < 20 and max(size[0], size[2]) > 600:
            rope |= sel
    col = grey(len(tris)); col[rope] = [.85, .05, .05]
    a = render(tris, col, 10, 15, title='ORIGINAL: Daredevil\'s baton by Anthony Yanez (CC BY 4.0)',
               sub='%d triangles; the rope (%d triangles) marked RED joins the two batons' % (len(tris), rope.sum()))
    l, cl, el = read_glb(os.path.join(models, 'Baton/Left/scene.glb'))
    r_, cr, er = read_glb(os.path.join(models, 'Baton/Right/scene.glb'))
    pair = np.concatenate([l + [0, -.05, 0], r_ + [0, .05, 0]])
    b = render(pair, np.concatenate([cl, cr]), 10, 15, title='CLEANED: rope removed, split into two batons (LEFT, RIGHT)',
               sub='LEFT %d + RIGHT %d triangles; each grip at the origin (its pivot), 57 cm' % (len(l), len(r_)))
    sheet([a, b]).save(os.path.join(out, 'baton_original_vs_cleaned.png'))
    print('wrote', out)


if __name__ == '__main__':
    main(*sys.argv[1:4])
