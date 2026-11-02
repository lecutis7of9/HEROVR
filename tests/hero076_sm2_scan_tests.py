"""HERO VR 0.0.76 REMOTE SUIT SCAN (tools/sm2_suits/sm2scan.py): the read-only walk from the suits menu list to the
.model files a suit wears, over a synthetic MSM2 game folder (a toc, one raw and one DSAR / LZ4 archive, configs in
Overstrike's SerializedSection_I30 layout). No game data here."""
import json, os, struct, sys, tempfile, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'sm2_suits'))
import sm2scan as S  # noqa: E402


def put(path, data):
    with open(path, 'wb') as f:
        f.write(data)


def get(path, mode='rb'):
    with open(path, mode) as f:
        return f.read()


def dat1(type_magic, sections, strings):
    """A DAT1: header, section table (sorted tags), the strings block, sections at 16-byte steps."""
    head = 16 + 12 * len(sections)
    blob = bytearray(strings)
    offs, pos = {}, head + len(blob)
    body = bytearray()
    for tag in sorted(sections):
        pad = (-pos) % 16
        body += b'\0' * pad
        pos += pad
        offs[tag] = pos
        body += sections[tag]
        pos += len(sections[tag])
    out = bytearray(struct.pack('<IIIHH', S.DAT1_MAGIC, type_magic, pos, len(sections), 0))
    for tag in sorted(sections):
        out += struct.pack('<III', tag, offs[tag], len(sections[tag]))
    return bytes(out + blob + body)


class Strings(object):
    def __init__(self, base):
        self.base, self.blob, self.at = base, bytearray(), {}

    def add(self, s):
        if s not in self.at:
            self.at[s] = self.base + len(self.blob)
            self.blob += s.encode() + b'\0'
        return self.at[s]


def kind(v):
    if isinstance(v, bool):
        return 0x0F
    if isinstance(v, int):
        return 0x02
    if isinstance(v, float):
        return 0x08
    if isinstance(v, str):
        return 0x0A
    if isinstance(v, dict):
        return 0x0D
    return 0x13


def write_tree(obj, names, out):
    """Overstrike SerializedSection_I30.WriteObject, written straight into one buffer."""
    start = len(out)
    out += struct.pack('<IIII', 0, S.OBJECT_MARKER, len(obj), 0)
    body = len(out)
    for k, v in obj.items():
        is_arr = isinstance(v, list)
        t = kind(v[0]) if is_arr and v else (0x13 if is_arr else kind(v))
        flags = ((len(v) << 4) | 1) if is_arr else (1 << 4)
        out += struct.pack('<IHBB', S.name_hash(k), flags, 0, t)
    for k in obj:
        out += struct.pack('<I', names.add(k))
    out += b'\0' * ((-len(out)) % 4)
    for v in obj.values():
        for x in (v if isinstance(v, list) else [v]) or [None]:
            if isinstance(v, list) and not v:
                out += b'\0'
            elif isinstance(x, dict):
                write_tree(x, names, out)
            elif isinstance(x, str):
                b = x.encode()
                out += struct.pack('<IIQ', len(b), 0, 0) + b + b'\0' * (1 + (-(len(b) + 1)) % 4)
            elif isinstance(x, bool):
                out += struct.pack('<?', x)
            elif isinstance(x, int):
                out += struct.pack('<I', x)
    out += b'\0' * ((-len(out)) % 4)
    struct.pack_into('<I', out, start + 12, len(out) - body)


def config(tree):
    # strings sit right after a one-section header (16 + 12)
    names = Strings(28)
    names.add('Config Built File')
    built = bytearray()
    write_tree(tree, names, built)
    return dat1(S.CONFIG_I30, {S.TAG_CONFIG_BUILT: bytes(built)}, bytes(names.blob))


