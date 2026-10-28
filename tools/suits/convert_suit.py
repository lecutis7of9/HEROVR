"""Build 9 SUITS: a downloaded Sketchfab suit -> Models/Suits/<folder>/scene.glb, a skinned body the game
wears (include/spidy/suit_wear.hpp). Re-runnable; one table row per suit, so tomorrow's downloads drop in.

  python3 tools/suits/convert_suit.py [--only Suit_x,...] [--downloads ~/Downloads] [--out build/h7out/Models]

Per suit:
  1. the zip is unpacked to SOURCES/<folder> (kept: the model check compares against it);
  2. bpy_suit_extract.py: the scene as Sketchfab shows it (its pose, not a rig's rest), Y up, its textures;
  3. the parts the suit hides (a head under a cowl, teeth, eyeballs) are dropped by material name;
  4. scaled to 1.8 m tall, feet on the floor, the body's middle at x = z = 0;
  5. the 16 suit joints: from the artist's rig by bone name, else from the body's shape;
  6. bpy_suit_skin.py: Blender's collapse decimation to TARGET triangles (the UV seams kept, never the
     quick clustering that flattened the 0.0.72 weapons), the artist's weights carried through it
     (mapped bone by bone onto the 16), or Blender's bone-heat weights for an unrigged suit;
  7. the base-colour textures packed into at most ATLASES atlases (each texture cropped to its used UVs);
  8. the GLB: POSITION, NORMAL, TEXCOORD_0, JOINTS_0 / WEIGHTS_0 (4 per vertex) and asset.extras.suit
     (the rest joints, the fingertips, the backs of the hands), with license.txt;
  9. a preview with the joints drawn on, in WORK/preview_<folder>.png."""
import argparse, glob, hashlib, io, json, math, os, re, shutil, struct, subprocess, sys, zipfile
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'models072'))
import suit_skeleton as sk  # noqa: E402

BPY = os.environ.get('SUITS_BPY', '/workspace/bpy-venv/bin/python')
SOURCES = os.environ.get('SUITS_SOURCES', '/workspace/hero-0.0.9-suits')
TARGET = 24000  # triangles: a whole body seen up close; the decimation keeps its shape (collapse)
ATLAS = 1024
ATLASES = 3  # at most 3 layers a suit: 23 suits + the rest stay near 110 layers (about 150 MB at 512 px)
HEIGHT = 1.8
REUSE = os.environ.get('SUITS_REUSE', '1') == '1'  # a decimated skin already made is reused

INNER = r'teeth|mouth_fog|wetline|eyeball|head_unmasked'
# folder, the zip's name, what to drop (a regex on material names), notes
SUITS = [
    dict(folder='Suit_batman_1', zip='batman_beyond_-_textured_and_rigged', drop=INNER),
    dict(folder='Suit_batman_2', zip='batman_1989', drop=INNER),
    dict(folder='Suit_batman_3', zip='bruce_wayne_batman_year_one', drop=INNER),
    dict(folder='Suit_batman_5', zip='batman_origins_suit_-_textured_and_rigged', drop=INNER),
    dict(folder='Suit_ironMan_1', zip='iron-man_mark_85__rigged', drop=INNER),
    dict(folder='Suit_ironMan_2', zip='iron_man_-_mark_1', drop=INNER),
    dict(folder='Suit_ironMan_3', zip='iron_man_mark-7subsurf', drop=INNER),
    # build 9: the user's own downloads (chat attachments, named by their sha256)
    dict(folder='Suit_ironMan_4', zip='b0c9a19f', drop=INNER),
    dict(folder='Suit_captainAmerica_1', zip='b3506424', drop=INNER, drop_flat_behind=True),
    dict(folder='Suit_captainAmerica_2', zip='0b6963a5', drop=INNER + '|shield'),
    dict(folder='Suit_daredevil_1', zip='9ca17e23', drop=INNER + '|fire'),
    dict(folder='Suit_daredevil_3', zip='16335a77', drop=INNER),
    dict(folder='Suit_hulk_2', zip='97935ec8', drop=INNER),
    dict(folder='Suit_wolverine_1', zip='ce04af29', drop=INNER),
    dict(folder='Suit_wolverine_3', zip='697d3bb3', drop=INNER),
    dict(folder='Suit_wolverine_4', zip='1073e863', drop=INNER),
    dict(folder='Suit_punisher_1', zip='f642847d', drop=INNER),
    dict(folder='Suit_punisher_2', zip='3a9db847', drop=INNER),
    dict(folder='Suit_thor_1', zip='9cba5a01', drop=INNER),
    dict(folder='Suit_thor_2', zip='0d36fd1f', drop=INNER),
    dict(folder='Suit_greenLantern_2', zip='10895eb2', drop=INNER),
    dict(folder='Suit_greenLantern_3', zip='75f1e3c7', drop=INNER),
    # Superman - DC (Chechorams16, CC BY; 19,926 triangles as its page says): the zip has no license.txt,
    # its glTF is in source/Superman.zip
    dict(folder='Suit_superman_4', zip='89179b02', drop=INNER, inner='source/Superman.zip',
         license='Model Information:\n* title:\tSuperman - DC\n* source:\thttps://sketchfab.com/3d-models/superman-dc-ccad133a5a684003a059a01c89216780\n'
                 '* author:\tChechorams16 (https://sketchfab.com/Chechorams16)\n\nModel License:\n* license type:\tCC-BY-4.0 '
                 '(http://creativecommons.org/licenses/by/4.0/)\n* requirements:\tAuthor must be credited. Commercial use is allowed.\n'),
]
ATTACHMENTS = os.environ.get('SUITS_ATTACHMENTS', '/home/box/agent-data/agents/95bbcd93-d825-48cb-8651-274d994f8774/attachments')
GLASS = [.06, .08, .11, 1]  # an untextured "Glass" (eye lenses, a visor): dark, as glass looks indoors


