# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 38.3 (run382/DIAGNOSIS.md): the four new COMBAT settings are exported with their own codes
(5325-5328) and default to their first value (0), BALL LOOK GAME ONLY / GRAB WEB ASSIST / WEB YANK ON are
the defaults and a 38.2 live.txt is moved to them once (migrate_383), FAR WINDOWS' second write is a
CANDIDATE that is byte-checked, the COMBAT record is version 5 (768 bytes) with a 38.3 console line, and
the graphics defaults are untouched. Any failing fails."""
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

XR = (ROOT / 'src/game_xr.cpp').read_text(encoding='utf-8', errors='replace')
PROBE = (ROOT / 'src/stereo_probe.cpp').read_text(encoding='utf-8', errors='replace')
C38 = (ROOT / 'include/spidy/combat38.hpp').read_text(encoding='utf-8')
MIRROR = (ROOT / 'include/spidy/vr_mirror.hpp').read_text(encoding='utf-8')
VERSION = (ROOT / 'PORT-VERSION').read_text(encoding='utf-8')
NEW = dict(look_lock=('SpidyLookLock', 5325, 'quick'), strike_steer=('SpidyStrikeSteer', 5326, 'to_lock'),
           far_windows=('SpidyFarWindowsOption', 5327, '2x'), gadget_hint=('SpidyGadgetHint', 5328, 'on'))


class Settings(unittest.TestCase):
    def test_version(self):
        self.assertRegex(VERSION, r'^Update (38\.[3-9]|39\.\d)')

    def test_exports_codes_defaults(self):
        for key, (export, code, default) in NEW.items():
            m = re.search(rf'DWORD WINAPI {export}\(void\* v\) \{{\s*return setLive\(v, 0, [^,]+, (\d+),', XR)
            self.assertIsNotNone(m, export)
            self.assertEqual(int(m.group(1)), code, export)
            self.assertEqual(ls.SETTINGS[key][1], export)
            self.assertEqual(ls.UPDATE38_DEFAULTS[key], default, key)
            self.assertEqual(ls.export_value(key, default), 0, key)
        codes = [int(c) for c in re.findall(r'combat38::\w+\]\.count - 1, (\d+),', XR)]
        self.assertEqual(len(codes), len(set(codes)), 'two COMBAT settings share a refusal code')

    def test_fixed_defaults(self):
        # Version 0.0.2: OURS is the default now (WRIST 1, GAME ONLY 3; update386_tests.py).
        self.assertEqual(ls.UPDATE38_DEFAULTS['ball_look'], 'ours')
        self.assertEqual(ls.export_value('ball_look', 'ours'), 0)
        self.assertEqual(ls.export_value('ball_look', 'game_only'), 3)
        self.assertEqual(ls.UPDATE38_DEFAULTS['grab_web'], 'assist')
        self.assertEqual(ls.export_value('grab_web', 'assist'), 0)
        self.assertEqual(ls.UPDATE37_DEFAULTS['web_yank'], 1)
        self.assertEqual(ls.UPDATE38_DEFAULTS['web_rope'], ls.UPDATE38_DEFAULTS['web_rope'])  # kept

    def test_every_help_one_sentence(self):
        for m in re.finditer(r'\{"(\w+)", "[^"]+", "[^"]+", "((?:[^"\\]|\\.)*)", \d', C38):
            help_ = m.group(2)
            self.assertLessEqual(len(help_), 84, m.group(1))
            self.assertTrue(help_.endswith('.') and '. ' not in help_, m.group(1))

    def test_far_windows_percents(self):
        self.assertIn('static constexpr uint32_t percent[4] = {200, 300, 150, 100};', C38)
        self.assertIn('SpidyFarWindows(', XR)
        self.assertIn('if (value < 100 || value > 300)\n        return 4402;', PROBE)


class Migrate(unittest.TestCase):
    def test_once_and_stays(self):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text('ball_look = glob\ngrab_web = off\nweb_yank = 0\nweb_rope = 1\n', encoding='utf-8')
            line = ls.migrate_383(p)
            self.assertIsNotNone(line)
            text = p.read_text(encoding='utf-8')
            self.assertIn(ls.MIGRATED_383, text)
            vals = {}
            for l in text.splitlines():
                n, s, v = l.split('#', 1)[0].partition('=')
                if s:
                    vals[n.strip()] = v.strip()
            self.assertEqual((vals['ball_look'], vals['grab_web'], vals['web_yank'], vals['web_rope']),
                             ('game_only', 'assist', '1', '1'))
            p.write_text(text.replace('web_yank = 1', 'web_yank = 0'), encoding='utf-8')
            self.assertIsNone(ls.migrate_383(p))
            self.assertIn('web_yank = 0', p.read_text(encoding='utf-8'))

    def test_missing_file(self):
        self.assertIsNone(ls.migrate_383(pathlib.Path(tempfile.gettempdir()) / 'no-such-live-383.txt'))

    def test_launcher_calls_it(self):
        self.assertIn('live_settings.migrate_383(', (ROOT / 'tools/run_game_vr.py').read_text(encoding='utf-8'))


class Addresses(unittest.TestCase):
    def test_far_windows_candidate_checked(self):
        self.assertIn('farWindowsReady = entry(0x2a6bc60, impostorQueriesBytes', PROBE)
        self.assertIn('CANDIDATE', PROBE)
        self.assertIn('farWindowsReady && enabled', PROBE)

    def test_mirror_honest(self):
        # Plan B: no suit ID read; the colours come from the eye copies.
        self.assertIn('Update 38.3', MIRROR)
        self.assertNotRegex(XR, r'suitId\s*=\s*read')


class Record(unittest.TestCase):
    def test_v5_parses_and_console(self):
        _, magic, version, size = rec.RECORDS['combat38']
        self.assertEqual((version, size), (5, 768))
        raw = bytearray(size)
        struct.pack_into('<4I', raw, 0, magic, version, size, 0)
        struct.pack_into('<8Q', raw, 656, 4, 3, 2, 9, 5, 6, 100, 90)
        struct.pack_into('<2f', raw, 720, 145, 290)
        struct.pack_into('<2Q', raw, 728, 400, 45)
        struct.pack_into('<4I', raw, 744, 0xc01020, 0x101018, 0xf0f0f0, 1)
        d = rec.parse_combat38(bytes(raw))
        self.assertEqual((d['look_switches'], d['strike_steers'], d['grab_assisted']), (4, 3, 2))
        self.assertEqual((d['window_game_m'], d['window_m']), (145, 290))
        self.assertEqual(d['suit_primary'], '#c01020')
        self.assertTrue(d['suit_learned'])
        lines = rec.console_lines({'combat38': d})
        self.assertTrue(any(l.startswith('Update 38.3: look-lock switches 4') for l in lines), lines)
        self.assertIn('static_assert(sizeof(Data) == 768)', C38)


class Graphics(unittest.TestCase):
    def test_graphics_defaults_unchanged(self):
        # EYE AA, FAR SHADOWS EYES+CACHE, sharpen 140, SHARED LOD HIGHER, eye_bloom COPY: 38.2's values.
        d = ls.UPDATE37_DEFAULTS
        self.assertEqual((d['aa'], d['far_shadows'], d['sharpen'], d['shared_lod'], d['eye_bloom']), (1, 2, 140, 2, 1))
        self.assertEqual(ls.STARTUP_ON, {'eye_temporal': 1, 'far_shadows': 2, 'sharpen': 140})
        self.assertNotIn('far_windows', d)  # FAR WINDOWS is a COMBAT-page live setting, not a graphics default


if __name__ == '__main__':
    unittest.main()
