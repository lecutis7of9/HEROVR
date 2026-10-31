"""HERO VR 0.0.74 test 10 WINDOW PROBE (include/spidy/window_probe.hpp, tools/window_probe.py): read-only logging of
what each view (the game's, the left eye's, the right eye's) culled the night windows with, on the impostor query
hook and isActiveView the mod already has. No new game address, no write to the game, no switch.
Made by Agent Mulder."""
import json, math, os, re, struct, sys, tempfile, pathlib, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import window_probe as wp  # noqa: E402
def src(p):
    with open(os.path.join(ROOT, p), encoding='utf-8') as f:
        return f.read()


def probe_body():
    s = src('src/stereo_probe.cpp')
    a = s.index('void windowProbe(void* view, void* cull, void* result) {')
    return s, s[a:s.index('\n}\n', a)]


def record(samples, count=None, calls=(10, 20, 30), results=(1, 2, 3), other=4, faults=0, sequence=2):
    raw = bytearray(wp.SIZE)
    struct.pack_into('<4I', raw, 0, wp.MAGIC, wp.VERSION, wp.SIZE, len(samples) if count is None else count)
    struct.pack_into('<3Q', raw, 16, *calls)
    struct.pack_into('<3Q', raw, 40, *results)
    struct.pack_into('<2Qd', raw, 64, other, faults, 12.5)
    start = (len(samples) if count is None else count) - len(samples)
    for k, s in enumerate(samples):
        wp.SAMPLE.pack_into(raw, wp.HEADER_BYTES + ((start + k) % wp.RING) * wp.SAMPLE_BYTES, *s)
    struct.pack_into('<Q', raw, wp.SIZE - 8, sequence)
    return bytes(raw)


def projection(l, r, t, b, n=.1, f=1000.):
    """A D3D-style (row vector) left-handed off-centre projection from tangents."""
    sx, sy = 2 / (r - l), 2 / (b - t)
    ox, oy = -(r + l) / (r - l), -(b + t) / (b - t)
    q = f / (f - n)
    return [sx, 0, 0, 0, 0, sy, 0, 0, ox, oy, q, 1, 0, 0, -q * n, 0]


def sample(view=1, lens=(-1.2, 1.0, -1.1, 1.1), matrix=None, flags=8 | 1, seconds=100.25, cull=None):
    pose = [1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 5, 6, 7, 1]  # right, down, ahead (+z)
    matrix = matrix or projection(*lens)
    cull = cull or [0.0] * 32
    return (seconds, 77, view, flags, *lens, .1, 1000., 300., 0, *pose, *matrix, *[0.0] * 16, *cull)


class Layout(unittest.TestCase):
    def test_header_matches_parser(self):
        h = src('include/spidy/window_probe.hpp')
        self.assertIn('static_assert(sizeof(Sample) == %d);' % wp.SAMPLE_BYTES, h)
        self.assertIn('static_assert(sizeof(Data) == %d);' % wp.SIZE, h)
        self.assertIn('bytes = %d' % wp.SIZE, h)
        self.assertIn('magic = 0x%08x, version = %d' % (wp.MAGIC, wp.VERSION), h)
        self.assertIn('ringSize = %d;' % wp.RING, h)
        self.assertEqual(re.findall(r'#include [<"]([^>"]+)[>"]', h), ['cstdint'])

    def test_exported_and_optional(self):
        s = src('src/stereo_probe.cpp')
        self.assertIn('__declspec(dllexport) spidy::window_probe::Data SpidyWindowProbeData;', s)
        r = src('tools/run_game_vr.py')
        self.assertIn('optional=(window_probe.EXPORT,))', r)
        self.assertNotIn('SpidyWindowProbeData', src('tools/update37_records.py'))  # never a required record
        b = src('tools/bridge_game.py')
        self.assertIn("for name in optional:", b)
        self.assertIn("ROOT / 'reports' / f\"window-probe-{time.strftime('%Y%m%d-%H%M%S')}.jsonl\")", r)


