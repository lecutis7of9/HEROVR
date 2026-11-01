"""Synthetic Spider-Man Remastered suit model for the tests and `sm1suits.py selftest`: a body built from one
of our own suit packs, written with the same section layout a game .model has (no game data used)."""
import io
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import inject  # noqa: E402
import assets  # noqa: E402
from inject import (T_BUILT, T_INDEXES, T_JOINTS, T_LOOK, T_MATERIALS, T_MESHES, T_MUSCLE,  # noqa: E402
                    T_SKIN_BATCH, T_SKIN_DATA, T_VERTEXES)
import dat1lib  # noqa: E402
import dat1lib.crc32 as crc32  # noqa: E402
import dat1lib.crc64 as crc64  # noqa: E402
import dat1lib.types.dat1  # noqa: E402
import dat1lib.types.model  # noqa: E402
import dat1lib.types.sections.model.geo as geo  # noqa: E402

BONE_NAMES = ['head', 'neck', 'spine2', 'spine', 'l_clavicle', 'l_upperarm', 'l_forearm', 'l_hand', 'r_clavicle',
              'r_upperarm', 'r_forearm', 'r_hand', 'l_thigh', 'l_calf', 'r_thigh', 'r_calf']


def axes(kind):
    """canonical (left, up, forward) -> a game-like axis system. 'zup': Z up, facing -Y (proper rotation);
    'mirror': Y up, facing +Z, the body's left at -X (a left-handed numbering)."""
    if kind == 'zup':
        return lambda p: (p[0], -p[2], p[1])
    if kind == 'mirror':
        return lambda p: (-p[0], p[1], p[2])
    return lambda p: tuple(p)