def bpy(script, *args, log, timeout=None, env=None):
    with open(log, 'w') as f:
        subprocess.run([BPY, os.path.join(HERE, script), *args], check=True, stdout=f, stderr=subprocess.STDOUT,
                       timeout=timeout, env=dict(os.environ, **(env or {})))


HEAT_SECONDS = 300  # Blender's bone heat can stall on a suit of many loose plates: then the smoothed distance weights


def unpack(downloads, s):
    dst = os.path.join(SOURCES, s['folder'])
    if not glob.glob(os.path.join(dst, '*.gltf')) and not glob.glob(os.path.join(dst, '*.glb')):
        z = os.path.join(downloads, s['zip'] + '.zip')
        if not os.path.exists(z):
            z = glob.glob(os.path.join(ATTACHMENTS, s['zip'] + '*.zip'))[0]
        os.makedirs(dst, exist_ok=True)
        with zipfile.ZipFile(z) as f:
            f.extractall(dst)
        if s.get('inner'):
            with zipfile.ZipFile(os.path.join(dst, s['inner'])) as f:
                f.extractall(dst)
        if s.get('license') and not os.path.exists(os.path.join(dst, 'license.txt')):
            open(os.path.join(dst, 'license.txt'), 'w').write(s['license'])
    g = (glob.glob(os.path.join(dst, '*.gltf')) + glob.glob(os.path.join(dst, '*.glb')))[0]
    return dst, g


def resolve_images(src, mats, notes):
    """A material whose base colour the importer could not follow (a node in between) gets its texture by name."""
    for m in mats:
        if m['image']:
            continue
        core = re.sub(r'\W+', '', m['name'].lower())
        for p in glob.glob(os.path.join(src, 'textures', '*_baseColor.*')):
            n = re.sub(r'\W+', '', os.path.basename(p).lower().rsplit('_basecolor', 1)[0])
            if n == core:
                m['image'] = p
                notes.append('%s: its base colour found by name (%s)' % (m['name'], os.path.basename(p)))
        if not m['image'] and 'glass' in m['name'].lower():
            m['factor'] = GLASS
            notes.append('%s: untextured glass, drawn dark' % m['name'])


