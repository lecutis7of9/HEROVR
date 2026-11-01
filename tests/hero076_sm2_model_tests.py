"""HERO VR 0.0.76 REMOTE SUIT MODEL CHECK (tools/sm2_suits/sm2model.py and include/spidy/sm2_model.hpp): the
CANDIDATE .model layouts (ALERT dat1lib, Rift Apart) checked on a synthetic model built here, end to end through a
synthetic game folder (LZ4 through sm2scan's reader). tests/data/sm2model/ holds the same synthetic model (and its
LZ4 block) for the C++ reader's test (tests/sm2_model_tests.cpp), so both read the same bytes. No game data."""
import json, os, struct, sys, tempfile, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'sm2_suits'))
sys.path.insert(0, os.path.join(ROOT, 'tests'))
import sm2scan as S  # noqa: E402
import sm2model as M  # noqa: E402
import hero076_sm2_scan_tests as T  # noqa: E402

DATA = os.path.join(ROOT, 'tests', 'data', 'sm2model')


def synth():
    """3 joints (root, spine, head), 2 meshes of one triangle each (relative indexes), 6 vertexes, 6 indexes."""
    tags = [M.TAG_JOINTS, M.TAG_JOINT_TRANSFORMS, M.TAG_BUILT, M.TAG_MESHES, M.TAG_VERTEXES, M.TAG_INDEXES]
    names = T.Strings(16 + 12 * len(tags))
    joints = b''
    for i, (parent, n) in enumerate([(-1, 'root'), (0, 'spine'), (1, 'head')]):
        joints += struct.pack('<hHHHII', parent, i, 0, 0, S.name_hash(n), names.add(n))
    mats = b''.join(struct.pack('<12f', 1, 0, 0, i, 0, 1, 0, 0, 0, 0, 1, 0) for i in range(3))
    mats += b'\0' * ((-len(mats)) % 64)
    mats += b''.join(struct.pack('<16f', *([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1])) for _ in range(3))
    built = bytearray(120)
    struct.pack_into('<3f', built, 0x1C, 0.0, 1.0, 0.0)
    struct.pack_into('<f', built, 0x2C, 1.0 / 4096)
    struct.pack_into('<I', built, 0x30, 4)
    struct.pack_into('<II', built, 0x64, 6, 6)
    meshes = b''
    for k in range(2):
        e = bytearray(64)
        struct.pack_into('<IQ', e, 0, 0, 0x1234000000000000 + k)
        struct.pack_into('<IIII', e, 20, 3 * k, 3 * k, 3, 3)
        struct.pack_into('<HHHH', e, 36, 0x11, k, 0, 0)
        meshes += bytes(e)
    verts = b''.join(struct.pack('<4hI2h', 4096 * i, -4096, 2048, 0, 0, 1024 * i, 512) for i in range(6))
    idx = struct.pack('<6H', 0, 1, 2, 0, 1, 2)
    secs = dict(zip(tags, [joints, mats, bytes(built), meshes, verts, idx]))
    return T.dat1(M.MODEL_TYPE, secs, bytes(names.blob))


