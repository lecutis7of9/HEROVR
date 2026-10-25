# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 35: FAR SHADOWS' record (tools/far_shadows.py) against the native layout."""
import pathlib
import re
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import far_shadows as fs  # noqa: E402
import live_settings  # noqa: E402

HEADER = (ROOT / 'include/spidy/far_shadows.hpp').read_text()


def raw(mode=1, reason=0, hooked=1, eyes=((60, 3, 2), (60, 3, 2)), game=(3, 2), seq=4):
    b = bytearray(fs.SIZE)
    struct.pack_into('<4I', b, 0, fs.MAGIC, fs.VERSION, fs.SIZE, mode | reason << 8)
    struct.pack_into('<4I', b, 16, hooked, 2, 1 if reason else 0, reason)
    struct.pack_into('<4Q', b, 32, 120, 0, 60, 7)
    struct.pack_into('<2I', b, 64, *game)
    struct.pack_into('<4f', b, 72, 0, 0, game[0], game[0] + game[1])
    struct.pack_into('<2Q', b, 104, 120, 120)
    for i, (fits, base, far) in enumerate(eyes):
        o = fs.EYE_AT + 64 * i
        struct.pack_into('<2Q3I', b, o, fits, fits if far else 0, base, far, 2)
    struct.pack_into('<Q', b, 248, seq)
    return bytes(b)


class FarShadows(unittest.TestCase):
    def test_layout_agrees(self):
        self.assertIn('dataMagic = 0x%x' % fs.MAGIC, HEADER)
        self.assertIn('dataVersion = %d' % fs.VERSION, HEADER)
        self.assertIn('sizeof(Data) == %d' % fs.SIZE, HEADER)
        self.assertIn('sizeof(EyeFit) == 64', HEADER)

    def test_reason_texts_agree(self):
        texts = dict(re.findall(r'case (\w+): return "([^"]*)";', HEADER.split('reasonText')[1].split('help =')[0]))
        order = ['none', 'noHook', 'noFar', 'gameNone', 'slow', 'unseen']
        for i, name in enumerate(order[1:], 1):
            self.assertEqual(texts[name], fs.REASONS[i])

    def test_working(self):
        d = fs.parse(raw())
        self.assertEqual(d['mode'], 'EYES')
        self.assertTrue(d['eyes_have_far'])
        self.assertIn('working', d['status'])
        self.assertIn('eyes far cascades 2/2', fs.console_line(d))

    def test_reverted_and_writing(self):
        d = fs.parse(raw(mode=0, reason=2, eyes=((240, 3, 0), (240, 3, 0))))
        self.assertEqual(d['mode'], 'OFF')
        self.assertFalse(d['eyes_have_far'])
        self.assertIn('back on its own: the eyes still got no far shadows', fs.console_line(d))
        self.assertIsNone(fs.parse(raw(seq=5)))
        self.assertIn('N/A', fs.console_line(fs.parse(raw(hooked=0, mode=0, reason=1))))
        with self.assertRaises(RuntimeError):
            fs.parse(b'\0' * fs.SIZE)

    def test_said_key_changes(self):
        self.assertNotEqual(fs.said_key(fs.parse(raw())), fs.said_key(fs.parse(raw(mode=2))))

    def test_live_key(self):
        self.assertEqual(live_settings.SETTINGS['far_shadows'][1], 'SpidyFarShadows')


if __name__ == '__main__':
    unittest.main()
