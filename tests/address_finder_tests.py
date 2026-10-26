# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
import json
import pathlib
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
import address_finder as af


def pack_data(**kw):
    status = kw.get('status', 2)
    seq = kw.get('sequence', 2)
    raw = struct.pack(
        '<4Iq4I4Q2I',
        af.MAGIC, af.VERSION, af.DATA_BYTES, status,
        seq,
        kw.get('seconds_left', 0), kw.get('candidates', 3), kw.get('generation', 1), kw.get('fault', 0),
        kw.get('frames', 100), kw.get('scans', 10), kw.get('export_hits', 1), kw.get('string_hits', 2),
        kw.get('lod_offset', 0xFFFFFFFF), kw.get('raw_hits', 0),
    )
    assert len(raw) == af.DATA_BYTES
    return raw


class Parse(unittest.TestCase):
    def test_parse_data(self):
        d = af.parse_data(pack_data(candidates=7, lod_offset=0x40))
        self.assertEqual(d['status_name'], 'done')
        self.assertEqual(d['candidates'], 7)
        self.assertEqual(d['lod_offset'], 0x40)
        self.assertEqual(d['label'], 'candidates_only')
        self.assertIsNone(af.parse_data(b'\0' * 16))

    def test_report_lines_and_files(self):
        text = (
            '{"type":"header","label":"candidates_only","update":27}\n'
            '{"type":"candidate","kind":"cbv","detail":"world","rva":16,"function_rva":0,'
            '"hits":2,"cb_offset":64,"root_slot":3,"signature":"48 8D ?? ?? ?? ??"}\n'
            '{"type":"footer","candidates":1}\n'
        )
        lines = af.parse_report_lines(text)
        self.assertEqual(lines[1]['kind'], 'cbv')
        data = af.parse_data(pack_data(candidates=1))
        with tempfile.TemporaryDirectory() as d:
            j, t = af.write_files(d, data, text, when=0)
            self.assertTrue(j.exists() and t.exists())
            payload = json.loads(j.read_text())
            self.assertEqual(payload['label'], 'candidates_only')
            self.assertEqual(payload['update'], 33)
            self.assertEqual(payload['port_version'], af.port_version())
            self.assertIn('CANDIDATES ONLY', t.read_text())
            self.assertEqual(af.done_line(data, j),
                             f'ADDRESS FINDER done: 1 confirmed (0 raw hits), report saved to {j}')
            latest_j = pathlib.Path(d) / 'address-finder-latest.json'
            latest_t = pathlib.Path(d) / 'address-finder-latest.txt'
            self.assertTrue(latest_j.exists() and latest_t.exists())
            self.assertEqual(latest_j.read_text(encoding='utf-8'), j.read_text(encoding='utf-8'))