def make_model(pos, nrm, uv, tris, bones, kind='zup', materials=('characters/hero/hero_spiderman/materials/body.material',
                                                                  'characters/hero/hero_spiderman/materials/eyes.material'),
               extra_meshes=1):
    """pos/nrm canonical, uv glTF, tris (a, b, c), bones: dominant canonical bone per vertex. Two skinned meshes
    (below / above the neck) in look 0 LOD 0, plus `extra_meshes` unskinned ones. Returns .model bytes."""
    tf = axes(kind)
    P = [tf(p) for p in pos]
    N = [tf(n) for n in nrm]
    if kind == 'mirror':
        tris = [(a, c, b) for a, b, c in tris]
    H = max(p[1] for p in pos)
    head = [i for i in range(len(pos)) if pos[i][1] > 0.86 * H]
    hs = set(head)
    body = [i for i in range(len(pos)) if i not in hs]
    order = body + head
    remap = {old: new for new, old in enumerate(order)}
    tb = [t for t in tris if all(v not in hs for v in t)]
    th = [t for t in tris if all(v in hs for v in t)]

    hdr = struct.pack('<III', dat1lib.types.model.Model.MAGIC, 0, 0) + bytes(24)
    m = dat1lib.types.model.Model(io.BytesIO(hdr + dat1lib.types.dat1.DAT1.EMPTY_DATA), version=dat1lib.VERSION_MSMR)
    d = m.dat1
    nmesh = 2 + extra_meshes
    for tag, size in ((T_BUILT, 0x80), (T_JOINTS, 16 * len(BONE_NAMES)), (T_MATERIALS, 32 * len(materials)),
                      (T_MESHES, 64 * nmesh), (T_LOOK, 256), (T_SKIN_BATCH, 0), (T_MUSCLE, 0x50),
                      (T_INDEXES, 0), (T_VERTEXES, 0), (T_SKIN_DATA, 0)):
        d.add_section(tag, bytes(size))
    built = bytearray(0x80)
    built[0x30:0x34] = struct.pack('<I', 0)  # uv shift 0: |uv| < 2
    struct.pack_into('<II', built, 0x64, 3 * (len(tb) + len(th)) + 3, len(order) + 1)
    d.add_section(T_BUILT, bytes(built))

    jd = bytearray()
    for k, n in enumerate(BONE_NAMES):
        jd += struct.pack('<hHHHII', -1 if k == 3 else 3, k, 0, 0, crc32.hash(n, normalize=False), d.add_string(n))
    d.add_section(T_JOINTS, bytes(jd))
    md = bytearray()
    names = []
    for k, path in enumerate(materials):
        name = 'mat%d' % k
        names.append(name)
        md += struct.pack('<QQ', d.add_string(path), d.add_string(name))
    for k, path in enumerate(materials):
        md += struct.pack('<QII', crc64.hash(path), crc32.hash(names[k], normalize=False), 0)
    d.add_section(T_MATERIALS, bytes(md))

    vs = d.get_section(T_VERTEXES)
    verts = []
    for i in order + [order[0]]:  # one spare vertex at the end
        v = geo.Vertex_I20((0, 0, 0), 0, (0, 0))
        v.x, v.y, v.z = P[i]
        v.nx, v.ny, v.nz = N[i]
        v.u, v.v = uv[i]
        verts.append(v)
    vs.vertexes = verts
    d._sections_data[d._sections_map[T_VERTEXES]] = bytearray(vs.save())

    nb = len(body)
    idx = [remap[v] for t in tb for v in t] + [remap[v] - nb for t in th for v in t] + [0, 0, 0]
    isec = d.get_section(T_INDEXES)
    isec.values = idx
    d._sections_data[d._sections_map[T_INDEXES]] = bytearray(isec.save())

    import dat1lib.types.sections.model.skin as skin
    raw = bytearray()
    batches = []
    for first, cnt in ((0, nb), (nb, len(head))):
        k = 0
        while k < cnt:
            c = min(inject.BATCH_MAX, cnt - k)
            sb = skin.SkinBatch()
            sb.offset = len(raw)
            sb.vertex_count = c
            sb.first_vertex = k
            for j in range(0, c, 16):
                grp = []
                for z in range(min(16, c - j)):
                    b = bones[order[first + k + j + z]]
                    grp.append([(b, 200), (3, 56)] if b != 3 else [(3, 256)])
                inject._encode_group(grp, raw)
            batches.append(sb)
            k += c
    d._sections_data[d._sections_map[T_SKIN_BATCH]] = bytearray(b''.join(b.save() for b in batches))
    d._sections_data[d._sections_map[T_SKIN_DATA]] = raw
    nbody = (nb + inject.BATCH_MAX - 1) // inject.BATCH_MAX

    me = bytearray()
    me += struct.pack('<IQHHHH', 0, 0xAAAA0001, 0, 0, 0, 0) + struct.pack('<IIII', 0, 0, 3 * len(tb), nb)
    me += struct.pack('<HHHH', 0x11, 0, 0, nbody) + struct.pack('<HHff', 0, 0, 1.0, 0.0) + struct.pack('<II', 0, 0)
    me += struct.pack('<IQHHHH', 0, 0xAAAA0002, 0, 0, 0, 0) + struct.pack('<IIII', nb, 3 * len(tb), 3 * len(th), len(head))
    me += struct.pack('<HHHH', 0x11, 1, nbody, len(batches) - nbody) + struct.pack('<HHff', 0, 0, 1.0, 0.0) + struct.pack('<II', 0, 0)
    for e in range(extra_meshes):
        me += struct.pack('<IQHHHH', 0, 0xAAAA0003 + e, 0, 0, 0, 0) + struct.pack('<IIII', len(order), len(idx) - 3, 3, 1)
        me += struct.pack('<HHHH', 0x0, 1, 0, 0) + struct.pack('<HHff', 0, 0, 1.0, 0.0) + struct.pack('<II', 0, 0)
    d.add_section(T_MESHES, bytes(me))
    look = bytearray()
    for lk in range(8):
        for lod in range(8):
            look += struct.pack('<HH', 0, 2) if lk == 0 and lod == 0 else struct.pack('<HH', 2, extra_meshes) if lod == 0 else bytes(4)
    d.add_section(T_LOOK, bytes(look))
    out = io.BytesIO()
    m.save(out)
    return out.getvalue()


# --- a whole fake game folder (tests and `sm1suits.py selftest`): asset_archive\toc + one archive -------------
# Everything below is synthetic and only as real as our readers need; the tags and magics are the ones
# assets.py / toc_ops.py read (CONFIRMED from ALERT / Overstrike), the section contents are made up.

FAKE_DIFFUSE = 'characters/hero/hero_spiderman/textures/hero_spiderman_classic_body_c.texture'
FAKE_NORMAL = 'characters/hero/hero_spiderman/textures/hero_spiderman_classic_body_n.texture'
FAKE_ICON = 'ui\\textures\\pause\\character\\suit_classic.texture'
TEST_TAG = 0x7E570001  # test-only section tag for the fake material (never written into a real game file)


def raw_dat1(sections, strings=b'', unk1=0):
    """DAT1 bytes: header, section table, strings, then each section 16-aligned."""
    head = 16 + 12 * len(sections)
    off = head + len(strings)
    table, body = b'', b''
    for tag, data in sections:
        pad = (-(off + len(body))) % 16
        body += bytes(pad)
        table += struct.pack('<III', tag, off + len(body), len(data))
        body += data
    total = off + len(body)
    return struct.pack('<IIIHH', 0x44415431, unk1, total, len(sections), 0) + table + strings + body