def lz4(data):
    """A greedy LZ4 block writer (small inputs only)."""
    out, i, lit = bytearray(), 0, 0

    def ext(n):
        r = bytearray()
        while n >= 255:
            r.append(255)
            n -= 255
        r.append(n)
        return r
    while i < len(data):
        best = (0, 0)
        if i + 4 <= len(data) - 5:
            for j in range(max(0, i - 65535), i):
                n = 0
                while i + n < len(data) - 5 and data[j + n] == data[i + n]:
                    n += 1
                if n > best[0]:
                    best = (n, i - j)
        if best[0] >= 4:
            ll, ml = i - lit, best[0] - 4
            out.append((min(ll, 15) << 4) | min(ml, 15))
            if ll >= 15:
                out += ext(ll - 15)
            out += data[lit:i] + struct.pack('<H', best[1])
            if ml >= 15:
                out += ext(ml - 15)
            i += best[0]
            lit = i
        else:
            i += 1
    ll = len(data) - lit
    out.append(min(ll, 15) << 4)
    if ll >= 15:
        out += ext(ll - 15)
    return bytes(out + data[lit:])


def make_game(root, assets, lz4_paths=()):
    """assets: {aid: bytes}. Those in lz4_paths' ids go into a DSAR archive of LZ4 blocks; the rest raw."""
    raw, packed = bytearray(b'RAW!'), bytearray()
    where = {}
    for aid in sorted(assets):
        if aid in lz4_paths:
            where[aid] = (1, len(packed))
            packed += assets[aid]
        else:
            where[aid] = (0, len(raw))
            raw += assets[aid]
    # DSAR: 2 blocks of the packed bytes
    half = len(packed) // 2
    parts = [(0, packed[:half]), (half, packed[half:])]
    comp = [lz4(bytes(p)) for _, p in parts]
    table_end = 32 + 32 * len(parts)
    dsar = bytearray(struct.pack('<IIII', S.DSAR_MAGIC, 0, 0, table_end) + b'\0' * 16)
    co = table_end
    for (ro, p), c in zip(parts, comp):
        dsar += struct.pack('<IIIIIIB7x', ro, 0, co, 0, len(p), len(c), 3)
        co += len(c)
    for c in comp:
        dsar += c
    os.makedirs(os.path.join(root, 'd'))
    put(os.path.join(root, 'd', 'raw0'), bytes(raw))
    put(os.path.join(root, 'd', 'packed1'), bytes(dsar))
    ids = sorted(assets)
    secs = {
        S.TAG_ASSET_IDS: struct.pack('<%dQ' % len(ids), *ids),
        S.TAG_SPANS: struct.pack('<II', 0, len(ids)),
        S.TAG_SIZES: b''.join(struct.pack('<IIIi', len(assets[a]), where[a][0], where[a][1], -1) for a in ids),
        S.TAG_ARCHIVES: b''.join(n.ljust(40, b'\0') + b'\0' * 26 for n in (b'd\\raw0', b'd\\packed1')),
    }
    body = dat1(0, secs, b'')
    put(os.path.join(root, 'toc'), struct.pack('<II', S.TOC_MAGIC, len(body)) + body)


REWARD = 'configs/rewards/reward_suit_advanced.config'
LOADOUT = 'configs/loadouts/suit_advanced.config'
VANITY = 'configs/vanitytorso/vanity_suit_advanced.config'
BODY = 'characters/hero/hero_spiderman_advanced/hero_spiderman_advanced.model'
MASK = 'characters/hero/hero_spiderman_advanced/hero_spiderman_advanced_mask.model'
MODEL_TYPE = 0x98906B9F


def game(root, model_lz4=True):
    A = S.asset_id
    assets = {
        S.SYSTEM_PROGRESSION: config({'SuitList': {'Suits': [
            {'Name': 'i30_Advanced_Suit', 'DisplayName': 'SUIT_ADVANCED_NAME', 'Item': REWARD},
            {'Name': 'Gone_Suit', 'DisplayName': 'X', 'Item': 'configs/rewards/missing.config'}]}}),
        A(REWARD): config({'DefaultLoadoutConfig': {'AssetPath': LOADOUT}, 'DefaultMaskModel': {'AssetPath': MASK},
                           'ValidCharacters': ['kSpiderManPeter'], 'Flag': True, 'Empty': []}),
        A(LOADOUT): config({'Loadout': {'ItemLoadoutLists': [{'Items': [{'Item': VANITY}, {'Item': 'configs/none.config'}]}]}}),
        A(VANITY): config({'ModelList': [{'Model': {'AssetPath': BODY}}, {'Model': {'AssetPath': BODY}},
                                         {'Model': {'AssetPath': 'textures/not_a_model.texture'}}]}),
        A(BODY): dat1(MODEL_TYPE, {0x11111111: b'\1' * 300, 0x22222222: b'ab' * 200}, b''),
        A(MASK): dat1(MODEL_TYPE, {0x33333333: b'm' * 40}, b''),
    }
    make_game(root, assets, {A(BODY)} if model_lz4 else set())


