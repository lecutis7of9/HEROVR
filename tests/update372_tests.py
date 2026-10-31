# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 37.2: the start-up defaults (EYE AA, FAR SHADOWS EYES+CACHE, sharpen 140 must start ON, on a fresh
install and over an existing live.txt), the launcher defaults, every export the launcher asks for, and the
37.2 telemetry records. Any of these failing fails the build."""
import pathlib
import re
import struct
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as ls  # noqa: E402
import update37_records as rec  # noqa: E402

SRC = '\n'.join(p.read_text(encoding='utf-8', errors='replace') for p in sorted((ROOT / 'src').glob('*.cpp')))
LAUNCHER = (ROOT / 'tools/run_game_vr.py').read_text(encoding='utf-8')


class StartupOn(unittest.TestCase):
    def test_required_values(self):
        self.assertEqual(ls.STARTUP_ON, {'eye_temporal': 1, 'far_shadows': 2, 'sharpen': 140})  # 0.0.74 test 10 PICTURE 72: sharpen 140 again

    def test_defaults_hold_them(self):
        for k, v in ls.STARTUP_ON.items():
            self.assertEqual(ls.UPDATE37_DEFAULTS.get(k), v, f'{k} does not start at {v}')
            self.assertEqual(ls.MIGRATE_372.get(k), v, f'the migration does not set {k} = {v}')

    def test_other_defaults(self):
        d = ls.UPDATE37_DEFAULTS
        self.assertEqual((d['eye_bloom'], d['shared_lod'], d['bloom_fx']), (0, 2, 0))  # 0.0.73 lights: EYE BLOOM OFF
        for k, v in dict(wall_crawl=0, aim_steer=0, ring_face=0, wall_body=0, zip_strike=1, web_yank=1,  # Update 38.3: yank ON
                                     
                         web_bend=100, color_fx=0, foveated='off', npc_fade='off').items():
            self.assertIn(k, d, k)
            self.assertEqual(d[k], v, k)

    def test_eye_aa_start_export(self):
        self.assertEqual(ls.SETTINGS['eye_temporal'][1], 'SpidyEyeAaStart')
        self.assertEqual(ls.export_value('eye_temporal', 1), 1)
        self.assertIn('DWORD WINAPI SpidyEyeAaStart(void* on)', SRC)

    def test_far_shadows_sharpen_exports(self):
        self.assertEqual(ls.export_value('far_shadows', 2), 2)
        self.assertEqual(ls.export_value('sharpen', 140), 140)

    def test_sent_at_start(self):
        """The launcher sends UPDATE37_DEFAULTS for whatever live.txt does not keep, after the migration."""
        self.assertIn('for name, value in live_settings.UPDATE37_DEFAULTS.items()', LAUNCHER)  # 0.0.74 test 10: no IMAGE look
        self.assertLess(LAUNCHER.index('live_settings.migrate_372(ROOT/'),
                        LAUNCHER.index("kept, _ = live_settings.persisted(ROOT/'live.txt')"))

    def test_startup_line(self):
        line = ls.startup_line(dict(ls.STARTUP_ON))
        self.assertNotIn('NOT ON', line)
        self.assertIn('NOT ON', ls.startup_line({**ls.STARTUP_ON, 'eye_temporal': 0}))
        self.assertIn('NOT ON', ls.startup_line({}))
        self.assertIn('live_settings.startup_line(live_file.current)', LAUNCHER)

    def test_native_verdicts_wait_for_eyes(self):
        """The 37.1 fault: a start-up verdict judged before both eyes were known turned them off."""
        probe = (ROOT / 'src/stereo_probe.cpp').read_text(encoding='utf-8', errors='replace')
        self.assertGreaterEqual(probe.count('Update 37.2: judged only while the eyes are known'), 2)  # EYE BLOOM, SHARED LOD
        self.assertIn('Update 37.2: the game view counts toward the verdict only while the eyes are known', probe)


class Migration(unittest.TestCase):
    def run_on(self, text):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            if text is not None:
                p.write_text(text, encoding='utf-8')
            line = ls.migrate_372(p)
            return line, (p.read_text(encoding='utf-8') if p.exists() else None), p, d

    def test_fresh_install(self):
        line, text, _, _ = self.run_on(None)
        self.assertIsNone(line)
        self.assertIsNone(text)
        # nothing kept: the defaults (STARTUP_ON) are what is sent
        for k in ls.STARTUP_ON:
            self.assertIn(k, ls.UPDATE37_DEFAULTS)

    def test_existing_live_txt(self):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text(ls.HEADER + 'sharpen = 100\neye_temporal = 0\nfar_shadows = 0\nweb_width = 2\n',
                         encoding='utf-8')
            line = ls.migrate_372(p)
            self.assertIsNotNone(line)
            values, _ = ls.parse(p.read_text(encoding='utf-8'))
            self.assertEqual(values['sharpen'], 140)
            self.assertEqual(values['eye_temporal'], 1)
            self.assertEqual(values['far_shadows'], 2)
            self.assertEqual(values['web_width'], 2)
            self.assertIn(ls.MIGRATED_372, p.read_text(encoding='utf-8'))
            kept, _ = ls.persisted(p)
            for k, v in ls.STARTUP_ON.items():
                self.assertEqual(kept.get(k, ls.UPDATE37_DEFAULTS[k]), v, f'{k} would start off')
            # once only: a later choice stays
            p.write_text(p.read_text(encoding='utf-8').replace('sharpen = 140', 'sharpen = 90'), encoding='utf-8')
            self.assertIsNone(ls.migrate_372(p))
            self.assertEqual(ls.parse(p.read_text(encoding='utf-8'))[0]['sharpen'], 90)

    def test_already_right(self):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text('sharpen = 140\n', encoding='utf-8')
            self.assertIsNone(ls.migrate_372(p))
            self.assertIn(ls.MIGRATED_372, p.read_text(encoding='utf-8'))


class Settings(unittest.TestCase):
    def test_every_export_exists(self):
        names = {e for _, e, _ in ls.SETTINGS.values() if e}
        for name in sorted(names):
            self.assertRegex(SRC, r'DWORD WINAPI ' + re.escape(name) + r'\(', name)

    def test_every_record_export_exists(self):
        for key, (name, *_rest) in rec.RECORDS.items():
            self.assertRegex(SRC, r'dllexport\)?[^;\n]*\b' + re.escape(name) + r'\b', name)
        for name in ('SpidyWorldTraceData', 'SpidyBloodData', 'SpidyPerfData', 'SpidyNpcFadeData', 'SpidyColorData',
                     'SpidyFoveaData'):
            self.assertIn(name, {r[0] for r in rec.RECORDS.values()})
        self.assertIn("for export, *_ in update37_records.RECORDS.values()", LAUNCHER)

    def test_values(self):
        self.assertEqual(ls.export_value('exposure', 1.5), 35)
        self.assertEqual(ls.export_value('exposure', -2), 0)
        for name, word, code in (('color_filter', 'noir', 5), ('foveated', 'balanced', 2), ('npc_fade', 'off', 1),
                                 ('npc_fade', 'game', 0), ('npc_fade', 'switch', 2)):
            self.assertEqual(ls.export_value(name, word), code, (name, word))
        values, problems = ls.parse('exposure = +1.5 ev\ncolor_filter = golden\nfoveated = light\nnpc_fade = switch\n')
        self.assertFalse(problems)
        self.assertEqual(values['exposure'], 1.5)

    def test_launcher_defaults(self):
        self.assertIn('DEFAULT_MONITOR_RATE = (0, 240), 30', LAUNCHER)
        self.assertRegex(LAUNCHER, r"'--building-reach', type=int, default=150")
        ps1 = (ROOT / 'tools/launch-game-vr.ps1').read_text(encoding='utf-8', errors='replace')
        self.assertRegex(ps1, r'\$MonitorRate\s*=\s*30')
        self.assertRegex(ps1, r'\$BuildingReach\s*=\s*150')

    def test_port_version(self):
        # 37.2 or a later update (Update 38 keeps the 37.2 defaults)
        self.assertRegex((ROOT / 'PORT-VERSION').read_text(encoding='utf-8'), r'Update (37\.2|3[89]|[4-9]\d)')


class Records(unittest.TestCase):
    def header(self, key):
        _, magic, version, size = rec.RECORDS[key]
        return bytearray(struct.pack('<3I', magic, version, size) + bytes(size - 12))

    def test_color(self):
        b = self.header('color')
        struct.pack_into('<5I', b, 12, 1, 4, 80, 110, 120)
        struct.pack_into('<3i', b, 32, -5, 20, -10)
        struct.pack_into('<Q', b, 64, 900)  # Update 38.5: version 2 moved it from 56
        r = rec.parse_color(bytes(b))
        self.assertEqual((r['on'], r['filter'], r['exposure_ev'], r['warmth'], r['graded_pairs']),
                         (True, '2000s RAIMI', -.5, 20, 900))

    def test_fovea(self):
        b = self.header('fovea')
        struct.pack_into('<9I', b, 12, 2, 100, 0, 2, 16, 1, 0, 2000, 2000)
        struct.pack_into('<I4f', b, 112 + 24 * 2, 30, 85.0, 80.0, 90.0, 6.5)
        r = rec.parse_fovea(bytes(b))
        self.assertEqual(r['level'], 'BALANCED')
        self.assertEqual(r['per_level']['BALANCED']['avg_fps'], 85.0)
        self.assertEqual(r['per_level']['BALANCED']['gpu_ms'], 6.5)

    def test_npc_fade(self):
        b = self.header('npc_fade')
        struct.pack_into('<5I', b, 12, 1, 0, 1, 1, 1)
        struct.pack_into('<f', b, 80, 1.25)
        r = rec.parse_npc_fade(bytes(b))
        self.assertEqual((r['mode'], r['ready'], r['nearest_m']), ('OFF', True, 1.25))

    def test_bad_magic(self):
        with self.assertRaises(RuntimeError):
            rec.parse_color(b'XXXX' + bytes(76))  # Update 38.5: COLR is 80 bytes


if __name__ == '__main__':
    unittest.main()