class Model(unittest.TestCase):
    def test_synthetic_model_checks_hold(self):
        r = M.check_model(synth())
        bad = [n for n in r['notes'] if n.startswith('no: ')]
        self.assertEqual(bad, [])
        self.assertEqual(r['joints']['count'], 3)
        self.assertEqual(r['joints']['names'], ['root', 'spine', 'head'])
        self.assertEqual(r['meshes']['count'], 2)
        self.assertEqual((r['built']['vertexes'], r['built']['indexes']), (6, 6))
        self.assertIn('ok: 6 of 6 mesh indexes name a vertex that exists', r['notes'])
        self.assertIn('2C2CA205', r['multiples'])

    def test_a_wrong_guess_says_no(self):
        data = bytearray(synth())
        d = S.Dat1(bytes(data))
        off, sz = d.sections[M.TAG_JOINTS]
        struct.pack_into('<I', data, off + 16 + 8, 0xDEADBEEF)  # spine's hash
        off, sz = d.sections[M.TAG_BUILT]
        struct.pack_into('<I', data, off + 0x68, 12)  # 12 vertexes: 96 bytes no longer 16 each
        off, sz = d.sections[M.TAG_INDEXES]
        struct.pack_into('<H', data, off + 10, 9)  # the second mesh's last index: vertex 3 + 9, past the 12
        r = M.check_model(bytes(data))
        self.assertIn('no: 2 of 3 joint names hash to their CRC-32', r['notes'])
        self.assertTrue(any(n.startswith('no: 2C2CA205 as 16-byte vertexes') for n in r['notes']))
        self.assertIn('no: 5 of 6 mesh indexes name a vertex that exists', r['notes'])

    def test_not_a_model(self):
        self.assertEqual(M.check_model(b'nope' * 8)['notes'], ['no: not a DAT1'])
        r = M.check_model(T.dat1(0x98906B9F, {0x11111111: b'x'}, b''))
        self.assertEqual(r['notes'], ['no: DAT1 type is not 1B069A8A'])

    def test_end_to_end_through_lz4(self):
        A = S.asset_id
        assets = {
            S.SYSTEM_PROGRESSION: T.config({'SuitList': {'Suits': [
                {'Name': 'SUIT_CLASSIC', 'DisplayName': 'X', 'Item': T.REWARD},
                {'Name': 'Other', 'DisplayName': 'Y', 'Item': 'configs/rewards/other.config'}]}}),
            A(T.REWARD): T.config({'DefaultLoadoutConfig': {'AssetPath': T.LOADOUT}, 'DefaultMaskModel': {'AssetPath': T.MASK}}),
            A(T.LOADOUT): T.config({'Loadout': {'ItemLoadoutLists': [{'Items': [{'Item': T.VANITY}]}]}}),
            A(T.VANITY): T.config({'ModelList': [{'Model': {'AssetPath': T.BODY}}]}),
            A(T.BODY): synth(),
            A(T.MASK): T.dat1(M.MODEL_TYPE, {0x33333333: b'm' * 40}, b''),
        }
        with tempfile.TemporaryDirectory() as d:
            T.make_game(d, assets, {A(T.BODY)})
            before = {f: os.path.getmtime(os.path.join(dp, f)) for dp, _, fs in os.walk(d) for f in fs}
            out = os.path.join(d, 'sm2model.json')
            self.assertEqual(M.main(['sm2model.py', d, out]), 0)
            r = json.loads(T.get(out, 'rb'))
            body, mask = r['models']
            self.assertEqual((body['suit'], body['kind'], body['compression']), ('SUIT_CLASSIC', 'model', ['lz4']))
            self.assertFalse([n for n in body['notes'] if n.startswith('no: ')])
            self.assertEqual(mask['kind'], 'mask')
            self.assertTrue(any(n.startswith('no: joints section missing') for n in mask['notes']))
            after = {f: os.path.getmtime(os.path.join(dp, f)) for dp, _, fs in os.walk(d) for f in fs if f != 'sm2model.json'}
            self.assertEqual(before, after)  # read-only
            self.assertEqual(M.report(d, ('Nobody',))['models'], [])

    def test_fixture_matches_for_the_cpp_test(self):
        data = synth()
        self.assertEqual(T.get(os.path.join(DATA, 'synth.model')), data)
        packed = T.get(os.path.join(DATA, 'synth.lz4'))
        self.assertEqual(S.lz4_block(packed, len(data)), data)

    def test_no_forbidden_names(self):
        for p in ('tools/sm2_suits/sm2model.py', 'include/spidy/sm2_model.hpp', 'tests/sm2_model_tests.cpp'):
            t = T.get(os.path.join(ROOT, p)).decode('utf-8').lower()
            self.assertNotIn('jim' + 'jam', t)
            self.assertNotIn('mil' + 'es', t)
        self.assertIn('CANDIDATE', T.get(os.path.join(ROOT, 'include', 'spidy', 'sm2_model.hpp')).decode())


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--write-fixture':
        os.makedirs(DATA, exist_ok=True)
        T.put(os.path.join(DATA, 'synth.model'), synth())
        T.put(os.path.join(DATA, 'synth.lz4'), T.lz4(synth()))
        sys.exit(0)
    unittest.main()