class AlmostBucket(unittest.TestCase):
    """Update 28: the report's CONFIRMED / ALMOST / other split and the console count."""

    def test_buckets_and_txt_sections(self):
        text = (
            '{"type":"header","label":"candidates_only","update":28}\n'
            '{"type":"candidate","kind":"sl_constants","rva":51928279,"function_rva":51927904,"hits":9,'
            '"cb_offset":0,"root_slot":0,"confirmed":true,"almost":false,"near_miss":false,'
            '"distinct_frames":5,"score":1}\n'
            '{"type":"candidate","kind":"taa_jitter","rva":0,"function_rva":0,"hits":4,"cb_offset":11776,'
            '"root_slot":0,"confirmed":false,"almost":true,"near_miss":false,"distinct_frames":4,"score":0}\n'
            '{"type":"candidate","kind":"cb_near","rva":1,"function_rva":0,"hits":1,"cb_offset":64,'
            '"root_slot":0,"confirmed":false,"almost":true,"near_miss":true,"distinct_frames":1,"score":0}\n'
            '{"type":"candidate","kind":"root_cbv","rva":2,"function_rva":0,"hits":1,"cb_offset":0,'
            '"root_slot":1,"confirmed":false,"almost":false,"near_miss":false,"distinct_frames":0,"score":0}\n'
            '{"type":"footer","confirmed":1,"almost":2,"raw_hits":4,"frames":100}\n'
        )
        lines = af.parse_report_lines(text)
        self.assertEqual([af.candidate_bucket(o) for o in lines],
                         [None, 'confirmed', 'almost', 'almost', 'raw', None])
        self.assertEqual(af.almost_count(lines), 2)
        data = af.parse_data(pack_data(candidates=1))
        with tempfile.TemporaryDirectory() as d:
            j, t = af.write_files(d, data, text, when=0)
            txt = t.read_text(encoding='utf-8')
            self.assertIn(f'(Update 33; Version {af.public_version()}, port {af.port_version()})', txt)  # Version 0.0.1
            self.assertIn('almost: 2', txt)
            conf, almost, raw = (txt.index('--- CONFIRMED ---'), txt.index('--- ALMOST'),
                                 txt.index('--- other raw hits ---'))
            self.assertTrue(conf < txt.index('[sl_constants]') < almost < txt.index('[taa_jitter]') < raw
                            < txt.index('[root_cbv]'))
            self.assertIn('NEAR', txt[almost:raw])
            self.assertEqual(af.done_line(data, j, 2),
                             f'ADDRESS FINDER done: 1 confirmed, 2 almost (0 raw hits), report saved to {j}')

    def test_is_almost_mirrors_cpp(self):
        self.assertTrue(af.is_almost(False, True, 2, False))
        self.assertTrue(af.is_almost(False, False, 1, True))
        self.assertFalse(af.is_almost(True, True, 5, True))
        self.assertFalse(af.is_almost(False, True, 1, False))
        hdr = (pathlib.Path(__file__).resolve().parents[1] / 'include' / 'spidy' /
               'address_finder.hpp').read_text(encoding='utf-8')
        self.assertIn('return nearMiss || (valueBearing && distinctFrames >= 2);', hdr)


class FrameBudget(unittest.TestCase):
    """Update 27.2: simulate the finder's per-frame live-sampling budget (mirrors C++)."""

    def test_allows_under_budget(self):
        self.assertTrue(af.frame_budget_allows(0, af.FRAME_BUDGET_MAX_OPS, 0, af.FRAME_BUDGET_NS))
        self.assertTrue(af.frame_budget_allows(7, af.FRAME_BUDGET_MAX_OPS, 499_999, af.FRAME_BUDGET_NS))

    def test_blocks_when_ops_or_time_spent(self):
        self.assertFalse(af.frame_budget_allows(8, af.FRAME_BUDGET_MAX_OPS, 0, af.FRAME_BUDGET_NS))
        self.assertFalse(af.frame_budget_allows(0, af.FRAME_BUDGET_MAX_OPS, 500_000, af.FRAME_BUDGET_NS))
        self.assertFalse(af.frame_budget_allows(0, af.FRAME_BUDGET_MAX_OPS, 1_000_000, af.FRAME_BUDGET_NS))

    def test_backoff_doubles_then_decays(self):
        skip = 0
        skip = af.frame_budget_next_skip(skip, True)
        self.assertEqual(skip, 2)
        skip = af.frame_budget_next_skip(skip, True)
        self.assertEqual(skip, 4)
        skip = af.frame_budget_next_skip(skip, True)
        self.assertEqual(skip, 8)
        # Cap at 32.
        skip = 32
        self.assertEqual(af.frame_budget_next_skip(skip, True), 32)
        # Decay when under budget.
        self.assertEqual(af.frame_budget_next_skip(3, False), 2)
        self.assertEqual(af.frame_budget_next_skip(1, False), 0)
        self.assertEqual(af.frame_budget_next_skip(0, False), 0)

    def test_header_constants_match_python(self):
        hdr = (pathlib.Path(__file__).resolve().parents[1] / 'include' / 'spidy' /
               'address_finder.hpp').read_text(encoding='utf-8')
        self.assertIn('frameBudgetNanoseconds = 500000', hdr)
        self.assertIn('frameBudgetMaxOps = 8', hdr)

    def test_partial_write_flag(self):
        data = af.parse_data(pack_data(status=1, candidates=2))
        with tempfile.TemporaryDirectory() as d:
            j, t = af.write_files(d, data, '{"type":"footer","candidates":2}\n', partial=True)
            self.assertIn('partial-', j.name)
            self.assertTrue(json.loads(j.read_text())['partial'])
            self.assertIn('PARTIAL', t.read_text())



