"""Version 0.0.72 test build 8 ROSTER PAGE pictures (the user, 11:27 PM: "wheres the roster page with the
profile pictures and suit options?"): one GLB, Models/Roster/scene.glb, that carries every picture the
wrist's ROSTER page shows, as real textures in the mod's own model texture array (mesh_model slotRoster):

  python3 tools/roster/make_roster_glb.py <alt-suits folder> <Models folder>

<alt-suits folder> holds suit_meta.json (each pick's Sketchfab uid, name, artist, licence, url),
thumbs1024/<uid>.jpg (each model's own Sketchfab thumbnail, a render of that model) and ext/ba_wolv.jpg
(BlendAtlas' image of the D AND W suit, which is not on Sketchfab).

PICTURES (its order is the page's: include/spidy/roster_page.hpp rosterPictureFolders mirrors it and
tests/roster_page_tests.py checks the two):
  0-10   the profile pictures, hero order (SPIDER-MAN .. GREEN LANTERN): a head-and-shoulders crop of one
         of THAT hero's picked suits' thumbnails (PORTRAITS says which), SPIDER-MAN a drawn web emblem
         (he has no picked suits: he wears the game's own);
  11-47  each suit's preview (the whole figure), by the catalog's folder name;
  48-68  0.0.73 COMING SOON: the SOON tab's 21 portraits (the 0.0.74 heroes, holo_roster.hpp soonHeroes
         order), tools/roster/soon/<key>.jpg made by soon_portraits.py (credits in soon/soon.json); one
         missing is drawn as a silhouette here (and the page draws its own silhouette without the model).
They are packed 3 x 3 into 1024 px JPEG atlases (8 images), each picture with an 8 px edge-repeated border
so mips do not bleed. Picture k is triangles 2k and 2k+1 (a quad whose UVs are its cell); the page reads
each picture's layer and UV box from them and draws its own flat quad - nothing is flattened, decimated or
re-lit. Models/Roster/license.txt credits every artist."""
import io, json, os, struct, sys

HERO_KEYS = ['spiderman', 'ironman', 'daredevil', 'hulk', 'batman', 'wolverine', 'superman', 'punisher',
             'captainamerica', 'thor', 'greenlantern']
CPP_HERO = {'ironMan': 'ironman', 'captainAmerica': 'captainamerica', 'greenLantern': 'greenlantern'}
# The profile picture: (suit_meta key, crop centre x, centre y, square size) in the 1024 x 576 thumbnail.
PORTRAITS = [
    None,  # SPIDER-MAN: drawn (spider_tile)
    ('ironman_1', 512, 285, 560),
    ('daredevil_1', 512, 135, 230),
    ('hulk_4', 512, 225, 420),
    ('batman_2', 512, 288, 576),
    ('wolverine_3', 512, 135, 220),
    ('superman_2', 512, 288, 576),
    ('punisher_warzone', 512, 200, 280),
    ('captainamerica_4', 512, 132, 215),
    ('thor_3', 512, 215, 420),
    ('greenlantern_1', 512, 185, 250),
]
# The suit previews, the catalog's order (hero072.hpp SuitCatalog::builtIn), by folder.
SUIT_FOLDERS = (['CapSuit'] + ['Suit_batman_%d' % n for n in (1, 2, 3, 5)] + ['Suit_ironMan_%d' % n for n in (1, 2, 3, 4)] +
                ['Suit_captainAmerica_%d' % n for n in (1, 2, 4)] + ['Suit_daredevil_%d' % n for n in (1, 2, 3)] +
                ['Suit_hulk_%d' % n for n in (1, 2, 4)] + ['Suit_wolverine_%d' % n for n in (1, 2, 3, 4, 5)] +
                ['Suit_punisher_%d' % n for n in (1, 2)] + ['Suit_thor_%d' % n for n in (1, 2, 3)] +
                ['Suit_greenLantern_%d' % n for n in (1, 2, 3, 4)] + ['Suit_superman_%d' % n for n in (1, 2, 3, 4)] +
                ['PunisherWarzone'])
