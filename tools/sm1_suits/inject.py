"""HERO VR SM1 suits: read the game's own suit model and write a new one with our hero's mesh in it.

Follows ALERT's gltf_to_model injector (Tkachov, GPL-3; vendor/LICENSE-ALERT.txt): same skin encoding
(16-vertex groups, a group-size byte, then a bone byte or bone/weight byte pairs per vertex, at most 2560
vertices a batch), same mesh flags mask (0x111), same look ranges, LOD distances and muscle-deformation
clear. Like ALERT it writes into the template's own buffers when our mesh fits in them (sizes unchanged:
the case ALERT's users run); when it does not fit, the buffers grow and the Built section's vertex and
index counts are updated too (CANDIDATE: ALERT never grows them). Pure Python."""
import io
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
VENDOR = os.path.join(HERE, "vendor")
if VENDOR not in sys.path:
    sys.path.insert(0, VENDOR)

import dat1lib  # noqa: E402
import dat1lib.crc32 as crc32  # noqa: E402
import dat1lib.crc64 as crc64  # noqa: E402
import dat1lib.types.model  # noqa: E402
import dat1lib.types.sections.model.geo as geo  # noqa: E402
import dat1lib.types.sections.model.skin as skin  # noqa: E402

MODEL_MAGIC = 0x98906B9F
T_INDEXES = 0x0859863D
T_VERTEXES = 0xA98BE69B
T_BUILT = 0x283D0383
T_MESHES = 0x78D9CBDE
T_LOOK = 0x06EB7EFC
T_SKIN_BATCH = 0xC61B1FF5
T_SKIN_DATA = 0xDCA379A2
T_MATERIALS = 0x3250BB80
T_JOINTS = 0x15DF9D3B
T_MUSCLE = 0x380A5744
BATCH_MAX = 2560


class ModelError(Exception):
    pass


