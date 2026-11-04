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
    dict(folder='Suit_captainAmerica_1', zip='b3506424', drop=INNER, props='^group_0$'),
    dict(folder='Suit_captainAmerica_2', zip='0b6963a5', drop=INNER, props='shield'),
    dict(folder='Suit_daredevil_1', zip='9ca17e23', drop=INNER, props='fire'),
    dict(folder='Suit_daredevil_3', zip='16335a77', drop=INNER),
    dict(folder='Suit_hulk_2', zip='97935ec8', drop=INNER),
    dict(folder='Suit_wolverine_1', zip='ce04af29', drop=INNER),
    dict(folder='Suit_wolverine_3', zip='697d3bb3', drop=INNER),
    dict(folder='Suit_wolverine_4', zip='1073e863', drop=INNER),
    # 0.0.78: the owner's own HQ Punisher (a single glb, 592,596 triangles, one 4096 px base colour with
    # normal/roughness/metallic maps; no rig) replaces build 9's VEST in the same folder (its slot and MP hash).
    # Its base colour is split over 4 atlases (UV quadrants of 2048 px: the 4096 px source at full size).
    # 0.0.78 PBR (t1895u, "not flat textures"): pbr ships the hi->lo normal map, an ORM map (occlusion from that
    # map's cavity, the model's own roughness and metallic maps) and TANGENT as maps beside the raw base colour
    # (the per-pixel model shader; no cavity folded into the base colour any more).
    dict(folder='Suit_punisher_1', glb='/workspace/drive-dl/model/model.glb', drop=INNER,
         texture='/workspace/drive-dl/model/texture_0.png', cavity='/workspace/drive-dl/model/texture_0_normal.png',
         roughness='/workspace/drive-dl/model/texture_0_roughness.png', metallic='/workspace/drive-dl/model/texture_0_metallic.png',
         pbr=True, atlas=2048,
         quadrants=True, atlases=4, turn_deg=-24, arm_keep=.07, target=80000, bake_normal=True,  # its body modelled turned 24 degrees to its right (top view of shoulders and belt)
         # target: 80,000 triangles (owner, t1889u: 24,000 lost its silhouette; mesh_model::suitHqTriangles is its draw cap)
         # arm_keep: its rest hands touch the holsters (keep_arms takes the hands' weights off them)
         # metres, in the converted frame (feet at y 0, facing +z, the character's right at -x): from the 5 cm grids
         joints=dict(shoulderR=(-.21, 1.43, -.06), elbowR=(-.30, 1.09, -.07), wristR=(-.31, .90, .02),
                     shoulderL=(.21, 1.43, -.04), elbowL=(.30, 1.09, -.04), wristL=(.30, .90, .06),
                     tipR=(-.27, .74, .05), tipL=(.27, .74, .06)),
         license='Model Information:\n* title:\tPunisher (HQ)\n* source:\tthe HERO VR owner\'s own model file '
                 '(model.glb, texture_0*.png), supplied by him for this mod\n\nModel License:\n* as its owner '
                 'supplied it, for use in HERO VR\n'),
    dict(folder='Suit_punisher_2', zip='3a9db847', drop=INNER),
    dict(folder='Suit_thor_1', zip='9cba5a01', drop=INNER, prop_bones='^Bone010'),  # the hammer
    dict(folder='Suit_thor_2', zip='0d36fd1f', drop=INNER, props='^07___Default$'),  # the hammer
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
    if s.get('glb'):  # a model file given directly (0.0.78's Punisher): read where it is
        if s.get('license'):
            os.makedirs(dst, exist_ok=True)
            open(os.path.join(dst, 'license.txt'), 'w').write(s['license'])
        return dst, s['glb']
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


