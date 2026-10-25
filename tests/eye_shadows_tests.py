"""Update 25/26: SpidyShadowData's reader (tools/eye_shadows.py) and its console line."""
import pathlib
import re
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import eye_shadows  # noqa: E402


def data(version=2, head=(0, 0, 0, 0), game=(0, (0, 0, 0, 0))):
    size = eye_shadows.SIZES[version]
    raw = bytearray(size)
    struct.pack_into('<4I', raw, 0, eye_shadows.MAGIC, version, size, 0)
    struct.pack_into('<3Q', raw, 24, 100, 5, 7)
    if version >= 2:
        struct.pack_into('<2Q2I', raw, 736, head[0], head[1], head[2], head[3])
        struct.pack_into('<Q4f', raw, 760, game[0], *game[1])
    return bytes(raw)


class Game:
    def __init__(self, raw):
        self.raw = raw

    def read(self, address, size):
        return self.raw[address:address+size]


class EyeShadows(unittest.TestCase):
    def test_sizes_match_the_dll(self):
        text = (ROOT / 'src' / 'stereo_probe.cpp').read_text(encoding='utf-8')
        self.assertIn('sizeof(ShadowData) == 784', text)
        self.assertIn('version = 2, bytes = sizeof(ShadowData)', text)
        self.assertEqual(eye_shadows.SIZES[2], 784)

    def test_both_versions(self):
        old = eye_shadows.parse(data(1))
        self.assertEqual((old['version'], old['passes'], old['covered']), (1, 100, 7))
        self.assertNotIn('head_fits', old)
        self.assertIsNone(eye_shadows.line(old))
        new = eye_shadows.parse(data(2, (90, 10, 0, 100), (50, (0.25, 0.5, 1.0, 2.0))))
        self.assertEqual((new['head_fits'], new['head_missed'], new['head_submits']), (90, 10, 100))
        self.assertEqual(new['game_delta_m'], [0.25, 0.5])
        self.assertIsNone(eye_shadows.parse(b'\0' * 784))
        self.assertIsNone(eye_shadows.parse(data(2)[:700]))

    def test_snapshot_reads_through_a_game(self):
        self.assertEqual(eye_shadows.snapshot(Game(data(2)), 0)['version'], 2)
        self.assertIsNone(eye_shadows.snapshot(Game(b''), 0))

        class Closed:
            def read(self, address, size):
                raise OSError('gone')
        self.assertIsNone(eye_shadows.snapshot(Closed(), 0))

    def test_line(self):
        # Update 26: HEAD SHADOWS is gone; only the comparison with the game view is said.
        self.assertIsNone(eye_shadows.line(eye_shadows.parse(data(2))))
        said = eye_shadows.line(eye_shadows.parse(data(2, (90, 10, 0, 100), (50, (0.25, 0.5, 1.0, 2.0)))))
        self.assertIn('2.000 m at most', said)
        self.assertNotIn('HEAD SHADOWS', said)

    def test_reported(self):
        text = (ROOT / 'tools' / 'run_game_vr.py').read_text(encoding='utf-8')
        self.assertIn("'SpidyShadowData'", text)
        self.assertTrue(re.search(r"eye_shadows=shadow_snapshot\(game, xr\['SpidyShadowData'\]\) or shadows_last", text))
        # Update 26: the report written when the game is closed has it too (the October 23 one did not).
        self.assertIn('eye_shadows=shadows_last,', text)
        self.assertIn("shadows_last = shadow_snapshot(game, xr['SpidyShadowData']) or shadows_last", text)

    def test_head_shadows_gone(self):
        # Update 26: no switch that cannot work.
        for name in ('src/stereo_probe.cpp', 'src/native_eye_gpu.cpp', 'src/game_xr.cpp', 'src/wrist_hud.cpp',
                     'include/spidy/eye_debug.hpp'):
            text = (ROOT / name).read_text(encoding='utf-8')
            self.assertNotIn('SpidyHeadShadows', text, name)
            self.assertNotIn('eyeDebugHeadShadows', text, name)


if __name__ == '__main__':
    unittest.main()
