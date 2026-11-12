# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 REMOTE SUIT SCAN: a read-only look at the user's own Marvel's Spider-Man 2 install.

PLAYER 2 should be the game's own Spider-Man in the suit his friend has on, read from each player's own game files on
his own PC (nothing is copied, sent or shipped). This tool only READS: it opens the game's 'toc' and archives, follows
every suit from the suits menu list to the .model files it wears, and writes a JSON report saying what is there and
what our port would still need to turn those files into a drawable body (archive compression kinds, model sections).

The formats are the ones Overstrike's working MSM2 suit installer reads (GPLv3, credited in THIRD_PARTY_NOTICES.md):
  toc        magic 0x34E89035, u32 size, then a DAT1 (AssetIds 0x506D7B8A, Spans 0xEDE8ADA9, Sizes 0x65BCF461
             16 bytes each: size, archive, offset, header offset; Archives 0x398ABFF0 66 bytes each, name in 40)
  archive    raw, or DSAR (0x52415344): 32-byte block headers from 32 to the u32 at 12; type 2 GDeflate, 3 LZ4
  config     DAT1 with type 0x35F7AFA5; 'Config Built' 0xE501186F is the serialized tree (SerializedSection_I30)
  suit chain configs/system/system_progression.config (0x9C9C72A303FCFA30) SuitList.Suits[].Item (reward)
             -> DefaultLoadoutConfig.AssetPath -> Loadout.ItemLoadoutLists[].Items[].Item -> ModelList[].Model
             .AssetPath (*.model); DefaultMaskModel.AssetPath is the mask.
Usage:  python sm2scan.py "C:\\...\\Marvel's Spider-Man 2" [report.json]
"""
import json
import os
import struct
import sys

TOC_MAGIC = 0x34E89035
DAT1_MAGIC = 0x44415431
DSAR_MAGIC = 0x52415344
CONFIG_I30 = 0x35F7AFA5
TAG_ASSET_IDS = 0x506D7B8A
TAG_SPANS = 0xEDE8ADA9
TAG_SIZES = 0x65BCF461
TAG_ARCHIVES = 0x398ABFF0
TAG_CONFIG_BUILT = 0xE501186F
SYSTEM_PROGRESSION = 0x9C9C72A303FCFA30  # configs/system/system_progression.config
OBJECT_MARKER = 0x03150044
COMPRESSION = {2: 'gdeflate', 3: 'lz4'}

# CRC-64 of the engine's asset paths (Overstrike DAT1/CRC64.cs; the same table as ALERT's dat1lib crc64.py).
_POLY = 0xC96C5795D7870F42


def _table():
    t = []
    for i in range(256):
        c = i
        for _ in range(8):
            c = (c >> 1) ^ _POLY if c & 1 else c >> 1
        t.append(c)
    return t


_T64 = _table()


def normalize(path):
    out, slash = [], False
    for ch in path.lower().replace('\\', '/'):
        if ch == '/':
            if slash:
                continue
            slash = True
        else:
            slash = False
        out.append(ch)
    return ''.join(out)


def asset_id(path):
    """The game's asset id of a path: CRC-64 (reflected, init 0xC96C5795D7870F42), >> 2, top bit set."""
    crc = 0xC96C5795D7870F42
    for b in normalize(path).encode('utf-8'):
        crc = (crc >> 8) ^ _T64[(crc ^ b) & 0xFF]
    return (crc >> 2) | 0x8000000000000000


def _crc32_table():
    t = []
    for i in range(256):
        c = i
        for _ in range(8):
            c = (c >> 1) ^ 0xEDB88320 if c & 1 else c >> 1
        t.append(c)
    return t


_T32 = _crc32_table()


def name_hash(name):
    """Overstrike CRC32.Hash(name, false): table CRC-32 seeded 0xEDB88320, no final xor (a key's hash in a tree)."""
    crc = 0xEDB88320
    for b in name.encode('utf-8'):
        crc = (crc >> 8) ^ _T32[(crc ^ b) & 0xFF]
    return crc


def lz4_block(src, size):
    """A standard LZ4 block (what K4os LZ4Codec.Decode reads for a DSAR type 3 block)."""
    out = bytearray()
    i, n = 0, len(src)
    while i < n:
        token = src[i]
        i += 1
        lit = token >> 4
        if lit == 15:
            while True:
                b = src[i]
                i += 1
                lit += b
                if b != 255:
                    break
        out += src[i:i + lit]
        i += lit
        if i >= n or len(out) >= size:
            break
        off = src[i] | (src[i + 1] << 8)
        i += 2
        if off == 0 or off > len(out):
            raise ValueError('bad lz4 offset')
        ml = (token & 15) + 4
        if (token & 15) == 15:
            while True:
                b = src[i]
                i += 1
                ml += b
                if b != 255:
                    break
        start = len(out) - off
        for k in range(ml):
            out.append(out[start + k])
    if len(out) != size:
        raise ValueError('lz4 size %d != %d' % (len(out), size))
    return bytes(out)