def gltf_images(scene):
    """-> {material name: its base colour image path} read straight from a .gltf (the importer shares one image
    between materials and leaves the others without it: Superman's cape, hair and head)."""
    out = {}
    if not scene.lower().endswith('.gltf'):
        return out
    try:
        g = json.load(open(scene, encoding='utf-8'))
        for m in g.get('materials', []):
            t = m.get('pbrMetallicRoughness', {}).get('baseColorTexture')
            if t is not None:
                uri = g['images'][g['textures'][t['index']]['source']].get('uri')
                p = os.path.join(os.path.dirname(scene), uri) if uri else None
                if p and os.path.exists(p):
                    out[m.get('name', '')] = p
    except Exception:
        pass
    return out


def resolve_images(src, mats, notes, scene=''):
    """A material whose base colour the importer could not follow (a node in between, or an image it shared
    with another material) gets its texture from the glTF itself, else by name."""
    direct = gltf_images(scene)
    for m in mats:
        if m['image']:
            continue
        if m['name'] in direct:
            m['image'] = direct[m['name']]
            notes.append('%s: its base colour from the glTF (%s)' % (m['name'], os.path.basename(m['image'])))
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


def bake_cavity(base, normal, path, strength=.45):
    """The base colour darkened where the tangent-space normal map tilts away (seams, folds, stitching, pores):
    the detail of the 592k-triangle sculpt the decimated suit cannot keep, independent of the light (the
    shader lights per vertex). -> path."""
    if os.path.exists(path):
        return path
    b = np.asarray(Image.open(base).convert('RGB'), np.float32) / 255
    n = np.asarray(Image.open(normal).convert('RGB').resize(Image.open(base).size, Image.BILINEAR), np.float32) / 255
    nz = np.clip(n[..., 2] * 2 - 1, 0, 1)
    cav = 1 - strength * (1 - nz ** 1.5)
    Image.fromarray((np.clip(b * cav[..., None], 0, 1) * 255 + .5).astype(np.uint8)).save(path)
    return path


