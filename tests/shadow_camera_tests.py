# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 31: SHADOW CAMERA's record (tools/shadow_camera.py), its HUD wiring and live.txt key.
Update 32: the record's FitViews; SHADOW CAMERA left the hologram and live.txt."""
import pathlib
import re
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import shadow_camera as sc  # noqa: E402
import live_settings as live  # noqa: E402


def raw(mode=0, reason=0, toggles=0, applied=(0, 0, 0), fits=(0, 0, 0), events=(), fit_hook=1, seq=2,
        samples=0, counts=(0, 0), head=(0, 0, 0), near=.02, extent=(), centre=()):
    b = bytearray(sc.SIZE)
    struct.pack_into('<4I', b, 0, sc.MAGIC, sc.VERSION, sc.SIZE, mode)
    struct.pack_into('<q', b, 16, seq)
    struct.pack_into('<2I', b, 24, reason, toggles)
    struct.pack_into('<3Q', b, 32, *applied)
    struct.pack_into('<3Q', b, 56, *fits)
    struct.pack_into('<2I', b, 80, len(events), fit_hook)
    for i, (ms, m, r, frames) in enumerate(events):
        struct.pack_into('<Q2IQ', b, sc.EVENTS_AT + (i % sc.MAX_EVENTS) * 24, ms, m, r, frames)
    struct.pack_into('<Q2I2I', b, sc.GAME_AT, samples, *counts, 1, 0)
    struct.pack_into('<3f', b, sc.GAME_AT + 24, *head)
    struct.pack_into('<f', b, sc.GAME_AT + 36, near)
    for i, e in enumerate(extent):
        struct.pack_into('<3f', b, sc.GAME_AT + 40 + 12 * i, *e)
    for i, c in enumerate(centre):
        struct.pack_into('<3f', b, sc.GAME_AT + 88 + 12 * i, *c)
    return bytes(b)


class Record(unittest.TestCase):
    def test_layout_matches_native(self):
        h = (ROOT / 'include/spidy/shadow_camera.hpp').read_text()
        self.assertIn('sizeof(Data) == 992 + sizeof(FitViews)', h)
        self.assertIn('sizeof(FitSample) == 96', h)
        self.assertIn('32 + 64 + 64 + 32 + 16 + 32 * 96', h)
        self.assertEqual(sc.SIZE, 992 + 3280)
        self.assertIn('sizeof(Event) == 24', h)
        self.assertIn('sizeof(GameCascades) == 136', h)
        self.assertIn('0x4d414353', h)
        self.assertIn(f'maxEvents = {sc.MAX_EVENTS}', h)

    def test_names_and_reasons_match_native(self):
        h = (ROOT / 'include/spidy/shadow_camera.hpp').read_text()
        self.assertEqual(re.findall(r'case lensRejected:\s*return "([^"]+)";', h), [sc.REASONS[1]])
        self.assertEqual(re.findall(r'case nearOutOfRange:\s*return "([^"]+)";', h), [sc.REASONS[2]])
        for name in sc.MODES.values():
            self.assertIn(f'"{name}"', h)

    def test_toggle_log_segments_and_lines(self):
        ev = [(1000, 1, 0, 500), (16000, 2, 0, 900), (31000, 0, 0, 880), (40000, 0, 1, 10)]
        d = sc.parse(raw(mode=0, reason=1, toggles=4, applied=(500, 900, 880), events=ev,
                         samples=99, counts=(3, 0), head=(0, 10, 0), extent=[(20, 20, 300), (60, 60, 300)],
                         centre=[(0, 10, 15), (0, 10, 40)]))
        self.assertEqual(d['mode'], 'OFF')
        self.assertEqual(d['reverted'], sc.REASONS[1])
        self.assertEqual(d['applied_frames'], {'OFF': 500, 'STOCK NEAR': 900, 'FLAT CAMERA': 880})
        self.assertEqual(d['game_cascades']['counts'], [3, 0])
        self.assertEqual(d['game_cascades']['cascades'][0], {'half_m': [20, 20, 300], 'centre_from_head_m': 15})
        self.assertEqual(d['game_cascades']['status'], 'candidates')
        seg = sc.segments(d)
        self.assertEqual(seg[0], {'mode': 'STOCK NEAR', 'seconds': 15.0, 'frames': 900})
        self.assertEqual(seg[-1]['seconds'], None)
        lines, said = sc.new_lines(d, 0)
        self.assertEqual(len(lines), 4)
        self.assertEqual(said, 4)
        self.assertIn('SHADOW CAMERA: STOCK NEAR (change 1, +0.0 s', lines[0])
        self.assertIn('back to OFF on its own: ' + sc.REASONS[1], lines[3])
        self.assertEqual(sc.new_lines(d, 4), ([], 4))
        per = sc.mode_step({}, d)
        f = sc.report_fields(d, per)
        self.assertEqual(f['shadow_camera_status'], 'OFF')
        self.assertIn('OFF', f['shadow_camera']['cascades_by_mode'])

    def test_ring_wraps(self):
        ev = [(i * 10, i % 3, 0, i) for i in range(40)]
        d = sc.parse(raw(events=ev))
        self.assertEqual(len(d['log']), sc.MAX_EVENTS)
        self.assertEqual(d['log'][0]['index'], 8)
        self.assertEqual(d['log'][0]['ms'], 80)

    def test_unconfirmed_cascades_say_so(self):
        d = sc.parse(raw())
        self.assertTrue(d['game_cascades']['status'].startswith('SM2TODO'))
        self.assertEqual(sc.segments(d), [])

    def test_partial_and_mismatch(self):
        self.assertIsNone(sc.parse(raw(seq=3)))
        self.assertIsNone(sc.parse(b'\0' * 10))
        self.assertEqual(sc.report_fields(None)['shadow_camera_status'], 'unknown')
        with self.assertRaises(RuntimeError):
            sc.parse(b'\0' * sc.SIZE)


