"""0.0.76 SM2 NATIVE WEBS: the game's own swing rope on Spider-Man 2 (values from SM2VR 0.0.27, CANDIDATE)."""
import os
import re
import struct
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read(path):
    with open(os.path.join(ROOT, path), encoding='utf-8') as f:
        return f.read()


EXPECTED = {
    0xaaac90: '48895c24105556574883ec50448b81bc',  # update (SM1 676dd0)
    0xaac730: '48895c2410564883ec20488bf1458bd1',  # ensure (SM1 677d20)
    0xab5180: '0fb7c23b41587355488b49504c69c818',  # target (SM1 67d7c0)
    0xab2590: '48895c2408574883ec20448b1a410fb6',  # release (SM1 67b610)
    0xab2740: '0fb7c23b4158732f4c69c8180a00004c',  # get (SM1 67b7b0)
    0x10ebce0: '488bc44c894820c5fa11501855535648',  # hand points (SM1 95f9c0)
}


class Sm2NativeWebsTests(unittest.TestCase):
    def setUp(self):
        self.src = read('src/native_webs.cpp')

    def test_no_longer_gated_as_not_ported(self):
        start = self.src[self.src.index('uint32_t native_webs::start('):]
        start = start[:start.index('void native_webs::retarget')]
        self.assertNotIn('notPorted', start)
        self.assertNotIn('websNotPorted', start)

    def test_no_remastered_addresses_left_in_code(self):
        code = '\n'.join(line.split('//')[0] for line in self.src.splitlines())
        for sm1 in ('0x676dd0', '0x677d20', '0x67d7c0', '0x67b610', '0x67b7b0', '0x95f9c0', '0x38b3df8',
                    '0x7a436e8', '0x7a43704', '0x7d40', '0x7d60', '0x7c8', '0x6c0', '0x70c', '0x71c'):
            self.assertNotIn(sm1, code, sm1)

    def test_sm2_values_present(self):
        for v in ('0xaaac90', '0xaac730', '0xab5180', '0xab2590', '0xab2740', '0x10ebce0', '0x53a1940',
                  '0xa18', '0x854', '0x8c0', '0x8cc', '0x8dc', '0xc2d2d08', '0xc2d2d24',
                  '0x1d8, 0x204, 0x230, 0x25c', '0x288, 0x30c, 0x390, 0x414'):
            self.assertIn(v, self.src, v)

    def test_check_bytes_are_sm2vr_bytes(self):
        names = {'updateBytes': 0xaaac90, 'ensureBytes': 0xaac730, 'targetBytes': 0xab5180,
                 'releaseBytes': 0xab2590, 'getBytes': 0xab2740, 'handPointsBytes': 0x10ebce0}
        for name, rva in names.items():
            m = re.search(r'\b' + name + r'\[\] = \{([^}]*)\}', self.src)
            self.assertIsNotNone(m, name)
            got = bytes(int(x, 16) for x in m.group(1).replace('\n', ' ').split(','))
            self.assertEqual(got.hex(), EXPECTED[rva], name)

    def test_bytes_match_the_game_when_present(self):
        exe = '/workspace/sm2exe/Spider-Man2.exe'
        if not os.path.exists(exe):
            self.skipTest('no Spider-Man2.exe here')
        try:
            import pefile
        except ImportError:
            self.skipTest('no pefile')
        pe = pefile.PE(exe, fast_load=True)
        for rva, hexbytes in EXPECTED.items():
            self.assertEqual(pe.get_data(rva, 16).hex(), hexbytes, hex(rva))

    def test_only_spider_man_goes_to_game_ropes(self):
        g = read('src/game_xr.cpp')
        self.assertIn('if (websStarted && motion.active && !flatScreen && gameWebsOnly()) {', g)
        self.assertIn('const bool gameWeb = gameWebsDrawn &&', g)


if __name__ == '__main__':
    unittest.main()