def quadrant_materials(mats, uv, mat, work, folder):
    """One textured material -> 4 (the triangles by the UV quadrant of their centre), so atlases() crops each
    quadrant of a 4096 px texture into its own 1024 px atlas (2048 px across, not 1024). The few triangles the
    decimation stretched over a UV seam (more than a quadrant across, about 40 of 24,000: their texture smeared
    either way) are clamped into their quadrant (2% over its edge) so they do not widen its crop. -> mats, mat,
    notes, uv."""
    out, newmat, notes, uv = [], mat.copy(), [], uv.copy()
    c = uv.mean(1)
    for i, m in enumerate(mats):
        sel = mat == i
        if not m['image'] or not sel.any():
            continue
        q = (c[:, 0] >= .5).astype(np.int64) + 2 * (c[:, 1] >= .5).astype(np.int64)
        for k in range(4):
            link = os.path.join(work, '%s_q%d%s' % (folder, k, os.path.splitext(m['image'])[1]))
            if os.path.lexists(link):
                os.remove(link)
            os.symlink(os.path.abspath(m['image']), link)
            mm = dict(m, name='%s_q%d' % (m['name'], k), image=link)
            mats = mats + [mm]
            here = sel & (q == k)
            newmat[here] = len(mats) - 1
            lo = np.array([.5 * (k % 2), .5 * (k // 2)]) - .02
            uv[here] = np.clip(np.clip(uv[here], lo, lo + .54), 0, 1)
        notes.append('%s split into 4 UV quadrants (one atlas each; seam-stretched triangles clamped)' % m['name'])
    return mats, newmat, notes, uv


def make_orm(normal, roughness, metallic, path, strength=.45):
    """0.0.78 PBR: the glTF ORM map in the base colour's UVs: R the occlusion (the normal map's cavity, as
    bake_cavity's darkening, now applied by the shader to the sky light only), G the roughness, B the metallic (the
    model's own maps, 1 and 0 without). -> path."""
    n = np.asarray(Image.open(normal).convert('RGB'), np.float32) / 255
    size = (n.shape[1], n.shape[0])
    nz = np.clip(n[..., 2] * 2 - 1, 0, 1)
    o = 1 - strength * (1 - nz ** 1.5)

    def grey(f, empty):
        if not f:
            return np.full(o.shape, empty, np.float32)
        return np.asarray(Image.open(f).convert('L').resize(size, Image.BICUBIC), np.float32) / 255
    out = np.stack([o, grey(roughness, 1), grey(metallic, 0)], -1)
    Image.fromarray((np.clip(out, 0, 1) * 255 + .5).astype(np.uint8)).save(path)
    return path


def corner_tangents(P, N, U):
    """0.0.78 PBR: glTF TANGENT per corner (P, N, U (C,3/3/2), three a triangle): T = dP/du of each triangle
    made perpendicular to the corner's normal, w +-1 with the bitangent cross(N, T) w = -dP/dv (glTF v runs down
    the image), as model_pbr::generateTangents. -> (C,4) float32 (0 where the uvs give none)."""
    p, n, u = P.reshape(-1, 3, 3).astype(np.float64), N.reshape(-1, 3, 3).astype(np.float64), U.reshape(-1, 3, 2).astype(np.float64)
    e1, e2 = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    d1, d2 = u[:, 1] - u[:, 0], u[:, 2] - u[:, 0]
    r = d1[:, 0] * d2[:, 1] - d2[:, 0] * d1[:, 1]
    ok = np.abs(r) > 1e-12
    r = np.where(ok, r, 1)
    t = (e1 * d2[:, 1:2] - e2 * d1[:, 1:2]) / r[:, None]
    b = (e2 * d1[:, 0:1] - e1 * d2[:, 0:1]) / r[:, None]
    fn = np.cross(e1, e2)
    w = np.where((np.cross(fn, t) * -b).sum(1) < 0, -1., 1.)
    out = np.zeros((len(p), 3, 4))
    for c in range(3):
        tc = t - n[:, c] * (n[:, c] * t).sum(1, keepdims=True)
        ln = np.linalg.norm(tc, axis=1, keepdims=True)
        good = ok & (ln[:, 0] > 1e-12)
        out[:, c, :3] = np.where(good[:, None], tc / np.maximum(ln, 1e-12), 0)
        out[:, c, 3] = np.where(good, w, 0)
    return out.reshape(-1, 4).astype(np.float32)


def combine_normals(baked, source, path):
    """The high-to-low bake where it is sound, the source's own normal map elsewhere (a texel no low triangle
    covers stays black; a ray that hit the wrong surface tilts past 70 degrees, or far from the source's own)."""
    from PIL import Image
    b = np.asarray(Image.open(baked).convert('RGB'), np.float32) / 255 * 2 - 1
    src = Image.open(source).convert('RGB')
    if src.size != (b.shape[1], b.shape[0]):
        src = src.resize((b.shape[1], b.shape[0]), Image.BILINEAR)
    a = np.asarray(src, np.float32) / 255 * 2 - 1
    ok = (b[..., 2] > .34) & (np.abs(b).sum(2) > .5)
    # and within 37 degrees of the source's own normal: the bake adds the sculpt's detail on top of it; a texel
    # that turns far from it is a ray that hit the wrong surface (the dark swaths of the first bake)
    cos = (a * b).sum(2) / np.maximum(np.linalg.norm(a, axis=2) * np.linalg.norm(b, axis=2), 1e-6)
    ok &= cos > .8
    out = np.where(ok[..., None], b, a)
    Image.fromarray(np.clip((out + 1) * 127.5 + .5, 0, 255).astype(np.uint8)).save(path)
    print('  normals: %.1f%% of texels from the high-to-low bake' % (100 * ok.mean()), flush=True)
    return path


def holster_off(tris, uv, skin, texture, J, ends):
    """0.0.78 (t1884u): the rest hands close over the holsters' tops, so a hand's weights also held the leather
    under its fingers (one welded surface: no part to cut by). A triangle mostly on a hand or forearm, below its
    wrist, whose base colour is the holsters' brown leather (not the black glove, not the bare fingertips: those
    are much redder and brighter) goes to the thigh of that side."""
    from PIL import Image
    im = np.asarray(Image.open(texture).convert('RGB'), np.float32) / 255
    h, w = im.shape[:2]
    m = np.mod(uv.mean(1), 1)
    col = im[np.clip((m[:, 1] * h).astype(int), 0, h - 1), np.clip((m[:, 0] * w).astype(int), 0, w - 1)]
    br, rb = col.mean(1), col[:, 0] - col[:, 2]
    leather = (br > .2) & (rb > .03) & (rb < .15)
    skin_ = (rb >= .15) & (br > .3)  # the bare fingertips
    c = tris.mean(1)
    moved = 0
    for side in 'LR':
        arm = [sk.BONES.index('fore' + side), sk.BONES.index('hand' + side)]
        wy = J[sk.JOINTS.index('wrist' + side)][1]
        sg = 1 if side == 'L' else -1
        tip = ends[sk.TIP_L if side == 'L' else sk.TIP_R]
        # and anything not glove-black, not bare skin, nearer the body than the fingertips (the holster's edge in the fist)
        inner = (br > .16) & ~skin_ & (sg * c[:, 0] < sg * tip[0] - .015)
        on = (skin[:, :, arm].sum(2).mean(1) > .5) & (leather | inner) & (c[:, 1] < wy - .01) & (np.sign(c[:, 0]) == sg)
        # and, round the wrist (4 cm either way along the forearm), what stands off it by more than 5.8 cm: the holster
        # box beside the rest wrist (the wrist itself is about 4.5 cm round its axis)
        e, wr = J[sk.JOINTS.index('elbow' + side)], J[sk.JOINTS.index('wrist' + side)]
        ax = (wr - e) / np.linalg.norm(wr - e)
        dv = c - wr
        al = dv @ ax
        rad = np.linalg.norm(dv - np.outer(al, ax), axis=1)
        on |= (skin[:, :, arm].sum(2).mean(1) > .3) & (np.abs(al) < .04) & (rad > .058) & (np.sign(c[:, 0]) == sg)
        thigh = sk.BONES.index('thigh' + side)
        skin[on] = 0
        skin[on, :, thigh] = 1
        moved += int(on.sum())
    return skin, ['%d holster-leather triangles under the rest hands put on the thighs (by their base colour)' % moved]


def keep_arms(tris, skin, ends, keep):
    """0.0.78 PUNISHER HQ: a suit whose rest hands touch its holsters (the owner's own model) got the hands' and
    forearms' bone-heat weights on the holster and thigh gear they touch, which then flew off with the hand. Each
    corner weighted to a forearm or hand but farther than `keep` (m at 1.8 m) from every forearm and hand bone and
    nearer another bone: that arm weight goes to the nearest other bone."""
    arm = [sk.BONES.index(n) for n in ('foreL', 'handL', 'foreR', 'handR')]
    E = np.asarray(ends, np.float64)
    seg = lambda P, a, b: np.linalg.norm(P - (a + np.outer(np.clip(((P - a) @ (b - a)) / max(1e-9, (b - a) @ (b - a)), 0, 1), b - a)), axis=1)
    P = tris.reshape(-1, 3).astype(np.float64)
    W = skin.reshape(-1, 16).copy()
    dist = np.stack([seg(P, E[sk.BONE_ENDS[b, 0]], E[sk.BONE_ENDS[b, 1]]) for b in range(16)], 1)
    others = [b for b in range(16) if b not in arm]
    darm, doth = dist[:, arm].min(1), dist[:, others].min(1)
    stray = (W[:, arm].sum(1) > 1e-4) & (darm > keep * HEIGHT / 1.8) & (doth < darm)
    near = np.array(others)[dist[:, others].argmin(1)]
    moved = W[stray][:, arm].sum(1)
    W[np.ix_(stray, arm)] = 0
    W[np.nonzero(stray)[0], near[stray]] += moved
    W /= np.maximum(W.sum(1, keepdims=True), 1e-9)
    n = int(stray.sum())
    notes = ['%d corners off the arms (farther than %.0f cm from the forearms and hands, nearer another bone: the '
             'holsters the rest hands touch)' % (n, keep * 100)] if n else []
    # The model is one welded surface: its rest hands are joined to the holsters they touch, so a triangle (or a
    # corner) shared by a hand and the thigh's gear stretched between them as the hand moved (21 cm spikes in
    # apps/wrist_twist_preview). Each corner keeps either its forearm/hand weight or the weight of the bones off
    # that arm (whichever is more), and each triangle goes wholly to the side most of its corners are on (the
    # others take its nearest such corner's weights): the hand parts from the holster cleanly.
    W = W.reshape(-1, 3, 16)
    cut_c = cut_t = 0
    for side in 'LR':
        a = [sk.BONES.index(n) for n in ('fore' + side, 'hand' + side)]
        chain = a + [sk.BONES.index(n) for n in ('clav' + side, 'upper' + side)]
        u = [b for b in range(16) if b not in chain]
        wa, wu = W[..., a].sum(-1), W[..., u].sum(-1)
        mixed = (wa > 1e-3) & (wu > 1e-3)
        keep_a = mixed & (wa > wu)
        keep_u = mixed & ~(wa > wu)
        W[..., a] = np.where(keep_u[..., None], 0, W[..., a])
        W[..., u] = np.where(keep_a[..., None], 0, W[..., u])
        cut_c += int(mixed.sum())
        on_a = W[..., a].sum(-1) > W[..., u].sum(-1)
        cnt = on_a.sum(1)
        for t in np.nonzero((cnt > 0) & (cnt < 3))[0]:
            maj = cnt[t] >= 2
            src = [c for c in range(3) if on_a[t, c] == maj]
            for c in range(3):
                if on_a[t, c] != maj:
                    d = [np.linalg.norm(tris[t, c] - tris[t, k]) for k in src]
                    W[t, c] = W[t, src[int(np.argmin(d))]]
            cut_t += 1
    W /= np.maximum(W.sum(-1, keepdims=True), 1e-9)
    if cut_c or cut_t:
        notes.append('%d corners and %d triangles parted between the hands and what the rest hands touch' % (cut_c, cut_t))
    return tris, W.reshape(skin.shape).astype(skin.dtype), notes


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


def atlases(mats, uv, mat, used, work, folder, most=ATLASES, size=ATLAS):
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
    n = min(most, len(tex))
    groups, load = [[] for _ in range(n)], [0.] * n
    for i in sorted(tex, key=lambda i: -area[i]):
        k = int(np.argmin(load))
        groups[k].append(i)
        load[k] += area[i]
    notes = []
    for k, gr in enumerate(groups):
        uv, mat, mats, scale = atlas(mats, uv, mat, gr, os.path.join(work, '%s_atlas%d.png' % (folder, k)), size=size,
                                    maps=('normal', 'orm'))
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
    T_all = corner_tangents(P_all, N_all, U_all) if any(mats[int(i)].get('normal') for i in set(mat.tolist())) else None
    cm = np.repeat(mat, 3)

    def texture(file, quality):
        if (file, quality) not in images:
            b = io.BytesIO()
            Image.open(file).convert('RGB').save(b, 'JPEG', quality=quality, subsampling=0)
            g['images'].append({'bufferView': view(b.getvalue()), 'mimeType': 'image/jpeg', 'name': os.path.basename(file)})
            g['textures'].append({'source': len(g['images']) - 1, 'sampler': 0})
            images[(file, quality)] = len(g['textures']) - 1
        return images[(file, quality)]
    for mi in sorted(set(int(m) for m in mat)):
        m = mats[mi]
        gm = {'name': m['name'][:120], 'pbrMetallicRoughness': {'baseColorFactor': list(m['factor']), 'metallicFactor': 0},
              'doubleSided': True}
        if m['image']:  # JPEG (a third of a PNG's size)
            gm['pbrMetallicRoughness']['baseColorTexture'] = {'index': texture(m['image'], 90)}
            gm['pbrMetallicRoughness']['baseColorFactor'] = [1, 1, 1, 1]
        mapped = T_all is not None and bool(m.get('normal'))
        if mapped:  # 0.0.78 PBR: the normal map (q95: its highlights show blocks sooner than colour does) and ORM
            gm['normalTexture'] = {'index': texture(m['normal'], 95)}
        if m.get('orm'):
            o = texture(m['orm'], 92)
            gm['pbrMetallicRoughness'].update(metallicRoughnessTexture={'index': o}, metallicFactor=1, roughnessFactor=1)
            gm['occlusionTexture'] = {'index': o}
        g['materials'].append(gm)
        sel = cm == mi
        P, N, U, Jm, Wm = P_all[sel], N_all[sel], U_all[sel], J[sel], Wq[sel]
        key = np.c_[np.round(P, 6), np.round(N, 4), np.round(U, 6), Jm, Wm]
        if mapped:  # a mirrored uv island's corners kept apart (their hands differ)
            key = np.c_[key, T_all[sel][:, 3]]
        uniq, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
        inv = inv.ravel().astype(np.uint32)
        if mapped:  # the corners' tangents summed onto their vertex, made perpendicular to its normal again
            T = np.zeros((len(first), 3), np.float64)
            np.add.at(T, inv, T_all[sel][:, :3])
            T -= N[first] * (N[first] * T).sum(1, keepdims=True)
            ln = np.linalg.norm(T, axis=1, keepdims=True)
            Tw = np.c_[np.where(ln > 1e-9, T / np.maximum(ln, 1e-9), 0), np.where(ln[:, 0] > 1e-9, T_all[sel][first, 3], 0)]
            Tw = Tw.astype(np.float32)
        P, N, U, Jm, Wm = P[first], N[first], U[first], Jm[first], Wm[first]
        acc = len(g['accessors'])
        g['accessors'] += [
            {'bufferView': view(P.tobytes()), 'componentType': 5126, 'count': len(P), 'type': 'VEC3', 'min': P.min(0).tolist(), 'max': P.max(0).tolist()},
            {'bufferView': view(N.tobytes()), 'componentType': 5126, 'count': len(N), 'type': 'VEC3'},
            {'bufferView': view(U.tobytes()), 'componentType': 5126, 'count': len(U), 'type': 'VEC2'},
            {'bufferView': view(Jm.tobytes()), 'componentType': 5121, 'count': len(Jm), 'type': 'VEC4'},
            {'bufferView': view(Wm.tobytes()), 'componentType': 5121, 'normalized': True, 'count': len(Wm), 'type': 'VEC4'},
            {'bufferView': view(inv.tobytes()), 'componentType': 5125, 'count': len(inv), 'type': 'SCALAR'}]
        attributes = {'POSITION': acc, 'NORMAL': acc + 1, 'TEXCOORD_0': acc + 2, 'JOINTS_0': acc + 3, 'WEIGHTS_0': acc + 4}
        if mapped:
            g['accessors'].append({'bufferView': view(Tw.tobytes()), 'componentType': 5126, 'count': len(Tw), 'type': 'VEC4'})
            attributes['TANGENT'] = acc + 6
        g['meshes'][0]['primitives'].append({'attributes': attributes, 'indices': acc + 5, 'material': len(g['materials']) - 1})
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
        share = float((mat == i).mean()) if len(mat) else 0
        if re.search(s['drop'], m['name'], re.I) and share >= .09:  # named like an eye, big as a head or a cape
            notes.append('kept %s (%d triangles: named like a hidden part, too big to be one)' % (m['name'], int((mat == i).sum())))
        elif re.search(s['drop'], m['name'], re.I):
            keep &= mat != i
            notes.append('dropped %s (%d triangles, hidden inside the suit)' % (m['name'], int((mat == i).sum())))
        elif s.get('props') and re.search(s['props'], m['name'], re.I):  # held / carried, not worn (Cap's shield)
            keep &= mat != i
            notes.append('dropped %s (%d triangles, a prop not worn: the game draws its own)' % (m['name'], int((mat == i).sum())))
    if s.get('prop_bones') and meta['bones'] and sj.size:  # a prop modelled in the mesh, on its own bone (Thor 1's hammer)
        names = [b['name'] if isinstance(b, dict) else str(b) for b in meta['bones']]
        top = np.take_along_axis(sj[:, 0, :], np.argmax(sw[:, 0, :], 1)[:, None], 1)[:, 0]
        on = np.array([t >= 0 and bool(re.search(s['prop_bones'], names[t], re.I)) for t in top])
        if on.any():
            keep &= ~on
            notes.append('dropped %d triangles on bone %s (a prop not worn: the game draws its own)' % (
                int(on.sum()), ', '.join(sorted({names[t] for t in top[on]}))))
    tris, uv, mat, sj, sw = tris[keep], uv[keep], mat[keep], sj[keep], sw[keep]
    if s.get('texture'):  # the base colour given beside the model (its glb packs it, the extract cannot follow)
        tex = s['texture']
        if s.get('cavity') and not s.get('pbr'):
            tex = bake_cavity(tex, s['cavity'], os.path.join(work, folder + '_cavity.png'))
            notes.append('the normal map\'s cavity baked into the base colour (the suit shader has no normal map)')
        for m in mats:
            m['image'], m['factor'] = tex, [1, 1, 1, 1]
    resolve_images(src, mats, notes, scene)
    P = tris.reshape(-1, 3)
    lo, hi = P.min(0), P.max(0)
    scale = HEIGHT / (hi[1] - lo[1])
    bones = meta['bones']
    rig = sk.rig_joints(bones) if bones else None
    if rig is not None:  # the game wears a suit facing +z, its left at +x: turn one modelled facing elsewhere
        sides = [rig[4] - rig[7], rig[10] - rig[13]]  # shoulderL - shoulderR, hipL - hipR
        turn = float(np.arctan2(sum(d[2] for d in sides), sum(d[0] for d in sides)))
        if abs(np.degrees(turn)) > 3:
            c, si = np.cos(turn), np.sin(turn)
            Ry = np.array([[c, 0, si], [0, 1, 0], [-si, 0, c]])
            tris = tris @ Ry.T
            rig = np.asarray(rig) @ Ry.T
            P = tris.reshape(-1, 3)
            lo, hi = P.min(0), P.max(0)
            notes.append('turned %d degrees to face the player\'s way (modelled facing %s)' % (
                round(np.degrees(turn)), 'sideways' if abs(abs(np.degrees(turn)) - 90) < 15 else 'off-axis'))
    elif s.get('turn_deg'):  # unrigged and modelled turned: measured by hand from a top view of its shoulders and belt
        turn = float(np.radians(s['turn_deg']))
        if abs(np.degrees(turn)) > 3:
            c, si = np.cos(turn), np.sin(turn)
            Ry = np.array([[c, 0, si], [0, 1, 0], [-si, 0, c]])
            tris = tris @ Ry.T
            P = tris.reshape(-1, 3)
            lo, hi = P.min(0), P.max(0)
            notes.append('turned %d degrees to face the player\'s way (measured: its shoulders and belt across +x)' % round(np.degrees(turn)))
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
    if s.get('joints'):  # measured by hand from the mesh's slices where the shape guess fails (a hand resting on a holster)
        J = np.array(J, float)
        for name, at in s['joints'].items():
            if name in sk.JOINTS:
                J[sk.JOINTS.index(name)] = at
        notes.append('joints set by hand: ' + ', '.join(sorted(s['joints'])))
    if s.get('drop_flat_behind'):  # a prop carried on the back (Cap's shield: the game draws its own)
        gone = flat_behind(tris)
        if gone.any():
            notes.append('dropped a flat prop on the back (%d triangles: the game draws its own)' % int(gone.sum()))
            tris, uv, mat, sj, sw = tris[~gone], uv[~gone], mat[~gone], sj[~gone], sw[~gone]
            P = tris.reshape(-1, 3)
    if rig is None:
        notes.append('joints from the body\'s shape (no rig)')
    ends = sk.ends_of(J, P)
    for name, k in (('tipL', sk.TIP_L), ('tipR', sk.TIP_R)):  # the fingertips, where the reach caught a holster
        if name in (s.get('joints') or {}):
            ends[k] = s['joints'][name]
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
    np.savez(sin, tris=tris, uv=uv, mat=mat, target=s.get('target', TARGET), ends=ends, bones=sk.BONE_ENDS, skin=skin)
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
    if s.get('bake_normal'):  # 0.0.78 (t1889u): the source's every triangle and its normal map baked onto the decimated suit
        baked = os.path.join(work, folder + '_hi2lo_normal.png')
        bk = baked + '.key'
        bkey = hashlib.sha256(key.encode() + open(sout, 'rb').read()[:1 << 20] + open(os.path.join(HERE, 'bpy_bake_normal.py'), 'rb').read()).hexdigest()
        if not (os.path.exists(baked) and os.path.exists(bk) and open(bk).read() == bkey):
            bpy('bpy_bake_normal.py', sin, sout, s['cavity'], baked, log=os.path.join(work, folder + '_bake.log'))
            open(bk, 'w').write(bkey)
        combined = combine_normals(baked, s['cavity'], os.path.join(work, folder + '_normal_combined.png'))
        if s.get('pbr'):  # 0.0.78 PBR: the maps shipped as maps (the model shader lights them per pixel)
            orm = make_orm(combined, s.get('roughness'), s.get('metallic'), os.path.join(work, folder + '_orm.png'))
            for m in mats:
                m['image'], m['normal'], m['orm'] = s['texture'], combined, orm
            notes.append('a high-to-low normal bake (%d source triangles and the source normal map onto the %d shipped), '
                         'shipped as its normal map with an ORM map (occlusion from its cavity, the model\'s roughness and '
                         'metallic) and TANGENT; the base colour raw' % (src_tris, len(tris)))
        else:
            tex = bake_cavity(s['texture'], combined, os.path.join(work, folder + '_cavity_hi2lo.png'))
            for m in mats:
                m['image'] = tex
            notes.append('a high-to-low normal bake (%d source triangles and the source normal map onto the %d shipped), its cavity '
                         'baked into the base colour' % (src_tris, len(tris)))
    if s.get('arm_keep'):  # 0.0.78: the hands rest on the holsters - heat spread them onto the thighs' gear
        tris, skin, an = keep_arms(tris, skin, ends, s['arm_keep'])
        notes += an
        if s.get('texture'):
            skin, an = holster_off(tris, uv, skin, s['texture'], J, ends)
            notes += an
    mats, mat = merge_images(mats, mat)
    used = sorted(set(mat.tolist()))
    if s.get('quadrants'):
        mats, mat, qn, uv = quadrant_materials(mats, wrap_uv(uv), mat, work, folder)
        notes += qn
        used = sorted(set(mat.tolist()))
    uv, mat, mats, an = atlases(mats, wrap_uv(uv), mat, used, work, folder, s.get('atlases', ATLASES), s.get('atlas', ATLAS))
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
    title = '%s  (%s)  %d -> %d triangles' % (folder, s.get('zip', os.path.basename(s.get('glb', ''))), src_tris, len(tris))
    preview(os.path.join(work, 'preview_%s.png' % folder), tris, ends, skin, title)
    return dict(folder=folder, source=s.get('zip', os.path.basename(s.get('glb', ''))), source_triangles=src_tris, triangles=len(tris),
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
        print('==', s['folder'], s.get('zip', s.get('glb')), flush=True)
        rep = convert(s, a.downloads, a.out, a.work)
        print(json.dumps(rep, indent=1), flush=True)
        report.append(rep)
    path = os.path.join(a.out, 'Suits', 'conversion-report.json')
    old = json.load(open(path)) if os.path.exists(path) else []
    keep = [x for x in old if x['folder'] not in {r['folder'] for r in report}]
    json.dump(sorted(keep + report, key=lambda x: x['folder']), open(path, 'w'), indent=1)


if __name__ == '__main__':
    main()