class ReadOnly(unittest.TestCase):
    def test_called_after_the_original_and_before_the_writes(self):
        s = src('src/stereo_probe.cpp')
        a = s.index('void* impostorQueries(void* view, void* cull) {')
        body = s[a:s.index('\n}\n', a)]
        self.assertTrue(body.split('\n')[1].strip() == 'const auto result = originalImpostorQueries(view, cull);')
        self.assertTrue(body.split('\n')[2].strip() == 'windowProbe(view, cull, result);')

    def test_no_write_and_no_new_address(self):
        s, body = probe_body()
        for w in ('writeFloat', 'writeByte', 'writeU32', 'WriteProcessMemory', 'VirtualProtect', 'MH_'):
            self.assertNotIn(w, body)
        # Every copy goes into our own locals / record, never a game pointer.
        for m in re.findall(r'std::memcpy\(([^,]+),', body):
            self.assertTrue(m.strip().startswith('sample.'), m)
        others = s.replace(body, '') + src('include/spidy/native_view.hpp')
        header = src('include/spidy/window_probe.hpp')
        for value in set(re.findall(r'0x[0-9a-fA-F]+', body)):
            # every offset/address is already used by the mod, or one of the cull build's documented inputs
            self.assertTrue(value in others or '+' + value[2:] in header, f'{value} is new')
        big = [v for v in re.findall(r'0x[0-9a-fA-F]+', body) if int(v, 16) >= 0x10000]
        self.assertEqual(set(big), {'0xc2ca220'})  # the game view pointer the mod already reads

    def test_no_switch(self):
        self.assertNotIn('window_probe', src('tools/live_settings.py'))


class Parser(unittest.TestCase):
    def test_round_trip(self):
        d = wp.parse(record([sample(view=0), sample(view=2, seconds=100.35)]))
        self.assertEqual(d['calls'], dict(game=10, left=20, right=30))
        self.assertEqual(d['results'], dict(game=1, left=2, right=3))
        self.assertEqual((d['other'], d['count'], len(d['samples'])), (4, 2, 2))
        self.assertEqual([s['view'] for s in d['samples']], [0, 2])
        self.assertAlmostEqual(d['samples'][1]['seconds'], 100.35)

    def test_rejects(self):
        self.assertIsNone(wp.parse(record([sample()], sequence=3)))  # being written
        self.assertIsNone(wp.parse(b'\0' * 10))
        self.assertIsNone(wp.parse(None))
        bad = bytearray(record([sample()]))
        struct.pack_into('<I', bad, 0, 1)
        self.assertIsNone(wp.parse(bytes(bad)))

    def test_ring_wraps(self):
        d = wp.parse(record([sample(seconds=float(i)) for i in range(wp.RING)], count=250))
        self.assertEqual([s['index'] for s in d['samples']], list(range(250 - wp.RING, 250)))
        self.assertEqual(d['samples'][-1]['seconds'], float(wp.RING - 1))

    def test_read_never_raises(self):
        class Broken:
            def read(self, *_):
                raise OSError('gone')
        self.assertIsNone(wp.read(Broken(), {wp.EXPORT: 1234}))
        self.assertIsNone(wp.read(Broken(), {}))
        self.assertIsNone(wp.read(Broken(), None))

        class Game:
            def __init__(self, raw): self.raw = raw
            def read(self, address, n): return self.raw[address - 1000:address - 1000 + n]
        raw = record([sample()])
        self.assertEqual(wp.read(Game(raw), {wp.EXPORT: 1000})['count'], 1)


