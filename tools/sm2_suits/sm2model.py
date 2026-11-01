# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 REMOTE SUIT MODEL CHECK: a read-only look inside the suit .model files of the user's own install.

The next step of the remote suit (PLAYER 2 drawn in his friend's suit from this PC's own game files) needs the
.model format: skeleton, meshes, vertexes and indexes. The container is known (sm2scan.py read it from this very
game); the layouts INSIDE a model come from ALERT's dat1lib for Ratchet & Clank: Rift Apart (same engine, GPL-3,
credited in THIRD_PARTY_NOTICES.md) and are CANDIDATE until this report from a real Spider-Man 2 install shows them
holding. It follows each chosen suit (default: Advanced Suit 2.0, Classic, Hybrid) to its .model files, reads each
one (LZ4 through sm2scan's reader) and writes, per model: its sections and sizes, the joints (count, names, whether
the names hash and the parents come first), the 64-byte mesh entries, the 120-byte built block (scale, counts) and
whether 2C2CA205 / F4EA9E18 have the sizes of 16-byte vertexes / 16-bit indexes for those counts. The same checks
as include/spidy/sm2_model.hpp, so the C++ reader is right when this report says "ok".

Nothing is written but the report; nothing of the game's is copied (names and sizes only, no geometry).
Usage:  python sm2model.py "C:\\...\\Marvel's Spider-Man 2" [sm2model.json] [--all | --suits NAME,NAME]
"""
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sm2scan as S  # noqa: E402

MODEL_TYPE = 0x1B069A8A
TAG_JOINTS = 0x15DF9D3B
TAG_JOINT_TRANSFORMS = 0xDCC88A19
TAG_BUILT = 0x283D0383
TAG_MESHES = 0x78D9CBDE
TAG_VERTEXES = 0x2C2CA205  # CANDIDATE guess
TAG_INDEXES = 0xF4EA9E18   # CANDIDATE guess
WATCHED = ('i30_Advanced_Suit', 'SUIT_CLASSIC', 'SUIT_NWH_HYBRID')


def stride_ok(nbytes, count, stride):
    return count > 0 and count * stride <= nbytes < count * stride + 64


def check_model(data):
    """The checks of include/spidy/sm2_model.hpp Model::read on one decompressed .model: a dict for the report."""
    out = {'bytes': len(data), 'notes': []}

    def note(ok, text):
        out['notes'].append(('ok: ' if ok else 'no: ') + text)

    try:
        d = S.Dat1(data)
    except (ValueError, struct.error):
        note(False, 'not a DAT1')
        return out
    out['type'] = '%08X' % d.type_magic
    out['dat1_size'] = d.size
    out['sections'] = {'%08X' % t: sz for t, (off, sz) in sorted(d.sections.items())}
    if d.type_magic != MODEL_TYPE:
        note(False, 'DAT1 type is not 1B069A8A')
        return out
    out['notes'].append('info: DAT1 size %d, asset bytes %d' % (d.size, len(data)))
    # joints: 16 bytes <hHHHII parent, index, ?, ?, name hash, name offset
    j = d.section(TAG_JOINTS) or b''
    if j and len(j) % 16 == 0:
        joints = [struct.unpack_from('<hHHHII', j, 16 * i) for i in range(len(j) // 16)]
        names = [d.string(o) for (_, _, _, _, _, o) in joints]
        parents = sum(1 for i, (p, ix, _, _, _, _) in enumerate(joints) if (p == -1 or 0 <= p < i) and ix == i)
        hashes = sum(1 for (_, _, _, _, h, _), n in zip(joints, names) if n and S.name_hash(n) == h)
        out['joints'] = {'count': len(joints), 'names': [n for n in names[:12]], 'last': names[-1] if names else None}
        note(parents == len(joints), '%d of %d joints have index = place and a parent before them' % (parents, len(joints)))
        note(hashes == len(joints), '%d of %d joint names hash to their CRC-32' % (hashes, len(joints)))
        t = d.section(TAG_JOINT_TRANSFORMS) or b''
        note(len(t) // (48 + 64) == len(joints),
             'joint transforms for %d joints (%d bytes)' % (len(t) // (48 + 64), len(t)))
    else:
        note(False, 'joints section missing or not 16-byte entries (%d bytes)' % len(j))
    # meshes: 64 bytes, <IQHHHH then <IIII vertexStart, indexStart, indexCount, vertexCount, then <HHHH flags...
    m = d.section(TAG_MESHES) or b''
    meshes = []
    if m and len(m) % 64 == 0:
        for i in range(len(m) // 64):
            vs, is_, ic, vc = struct.unpack_from('<IIII', m, 64 * i + 20)
            flags, mat, fsb, sbc = struct.unpack_from('<HHHH', m, 64 * i + 36)
            meshes.append((vs, is_, ic, vc, flags, mat))
        out['meshes'] = {'count': len(meshes), 'first': [list(x) for x in meshes[:6]],
                         'flags': sorted({'%04X' % x[4] for x in meshes})}
    else:
        note(False, 'meshes section missing or not 64-byte entries (%d bytes)' % len(m))
    # built: 120 bytes; offset 0x1C..0x24, scale 0x2C, uv shift 0x30, index count 0x64, vertex count 0x68
    b = d.section(TAG_BUILT) or b''
    if len(b) >= 120:
        off = struct.unpack_from('<3f', b, 0x1C)
        scale = struct.unpack_from('<f', b, 0x2C)[0]
        uv = struct.unpack_from('<I', b, 0x30)[0]
        ic, vc = struct.unpack_from('<II', b, 0x64)
        out['built'] = {'offset': list(off), 'scale': scale, 'uv_shift': uv, 'indexes': ic, 'vertexes': vc,
                        'words': ['%08X' % w for w in struct.unpack_from('<30I', b, 0)]}
        note(0 < scale < 1 and vc > 0 and ic > 0 and ic % 3 == 0,
             'built: scale %f, %d vertexes, %d indexes' % (scale, vc, ic))
        ie = max([x[1] + x[2] for x in meshes] or [0])
        ve = max([x[0] + x[3] for x in meshes] or [0])
        note(bool(meshes) and ie <= ic and ve <= vc,
             '%d meshes inside the built counts (indexes to %d, vertexes to %d)' % (len(meshes), ie, ve))
        vb = d.sections.get(TAG_VERTEXES, (0, 0))[1]
        ib = d.sections.get(TAG_INDEXES, (0, 0))[1]
        note(stride_ok(vb, vc, 16), '2C2CA205 as 16-byte vertexes (%d bytes for %d)' % (vb, vc))
        note(stride_ok(ib, ic, 2), 'F4EA9E18 as 16-bit indexes (%d bytes for %d)' % (ib, ic))
        # every section whose size is a whole multiple of a count: what else could hold the geometry
        multiples = {}
        for t, (o, sz) in sorted(d.sections.items()):
            hit = {k: sz // c for k, c in (('vertexes', vc), ('indexes', ic)) if c and sz % c == 0 and sz // c <= 64}
            if hit:
                multiples['%08X' % t] = hit
        out['multiples'] = multiples  # every section a whole number of bytes per vertex or index: geometry suspects
        if stride_ok(ib, ic, 2):
            ix = struct.unpack_from('<%dH' % ic, d.section(TAG_INDEXES), 0)
            inside = total = 0
            for (vs, is_, icn, vcn, flags, _) in meshes:
                for k in range(icn):
                    total += 1
                    if is_ + k < len(ix):
                        v = ix[is_ + k] + (vs if flags & 0x10 else 0)
                        inside += 1 if v < vc else 0
            note(total > 0 and inside == total, '%d of %d mesh indexes name a vertex that exists' % (inside, total))
    else:
        note(False, 'built section missing or short (%d bytes)' % len(b))
    return out


def report(game_dir, which=WATCHED):
    toc = S.Toc(game_dir)
    rep = {'tool': 'sm2model 1', 'format': 'CANDIDATE (ALERT dat1lib Rift Apart layouts)', 'models': [], 'notes': []}
    progression = S.read_config(toc.read(S.SYSTEM_PROGRESSION))
    seen = set()
    for suit in (progression.get('SuitList') or {}).get('Suits') or []:
        name, item = suit.get('Name'), suit.get('Item')
        if which and name not in which or not isinstance(item, str) or not item:
            continue
        notes = []
        try:
            body, mask, _ = S.suit_models(toc, item, notes)
        except (KeyError, ValueError, S.NeedsDecoder, struct.error) as e:
            rep['notes'].append('%s: %s' % (name, e))
            continue
        for path, kind in [(p, 'model') for p in body] + ([(mask, 'mask')] if mask else []):
            aid = S.asset_id(path)
            entry = {'suit': name, 'kind': kind, 'path': path, 'id': '%016X' % aid}
            if aid in seen:
                entry['same_as_above'] = True
                rep['models'].append(entry)
                continue
            seen.add(aid)
            loc = toc.locate(aid)
            if loc is None:
                entry['present'] = False
            else:
                entry['compression'] = toc.kinds(loc)
                try:
                    entry.update(check_model(toc.read(aid)))
                except S.NeedsDecoder as e:
                    entry['needs'] = str(e)
                except (ValueError, struct.error) as e:
                    entry['error'] = str(e)
            rep['models'].append(entry)
    oks = [n for m in rep['models'] for n in m.get('notes', [])]
    rep['summary'] = {'checks': len(oks), 'ok': sum(1 for n in oks if n.startswith('ok: '))}
    return rep


def main(argv):
    args = [a for a in argv[1:]]
    which = WATCHED
    if '--all' in args:
        args.remove('--all')
        which = None
    if '--suits' in args:
        i = args.index('--suits')
        which = tuple(x for x in args[i + 1].split(',') if x) if i + 1 < len(args) else WATCHED
        del args[i:i + 2]
    if not args:
        print(__doc__)
        return 2
    rep = report(args[0], which)
    text = json.dumps(rep, indent=1, sort_keys=True)
    if len(args) > 1:
        with open(args[1], 'w', encoding='utf-8') as f:
            f.write(text)
        print('sm2model: %d models, %d of %d checks ok -> %s' % (len(rep['models']), rep['summary']['ok'],
                                                                rep['summary']['checks'], args[1]))
    else:
        print(text)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