def fake_texture(fmt, w, h, mips, pixels):
    hdr = struct.pack('<IIHHHHHBBH', len(pixels), 0, w, h, w, h, 1, 0, 1, fmt) + bytes(8) + bytes([mips, 0, 0, 0]) + bytes(6)
    d = raw_dat1([(assets.T_TEX_HEADER, hdr)])
    return struct.pack('<II', assets.TEXTURE_MAGIC, len(d)) + bytes(28) + d + pixels


def fake_material():
    strings = b'\0' + FAKE_DIFFUSE.encode() + b'\0' + FAKE_NORMAL.encode() + b'\0'
    refs = struct.pack('<QQ', assets.aid(FAKE_DIFFUSE), assets.aid(FAKE_NORMAL))
    d = raw_dat1([(TEST_TAG, refs)], strings + bytes((-len(strings)) % 4))
    return struct.pack('<II', assets.MATERIAL_MAGIC, len(d)) + bytes(28) + d


def fake_localization(values):
    keys = sorted(values)
    kd, vd, ko, vo = b'', b'', [], []
    for k in keys:
        ko.append(len(kd))
        kd += k.encode() + b'\0'
        vo.append(len(vd))
        vd += values[k].encode() + b'\0'
    n = len(keys)
    hashes = [assets.crc32.hash(k, normalize=False) for k in keys]
    pairs = sorted((h, i) for i, h in enumerate(hashes))
    secs = [(assets.T_LOC_COUNT, struct.pack('<I', n)), (assets.T_LOC_KEYS, kd), (assets.T_LOC_KEYOFF, struct.pack('<%dI' % n, *ko)),
            (assets.T_LOC_VALS, vd), (assets.T_LOC_VALOFF, struct.pack('<%dI' % n, *vo)), (assets.T_LOC_FLAGS, bytes(4 * n)),
            (assets.T_LOC_HASH, struct.pack('<%dI' % n, *hashes)), (assets.T_LOC_SHASH, struct.pack('<%dI' % n, *[p[0] for p in pairs])),
            (assets.T_LOC_SIDX, struct.pack('<%dH' % n, *[p[1] for p in pairs]))]
    d = raw_dat1(secs)
    return struct.pack('<II', assets.LOC_MAGIC, len(d)) + bytes(28) + d


def make_game(folder, template_model):
    """A fake Spider-Man Remastered folder: asset_archive\\toc (two spans) and asset_archive\\g00s000."""
    import zlib
    import bcn
    aa = os.path.join(folder, 'asset_archive')
    os.makedirs(aa, exist_ok=True)
    model_path = 'characters/hero/hero_spiderman/hero_spiderman_classic.model'
    px = bcn.constant_block(72, (200, 30, 30, 255)) * 16 + bcn.constant_block(72, (200, 30, 30, 255)) * 4 + bcn.constant_block(72, (200, 30, 30, 255))
    span0 = {
        CLASSIC_VANITY_PATH: assets.make_config('VanityConfig', {'ModelList': {'Model': {'AssetPath': model_path}}}),
        model_path: template_model,
        'characters/hero/hero_spiderman/materials/body.material': fake_material(),
        FAKE_DIFFUSE: fake_texture(72, 16, 16, 3, px),
        FAKE_NORMAL: fake_texture(83, 16, 16, 3, bcn.flat_normal(83) * 21),
        FAKE_ICON: fake_texture(72, 16, 16, 3, px),
        assets.AID_PROGRESSION: assets.make_config('SystemProgressionConfig', {
            'TechWebLists': [{'Description': 'Gadgets', 'TechWebItems': []},
                             {'Description': 'Suits', 'TechWebItems': [{'Name': 'SUIT_classic', 'PreviewImage': FAKE_ICON}]}],
            'UnlockForFree': ['SUIT_classic']}),
        assets.AID_LOADOUTLIST: assets.make_config('MasterItemLoadoutListConfig', {
            'ItemLoadoutCategoryList': [{'Category': 'kVanity', 'ItemLoadoutConfigs': ['configs/masteritemloadoutlist/itemloadout_spiderman_classic.config']}]}),
        assets.AID_VANITY_MASTER: assets.make_config('VanityMasterListConfig', {'List': []}),
        assets.AID_VANITY_LAUNCH: assets.make_config('VanityLaunchListConfig', {'List': []}),
        assets.AID_LOCALIZATION: fake_localization({'INVALID': '', 'CHARWEB_MAY': 'May', 'SUIT_CLASSIC': 'Classic Suit'}),
    }
    span1 = {assets.AID_LOCALIZATION: fake_localization({'INVALID': '', 'SUIT_CLASSIC': 'Traje clasico'}),
             'configs/other.config': assets.make_config('Other', {'x': 1})}
    blob = bytearray(b'FAKE')  # not 'DSAR': an uncompressed archive
    ids, sizes, offs, spans = [], [], [], []
    for sp in (span0, span1):
        items = sorted(((k if isinstance(k, int) else assets.aid(k)), v) for k, v in sp.items())
        spans.append((len(ids), len(items)))
        for a, v in items:
            ids.append(a)
            sizes.append(len(v))
            offs.append(len(blob))
            blob += v
            blob += bytes((-len(blob)) % 16)
    with open(os.path.join(aa, 'g00s000'), 'wb') as f:
        f.write(blob)
    arch = struct.pack('<II', 0, 0) + b'g00s000'.ljust(64, b'\0')
    secs = [(0x398ABFF0, arch), (0x506D7B8A, b''.join(struct.pack('<Q', a) for a in ids)),
            (0x65BCF461, b''.join(struct.pack('<III', 1, s, i) for i, s in enumerate(sizes))),
            (0xDCD720B5, b''.join(struct.pack('<II', 0, o) for o in offs)),
            (0xEDE8ADA9, b''.join(struct.pack('<II', a, n) for a, n in spans))]
    d = raw_dat1(secs)
    with open(os.path.join(aa, 'toc'), 'wb') as f:
        f.write(struct.pack('<II', 0x77AF12AF, len(d)) + zlib.compress(d))
    return aa


