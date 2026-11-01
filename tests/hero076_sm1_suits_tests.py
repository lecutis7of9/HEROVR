"""HERO VR 0.0.76 SM1 suits (tools/sm1_suits): our heroes as real Spider-Man Remastered suits, installed through
the game's own toc the way Overstrike / ALERT suit mods are (no game data here: a synthetic game folder built by
synth.make_game from our own packs; tests/data/sm1suits holds thinned copies of three packs)."""
import hashlib, os, shutil, sys, tempfile, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOL = os.path.join(ROOT, 'tools', 'sm1_suits')
sys.path.insert(0, TOOL)
import assets, bcn, inject, retarget, synth, sm1suits  # noqa: E402
PACKS = os.path.join(ROOT, 'tests', 'data', 'sm1suits')


def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest()


def template(kind):
    _, groups = sm1suits.load_pack(os.path.join(PACKS, 'herovr_superman.hsm1'))
    g = groups[0]
    bones = [g['jnt'][i][max(range(4), key=lambda k: g['wgt'][i][k])] for i in range(len(g['pos']))]
    return synth.make_model(g['pos'], g['nrm'], g['uv'], g['tris'], bones, kind=kind)


class Quiet(object):
    def __init__(self):
        self.lines = []

    def __call__(self, s):
        self.lines.append(s)