def flat_behind(tris):
    """Triangles of connected parts (welded at 0.1 mm) at least 40 cm across, under 12 cm thick, their centre
    more than 10 cm behind the pelvis: a shield on the back."""
    P = tris.reshape(-1, 3)
    key = np.round(P / 1e-4).astype(np.int64)
    _, inv = np.unique(key, axis=0, return_inverse=True)
    inv = inv.reshape(-1, 3)
    parent = np.arange(inv.max() + 1)

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x
    for a, b, c in inv:
        ra, rb, rc = find(a), find(b), find(c)
        parent[rb] = ra
        parent[find(rc)] = ra
    comp = np.array([find(x) for x in inv[:, 0]])
    gone = np.zeros(len(tris), bool)
    for c in np.unique(comp):
        sel = comp == c
        Q = tris[sel].reshape(-1, 3)
        ext = np.sort(np.ptp(Q, 0))
        if ext[2] > .4 and ext[0] < .12 and Q[:, 2].mean() < -.1:
            gone |= sel
    return gone


def wrap_uv(uv):
    """Tiled UVs (outside 0..1) moved by whole tiles, triangle by triangle, into 0..1 (the atlas clamps)."""
    uv = uv.copy()
    lo = np.floor(uv.min(1, keepdims=True) + 1e-6)
    return uv - lo


def merge_images(mats, mat):
    """Materials sharing one image become one (so the atlas holds it once)."""
    by, remap, out = {}, {}, []
    for i, m in enumerate(mats):
        k = ('img', m['image']) if m['image'] else ('f', tuple(m['factor']), m['name'])
        if k not in by:
            by[k] = len(out)
            out.append(dict(m))
        remap[i] = by[k]
    return out, np.array([remap[int(x)] for x in mat])


def atlases(mats, uv, mat, used, work, folder):
    """Textured materials -> at most ATLASES atlases (balanced by cropped texture area, each cropped to its
    UVs, one scale per atlas). -> uv, mat, mats, notes."""
    from convert_thor_van_072 import atlas  # crop + pack + remap (the 0.0.72 van's)
    tex = [i for i in used if mats[i]['image']]
    if not tex:
        return uv, mat, mats, []
    area = {}
    for i in tex:
        W, H = Image.open(mats[i]['image']).size
        q = np.clip(uv[mat == i].reshape(-1, 2), 0, 1)
        area[i] = max(1, (np.ptp(q[:, 0]) * W) * (np.ptp(q[:, 1]) * H)) if len(q) else 1
    n = min(ATLASES, len(tex))
    groups, load = [[] for _ in range(n)], [0.] * n
    for i in sorted(tex, key=lambda i: -area[i]):
        k = int(np.argmin(load))
        groups[k].append(i)
        load[k] += area[i]
    notes = []
    for k, gr in enumerate(groups):
        uv, mat, mats, scale = atlas(mats, uv, mat, gr, os.path.join(work, '%s_atlas%d.png' % (folder, k)), size=ATLAS)
        notes.append('atlas %d: %s (scale %.3f)' % (k, ', '.join(mats[i]['name'] for i in gr), scale))
    return uv, mat, mats, notes