class FitViews(unittest.TestCase):
    def test_parse(self):
        b = bytearray(raw())
        at = sc.FIT_AT
        struct.pack_into('<4Q', b, at, 1, 2, 3, 4944)
        struct.pack_into('<8I', b, at + 32, 0x2dd1234, 0x2de0000, 0, 0, 0, 0, 0, 0)
        struct.pack_into('<8I', b, at + 64, 4000, 950, 0, 0, 0, 0, 0, 0)
        struct.pack_into('<8Q', b, at + 96, 0x1000, 0x2000, 0x3000, 0, 0, 0, 0, 0)
        struct.pack_into('<8I', b, at + 160, 10, 20, 30, 0, 0, 0, 0, 0)
        struct.pack_into('<2I', b, at + 192, 34, 0)
        for i in range(34):
            o = at + 208 + (i % sc.FIT_SAMPLES) * sc.FIT_SAMPLE_BYTES
            struct.pack_into('<Q', b, o, 1000 * i)
            struct.pack_into('<6f', b, o + 8, 0, 2, -4, 0, 0, 1)            # view: the flat camera
            struct.pack_into('<6f', b, o + 32, 0, 2, -4, 0, 0, 1)           # flat
            struct.pack_into('<6f', b, o + 56, 0, 1.7, 0, 0, -.7071, .7071)  # head, 45 degrees down
            struct.pack_into('<2f', b, o + 80, .1, 5000)
            struct.pack_into('<2I', b, o + 88, 1, 0x2dd1234)
        f = sc.parse(bytes(b))['fit_views']
        self.assertEqual(f['by_kind'], {'unreadable': 1, 'flat': 2, 'head': 3, 'other': 4944})
        self.assertEqual(f['callers'], [{'rva': '0x2dd1234', 'calls': 4000}, {'rva': '0x2de0000', 'calls': 950}])
        self.assertEqual(f['views'], 3)
        self.assertEqual(len(f['ring']), sc.FIT_SAMPLES)
        self.assertEqual(f['ring'][0]['ms'], 2000)
        r = f['ring'][-1]
        self.assertEqual(r['kind'], 'flat')
        self.assertEqual(r['head']['pitch_yaw'][0], -45.0)
        self.assertEqual(r['view_to_flat_m'], 0)
        self.assertAlmostEqual(r['view_to_head_m'], 4.01, places=2)

    def test_kind_names_match_native(self):
        h = (ROOT / 'include/spidy/shadow_camera.hpp').read_text()
        for k in sc.FIT_KINDS:
            self.assertIn(f'"{k}"', h)
        self.assertIn(f'fitSlots = {sc.FIT_SLOTS}, fitSamples = {sc.FIT_SAMPLES}', h)


class Wiring(unittest.TestCase):
    def read(self, rel):
        return (ROOT / rel).read_text(encoding='utf-8')

    def test_session_reads_and_reports(self):
        s = self.read('tools/run_game_vr.py')
        for needle in ("'SpidyShadowCamera'", "'SpidyShadowCameraData'", 'shadow_camera.report_fields(',
                       'shadow_camera.new_lines('):
            self.assertIn(needle, s)
        self.assertEqual(s.count('shadow_camera.report_fields('), 2)  # closed-game and normal reports

    def test_native_exports(self):
        s = self.read('src/stereo_probe.cpp')
        self.assertIn('DWORD WINAPI SpidyShadowCamera(void* mode)', s)
        self.assertIn('spidy::shadow_camera::Data SpidyShadowCameraData', s)
        self.assertIn('OutputDebugStringA(line)', s)
        g = self.read('src/game_xr.cpp')
        self.assertIn('if (result.shadowCamera)', g)
        self.assertIn('hud.shadowCamera = SpidyShadowCameraNow()', g)

    def test_live_key_retired(self):
        # Update 32: no switch that does nothing: shadow_camera is no longer a live.txt key.
        self.assertNotIn('shadow_camera', live.SETTINGS)
        self.assertNotIn('shadow_camera', live.PERSISTED)
        self.assertNotIn('shadowcamera', live.ALIASES)

    def test_hud_page_retired(self):
        w = self.read('src/wrist_hud.cpp')
        self.assertNotIn('toggleItem(Control::shadowCamera)', w)

    def test_fit_hook_records_views(self):
        s = self.read('src/stereo_probe.cpp')
        self.assertIn('fitViewSeen(view, reinterpret_cast<uintptr_t>(_ReturnAddress()))', s)
        self.assertIn('fitRefs(stock.values, head.pose.data())', s)

    def test_help_length(self):
        h = self.read('include/spidy/shadow_camera.hpp')
        help_text = re.search(r'help = "([^"]+)"', h).group(1)
        self.assertLessEqual(len(help_text), 80)


if __name__ == '__main__':
    unittest.main()