CLASSIC_VANITY_PATH = 'configs/vanitytor1/vanity_spiderman_classic.config'


def mini_pack(src, dst, keep_every=6, tex=64):
    """A small copy of a suit pack for the tests: every `keep_every`-th triangle (vertices re-indexed) and each
    texture replaced by a one-colour BC1 mip chain (`tex` x `tex`, or 4 x 4 for a small one)."""
    import hsm1
    import bcn
    p = hsm1.Pack(src)
    h = dict(p.header)
    h.pop('blobs', None)
    groups, blobs = [], {}
    for gi, g in enumerate(p.header['groups']):
        ix = p.u32s(g['idx'])
        tris = [ix[i:i + 3] for i in range(0, len(ix) - 2, 3)][::keep_every]
        used = sorted(set(v for t in tris for v in t))
        remap = {v: k for k, v in enumerate(used)}
        f3 = p.floats(g['pos'])
        n3 = p.floats(g['nrm'])
        f2 = p.floats(g['uv'])
        j, w = p.u8s(g['jnt']), p.u8s(g['wgt'])
        n = 'g%d_' % gi
        blobs[n + 'pos'] = struct.pack('<%df' % (3 * len(used)), *[f3[3 * v + c] for v in used for c in range(3)])
        blobs[n + 'nrm'] = struct.pack('<%df' % (3 * len(used)), *[n3[3 * v + c] for v in used for c in range(3)])
        blobs[n + 'uv'] = struct.pack('<%df' % (2 * len(used)), *[f2[2 * v + c] for v in used for c in range(2)])
        blobs[n + 'jnt'] = bytes(j[4 * v + c] for v in used for c in range(4))
        blobs[n + 'wgt'] = bytes(w[4 * v + c] for v in used for c in range(4))
        blobs[n + 'idx'] = struct.pack('<%dI' % (3 * len(tris)), *[remap[v] for t in tris for v in t])
        size = 4 if g['tex']['width'] <= 4 else tex
        colour = (40 + 50 * gi, 90, 200 - 40 * gi, 255)
        chain, s, mips = b'', size, 0
        while True:
            b = max(1, s // 4)
            chain += bcn.constant_block(71, colour) * (b * b)
            mips += 1
            if s == 1:
                break
            s //= 2
        blobs[n + 'tex'] = chain
        groups.append({'vertices': len(used), 'indices': 3 * len(tris), 'pos': n + 'pos', 'nrm': n + 'nrm', 'uv': n + 'uv',
                       'jnt': n + 'jnt', 'wgt': n + 'wgt', 'idx': n + 'idx',
                       'tex': {'width': size, 'height': size, 'mips': mips, 'blob': n + 'tex'}})
    h['groups'] = groups
    hsm1.write(dst, h, blobs)