def write_glb(path, tris, uv, nrm, mat, mats, skin, extras):
    """skin (T,3,16) -> the 4 largest per corner, uint8 (summing to 255)."""
    w = skin.reshape(-1, 16)
    top = np.argsort(-w, axis=1)[:, :4]
    tw = np.take_along_axis(w, top, 1)
    tw /= np.maximum(tw.sum(1, keepdims=True), 1e-9)
    q = np.floor(tw * 255 + .5).astype(np.int64)
    q[:, 0] += 255 - q.sum(1)
    J = np.where(q > 0, top, 0).astype(np.uint8)
    Wq = np.clip(q, 0, 255).astype(np.uint8)
    binb = bytearray()
    g = {'asset': {'version': '2.0', 'generator': 'HERO VR build 9 tools/suits', 'extras': extras}, 'scene': 0,
         'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0}], 'meshes': [{'primitives': []}], 'buffers': [{}],
         'bufferViews': [], 'accessors': [], 'materials': [], 'textures': [], 'images': [], 'samplers': [{}]}

    def view(data):
        off = len(binb)
        binb.extend(data)
        binb.extend(b'\0' * (-len(binb) % 4))
        g['bufferViews'].append({'buffer': 0, 'byteOffset': off, 'byteLength': len(data)})
        return len(g['bufferViews']) - 1
    images = {}
    P_all = tris.reshape(-1, 3).astype(np.float32)
    N_all = nrm.reshape(-1, 3).astype(np.float32)
    N_all /= np.maximum(np.linalg.norm(N_all, axis=1, keepdims=True), 1e-9)
    U_all = uv.reshape(-1, 2).astype(np.float32)
    cm = np.repeat(mat, 3)
    for mi in sorted(set(int(m) for m in mat)):
        m = mats[mi]
        gm = {'name': m['name'][:120], 'pbrMetallicRoughness': {'baseColorFactor': list(m['factor']), 'metallicFactor': 0},
              'doubleSided': True}
        if m['image']:
            if m['image'] not in images:
                im = Image.open(m['image']).convert('RGB')  # base colour only: JPEG (a third of a PNG's size)
                b = io.BytesIO()
                im.save(b, 'JPEG', quality=90, subsampling=0)
                g['images'].append({'bufferView': view(b.getvalue()), 'mimeType': 'image/jpeg', 'name': os.path.basename(m['image'])})
                g['textures'].append({'source': len(g['images']) - 1, 'sampler': 0})
                images[m['image']] = len(g['textures']) - 1
            gm['pbrMetallicRoughness']['baseColorTexture'] = {'index': images[m['image']]}
            gm['pbrMetallicRoughness']['baseColorFactor'] = [1, 1, 1, 1]
        g['materials'].append(gm)
        sel = cm == mi
        P, N, U, Jm, Wm = P_all[sel], N_all[sel], U_all[sel], J[sel], Wq[sel]
        key = np.c_[np.round(P, 6), np.round(N, 4), np.round(U, 6), Jm, Wm]
        uniq, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
        inv = inv.ravel().astype(np.uint32)
        P, N, U, Jm, Wm = P[first], N[first], U[first], Jm[first], Wm[first]
        acc = len(g['accessors'])
        g['accessors'] += [
            {'bufferView': view(P.tobytes()), 'componentType': 5126, 'count': len(P), 'type': 'VEC3', 'min': P.min(0).tolist(), 'max': P.max(0).tolist()},
            {'bufferView': view(N.tobytes()), 'componentType': 5126, 'count': len(N), 'type': 'VEC3'},
            {'bufferView': view(U.tobytes()), 'componentType': 5126, 'count': len(U), 'type': 'VEC2'},
            {'bufferView': view(Jm.tobytes()), 'componentType': 5121, 'count': len(Jm), 'type': 'VEC4'},
            {'bufferView': view(Wm.tobytes()), 'componentType': 5121, 'normalized': True, 'count': len(Wm), 'type': 'VEC4'},
            {'bufferView': view(inv.tobytes()), 'componentType': 5125, 'count': len(inv), 'type': 'SCALAR'}]
        g['meshes'][0]['primitives'].append({'attributes': {'POSITION': acc, 'NORMAL': acc + 1, 'TEXCOORD_0': acc + 2,
                                                            'JOINTS_0': acc + 3, 'WEIGHTS_0': acc + 4},
                                             'indices': acc + 5, 'material': len(g['materials']) - 1})
    for k in ('textures', 'images'):
        if not g[k]:
            del g[k]
    if 'textures' not in g:
        del g['samplers']
    g['buffers'][0]['byteLength'] = len(binb)
    j = json.dumps(g, separators=(',', ':')).encode()
    j += b' ' * (-len(j) % 4)
    body = struct.pack('<II', len(j), 0x4E4F534A) + j + struct.pack('<II', len(binb), 0x004E4942) + bytes(binb)
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', 0x46546C67, 2, 12 + len(body)) + body)