class Scan(unittest.TestCase):
    def test_hashes_match_the_game(self):
        self.assertEqual(S.asset_id('configs/system/system_progression.config'), S.SYSTEM_PROGRESSION)
        self.assertEqual(S.asset_id('CONFIGS\\System//system_progression.config'), S.SYSTEM_PROGRESSION)

    def test_lz4_round_trip(self):
        for data in (b'', b'x', b'abcd' * 50 + b'tail!', bytes(range(256)) * 3):
            self.assertEqual(S.lz4_block(lz4(data), len(data)), data)
        with self.assertRaises(ValueError):
            S.lz4_block(b'\x10a\x05\x00', 10)  # an offset past the start

    def test_walks_suit_to_models(self):
        with tempfile.TemporaryDirectory() as d:
            game(d)
            r = S.scan(d)
            self.assertEqual(r['archives'], 2)
            self.assertEqual(r['blocks'], {'raw': 1, 'lz4': 2})
            adv, gone = r['suits']
            self.assertEqual(adv['name'], 'i30_Advanced_Suit')
            self.assertEqual(adv['characters'], ['kSpiderManPeter'])
            self.assertEqual([m['path'] for m in adv['models']], [BODY])  # deduplicated; the texture skipped
            m = adv['models'][0]
            self.assertEqual((m['present'], m['compression'], m['type']), (True, ['lz4'], '98906B9F'))
            self.assertEqual(m['sections'], ['11111111', '22222222'])
            self.assertEqual(adv['mask']['compression'], ['raw'])
            self.assertTrue(any('configs/none.config' in n for n in adv['notes']))
            self.assertIn('notes', gone)  # a reward not in the toc is a note, never a crash
            json.dumps(r)

    def test_look_ids_for_probe5(self):
        # 0.0.76 SUIT PROBE 5: the equipped loadout, its vanity items and every .material/.texture they name
        with tempfile.TemporaryDirectory() as d:
            game(d)
            adv = S.scan(d)['suits'][0]
            self.assertEqual(adv['loadout'], {'path': LOADOUT, 'id': '%016X' % S.asset_id(LOADOUT)})
            self.assertEqual([v['path'] for v in adv['vanity']], [VANITY, 'configs/none.config'])
            self.assertEqual([m['path'] for m in adv['materials']], ['textures/not_a_model.texture'])
            self.assertEqual(adv['materials'][0]['id'], '%016X' % S.asset_id('textures/not_a_model.texture'))

    def test_gdeflate_is_reported_not_guessed(self):
        with tempfile.TemporaryDirectory() as d:
            game(d)
            p = os.path.join(d, 'd', 'packed1')
            b = bytearray(get(p))
            b[32 + 24] = 2  # first block now says GDeflate
            put(p, bytes(b))
            m = S.scan(d)['suits'][0]['models'][0]
            self.assertIn('gdeflate', m['compression'])
            self.assertIn('GDeflate', m['needs'])

    def test_main_writes_the_report_and_never_writes_the_game(self):
        with tempfile.TemporaryDirectory() as d:
            game(d, model_lz4=False)
            before = {f: os.path.getmtime(os.path.join(dp, f)) for dp, _, fs in os.walk(d) for f in fs}
            out = os.path.join(d, 'report.json')
            self.assertEqual(S.main(['sm2scan', d, out]), 0)
            self.assertEqual(json.loads(get(out, 'r'))['suits'][0]['models'][0]['compression'], ['raw'])
            after = {f: os.path.getmtime(os.path.join(dp, f)) for dp, _, fs in os.walk(d) for f in fs if f != 'report.json'}
            self.assertEqual(before, after)

    def test_bad_toc(self):
        with tempfile.TemporaryDirectory() as d:
            put(os.path.join(d, 'toc'), b'\0' * 32)
            with self.assertRaises(ValueError):
                S.scan(d)

    def test_no_forbidden_names(self):
        text = get(os.path.join(ROOT, 'tools', 'sm2_suits', 'sm2scan.py')).decode('utf-8').lower()
        self.assertNotIn('miles', text)


if __name__ == '__main__':
    unittest.main()
