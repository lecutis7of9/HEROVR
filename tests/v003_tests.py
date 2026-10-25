"""Version 0.0.3: the V003 record (v003.hpp) and its report lines, the release's version, and the
new settings' defaults (every experimental feature OFF)."""
import pathlib
import re
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import update37_records as rec  # noqa: E402


def blank(**fields):
    raw = bytearray(5200)
    struct.pack_into('<3I', raw, 0, 0x33303056, 1, 5200)
    for offset, (fmt, value) in fields.items():
        struct.pack_into(fmt, raw, int(offset[1:]), *value)
    return bytes(raw)


class V003Record(unittest.TestCase):
    def test_entry_matches_header(self):
        self.assertEqual(rec.RECORDS['v003'], ('SpidyV003Data', 0x33303056, 1, 5200))
        header = (ROOT / 'include/spidy/v003.hpp').read_text()
        self.assertIn('bytes = 5200', header)
        self.assertIn('static_assert(sizeof(Data) == 5200', header)
        self.assertIn('spidy::v003::Data SpidyV003Data', (ROOT / 'src/game_xr.cpp').read_text())

    def test_parse_and_lines(self):
        raw = blank(o16=('<7I', (1, 2, 2, 1, 3, 0, 0)), o44=('<4I', (1, 2, 5, 3)), o60=('<4f', (90, 84.5, .3, 90)),
                    o76=('<3I', (250, 2, 2)), o88=('<5I', (100, 2, 2, 0, 1)), o112=('<5Q', (2, 1, 0, 1, 0)),
                    o152=('<2f4I', (12.5, 70.0, 0, 1, 1, 5)), o584=('<5Q', (4, 3, 1, 2, 1)),
                    o688=('<6Q', (2, 2, 1, 1, 0, 3)), o744=('<2I', (17, 1)), o752=('<4Q', (40, 0, 1, 1)))
        d = rec.parse_v003(raw)
        self.assertEqual(d['values']['rope_grab'], 'ON')
        self.assertEqual(d['values']['wall_stick'], 'ALWAYS')
        self.assertEqual(d['values']['perf_target'], '90')
        ap = d['auto_perf']
        self.assertEqual(ap['rung'], 'FAR WINDOWS GAME')
        self.assertEqual(ap['user'], dict(far_windows_percent=250, shared_lod=2, far_shadows=2))
        self.assertEqual(ap['log'][0]['to'], 'FAR WINDOWS 1.5X')
        self.assertEqual(ap['log'][0]['why'], 'frames missed')
        self.assertEqual(d['rope_grab']['attaches'], 4)
        self.assertEqual(d['wall_stick']['floors'], 1)
        self.assertEqual(d['suit_watch']['suit_id'], 17)
        lines = rec.console_lines({'v003': d})
        text = '\n'.join(lines)
        self.assertIn('Version 0.0.3 AUTO PERFORMANCE', text)
        self.assertIn('CANDIDATE', text)
        self.assertIn('the enemy is not moved by the mod', text)
        self.assertIn("not the game's takedown", text)

    def test_wrong_size_or_writing(self):
        self.assertIsNone(rec.parse_v003(b'\0' * 64))
        raw = bytearray(blank())
        struct.pack_into('<Q', raw, 5192, 3)  # odd sequence: being written
        self.assertIsNone(rec.parse_v003(bytes(raw)))
        bad = bytearray(blank())
        struct.pack_into('<I', bad, 4, 9)
        with self.assertRaises(RuntimeError):
            rec.parse_v003(bytes(bad))


class V003Release(unittest.TestCase):
    def test_version(self):
        # 0.0.3 was build Update 38.7 (0.0.4's bump moved VERSION on; the changelog keeps it).
        self.assertIn('**Version 0.0.3 (build Update 38.7', (ROOT / 'docs/CHANGELOG.md').read_text(encoding='utf-8'))

    def test_experimental_defaults_off(self):
        self.assertEqual(rec.V003_OPTIONS[0], ('rope_grab', ('OFF', 'ON')))
        self.assertEqual(rec.V003_OPTIONS[2][1][0], 'OFF')
        self.assertEqual(rec.V003_OPTIONS[3], ('auto_perf', ('OFF', 'ON')))

    def test_replay_and_perf_data(self):
        rows = [l for l in (ROOT / 'tests/replay/perf_fps.csv').read_text().splitlines() if l and l[0] != '#']
        self.assertGreaterEqual(len(rows), 100)
        self.assertTrue(all(re.fullmatch(r'(run|rep)\w+,[\d.]+,[\d.]+', r) for r in rows))
        replay = (ROOT / 'tests/replay_tests.cpp').read_text()
        self.assertIn('ropeGrab();', replay)
        self.assertIn('autoPerf();', replay)


if __name__ == '__main__':
    unittest.main()