class QualityGate(unittest.TestCase):
    """Update 27.3: trivial reject, K-frame confirm, raw-hit cap (mirrors C++ helpers)."""

    def test_trivial_scalar_and_mat(self):
        self.assertTrue(af.is_trivial_scalar(0.0))
        self.assertTrue(af.is_trivial_scalar(1.0))
        self.assertTrue(af.is_trivial_scalar(-1.0))
        self.assertTrue(af.is_trivial_scalar(float('nan')))
        self.assertTrue(af.is_trivial_scalar(1e-45))
        self.assertFalse(af.is_trivial_scalar(0.5))
        ident = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
        zero = [0] * 16
        self.assertTrue(af.is_trivial_mat(ident))
        self.assertTrue(af.is_trivial_mat(zero))
        pose = list(ident)
        pose[12] = 3.0
        self.assertFalse(af.is_trivial_mat(pose))

    def test_confirm_needs_k_changing_frames(self):
        st = {}
        self.assertFalse(af.note_confirm_sample(st, 1, [1, 0, 0, 0]))
        self.assertFalse(af.note_confirm_sample(st, 1, [2, 0, 0, 0]))  # same frame ignored
        self.assertFalse(af.note_confirm_sample(st, 2, [1.00000001, 0, 0, 0]))  # no change
        self.assertFalse(af.note_confirm_sample(st, 2, [1.1, 0, 0, 0]))
        self.assertTrue(af.note_confirm_sample(st, 3, [1.2, 0, 0, 0]))
        self.assertEqual(st['distinct_frames'], 3)

    def test_raw_hits_cap_does_not_imply_done(self):
        self.assertFalse(af.raw_hits_at_cap(255))
        self.assertTrue(af.raw_hits_at_cap(256))
        self.assertTrue(af.raw_hits_at_cap(400))
        # Cap is storage only — confirmed count is independent.
        self.assertEqual(af.MAX_RAW_HITS, 256)
        self.assertEqual(af.CONFIRM_FRAMES_K, 3)

    def test_score_prefers_category_and_frames(self):
        weak = af.score_confirmed(3, 3, 0.01, 'other')
        strong = af.score_confirmed(5, 10, 0.0, 'lighting')
        self.assertGreater(strong, weak)
        self.assertGreaterEqual(strong, af.STRONG_SCORE)

    def test_parse_raw_hits_field(self):
        d = af.parse_data(pack_data(candidates=2, raw_hits=40))
        self.assertEqual(d['candidates'], 2)
        self.assertEqual(d['raw_hits'], 40)

    def test_header_confirm_constants(self):
        hdr = (pathlib.Path(__file__).resolve().parents[1] / 'include' / 'spidy' /
               'address_finder.hpp').read_text(encoding='utf-8')
        self.assertIn('confirmFramesK = 3', hdr)
        self.assertIn('maxRawHits = 256', hdr)

class WhichCamera(unittest.TestCase):
    """Update 29: the txt's WHICH CAMERA summary from cams= / FLAT-ONLY in candidate details."""

    def test_summary(self):
        def cand(kind, fn, detail):
            return {'type': 'candidate', 'kind': kind, 'function_rva': fn, 'detail': detail}
        lines = [cand('cb_match', 0x2ccbfc0, 'copy_buffer position cams=flat+flat-1 FLAT-ONLY lag=0 err=0'),
                 cand('cb_near', 0x2ccbfc0, 'copy_buffer jitter NEAR cams=flat FLAT-ONLY lag=0 err=1e-3'),
                 cand('cb_match', 0x500587a, 'root_cbv proj_diag cams=eye0 EYE-ONLY lag=0 err=0'),
                 cand('cb_match', 1, 'root_cbv position cams=flat+head lag=0 err=0'),
                 cand('cb_match', 2, 'copy_buffer position lag=3 err=0')]  # Update 28: not counted
        out = '\n'.join(af.camera_summary(lines))
        self.assertIn('--- WHICH CAMERA', out)
        self.assertIn('FLAT-ONLY: 2 (1 exact)', out)
        self.assertIn('functions: 0x2ccbfc0', out)
        self.assertIn('EYE-ONLY: 1 (1 exact)', out)
        self.assertIn('HEAD-ONLY: 0 (0 exact)', out)
        self.assertIn('mixed or not told apart: 1', out)
        self.assertEqual(af.camera_summary(lines[-1:]), [])