class Sm1Suits(unittest.TestCase):
    def test_build_round_trip_and_sides(self):
        # zup (a proper rotation) and mirror (left at -X): the fitted Captain America reads back, and the
        # vertices on our left arm (canonical +X) are skinned to the game's l_ joints
        header, groups = sm1suits.load_pack(os.path.join(PACKS, 'herovr_captainamerica.hsm1'))
        for kind in ('zup', 'mirror'):
            data = template(kind)
            T = inject.Template(data)
            pts, sk = T.body_points()
            names = T.joint_names()
            F = retarget.Fitter(pts, sk, retarget.joint_sides(names))
            self.assertEqual(F.mirrored, kind == 'mirror')
            g = groups[0]
            p, n, s = F.fit(header['ends'], g['pos'], g['nrm'], g['jnt'], g['wgt'])
            tris = [(a, c, b) for a, b, c in g['tris']] if F.mirrored else g['tris']
            built = inject.build(data, [{'pos': p, 'nrm': n, 'uv': g['uv'], 'tris': tris, 'skin': s}], ['herovr/t/m0.material'], Quiet())
            T2 = inject.Template(built)
            self.assertEqual(len(T2.skin), len(g['pos']))
            left = [i for i, q in enumerate(g['pos']) if q[0] > 0.45]
            self.assertTrue(left)
            sides = [names[max(T2.skin[i], key=lambda jw: jw[1])[0]] for i in left]
            share = sum(1 for nm in sides if nm and nm.startswith('l_')) / float(len(sides))
            self.assertGreater(share, 0.9, kind)

    def test_fold_small_groups(self):
        # Daredevil has 4 texture groups, the suit model 3 mesh slots: the 4x4 one folds into a free patch
        _, groups = sm1suits.load_pack(os.path.join(PACKS, 'herovr_daredevil.hsm1'))
        self.assertEqual(len(groups), 4)
        small = [g for g in groups if g['tex']['width'] == 4][0]
        colour = bcn.decode_bc1_block(small['tex_bytes'][:8])[0]
        nv = sum(len(g['pos']) for g in groups)
        out = sm1suits.merge_small_groups(groups, 3, Quiet())
        self.assertEqual(len(out), 3)
        self.assertEqual(sum(len(g['pos']) for g in out), nv)
        tgt = [g for g in out if g.get('patches')][0]
        (cx, cy), = tgt['patches']
        W = tgt['tex']['width']
        bw = W // 4
        blk = tgt['tex_bytes'][8 * ((cy * 4) * bw + cx * 4):][:8]
        self.assertEqual(bcn.decode_bc1_block(blk)[0][:3], colour[:3])
        moved = tgt['uv'][-len(small['uv']):]
        for u, v in moved:
            self.assertTrue(cx * 16 <= u * W <= cx * 16 + 16 and cy * 16 <= v * W <= cy * 16 + 16)
        with self.assertRaises(inject.ModelError):
            sm1suits.merge_small_groups([g for g in groups if g['tex']['width'] > 4] * 2, 3, Quiet())

    def test_config_edits_single_item_and_empty_arrays(self):
        tmp = tempfile.mkdtemp()
        try:
            synth.make_game(tmp, template('zup'))
            P = sm1suits.paths(tmp)
            toc = sm1suits.open_toc(P)
            prog = toc.read_asset(assets.AID_PROGRESSION if isinstance(assets.AID_PROGRESSION, int) else assets.aid(assets.AID_PROGRESSION), 0)
            toc.close()
            self.assertIsNotNone(prog)
            names = lambda d: [x['Name'] if isinstance(x, dict) else x for x in assets.suits_in_progression(d)]
            self.assertEqual(names(prog), ['SUIT_classic'])  # one item: read as a scalar, kept a list
            new = assets.edit_progression(prog, [('herovr_thor', True), ('herovr_hulk', True)])
            got = names(new)
            self.assertIn('SUIT_classic', got)
            self.assertEqual(len(got), 3)
            assets.read_config(new)  # the empty Gadgets list survives a save and reads back
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    def test_install_status_revert(self):
        tmp = tempfile.mkdtemp()
        try:
            synth.make_game(tmp, template('zup'))
            toc = os.path.join(tmp, 'asset_archive', 'toc')
            before = sha(toc)
            args = ['--game', tmp, '--packs', PACKS, '--log', os.path.join(tmp, 'log.txt')]
            self.assertEqual(sm1suits.main(['install', '--heroes', 'herovr_captainamerica,herovr_daredevil'] + args), 0)
            self.assertNotEqual(sha(toc), before)
            st = sm1suits.cmd_status(sm1suits.paths(tmp), Quiet())
            self.assertTrue(st['installed'] and st['toc_matches_install'])
            self.assertEqual(sorted(st['suits']), ['herovr_captainamerica', 'herovr_daredevil'])
            t = sm1suits.open_toc(sm1suits.paths(tmp))
            loc = [t.read_asset(assets.aid(assets.AID_LOCALIZATION) if not isinstance(assets.AID_LOCALIZATION, int) else assets.AID_LOCALIZATION, s) for s in (0, 1)]
            t.close()
            for data in loc:
                self.assertEqual(assets.read_localization(data, 'SUIT_HEROVR_DAREDEVIL'), 'Daredevil')
            # a second install reverts the first, then installs
            self.assertEqual(sm1suits.main(['install', '--heroes', 'herovr_captainamerica'] + args), 0)
            # a toc changed by someone else that still uses our archive: refused, nothing touched
            good = open(toc, 'rb').read()
            open(toc, 'wb').write(good + b'\0')
            self.assertEqual(sm1suits.main(['revert'] + args), 1)
            self.assertEqual(open(toc, 'rb').read(), good + b'\0')
            open(toc, 'wb').write(good)
            self.assertEqual(sm1suits.main(['revert'] + args), 0)
            self.assertEqual(sha(toc), before)
            self.assertEqual(sorted(os.listdir(os.path.join(tmp, 'asset_archive'))), ['g00s000', 'toc'])
            # a game update replaced the toc: revert only cleans up our files
            self.assertEqual(sm1suits.main(['install', '--heroes', 'herovr_captainamerica'] + args), 0)
            shutil.copyfile(os.path.join(tmp, 'asset_archive', sm1suits.BACKUP), toc)
            self.assertEqual(sm1suits.main(['revert'] + args), 0)
            self.assertEqual(sorted(os.listdir(os.path.join(tmp, 'asset_archive'))), ['g00s000', 'toc'])
            # dump: read only
            self.assertEqual(sm1suits.main(['dump', '--out', os.path.join(tmp, 'dump'), '--no-raw', '--heroes', 'herovr_captainamerica'] + args), 0)
            self.assertEqual(sha(toc), before)
            self.assertTrue(os.path.isfile(os.path.join(tmp, 'dump', 'herovr_sm1_dump.json')))
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


    def test_install_is_purely_additive(self):
        # SM2 style: the heroes are new suits next to Spider-Man's; no game suit, model, texture or config is replaced
        tmp = tempfile.mkdtemp()
        try:
            synth.make_game(tmp, template('zup'))
            P = sm1suits.paths(tmp)

            def table(t):
                return {(a, t.span_of(i)): (t.offsets[i].archive_index, t.offsets[i].offset, t.sizes[i].value)
                        for i, a in enumerate(t.ids)}
            t = sm1suits.open_toc(P)
            before = table(t)
            prog0 = t.read_asset(assets.AID_PROGRESSION, 0)
            t.close()
            args = ['--game', tmp, '--packs', PACKS, '--log', os.path.join(tmp, 'log.txt')]
            self.assertEqual(sm1suits.main(['install', '--heroes', 'herovr_captainamerica'] + args), 0)
            t = sm1suits.open_toc(P)
            after = table(t)
            prog1 = t.read_asset(assets.AID_PROGRESSION, 0)
            t.close()
            for key, where in before.items():
                self.assertIn(key, after)
                if key[0] not in sm1suits.SHARED_LISTS:
                    self.assertEqual(after[key], where, '%016X re-pointed' % key[0])
            self.assertGreater(len(after), len(before))
            names = lambda d: [i.get('Name') for i in assets.suits_in_progression(d)]  # noqa: E731
            self.assertEqual(names(prog1)[:len(names(prog0))], names(prog0))
            self.assertIn('SUIT_herovr_captainamerica', names(prog1))
            # a hero whose new path is already a game asset is skipped, the rest still go in
            self.assertEqual(sm1suits.main(['revert'] + args), 0)
            t = sm1suits.open_toc(P)
            plan = sm1suits.Plan(Quiet())
            plan.add('herovr/thor/thor.model', b'x')
            a = plan.assets[0][0]
            self.assertLess(t.find(a, 0), 0)
            t.close()
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    def test_additive_check_refuses_a_replacement(self):
        tmp = tempfile.mkdtemp()
        try:
            synth.make_game(tmp, template('zup'))
            t = sm1suits.open_toc(sm1suits.paths(tmp))
            plan = sm1suits.Plan(Quiet())
            plan.add(t.ids[0], b'not the game asset', t.span_of(0), 'a game asset')
            if t.ids[0] not in sm1suits.SHARED_LISTS:
                with self.assertRaises(assets.AssetError):
                    sm1suits.check_additive(t, plan)
            prog = t.read_asset(assets.AID_PROGRESSION, 0)
            plan = sm1suits.Plan(Quiet())
            plan.add(assets.AID_PROGRESSION, assets.edit_progression(prog, [('herovr_thor', True)]), 0, 'progression')
            self.assertEqual(sm1suits.check_additive(t, plan)['shared_lists_edited'], 1)
            t.close()
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    def test_flat_normals(self):
        for fmt in (71, 72, 77, 83, 84, 98):
            self.assertIsNotNone(bcn.flat_normal(fmt), fmt)
        for got, want in zip(bcn.decode_bc7_mode6_block(bcn.flat_normal(98))[:3], (128, 128, 255)):
            self.assertLessEqual(abs(got - want), 1)  # one shared p-bit: within 1

    def test_source_rules(self):
        for root, _, names in os.walk(TOOL):
            for f in names:
                if f.endswith('.py'):
                    s = open(os.path.join(root, f), encoding='utf-8', errors='replace').read().lower()
                    self.assertNotIn('jim' + 'jam', s, f)
        s = open(os.path.join(TOOL, 'sm1suits.py'), encoding='utf-8').read()
        self.assertIn("ARCHIVE_NAME = 'HeroVR\\\\herovr_suits'", s)
        self.assertNotIn('spidy', s.lower().replace('spidyvr', ''))  # never touches SpidyVR's files


class EmbeddedPython(unittest.TestCase):
    """The launcher's embedded Python has a ._pth file, so the script's own folder is not on sys.path.
    python -I simulates that: the tool must still find its sibling modules."""

    def test_isolated_mode_finds_sibling_modules(self):
        import subprocess
        tmp = tempfile.mkdtemp()
        try:
            r = subprocess.run([sys.executable, '-I', '-B', os.path.join(TOOL, 'sm1suits.py'), 'status',
                                '--game', tmp, '--packs', tmp], cwd=tmp, capture_output=True, text=True,
                               timeout=120)
            last = [l for l in r.stdout.splitlines() if l.startswith('RESULT ')]
            self.assertTrue(last, r.stdout + r.stderr)
            self.assertNotIn('ModuleNotFoundError', r.stdout + r.stderr)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    unittest.main()