class Angles(unittest.TestCase):
    def test_lens_and_matrix_agree(self):
        line = wp.analyse(wp.parse(record([sample()]))['samples'][0])
        self.assertAlmostEqual(line['lens_deg']['left'], math.degrees(math.atan(-1.2)), places=2)
        self.assertAlmostEqual(line['lens_deg']['width'], math.degrees(math.atan(1.0) + math.atan(1.2)), places=2)
        m = line['matrix_deg']
        self.assertEqual((m['source'], m['layout']), ('+2e0', 'columns'))
        for k in ('left', 'right', 'top', 'bottom'):
            self.assertAlmostEqual(m[k], line['lens_deg'][k], places=2)
        self.assertAlmostEqual(line['edge_gap_deg']['left'], 0, places=2)
        self.assertAlmostEqual(line['edge_gap_deg']['right'], 0, places=2)

    def test_narrow_cull_shows_a_negative_gap(self):
        s = sample(matrix=projection(-1.0, .8, -1.1, 1.1))
        line = wp.analyse(wp.parse(record([s]))['samples'][0])
        self.assertLess(line['edge_gap_deg']['left'], -4)
        self.assertLess(line['edge_gap_deg']['right'], -4)

    def test_transposed_layout(self):
        m = projection(-1.2, 1.0, -1.1, 1.1)
        t = [m[c * 4 + r] for r in range(4) for c in range(4)]
        self.assertEqual(wp.projection_tangents(t)['layout'], 'rows')
        x = wp.projection_tangents(t)['x']
        self.assertAlmostEqual(x[0], -1.2, places=4)
        self.assertAlmostEqual(x[1], 1.0, places=4)

    def test_not_a_projection(self):
        self.assertIsNone(wp.projection_tangents([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]))
        self.assertIsNone(wp.projection_tangents([float('nan')] * 16))

    def test_plane_half_angles(self):
        # Side planes of a 45-degree half-angle frustum looking along +z, inward normals, as 4-wide x/y/z/d.
        c = math.sqrt(.5)
        normals = [(c, 0, c), (-c, 0, c), (0, c, c), (0, -c, c)]
        cull = [n[0] for n in normals] + [n[1] for n in normals] + [n[2] for n in normals] + [0] * 4 + [0.0] * 16
        line = wp.analyse(wp.parse(record([sample(cull=cull)]))['samples'][0])
        self.assertEqual(line['plane_deg'][:4], [45.0] * 4)
        self.assertEqual(line['plane_deg'][4:], [None] * 4)

    def test_failed_read_sample(self):
        line = wp.analyse(wp.parse(record([sample(flags=1)]))['samples'][0])
        self.assertFalse(line['read_ok'])
        self.assertNotIn('lens_deg', line)


class JournalTests(unittest.TestCase):
    def test_appends_each_sample_once(self):
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / 'reports' / 'window-probe-x.jsonl'
            j = wp.Journal(path)
            first = wp.parse(record([sample(seconds=1.0), sample(seconds=1.1)]))
            self.assertEqual(j.add(first), 2)
            self.assertEqual(j.add(first), 0)
            second = wp.parse(record([sample(seconds=1.0), sample(seconds=1.1), sample(view=0, seconds=1.2)]))
            self.assertEqual(j.add(second), 1)
            lines = [json.loads(x) for x in path.read_text().splitlines()]
            views = [x['view'] for x in lines if 'view' in x]
            self.assertEqual(views, ['left', 'left', 'game'])
            counts = [x for x in lines if 'counts' in x]
            self.assertEqual(counts[-1]['counts']['calls'], dict(game=10, left=20, right=30))
            self.assertIn('not available', counts[-1]['counts']['instance_counts'])
            self.assertEqual(j.add(None), 0)

    def test_notes_missed_samples(self):
        with tempfile.TemporaryDirectory() as d:
            j = wp.Journal(pathlib.Path(d) / 'w.jsonl')
            j.add(wp.parse(record([sample()])))
            j.add(wp.parse(record([sample() for _ in range(wp.RING)], count=300)))
            lines = [json.loads(x) for x in (pathlib.Path(d) / 'w.jsonl').read_text().splitlines()]
            self.assertEqual([x['missed_samples'] for x in lines if 'missed_samples' in x], [300 - wp.RING - 1])

    def test_unwritable_never_raises(self):
        with tempfile.TemporaryDirectory() as d:
            blocker = pathlib.Path(d) / 'file'
            blocker.write_text('x')
            self.assertEqual(wp.Journal(blocker / 'sub' / 'w.jsonl').add(wp.parse(record([sample()]))), 0)


class Names(unittest.TestCase):
    def test_no_forbidden_name(self):
        for p in ('include/spidy/window_probe.hpp', 'tools/window_probe.py', 'tests/hero074t10_window_probe_tests.py',
                  'src/stereo_probe.cpp', 'tools/run_game_vr.py', 'tools/bridge_game.py'):
            self.assertNotIn('jim' + 'jam', src(p).lower())


if __name__ == '__main__':
    unittest.main()
