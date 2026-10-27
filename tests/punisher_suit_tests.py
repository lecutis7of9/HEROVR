"""Version 0.0.72 PUNISHER SUIT kit tests (tools/punisher_suit/prep_punisher.py): a synthetic T-pose
humanoid OBJ (Z-up and facing -Z, quads, a texture), prepped as each suit: Y-up, facing +Z, 1.80 m on
its feet, triangulated and welded, the slot under the submesh limit, the credit names only the artist
and licence. Needs numpy; skipped (passes) without it, as on the CI runner."""
import json, os, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
try:
    import numpy as np  # noqa: F401
except ImportError:
    print('SKIP punisher_suit_tests (no numpy)')
    sys.exit(0)
sys.path.insert(0, os.path.join(ROOT, 'tools', 'punisher_suit'))
import prep_punisher  # noqa: E402

fails = []


def check(ok, what):
    print(('PASS ' if ok else 'FAIL ') + what)
    if not ok:
        fails.append(what)


def box(V, F, lo, hi):
    """An axis box as 6 quads, in (x, depth, up) = OBJ (x, y, z) Z-up."""
    b = len(V)
    for i in range(8):
        V.append((hi[0] if i & 1 else lo[0], hi[1] if i & 2 else lo[1], hi[2] if i & 4 else lo[2]))
    for q in ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)):
        F.append([b + k + 1 for k in q])


def humanoid(folder, units=100.0):
    """Z-up, the toes toward -Y (OBJ y is the depth here), 1.8 units * `units` tall, arms out (T-pose)."""
    V, F = [], []
    u = units
    box(V, F, (-.20 * u, -.10 * u, .90 * u), (.20 * u, .10 * u, 1.50 * u))  # torso
    box(V, F, (-.10 * u, -.10 * u, 1.52 * u), (.10 * u, .10 * u, 1.80 * u))  # head
    for x in range(20, 85, 5):  # the arms in 5 cm pieces
        box(V, F, (x / 100 * u, -.05 * u, 1.38 * u), ((x + 5) / 100 * u, .05 * u, 1.48 * u))
        box(V, F, (-(x + 5) / 100 * u, -.05 * u, 1.38 * u), (-x / 100 * u, .05 * u, 1.48 * u))
    for x in (-.12, .12):
        for z in range(5, 90, 5):  # the leg in 5 cm pieces (vertices at shin height for the facing test)
            box(V, F, ((x - .07) * u, -.07 * u, z / 100 * u), ((x + .07) * u, .07 * u, (z + 5) / 100 * u))
        box(V, F, ((x - .06) * u, -.25 * u, 0), ((x + .06) * u, .06 * u, .05 * u))  # foot, toes to -y
    with open(os.path.join(folder, 'punisher.mtl'), 'w') as m:
        m.write('newmtl body\nKd 1 1 1\nmap_Kd textures/body.png\n')
    os.makedirs(os.path.join(folder, 'textures'), exist_ok=True)
    open(os.path.join(folder, 'textures', 'body.png'), 'wb').write(b'\x89PNG\r\n\x1a\n')
    with open(os.path.join(folder, 'punisher.obj'), 'w') as o:
        o.write('mtllib punisher.mtl\n')
        for v in V:
            o.write('v %.4f %.4f %.4f\n' % v)
        o.write('vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nusemtl body\n')
        for f in F:
            o.write('f ' + ' '.join('%d/%d' % (k, n + 1) for n, k in enumerate(f)) + '\n')
    return len(F) * 2


with tempfile.TemporaryDirectory() as tmp:
    src = os.path.join(tmp, 'src')
    os.makedirs(src)
    tris = humanoid(src)
    for suit in ('vest', 'warzone'):
        out = os.path.join(tmp, suit.upper())
        rep = prep_punisher.prep(suit, src, out, up='z')
        g = json.load(open(os.path.join(out, 'punisher_suit.gltf')))
        check(rep['triangles'] == tris and rep['source_triangles'] == tris, '%s: every quad fanned to 2 triangles (%d)' % (suit, rep['triangles']))
        check(abs(rep['height_m'] - 1.80) < 1e-3, '%s: 1.80 m tall (%.4f)' % (suit, rep['height_m']))
        lo, hi = rep['bounds_m']['low'], rep['bounds_m']['high']
        check(abs(lo[1]) < 1e-4, '%s: feet on y = 0' % suit)
        check(abs(lo[0] + hi[0]) < 1e-3 and abs(lo[2] + hi[2]) < 1e-3, '%s: centred over the origin' % suit)
        check(hi[0] - lo[0] > 1.6, '%s: the arm span along X (%.2f m)' % (suit, hi[0] - lo[0]))
        acc = g['accessors'][g['meshes'][0]['primitives'][0]['attributes']['POSITION']]
        bv = g['bufferViews'][acc['bufferView']]
        raw = open(os.path.join(out, 'punisher_suit.bin'), 'rb').read()[bv['byteOffset']:bv['byteOffset'] + bv['byteLength']]
        P = np.frombuffer(raw, np.float32).reshape(-1, 3)
        feet, shins = P[P[:, 1] < .04 * 1.8, 2].mean(), P[(P[:, 1] > .08 * 1.8) & (P[:, 1] < .2 * 1.8), 2].mean()
        check(feet > shins, '%s: the toes point along +Z (feet %.3f, shins %.3f)' % (suit, feet, shins))
        check(acc['count'] <= prep_punisher.SUBMESH_VERTS, '%s: one slot under the submesh limit' % suit)
        check(g['images'] == [{'uri': 'textures/body.png'}] and os.path.isfile(os.path.join(out, 'textures', 'body.png')), '%s: the texture copied and referenced' % suit)
        cr = open(os.path.join(out, 'CREDITS.txt')).read()
        artist = 'floh' if suit == 'vest' else 'spybg'
        lic = 'CC BY-NC 4.0' if suit == 'vest' else 'CC BY 4.0'
        check(artist in cr and lic in cr and 'Marvel' not in cr and 'MARVEL' not in cr, '%s: the credit names %s and %s, no Marvel' % (suit, artist, lic))
        check(g['asset']['copyright'] == cr.strip(), '%s: the glTF carries the credit' % suit)
    # the NonCommercial note only on the vest
    check('never monetized' in prep_punisher.credit('vest') and 'never monetized' not in prep_punisher.credit('warzone'),
          'the NonCommercial note on the VEST credit only')
    try:
        prep_punisher.prep('robe', src, os.path.join(tmp, 'x'))
        check(False, 'an unknown suit refused')
    except SystemExit:
        check(True, 'an unknown suit refused')
    empty = os.path.join(tmp, 'empty')
    os.makedirs(empty)
    try:
        prep_punisher.prep('vest', empty, os.path.join(tmp, 'y'))
        check(False, 'a folder with no model refused')
    except SystemExit:
        check(True, 'a folder with no model refused')

print('punisher_suit_tests:', 'PASS' if not fails else 'FAIL %d' % len(fails))
sys.exit(1 if fails else 0)