class ShadowPassCandidates(unittest.TestCase):
    """Update 30: bind rows from the native report become SHADOW PASS CANDIDATES."""

    def bind(self, **kw):
        row = {'type': 'bind', 'api': 'graphics_cbv', 'rva': 0x1234, 'function_rva': 0x1200, 'root_slot': 2,
               'cb_offset': 128, 'cams': 'head', 'tag': 'HEAD-ONLY', 'match': 'view_proj', 'seed29': False,
               'pass': 'depth_only', 'rt_count': 0, 'dsv': True, 'viewport': [2048, 2048],
               'depth': {'format': 40, 'width': 4096, 'height': 4096}, 'hits': 9, 'frames': 5,
               'shadow_candidate': True}
        row.update(kw)
        return row

    def test_report(self):
        rows = [self.bind(),
                self.bind(rva=0x99, seed29=True, cams='none', tag=''),          # seeds never count
                self.bind(rva=0x77, api='compute_cbv', **{'pass': 'compute'}, tag='FLAT-ONLY', cams='flat'),
                self.bind(rva=0x55, cams='none', tag='')]                      # no camera: not a candidate
        summary = {'type': 'bind_summary', 'rows': 4, 'cap': 96, 'dropped': 0, 'watched': 12,
                   'shadow_candidates': 1, 'pass_hooks': 'on', 'descriptor_tables': 'on', 'watched_cap': 256}
        self.assertEqual(af.shadow_count(rows), 1)
        out = '\n'.join(af.shadow_pass_summary(rows + [summary]))
        self.assertIn('--- SHADOW PASS CANDIDATES', out)
        self.assertIn('depth-only passes that bound camera data: 1', out)
        self.assertIn('pass=depth_only rt=0 dsv=True viewport=2048x2048 depth fmt=40 4096x4096', out)
        self.assertIn('slot=2 cb+128', out)
        self.assertIn('rva=0x1234 fn=0x1200', out)
        self.assertIn('SEED-29', out)
        self.assertIn('descriptor-table hooks on, watch cap 256', out)
        rest = out.split('other binds')[1]
        self.assertLess(rest.index('rva=0x77'), rest.index('rva=0x99'))  # FLAT-ONLY first, seeds last
        self.assertEqual(af.shadow_pass_summary([]), [])
        text = '\n'.join(json.dumps(o) for o in [{'type': 'header', 'update': 30}] + rows + [summary]) + '\n'
        data = af.parse_data(pack_data(candidates=0))
        with tempfile.TemporaryDirectory() as d:
            j, t = af.write_files(d, data, text, when=0)
            txt = t.read_text(encoding='utf-8')
            self.assertIn('shadow_pass_candidates: 1', txt)
            self.assertIn('SHADOW PASS CANDIDATES', txt)
            self.assertIn("'type': 'bind_summary'", txt)
            payload = json.loads(j.read_text(encoding='utf-8'))
            self.assertEqual(sum(1 for o in payload['lines'] if o.get('type') == 'bind'), 4)
            self.assertTrue(af.done_line(data, j, 0, 1).startswith(
                'ADDRESS FINDER done: 0 confirmed, 1 shadow pass candidates'))