def preview(path, tris, ends, skin, title):
    """Front and side, each triangle coloured by its strongest bone, the joints and bones drawn on."""
    from PIL import ImageDraw
    import model_check as mc
    pal = np.array([[.9, .9, .9], [.7, .7, .3], [.3, .7, .9], [.2, .4, .9], [.9, .5, .2], [.9, .2, .2], [.9, .2, .7], [.6, .2, .9],
                    [.5, .9, .2], [.2, .9, .2], [.2, .9, .6], [.1, .6, .4], [.9, .8, .1], [.6, .4, .1], [.2, .8, .9], [.1, .4, .6]])
    col = pal[skin.sum(1).argmax(1)]
    size = 560
    ims = []
    for rot in (0, 90):
        a = math.radians(rot)
        R = np.array([[math.cos(a), 0, math.sin(a)], [0, 1, 0], [-math.sin(a), 0, math.cos(a)]])
        v = tris.reshape(-1, 3) @ R.T
        E = ends @ R.T
        c = np.array([0, HEIGHT / 2, 0])
        sc = 1 / (HEIGHT * .55)
        vt = ((v - c) * sc).reshape(-1, 3, 3)
        n = np.cross(vt[:, 1] - vt[:, 0], vt[:, 2] - vt[:, 0])
        lit = .35 + .65 * np.abs(n[:, 2]) / np.maximum(np.linalg.norm(n, axis=1), 1e-12)
        cl = mc.s2l(col * lit[:, None])
        img, _ = mc.raster(vt, lambda t, l0, l1, l2: np.tile(cl[t], (len(l0), 1)), size=size)
        im = Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))
        d = ImageDraw.Draw(im)
        px = lambda p: ((p[0] - c[0]) * sc * size / 2 + size / 2, size / 2 - (p[1] - c[1]) * sc * size / 2)
        for a_, b_ in sk.BONE_ENDS:
            d.line([px(E[a_]), px(E[b_])], fill=(255, 255, 255), width=2)
        for p in E:
            x, y = px(p)
            d.ellipse([x - 4, y - 4, x + 4, y + 4], fill=(255, 40, 40))
        ims.append(im)
    out = Image.new('RGB', (size * 2, size + 24), (20, 20, 24))
    for i, im in enumerate(ims):
        out.paste(im, (i * size, 24))
    ImageDraw.Draw(out).text((8, 6), title, fill=(255, 255, 255))
    out.save(path)