class NeedsDecoder(Exception):
    """A block in a compression our port cannot read yet (GDeflate)."""


class Dat1(object):
    def __init__(self, data):
        magic, self.type_magic, self.size, count, unknowns = struct.unpack_from('<IIIHH', data, 0)
        if magic != DAT1_MAGIC:
            raise ValueError('not DAT1')
        self.data = data
        self.sections = {}
        first = len(data)
        for k in range(count):
            tag, off, sz = struct.unpack_from('<III', data, 16 + 12 * k)
            self.sections[tag] = (off, sz)
            first = min(first, off)
        self.strings_start = 16 + 12 * count + 8 * unknowns
        self.strings_end = first

    def section(self, tag):
        if tag not in self.sections:
            return None
        off, sz = self.sections[tag]
        return self.data[off:off + sz]

    def string(self, offset):
        if offset < self.strings_start or offset >= self.strings_end:
            return None
        end = self.data.index(b'\0', offset)
        return self.data[offset:end].decode('utf-8', 'replace')


class Tree(object):
    """Overstrike SerializedSection_I30's reader: an object is (0, 0x03150044, children, data length), then
    child headers (hash u32, flags u16, unk u8, type u8), child name offsets (u32 into the DAT1 strings), align 4,
    then the values; flags bit 0 is an array of flags >> 4 items."""

    def __init__(self, dat1, data):
        self.dat1, self.data, self.pos = dat1, data, 0

    def u(self, fmt):
        v = struct.unpack_from('<' + fmt, self.data, self.pos)
        self.pos += struct.calcsize('<' + fmt)
        return v[0]

    def read_object(self):
        zero, marker, count, length = struct.unpack_from('<IIII', self.data, self.pos)
        self.pos += 16
        if zero != 0 or marker != OBJECT_MARKER:
            raise ValueError('bad tree object')
        start = self.pos
        result = {}
        if count:
            heads = [struct.unpack_from('<IHBB', self.data, self.pos + 8 * k) for k in range(count)]
            self.pos += 8 * count
            names = [self.u('I') for _ in range(count)]
            self.pos += (-self.pos) % 4
            for (h, flags, _, kind), noff in zip(heads, names):
                name = self.dat1.string(noff) or ('#%08x' % h)
                if flags & 1:
                    items = flags >> 4
                    if items == 0:
                        self.pos += 1
                        result[name] = []
                    else:
                        result[name] = [self.read_node(kind) for _ in range(items)]
                else:
                    result[name] = self.read_node(kind)
        left = length - (self.pos - start)
        if left < 0:
            raise ValueError('tree overrun')
        self.pos += left
        return result

    def read_node(self, kind):
        simple = {0: 'B', 1: 'H', 2: 'I', 4: 'b', 5: 'h', 6: 'i', 8: 'f', 0x0F: '?', 0x11: 'Q'}
        if kind in simple:
            return self.u(simple[kind])
        if kind == 0x0A:
            n = self.u('I')
            self.pos += 12
            extra = 1 + ((-(n + 1)) % 4)
            s = self.data[self.pos:self.pos + n].decode('utf-8', 'replace')
            self.pos += n + extra
            return s
        if kind == 0x0D:
            return self.read_object()
        self.pos += 1  # NULL and unknown kinds: Overstrike reads nothing and returns null; its writer puts one byte
        return None


def read_config(data):
    d = Dat1(data)
    if d.type_magic != CONFIG_I30:
        raise ValueError('not an MSM2 config (type 0x%08X)' % d.type_magic)
    built = d.section(TAG_CONFIG_BUILT)
    if built is None:
        raise ValueError('config has no built section')
    return Tree(d, built).read_object()