class VersionShownAndFullWindow(unittest.TestCase):
    """Update 30.1: the installed update is obvious, and the bind trace gets the full window."""
    ROOT = pathlib.Path(__file__).resolve().parent.parent

    def read(self, rel):
        return (self.ROOT / rel).read_text(encoding='utf-8')

    def test_port_version_file(self):
        self.assertEqual(af.port_version(), self.read('PORT-VERSION').splitlines()[0].strip())
        self.assertEqual(af.port_version(), 'Update 39.1.1')

    def test_launcher_prints_version_first_in_green(self):
        s = self.read('tools/launch-game-vr.ps1')
        line = 'Write-Host "SM2 VR Version $publicVersion (build $portVersion) - Made by Agent Mulder" -ForegroundColor Green'  # Version 0.0.1, 0.0.6 credit
        self.assertIn(line, s)
        self.assertLess(s.index(line), s.index("foreach($name in @('spidy_headset_probe.exe'"))
        self.assertIn('Write-Host "SM2 VR Version $publicVersion (build $portVersion) - Made by Agent Mulder" -ForegroundColor Green', self.read('tools/build.ps1'))

    def test_session_console_names_version(self):
        s = self.read('tools/run_game_vr.py')
        self.assertLess(s.index("print(f'SM2 VR {version_line()}"), s.index("report will be written to"))

    def test_native_report_and_full_window(self):
        s = self.read('src/native_address_finder.cpp')
        self.assertIn('\\"update\\":33,', s)
        self.assertIn('address_finder::finishNow(timedOut, categoriesDone, !bindTraceFailed.load())', s)
        h = self.read('include/spidy/native_address_finder.hpp')
        self.assertIn('maxFrames = 600 * 70;', h)


class TableBindsUpdate31(unittest.TestCase):
    """Update 31: table binds, bindless verdict, depth targets and ortho boxes in the text report."""

    def test_sections(self):
        lines = [
            {'type': 'bind', 'api': 'graphics_table', 'table': 5, 'rva': 0x10, 'function_rva': 0, 'root_slot': 1,
             'cb_offset': 0, 'cams': 'head', 'tag': 'HEAD-ONLY', 'match': 'view_proj', 'seed29': False,
             'pass': 'depth_only', 'rt_count': 0, 'dsv': True, 'viewport': [2048, 2048], 'hits': 3, 'frames': 2},
            {'type': 'bind_summary', 'rows': 1, 'cap': 96, 'dropped': 0, 'watched': 3, 'watched_cap': 256,
             'pass_hooks': 'on', 'descriptor_tables': 'on'},
            {'type': 'table_summary', 'hooks': 'on', 'table_binds': 900, 'heap_start_binds': 800,
             'bindless': 'likely', 'heap': {'descriptors': 1000000, 'increment': 32, 'known': True},
             'descriptors_mapped': 40, 'descriptors_created': 50, 'descriptors_copied': 6,
             'tables_covering_camera': 2,
             'binds_by_pass': {'graphics_table': {'unknown': 0, 'depth_only': 7, 'color': 3, 'compute': 0},
                               'compute_table': {'unknown': 0, 'depth_only': 0, 'color': 0, 'compute': 0}}},
            {'type': 'depth_target', 'format': 40, 'width': 4096, 'height': 4096, 'transitions': 9,
             'depth_only_binds': 4},
            {'type': 'ortho_box', 'rva': 0x2ccbfc0, 'record_offset': 64, 'half_m': [30, 30, 200], 'hits': 12},
        ]
        out = '\n'.join(af.shadow_pass_summary(lines))
        self.assertIn('[graphics_table] table[4] rva=0x10', out)
        self.assertIn('-> bindless: likely', out)
        self.assertIn('graphics_table: unknown=0 depth_only=7 color=3 compute=0', out)
        self.assertNotIn('compute_table:', out)
        self.assertIn('fmt=40 4096x4096 transitions=9 depth_only_binds=4', out)
        self.assertIn('rva=0x2ccbfc0 record+64 half=30x30x200 hits=12', out)
        self.assertEqual(af.table_summary_text([]), [])

    def test_native_report_lines(self):
        s = (pathlib.Path(__file__).resolve().parent.parent / 'src/native_address_finder.cpp').read_text()
        for key in ('table_summary', 'depth_target', 'ortho_box', 'bindless', 'watched_cap'):
            self.assertIn(key, s)
        h = (pathlib.Path(__file__).resolve().parent.parent / 'include/spidy/address_finder.hpp').read_text()
        self.assertIn('maxWatched = 256', h)


if __name__ == '__main__':
    unittest.main()