def convert(s, downloads, out_models, work):
    folder = s['folder']
    notes = []
    src, scene = unpack(downloads, s)
    npz, js = os.path.join(work, folder + '_ex.npz'), os.path.join(work, folder + '_ex.json')
    if not os.path.exists(js):
        bpy('bpy_suit_extract.py', scene, npz, js, log=os.path.join(work, folder + '_ex.log'))
    d = np.load(npz)
    meta = json.load(open(js))
    tris, uv, mat = d['tris'].astype(np.float64), d['uv'].astype(np.float64), d['mat'].astype(np.int64)
    sj, sw = d['joints'], d['weights']
    mats = meta['materials']
    src_tris = len(tris)
    keep = np.ones(len(tris), bool)
    for i, m in enumerate(mats):
        if re.search(s['drop'], m['name'], re.I):
            keep &= mat != i
            notes.append('dropped %s (%d triangles, hidden inside the suit)' % (m['name'], int((mat == i).sum())))
    tris, uv, mat, sj, sw = tris[keep], uv[keep], mat[keep], sj[keep], sw[keep]
    resolve_images(src, mats, notes)
    P = tris.reshape(-1, 3)
    lo, hi = P.min(0), P.max(0)
    scale = HEIGHT / (hi[1] - lo[1])
    bones = meta['bones']
    rig = sk.rig_joints(bones) if bones else None
    P0 = (P - [0, lo[1], 0]) * scale
    band = P0[(P0[:, 1] > .55 * HEIGHT) & (P0[:, 1] < .75 * HEIGHT)]
    cx = np.median(band[:, 0])
    if rig is not None:
        J = (rig - [0, lo[1], 0]) * scale
        cz = J[3][2]
        notes.append('joints from the artist\'s rig')
    else:
        J0 = sk.shape_joints(P0)
        cz = J0[3][2]
    shift = np.array([cx, 0, cz])
    tris = ((tris - [0, lo[1], 0]) * scale - shift)
    P = tris.reshape(-1, 3)
    J = J - shift if rig is not None else sk.shape_joints(P)
    if s.get('drop_flat_behind'):  # a prop carried on the back (Cap's shield: the game draws its own)
        gone = flat_behind(tris)
        if gone.any():
            notes.append('dropped a flat prop on the back (%d triangles: the game draws its own)' % int(gone.sum()))
            tris, uv, mat, sj, sw = tris[~gone], uv[~gone], mat[~gone], sj[~gone], sw[~gone]
            P = tris.reshape(-1, 3)
    if rig is None:
        notes.append('joints from the body\'s shape (no rig)')
    ends = sk.ends_of(J, P)
    # the artist's weights onto the 16 bones
    if rig is not None and sj.size:
        roles = np.array(sk.rig_roles(bones))
        skin = np.zeros((len(tris), 3, 16), np.float32)
        for k in range(4):
            jj = sj[..., k]
            ok = jj >= 0
            r = np.where(ok, roles[np.clip(jj, 0, None)], 0)
            np.add.at(skin, (np.nonzero(ok)[0], np.nonzero(ok)[1], r[ok]), sw[..., k][ok])
        none = skin.sum(2) < 1e-6
        skin[none, 3] = 1
    else:
        skin = np.zeros((0,), np.float32)
    uv = wrap_uv(uv)
    sin = os.path.join(work, folder + '_skin_in.npz')
    sout = os.path.join(work, folder + '_skin.npz')
    np.savez(sin, tris=tris, uv=uv, mat=mat, target=TARGET, ends=ends, bones=sk.BONE_ENDS, skin=skin)
    key = hashlib.sha256(open(sin, 'rb').read() + open(os.path.join(HERE, 'bpy_suit_skin.py'), 'rb').read()).hexdigest()
    keyfile = sout + '.key'
    if not (REUSE and os.path.exists(sout) and os.path.exists(keyfile) and open(keyfile).read() == key):
        try:
            bpy('bpy_suit_skin.py', sin, sout, log=os.path.join(work, folder + '_skin.log'), timeout=HEAT_SECONDS)
        except subprocess.TimeoutExpired:
            print('  bone heat over %d s: the smoothed distance weights' % HEAT_SECONDS, flush=True)
            bpy('bpy_suit_skin.py', sin, sout, log=os.path.join(work, folder + '_skin.log'), env={'SUITS_NOHEAT': '1'})
        open(keyfile, 'w').write(key)
    r = np.load(sout)
    tris, uv, mat, nrm, skin = r['tris'], r['uv'], r['mat'], r['nrm'], r['skin']
    if int(r['lost']):
        notes.append('%d vertices weighted to their nearest bone (bone heat found no path)' % int(r['lost']))
    mats, mat = merge_images(mats, mat)
    used = sorted(set(mat.tolist()))
    uv, mat, mats, an = atlases(mats, wrap_uv(uv), mat, used, work, folder)
    notes += an
    hb = [sk.hand_back(J, ends, P, 'L').round(4).tolist(), sk.hand_back(J, ends, P, 'R').round(4).tolist()]
    extras = {'suit': {'joints': J.round(5).ravel().tolist(), 'ends': ends[16:].round(5).ravel().tolist(),
                       'handBack': hb[0] + hb[1], 'height': HEIGHT}}
    dst = os.path.join(out_models, 'Suits', folder)
    os.makedirs(dst, exist_ok=True)
    write_glb(os.path.join(dst, 'scene.glb'), tris, uv, nrm, mat, mats, skin, extras)
    lic = os.path.join(src, 'license.txt')
    if os.path.exists(lic):
        shutil.copy(lic, os.path.join(dst, 'license.txt'))
    title = '%s  (%s)  %d -> %d triangles' % (folder, s['zip'], src_tris, len(tris))
    preview(os.path.join(work, 'preview_%s.png' % folder), tris, ends, skin, title)
    return dict(folder=folder, source=s['zip'], source_triangles=src_tris, triangles=len(tris),
                bytes=os.path.getsize(os.path.join(dst, 'scene.glb')), rigged=rig is not None, notes=notes)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--only', default='')
    ap.add_argument('--downloads', default=os.path.expanduser('~/Downloads'))
    ap.add_argument('--out', default=os.path.join(HERE, '..', '..', 'build', 'h7out', 'Models'))
    ap.add_argument('--work', default='/tmp/suits9/work')
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)
    only = set(x for x in a.only.split(',') if x)
    report = []
    for s in SUITS:
        if only and s['folder'] not in only:
            continue
        print('==', s['folder'], s['zip'], flush=True)
        rep = convert(s, a.downloads, a.out, a.work)
        print(json.dumps(rep, indent=1), flush=True)
        report.append(rep)
    path = os.path.join(a.out, 'Suits', 'conversion-report.json')
    old = json.load(open(path)) if os.path.exists(path) else []
    keep = [x for x in old if x['folder'] not in {r['folder'] for r in report}]
    json.dump(sorted(keep + report, key=lambda x: x['folder']), open(path, 'w'), indent=1)


if __name__ == '__main__':
    main()