class Toc(object):
    def __init__(self, game_dir):
        self.dir = game_dir
        with open(os.path.join(game_dir, 'toc'), 'rb') as f:
            raw = f.read()
        magic, _ = struct.unpack_from('<II', raw, 0)
        if magic != TOC_MAGIC:
            raise ValueError('toc magic 0x%08X (not MSM2 / RCRA)' % magic)
        self.dat1 = Dat1(raw[8:])
        ids = self.dat1.section(TAG_ASSET_IDS)
        self.ids = struct.unpack('<%dQ' % (len(ids) // 8), ids)
        sizes = self.dat1.section(TAG_SIZES)
        self.sizes = [struct.unpack_from('<IIIi', sizes, 16 * k) for k in range(len(sizes) // 16)]
        arch = self.dat1.section(TAG_ARCHIVES)
        self.archives = []
        for k in range(len(arch) // 66):
            name = arch[66 * k:66 * k + 40].split(b'\0', 1)[0].decode('ascii', 'replace').replace('\\', '/')
            self.archives.append(name)
        spans = self.dat1.section(TAG_SPANS) or b''
        self.spans = [struct.unpack_from('<II', spans, 8 * k) for k in range(len(spans) // 8)]
        self.first = {}
        for i, aid in enumerate(self.ids):
            self.first.setdefault(aid, i)
        self._blocks = {}

    def blocks(self, archive):
        if archive not in self._blocks:
            path = os.path.join(self.dir, self.archives[archive])
            with open(path, 'rb') as f:
                head = f.read(32)
                if len(head) < 16 or struct.unpack_from('<I', head, 0)[0] != DSAR_MAGIC:
                    self._blocks[archive] = None
                else:
                    end = struct.unpack_from('<I', head, 12)[0]
                    table = f.read(max(0, end - 32))
                    self._blocks[archive] = [struct.unpack_from('<IIIIIIB', table, 32 * k)
                                             for k in range(len(table) // 32)]
        return self._blocks[archive]

    def locate(self, aid):
        i = self.first.get(aid)
        if i is None:
            return None
        size, archive, offset, header = self.sizes[i]
        return {'index': i, 'archive': archive, 'offset': offset, 'size': size, 'header': header}

    def kinds(self, loc):
        """The compression kinds of the archive blocks that hold an asset ('raw' for an uncompressed archive)."""
        blocks = self.blocks(loc['archive'])
        if blocks is None:
            return ['raw']
        a, b = loc['offset'], loc['offset'] + loc['size']
        return sorted({COMPRESSION.get(t, 'type%d' % t) for (ro, _, co, _, rs, cs, t) in blocks if ro < b and a < ro + rs})

    def read(self, aid):
        loc = self.locate(aid)
        if loc is None:
            raise KeyError('asset %016X not in the toc' % aid)
        path = os.path.join(self.dir, self.archives[loc['archive']])
        blocks = self.blocks(loc['archive'])
        a, b = loc['offset'], loc['offset'] + loc['size']
        with open(path, 'rb') as f:
            if blocks is None:
                f.seek(a)
                return f.read(loc['size'])
            out = bytearray()
            for (ro, _, co, _, rs, cs, t) in blocks:
                if ro + rs <= a or ro >= b:
                    continue
                f.seek(co)
                comp = f.read(cs)
                if t == 3:
                    plain = lz4_block(comp, rs)
                elif t == 2:
                    raise NeedsDecoder('GDeflate block in %s' % self.archives[loc['archive']])
                else:
                    raise NeedsDecoder('compression type %d' % t)
                out += plain[max(ro, a) - ro:min(b, ro + rs) - ro]
            return bytes(out)


def _path(node):
    if isinstance(node, dict):
        node = node.get('AssetPath')
    return normalize(node) if isinstance(node, str) and node else None


def model_info(toc, path):
    info = {'path': path, 'id': '%016X' % asset_id(path)}
    loc = toc.locate(asset_id(path))
    if loc is None:
        info['present'] = False
        return info
    info.update(present=True, archive=toc.archives[loc['archive']], size=loc['size'], compression=toc.kinds(loc))
    try:
        data = toc.read(asset_id(path))
        d = Dat1(data)
        info['type'] = '%08X' % d.type_magic
        info['sections'] = ['%08X' % t for t in sorted(d.sections)]
    except NeedsDecoder as e:
        info['needs'] = str(e)
    except (ValueError, struct.error) as e:
        info['error'] = str(e)
    return info


LOOK_EXTS = ('.material', '.texture')


def _asset_paths(node, exts, out, depth=0):
    """Every asset path string (or {'AssetPath': ...}) under a config tree ending in one of exts, in order, once."""
    if depth > 24:
        return out
    if isinstance(node, str):
        p = normalize(node) if node else None
        if p and p.endswith(exts) and p not in out:
            out.append(p)
    elif isinstance(node, dict):
        for v in node.values():
            _asset_paths(v, exts, out, depth + 1)
    elif isinstance(node, (list, tuple)):
        for v in node:
            _asset_paths(v, exts, out, depth + 1)
    return out


def _ref(path):
    return {'path': path, 'id': '%016X' % asset_id(path)}


def suit_look(toc, reward_path, notes):
    """0.0.76 SUIT PROBE 5: what tells two suits on the same model apart (the Hybrid case): the equipped loadout
    .config (DefaultLoadoutConfig), its vanity item .configs, and every .material / .texture path named in the
    reward, the loadout and those items. Each as {'path', 'id'}; a part that cannot be read is a note."""
    look = {'loadout': None, 'vanity': [], 'materials': []}
    reward = read_config(toc.read(asset_id(reward_path)))
    mats = _asset_paths(reward, LOOK_EXTS, [])
    loadout = _path(reward.get('DefaultLoadoutConfig'))
    if loadout:
        look['loadout'] = _ref(loadout)
        try:
            tree = read_config(toc.read(asset_id(loadout)))
        except (KeyError, ValueError, NeedsDecoder, struct.error) as e:
            notes.append('%s: %s' % (loadout, e))
            tree = {}
        _asset_paths(tree, LOOK_EXTS, mats)
        for lst in (tree.get('Loadout') or {}).get('ItemLoadoutLists') or []:
            for entry in (lst or {}).get('Items') or []:
                item = (entry or {}).get('Item')
                if not isinstance(item, str) or not item:
                    continue
                look['vanity'].append(_ref(normalize(item)))
                try:
                    _asset_paths(read_config(toc.read(asset_id(item))), LOOK_EXTS, mats)
                except (KeyError, ValueError, NeedsDecoder, struct.error):
                    pass  # suit_models already noted it
    look['materials'] = [_ref(m) for m in mats]
    return look


def suit_models(toc, reward_path, notes):
    """Overstrike ResolveSuitModelPaths: the body .model files and the mask of one suit's reward loadout."""
    reward = read_config(toc.read(asset_id(reward_path)))
    mask = _path(reward.get('DefaultMaskModel'))
    body = []
    loadout = _path(reward.get('DefaultLoadoutConfig'))
    if loadout:
        tree = read_config(toc.read(asset_id(loadout)))
        for lst in (tree.get('Loadout') or {}).get('ItemLoadoutLists') or []:
            for entry in (lst or {}).get('Items') or []:
                item = (entry or {}).get('Item')
                if not isinstance(item, str) or not item:
                    continue
                try:
                    vanity = read_config(toc.read(asset_id(item)))
                except (KeyError, ValueError, NeedsDecoder, struct.error) as e:
                    notes.append('%s: %s' % (item, e))
                    continue
                for m in vanity.get('ModelList') or []:
                    p = _path((m or {}).get('Model'))
                    if p and p.endswith('.model') and p not in body:
                        body.append(p)
    chars = reward.get('ValidCharacters') or []
    return body, (mask if mask and mask.endswith('.model') else None), chars


def scan(game_dir):
    toc = Toc(game_dir)
    report = {'tool': 'sm2scan 2', 'assets': len(toc.ids), 'archives': len(toc.archives), 'suits': [], 'notes': []}
    kinds = {}
    for k in range(len(toc.archives)):
        try:
            blocks = toc.blocks(k)
        except OSError:
            kinds['missing'] = kinds.get('missing', 0) + 1
            continue
        if blocks is None:
            kinds['raw'] = kinds.get('raw', 0) + 1
        for b in blocks or []:
            name = COMPRESSION.get(b[6], 'type%d' % b[6])
            kinds[name] = kinds.get(name, 0) + 1
    report['blocks'] = kinds
    try:
        progression = read_config(toc.read(SYSTEM_PROGRESSION))
    except (KeyError, ValueError, NeedsDecoder, struct.error) as e:
        report['notes'].append('system_progression.config: %s' % e)
        return report
    for suit in (progression.get('SuitList') or {}).get('Suits') or []:
        entry = {'name': suit.get('Name'), 'display': suit.get('DisplayName'), 'item': suit.get('Item')}
        item = suit.get('Item')
        if isinstance(item, str) and item:
            entry['id'] = '%016X' % asset_id(item)
            notes = []
            try:
                body, mask, chars = suit_models(toc, item, notes)
                entry['characters'] = chars
                entry['models'] = [model_info(toc, p) for p in body]
                entry['mask'] = model_info(toc, mask) if mask else None
                entry.update(suit_look(toc, item, notes))
            except (KeyError, ValueError, NeedsDecoder, struct.error) as e:
                notes.append(str(e))
            if notes:
                entry['notes'] = notes
        report['suits'].append(entry)
    return report


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    report = scan(argv[1])
    text = json.dumps(report, indent=1, sort_keys=True)
    if len(argv) > 2:
        with open(argv[2], 'w', encoding='utf-8') as f:
            f.write(text)
        print('sm2scan: %d suits, blocks %s -> %s' % (len(report['suits']), report['blocks'], argv[2]))
    else:
        print(text)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