def f2u(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def u2f(u):
    return struct.unpack('<f', struct.pack('<I', u & 0xFFFFFFFF))[0]


def read_model(data):
    if len(data) < 40 or struct.unpack('<I', data[:4])[0] != MODEL_MAGIC:
        raise ModelError('not a Spider-Man Remastered .model (magic %08X)' % struct.unpack('<I', data[:4])[0])
    m = dat1lib.types.model.Model(io.BytesIO(data), version=dat1lib.VERSION_MSMR)
    for tag in (T_INDEXES, T_VERTEXES, T_MESHES, T_LOOK, T_MATERIALS):
        if m.dat1.get_section(tag) is None:
            raise ModelError('model has no section %08X' % tag)
    return m


def decode_skin(m):
    """Per model vertex: [(joint, weight 0..256), ...] or None, decoded like ALERT's model_to_gltf.
    Returns (skin dict vertex -> list, notes)."""
    d = m.dat1
    sd = d.get_section(T_SKIN_DATA)
    sb = d.get_section(T_SKIN_BATCH)
    meshes = d.get_section(T_MESHES).meshes
    out, notes = {}, []
    if sd is None or sb is None:
        return out, ['no skin sections']
    raw = bytes(sd._raw)
    owner = {}
    for mi, me in enumerate(meshes):
        if (me.flags & 0x1) and (me.flags & 0x10):
            for b in range(me.first_skin_batch, me.first_skin_batch + me.skin_batches_count):
                owner[b] = mi
    seq = 0  # ALERT's model_to_gltf reads the batches in order as one run of vertexes (vertex i = entry i)
    disagree = 0
    for bi, b in enumerate(sb.batches):
        off = b.offset
        if bi in owner and meshes[owner[bi]].vertexStart + b.first_vertex != seq:
            disagree += 1
        for j in range(0, b.vertex_count, 16):
            if off >= len(raw):
                break
            groups = raw[off] + 1
            off += 1
            for z in range(16):
                if j + z >= b.vertex_count:
                    break
                if groups == 1:
                    out[seq] = [(raw[off], 256)]
                    off += 1
                else:
                    v, order = {}, []
                    for _ in range(groups):
                        bone, w = raw[off], raw[off + 1]
                        off += 2
                        if w == 0:
                            continue
                        if bone not in v:
                            order.append(bone)
                        v[bone] = v.get(bone, 0) + w
                    out[seq] = [(bb, v[bb]) for bb in order] or [(0, 256)]
                seq += 1
    notes.append('%d skin batches, %d vertexes decoded in sequence (as ALERT reads them); %d batch(es) whose mesh start + first vertex differs from the sequence' % (
        len(sb.batches), seq, disagree))
    return out, notes


class Template(object):
    """The game's suit model, read once: its skinned body (look 0, LOD 0) and everything the dump reports."""

    def __init__(self, data):
        self.data = data
        self.m = read_model(data)
        d = self.m.dat1
        self.meshes = d.get_section(T_MESHES).meshes
        self.looks = d.get_section(T_LOOK).looks
        self.verts = d.get_section(T_VERTEXES).vertexes
        self.skin, self.skin_notes = decode_skin(self.m)
        look0 = self.looks[0].lods[0]
        self.body = [i for i in range(look0.start, look0.start + look0.count)
                     if i < len(self.meshes) and (self.meshes[i].flags & 0x1) and self.meshes[i].vertexCount > 0]
        if not self.body:
            raise ModelError('the suit model has no skinned mesh in look 0 / LOD 0')
        self.ref = max(self.body, key=lambda i: self.meshes[i].vertexCount)

    def body_points(self, limit=40000):
        """(points, skin lists) of the skinned look-0 LOD-0 meshes, at most `limit` (evenly thinned)."""
        idx = []
        for mi in self.body:
            me = self.meshes[mi]
            idx.extend(range(me.vertexStart, me.vertexStart + me.vertexCount))
        idx = [i for i in idx if i in self.skin and i < len(self.verts)]
        if not idx:
            raise ModelError('no decoded skin for the body meshes')
        step = max(1, int(math.ceil(len(idx) / float(limit))))
        idx = idx[::step]
        pts = [(self.verts[i].x, self.verts[i].y, self.verts[i].z) for i in idx]
        return pts, [self.skin[i] for i in idx]

    def materials(self):
        s = self.m.dat1.get_section(T_MATERIALS)
        out = []
        for k, (po, no) in enumerate(s.string_offsets):
            out.append({'index': k, 'path': self.m.dat1.get_string(po), 'name': self.m.dat1.get_string(no),
                        'aid': '%016X' % s.triples[k][0], 'name_crc32': '%08X' % s.triples[k][1], 'unk': s.triples[k][2]})
        return out

    def body_material_path(self):
        mats = self.materials()
        mi = self.meshes[self.ref].material_index
        return mats[mi]['path'] if mi < len(mats) else None

    def joint_names(self):
        s = self.m.dat1.get_section(T_JOINTS)
        out = []
        if s is None:
            return out
        for j in getattr(s, 'joints', []):
            try:
                out.append(self.m.dat1.get_string(j.string_offset))
            except Exception:
                out.append(None)
        return out

    def info(self):
        d = self.m.dat1
        built = d.get_section(T_BUILT)
        bi = {}
        if built is not None:
            raw = bytes(d._sections_data[d._sections_map[T_BUILT]])
            u = [struct.unpack('<I', raw[i:i + 4])[0] for i in range(0, len(raw) - 3, 4)]
            bi = {'u32': ['%08X' % x for x in u], 'uv_shift': (u[0x30 // 4] & 0xF) if len(u) > 0x30 // 4 else None,
                  'indexes': u[0x64 // 4] if len(u) > 0x64 // 4 else None, 'vertexes': u[0x68 // 4] if len(u) > 0x68 // 4 else None}
        sb = d.get_section(T_SKIN_BATCH)
        sd = d.get_section(T_SKIN_DATA)
        return {
            'sections': ['%08X:%d' % (h.tag, h.size) for h in d.header.sections],
            'stream_offset': self.m.offset_to_stream_sections,
            'vertexes': len(self.verts), 'indexes': len(d.get_section(T_INDEXES).values),
            'skin_batches': len(sb.batches) if sb else 0, 'skin_bytes': len(sd._raw) if sd else 0,
            'skin_decoded': len(self.skin), 'skin_notes': self.skin_notes,
            'built': bi,
            'meshes': [{'i': i, 'id': '%X' % me.unknowns[1], 'vstart': me.vertexStart, 'vcount': me.vertexCount,
                        'istart': me.indexStart, 'icount': me.indexCount, 'flags': '%X' % me.flags,
                        'material': me.material_index, 'skin_batches': [me.first_skin_batch, me.skin_batches_count],
                        'unknowns': list(me.unknowns), 'unknowns2': list(me.unknowns2)} for i, me in enumerate(self.meshes)],
            'looks': [[(l.start, l.count) for l in lk.lods] for lk in self.looks],
            'body_meshes': self.body, 'reference_mesh': self.ref,
            'materials': self.materials(), 'joints': self.joint_names(),
        }


def _encode_group(group, out):
    """ALERT's write_weights_group for up to 16 vertices; each vertex is [(bone, weight 0..256)] heaviest first."""
    n = 1
    for w in group:
        n = max(n, len(w))
    out.append(n - 1)
    for w in group:
        if n == 1:
            out.append(w[0][0] & 0xFF)
            continue
        bones = [0] * n
        weights = [0] * n
        bones[0] = w[0][0]
        weights[0] = 256
        for k in range(1, len(w)):
            bones[k] = w[k][0]
            weights[k] = max(0, min(255, int(w[k][1])))
            weights[0] -= weights[k]
        if weights[0] < 0:
            weights[0] = 0
        elif weights[0] > 255:
            weights[0] = 255
            weights[1] = 1
            bones[1] = bones[0]
        for k in range(n):
            if k > 0 and weights[k] == 0:
                bones[k] = bones[k - 1]
            out.append(bones[k] & 0xFF)
            out.append(weights[k] & 0xFF)


def tidy_weights(v, max_bones=4):
    """Merge repeats, drop zeros, keep the heaviest `max_bones`, renormalise to 256, heaviest first."""
    acc = {}
    for b, w in v:
        if w > 0:
            acc[b] = acc.get(b, 0) + w
    items = sorted(acc.items(), key=lambda t: -t[1])[:max_bones]
    if not items:
        return [(0, 256)]
    tot = float(sum(w for _, w in items))
    out = [(b, int(round(256.0 * w / tot))) for b, w in items]
    out = [(b, w) for b, w in out if w > 0] or [(items[0][0], 256)]
    return out


def _tangents(pos, uv, tris):
    n = len(pos)
    T = [[0.0, 0.0, 0.0] for _ in range(n)]
    B = [[0.0, 0.0, 0.0] for _ in range(n)]
    for a, b, c in tris:
        p0, p1, p2 = pos[a], pos[b], pos[c]
        e1 = (p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2])
        e2 = (p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2])
        x1, x2 = uv[b][0] - uv[a][0], uv[c][0] - uv[a][0]
        y1, y2 = uv[b][1] - uv[a][1], uv[c][1] - uv[a][1]
        r = x1 * y2 - x2 * y1
        if abs(r) < 1e-20:
            continue
        r = 1.0 / r
        t = ((e1[0] * y2 - e2[0] * y1) * r, (e1[1] * y2 - e2[1] * y1) * r, (e1[2] * y2 - e2[2] * y1) * r)
        bt = ((e2[0] * x1 - e1[0] * x2) * r, (e2[1] * x1 - e1[1] * x2) * r, (e2[2] * x1 - e1[2] * x2) * r)
        for i in (a, b, c):
            T[i][0] += t[0]; T[i][1] += t[1]; T[i][2] += t[2]  # noqa: E702
            B[i][0] += bt[0]; B[i][1] += bt[1]; B[i][2] += bt[2]  # noqa: E702
    return [tuple(t) for t in T], [tuple(b) for b in B]


def build(template_bytes, groups, material_paths, log):
    """groups: [{pos, nrm, uv, tris, skin}] in the template model's own axes (skin: [(joint, w)] per vertex,
    tris: (a, b, c) relative to the group, glTF winding). material_paths: one new material path per group.
    Returns the new model's bytes. Raises ModelError when it cannot be done (that hero is then skipped)."""
    m = read_model(template_bytes)
    d = m.dat1
    meshes = d.get_section(T_MESHES).meshes
    tpl = Template(template_bytes)
    refm = meshes[tpl.ref]
    if len(groups) > len(meshes):
        raise ModelError('%d mesh groups but the suit model has only %d mesh slots' % (len(groups), len(meshes)))
    rel = bool(refm.flags & 0x10)
    vsec = d.get_section(T_VERTEXES)
    isec = d.get_section(T_INDEXES)
    sbs = d.get_section(T_SKIN_BATCH)
    sds = d.get_section(T_SKIN_DATA)
    if sbs is None or sds is None:
        raise ModelError('the suit model has no skin sections')
    if refm.flags & 0x100:
        raise ModelError('the suit model body uses mesh flag 0x100 (a second weight table) that this tool does not write')

    total_v = sum(len(g['pos']) for g in groups)
    total_i = sum(3 * len(g['tris']) for g in groups)
    if not rel and total_v > 0xFFFF:
        raise ModelError('%d vertices do not fit 16-bit absolute indexes' % total_v)
    for g in groups:
        if len(g['pos']) > 0xFFFF:
            raise ModelError('a mesh group has more than 65535 vertices')

    # uv range: grow the Built uv scale (low 4 bits at 0x30) only if our uvs need it (CANDIDATE)
    built_ndx = d._sections_map.get(T_BUILT)
    built_raw = bytearray(d._sections_data[built_ndx]) if built_ndx is not None else None
    uv_shift = (struct.unpack('<I', built_raw[0x30:0x34])[0] & 0xF) if built_raw is not None and len(built_raw) >= 0x34 else 0
    uvmax = max([abs(c) for g in groups for uv in g['uv'] for c in uv] + [0.0])
    need = uv_shift
    while need < 15 and uvmax / ((1 << need) / 16384.0) > 32767:
        need += 1
    if need != uv_shift:
        if built_raw is None:
            raise ModelError('uvs out of range and no Built section')
        u = struct.unpack('<I', built_raw[0x30:0x34])[0]
        built_raw[0x30:0x34] = struct.pack('<I', (u & ~0xF) | need)
        log('uv scale shift %d -> %d for uvs up to %.2f (CANDIDATE)' % (uv_shift, need, uvmax))
    uv_scale = (1 << need) / 16384.0

    old_v, old_i = len(vsec.vertexes), len(isec.values)
    grow = total_v > old_v or total_i > old_i
    verts = list(vsec.vertexes)
    if total_v > old_v:
        verts += [geo.Vertex_I20((0, 0, 0), 0, (0, 0)) for _ in range(total_v - old_v)]
    for v in verts:  # unused template vertexes: zeroed (read-back uvs are raw ints the writer would rescale)
        v.x = v.y = v.z = 0.0
        v.u = v.v = 0
        v.tangent = None
        v.bitangent = None
    idx = list(isec.values)
    if total_i > old_i:
        idx += [0] * (total_i - old_i)

    skin_out = bytearray()
    batches = []
    vcur, icur = 0, 0
    updates = []
    for gi, g in enumerate(groups):
        pos, nrm, uv, tris, sk = g['pos'], g['nrm'], g['uv'], g['tris'], g['skin']
        n = len(pos)
        tans, bits = _tangents(pos, uv, tris)
        for k in range(n):
            v = verts[vcur + k]
            v.x, v.y, v.z = pos[k]
            v.nx, v.ny, v.nz = nrm[k]
            v.u, v.v = uv[k]
            v.tangent, v.bitangent = tans[k], bits[k]
        base = 0 if rel else vcur
        for t, (a, b, c) in enumerate(tris):
            idx[icur + 3 * t] = a + base
            idx[icur + 3 * t + 1] = b + base
            idx[icur + 3 * t + 2] = c + base
        # skin, split in batches of at most 2560 vertices and 16-vertex groups (ALERT)
        first_batch = len(batches)
        k = 0
        while k < n:
            cnt = min(BATCH_MAX, n - k)
            sb = skin.SkinBatch()
            sb.offset = len(skin_out)
            sb.unk1 = 0
            sb.vertex_count = cnt
            sb.first_vertex = k
            for j in range(0, cnt, 16):
                _encode_group([tidy_weights(sk[k + j + z]) for z in range(min(16, cnt - j))], skin_out)
            batches.append(sb)
            k += cnt
        updates.append((vcur, n, icur, 3 * len(tris), first_batch, len(batches) - first_batch))
        vcur += n
        icur += 3 * len(tris)

    # fill any leftover template batches like ALERT leaves them (offset at the end, nothing in them)
    old_batches = list(sbs.batches)
    if len(batches) < len(old_batches):
        for b in old_batches[len(batches):]:
            b.offset = len(skin_out)
            b.vertex_count = 0
            b.first_vertex = 0
            batches.append(b)
    old_skin = len(sds._raw)
    if len(skin_out) < old_skin:
        skin_out += bytes(old_skin - len(skin_out))
    grow = grow or len(skin_out) > old_skin or len(batches) > len(old_batches)

    vsec.vertexes = verts
    isec.values = idx
    sbs.batches = batches
    sds._raw = bytearray(skin_out)

    # meshes: ours in slots 0..n-1 cloned from the body mesh (keeping each slot's own id), the rest cleared
    mats = d.get_section(T_MATERIALS)
    ref_unk, ref_unk2, ref_flags = refm.unknowns, refm.unknowns2, refm.flags & 0x111
    for gi, (vs, vc, ist, ic, fb, bc) in enumerate(updates):
        me = meshes[gi]
        me.unknowns = (ref_unk[0], me.unknowns[1]) + tuple(ref_unk[2:])
        me.unknowns2 = ref_unk2
        me.flags = ref_flags
        me.vertexStart, me.vertexCount, me.indexStart, me.indexCount = vs, vc, ist, ic
        me.first_skin_batch, me.skin_batches_count = fb, bc
        me.material_index = gi
    for me in meshes[len(updates):]:
        me.clear()

    # materials: slot gi -> our material path (name kept); more groups than slots -> append (CANDIDATE)
    for gi, path in enumerate(material_paths):
        aid = crc64.hash(path)
        po = d.add_string(path)
        if gi < len(mats.string_offsets):
            _, no = mats.string_offsets[gi]
            mats.string_offsets[gi] = (po, no)
            mats.triples[gi] = (aid, mats.triples[gi][1], mats.triples[gi][2])
        else:
            name = 'herovr_%d' % gi
            no = d.add_string(name)
            mats.string_offsets.append((po, no))
            mats.triples.append((aid, crc32.hash(name, normalize=False), mats.triples[0][2] if mats.triples else 0))
            log('material slot %d appended (CANDIDATE)' % gi)

    # looks: every look and LOD shows our meshes (ALERT sets looks 0 and 1; the rest would show nothing)
    for lk in d.get_section(T_LOOK).looks:
        for lod in lk.lods:
            lod.start, lod.count = 0, len(updates)

    # muscle deformation off and LOD distances far, as ALERT (and daemon's mi.exe) do
    mus = d.get_section(T_MUSCLE)
    if mus is not None and len(mus._raw) >= 0x50:
        mus._raw = bytearray(mus._raw)
        mus._raw[:24] = struct.pack('<6i', 0, 0, 0, 0x40, 0x48, 0)
        mus._raw[0x40:0x50] = struct.pack('<2q', -1, -1)
        d._sections_data[d._sections_map[T_MUSCLE]] = bytearray(mus._raw)
    if built_raw is not None and len(built_raw) >= 0x48:
        for i in range(5):
            built_raw[0x34 + 4 * i:0x38 + 4 * i] = struct.pack('<f', 4096.0)
        if grow and len(built_raw) >= 0x6C:
            built_raw[0x64:0x68] = struct.pack('<I', len(idx))
            built_raw[0x68:0x6C] = struct.pack('<I', len(verts))
            log('buffers grew: Built counts set to %d indexes / %d vertexes (CANDIDATE)' % (len(idx), len(verts)))
        d._sections_data[built_ndx] = built_raw
        bs = d.get_section(T_BUILT)
        bs.values = [u2f(struct.unpack('<I', built_raw[i:i + 4])[0]) for i in range(0, len(built_raw) - 3, 4)]

    for tag in (T_VERTEXES, T_INDEXES, T_SKIN_BATCH, T_SKIN_DATA, T_MESHES, T_MATERIALS, T_LOOK):
        sec = d.get_section(tag)
        if tag == T_SKIN_BATCH:
            d._sections_data[d._sections_map[tag]] = bytearray(b''.join(b.save() for b in sec.batches))
        else:
            d._sections_data[d._sections_map[tag]] = bytearray(sec.save())
    if built_raw is not None:
        d._sections_data[built_ndx] = built_raw  # Vertexes.save read the uv scale from bs.values above
    out = io.BytesIO()
    m.save(out)
    log('model: %d meshes, %d/%d vertexes, %d/%d indexes, %d skin bytes, %d batches%s' % (
        len(updates), total_v, len(verts), total_i, len(idx), len(skin_out), len(batches), ' (grown)' if grow else ' (in place)'))
    return out.getvalue()