SOON_KEYS = ['deadpool', 'nightcrawler', 'moonknight', 'magneto', 'cyclops', 'jeangrey', 'vision', 'nickfury',
             'wintersoldier', 'doctorstrange', 'mrfantastic', 'ghostrider', 'humantorch', 'silversurfer',
             'thething', 'cyborg', 'flash', 'hawkman', 'robin', 'nightwing', 'raven']
SOON_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'soon')
PICTURE_FOLDERS = ['portrait_' + k for k in HERO_KEYS] + SUIT_FOLDERS + ['soon_' + k for k in SOON_KEYS]
ATLAS, CELLS, BORDER = 1024, 3, 8
EXT_WOLVERINE = ('ext/ba_wolv.jpg', 'BlendAtlas Wolverine (D&W suit)', 'BlendAtlas', 'image from the artist\'s page',
                 'https://blendatlas.com (not on Sketchfab)')


def meta_key(folder):
    if folder == 'CapSuit':
        return 'cap_iqbal'
    if folder == 'PunisherWarzone':
        return 'punisher_warzone'
    _, hero, n = folder.split('_')
    return '%s_%s' % (CPP_HERO.get(hero, hero), n)


def spider_tile(size):
    """SPIDER-MAN's tile: the game's own suit, so no artist's thumbnail - a drawn web and spider emblem."""
    import math
    from PIL import Image, ImageDraw
    s = size * 4
    im = Image.new('RGB', (s, s), (178, 18, 30))
    d = ImageDraw.Draw(im)
    c = s / 2
    for i in range(16):
        a = i * math.pi / 8
        d.line([(c, c), (c + math.cos(a) * s, c + math.sin(a) * s)], fill=(20, 8, 10), width=s // 140)
    for r in range(1, 9):
        rr = r * s / 13
        pts = [(c + math.cos(i * math.pi / 8) * rr, c + math.sin(i * math.pi / 8) * rr) for i in range(17)]
        d.line(pts, fill=(20, 8, 10), width=s // 160)
    k = s / 512  # the spider (body, head, eight legs)
    def P(x, y):
        return (c + x * k, c + y * k)
    for side in (-1, 1):
        for (ax, ay, bx, by, ex, ey) in ((14, -40, 70, -110, 90, -190), (16, -18, 95, -50, 150, -95),
                                         (16, 8, 95, 40, 150, 95), (14, 30, 70, 100, 90, 190)):
            d.line([P(side * ax, ay), P(side * bx, by), P(side * ex, ey)], fill=(12, 6, 8), width=int(14 * k))
    d.ellipse([P(-30, -62), P(30, 0)], fill=(12, 6, 8))
    d.ellipse([P(-38, -10), P(38, 110)], fill=(12, 6, 8))
    d.ellipse([P(-22, -100), P(22, -56)], fill=(12, 6, 8))
    return im.resize((size, size), Image.LANCZOS)


def silhouette(size):
    """A COMING SOON picture with no portrait file: a plain head-and-shoulders silhouette."""
    from PIL import Image, ImageDraw
    im = Image.new('RGB', (size, size), (18, 22, 32))
    d = ImageDraw.Draw(im)
    s = size / 100.
    d.ellipse([35 * s, 18 * s, 65 * s, 52 * s], fill=(70, 78, 96))
    d.rounded_rectangle([18 * s, 56 * s, 82 * s, 112 * s], radius=int(22 * s), fill=(70, 78, 96))
    return im


def soon_pictures(soon_dir=SOON_DIR):
    """The SOON tab's pictures and credits; a missing file is a silhouette (never an error)."""
    from PIL import Image
    path = os.path.join(soon_dir, 'soon.json')
    meta = json.load(open(path)) if os.path.exists(path) else {}
    out, credits = [], []
    for key in SOON_KEYS:
        f = os.path.join(soon_dir, key + '.jpg')
        if os.path.exists(f):
            out.append(Image.open(f).convert('RGB'))
            m = meta.get(key, {})
            credits.append(('soon_' + key, m.get('name', '?'), m.get('user', '?'), m.get('lic', '?'), m.get('url', '')))
        else:
            out.append(silhouette(400))
            credits.append(('soon_' + key, 'silhouette placeholder (no portrait yet)', '', '', ''))
    return out, credits


def pictures(src):
    from PIL import Image
    meta = json.load(open(os.path.join(src, 'suit_meta.json')))
    out, credits = [], []
    for h, p in enumerate(PORTRAITS):
        if p is None:
            out.append(spider_tile(512))
            credits.append((PICTURE_FOLDERS[h], 'drawn for HERO VR (the game\'s own suit; no artist\'s model)', '', '', ''))
            continue
        key, x, y, n = p
        m = meta[key]
        im = Image.open(os.path.join(src, 'thumbs1024', m['uid'] + '.jpg')).convert('RGB')
        out.append(im.crop((x - n // 2, y - n // 2, x + n // 2, y + n // 2)))
        credits.append((PICTURE_FOLDERS[h], m['name'], m['user'], m['lic'], m['url']))
    for folder in SUIT_FOLDERS:
        if folder == 'Suit_wolverine_5':
            path, name, user, lic, url = EXT_WOLVERINE
            im = Image.open(os.path.join(src, path)).convert('RGB')
        else:
            m = meta[meta_key(folder)]
            im = Image.open(os.path.join(src, 'thumbs1024', m['uid'] + '.jpg')).convert('RGB')
            name, user, lic, url = m['name'], m['user'], m['lic'], m['url']
        w, h = im.size
        s = min(w, h)
        out.append(im.crop(((w - s) // 2, (h - s) // 2, (w - s) // 2 + s, (h - s) // 2 + s)))
        credits.append((folder, name, user, lic, url))
    soon, soon_credits = soon_pictures()
    return out + soon, credits + soon_credits


def atlases(images):
    """3 x 3 pictures per 1024 px atlas; returns the atlases and each picture's (atlas, u0, v0, u1, v1)."""
    from PIL import Image
    cell = ATLAS // CELLS
    inner = cell - 2 * BORDER
    per = CELLS * CELLS
    sheets, boxes = [], []
    for k, im in enumerate(images):
        if k % per == 0:
            sheets.append(Image.new('RGB', (ATLAS, ATLAS), (0, 0, 0)))
        a, j = divmod(k, per)
        cx, cy = (j % CELLS) * cell, (j // CELLS) * cell
        pic = im.resize((inner, inner), Image.LANCZOS)
        # the border: the picture's edges repeated (a mip never reaches its neighbour's colours)
        edge = pic.resize((inner + 2 * BORDER, inner + 2 * BORDER), Image.NEAREST)
        edge.paste(pic, (BORDER, BORDER))
        sheets[a].paste(edge, (cx, cy))
        u0, v0 = (cx + BORDER + .5) / ATLAS, (cy + BORDER + .5) / ATLAS
        u1, v1 = (cx + BORDER + inner - .5) / ATLAS, (cy + BORDER + inner - .5) / ATLAS
        boxes.append((a, u0, v0, u1, v1))
    return sheets, boxes


def glb(sheets, boxes):
    """The GLB: one mesh, a primitive per atlas (its material -> texture -> embedded JPEG), picture k the
    quad of triangles 2k, 2k+1 at x = k (positions do not matter: the page draws its own quads)."""
    jpegs = []
    for s in sheets:
        b = io.BytesIO()
        s.save(b, 'JPEG', quality=92, subsampling=0, optimize=True)
        jpegs.append(b.getvalue())
    bin_ = bytearray()
    views, accessors, prims = [], [], []

    def add_view(data, target=None):
        while len(bin_) % 4:
            bin_.append(0)
        v = {'buffer': 0, 'byteOffset': len(bin_), 'byteLength': len(data)}
        if target:
            v['target'] = target
        bin_.extend(data)
        views.append(v)
        return len(views) - 1

    for a in range(len(sheets)):
        pos, uv = [], []
        for k, (ak, u0, v0, u1, v1) in enumerate(boxes):
            if ak != a:
                continue
            x0, x1 = float(k), k + .9
            # two triangles: (TL, BL, BR) (TL, BR, TR); glTF UVs, v down
            for (x, y, u, v) in ((x0, 1, u0, v0), (x0, 0, u0, v1), (x1, 0, u1, v1), (x0, 1, u0, v0), (x1, 0, u1, v1), (x1, 1, u1, v0)):
                pos += [x, y, 0.0]
                uv += [u, v]
        n = len(pos) // 3
        pv = add_view(struct.pack('<%df' % len(pos), *pos), 34962)
        tv = add_view(struct.pack('<%df' % len(uv), *uv), 34962)
        xs, ys = pos[0::3], pos[1::3]
        accessors.append({'bufferView': pv, 'componentType': 5126, 'count': n, 'type': 'VEC3',
                          'min': [min(xs), min(ys), 0.0], 'max': [max(xs), max(ys), 0.0]})
        accessors.append({'bufferView': tv, 'componentType': 5126, 'count': n, 'type': 'VEC2'})
        prims.append({'attributes': {'POSITION': len(accessors) - 2, 'TEXCOORD_0': len(accessors) - 1}, 'material': a, 'mode': 4})
    images = [{'bufferView': add_view(j), 'mimeType': 'image/jpeg', 'name': 'roster_atlas_%d' % i} for i, j in enumerate(jpegs)]
    while len(bin_) % 4:
        bin_.append(0)
    doc = {
        'asset': {'version': '2.0', 'generator': 'HERO VR tools/roster/make_roster_glb.py'},
        'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0, 'name': 'roster_pictures'}],
        'meshes': [{'primitives': prims, 'name': 'roster_pictures'}],
        'materials': [{'name': 'roster_atlas_%d' % i, 'pbrMetallicRoughness': {'baseColorTexture': {'index': i},
                       'metallicFactor': 0.0, 'roughnessFactor': 1.0}} for i in range(len(sheets))],
        'textures': [{'source': i} for i in range(len(sheets))],
        'images': images, 'accessors': accessors, 'bufferViews': views,
        'buffers': [{'byteLength': len(bin_)}],
    }
    js = json.dumps(doc, separators=(',', ':')).encode()
    js += b' ' * ((4 - len(js) % 4) % 4)
    total = 12 + 8 + len(js) + 8 + len(bin_)
    return (struct.pack('<III', 0x46546C67, 2, total) + struct.pack('<II', len(js), 0x4E4F534A) + js +
            struct.pack('<II', len(bin_), 0x004E4942) + bytes(bin_))


def license_text(credits):
    lines = ['HERO VR - Models/Roster/scene.glb: the ROSTER page\'s pictures (test build 8).',
             '', 'Each picture is the Sketchfab thumbnail (a render of the model) of the artist\'s model below,',
             'cropped and scaled; nothing else was changed. They are previews of the suits you picked; the',
             'suit models themselves are NOT in this build (MODEL MISSING) until they are downloaded.',
             'Licences: CC BY 4.0 https://creativecommons.org/licenses/by/4.0/ ; CC BY-NC 4.0',
             'https://creativecommons.org/licenses/by-nc/4.0/ ; CC BY-NC-SA 4.0',
             'https://creativecommons.org/licenses/by-nc-sa/4.0/ ; "Free Standard" = the Sketchfab Standard licence.',
             'soon_*: COMING SOON portraits (0.0.74 heroes, not playable yet): cropped from the model\'s Sketchfab',
             'thumbnail, or (FLASH, HAWKMAN) rendered from the model in Blender.', '']
    for k, (folder, name, user, lic, url) in enumerate(credits):
        if not user:
            lines.append('%2d %s: %s' % (k, folder, name))
        else:
            lines.append('%2d %s: "%s" by %s (%s) %s' % (k, folder, name, user, lic, url))
    return '\n'.join(lines) + '\n'


def main(src, models):
    images, credits = pictures(src)
    assert len(images) == len(PICTURE_FOLDERS) == 69, len(images)
    sheets, boxes = atlases(images)
    out = os.path.join(models, 'Roster')
    os.makedirs(out, exist_ok=True)
    data = glb(sheets, boxes)
    open(os.path.join(out, 'scene.glb'), 'wb').write(data)
    open(os.path.join(out, 'license.txt'), 'w').write(license_text(credits))
    print('Roster: %d pictures, %d atlases, %d bytes' % (len(images), len(sheets), len(data)))
    return sheets, boxes


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
