# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
# Made by Agent Mulder.
"""0.0.73 MP SOLO TEST: the Python Test Buddy speaks the C++ packet (src/mp_net.cpp) byte for byte, refuses
bad packets, and (when the probe is built) really connects: the game side switches from the loopback ghost
to the buddy, sees its name and suit, and goes back to loopback after the buddy's BYE."""
import json
import os
import pathlib
import socket
import struct
import subprocess
import sys
import threading
import time
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import mp_test_buddy as mb  # noqa: E402

# The C++ side's output of `spidy_mp_probe golden` (the same fixed packets; tests/mp_tests.cpp checks it).
GOLDEN = {
    # 0.0.74 test 1: 136 payload bytes (88) - the pose, the flight lean (upright) and the 0.0.73 MPSUIT LOOK tail (4c 'L', no look: zeros)
    'pose': '48564d50010102002a000000e80300000000880000032200d43c404b0000003f00000000000080be0000003f00000000cdcccc3f'
            '000000000000000000000000000000000000803f000080be0000803f000080be0000000000000000000000000000803f0000803e'
            '0000e03f000080be000000000000003f00000000d7b35d3fff007206000000000000803f000000004c000000000000000000000084e89fd9',
    'hello': '48564d50010202000100000007000000000014005445535420425544445900000000000001000000aebad988',
    'bye': '48564d50010602000300000000000000000000002421337d',
}


def find_probe():
    exe = 'spidy_mp_probe.exe' if os.name == 'nt' else 'spidy_mp_probe'
    for d in ('build/windows-ninja', 'build/core-ninja', 'build', 'build/gate', 'build/Release'):
        p = ROOT / d / exe
        if p.is_file():
            return p
    return None


def selftest_lines():
    import io
    import contextlib
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        mb.selftest()
    return dict(line.split(' ', 1) for line in buf.getvalue().splitlines())


class Golden(unittest.TestCase):
    def test_same_bytes_as_cpp(self):
        got = selftest_lines()
        for k, v in GOLDEN.items():
            self.assertEqual(got[k], v, k)
        self.assertEqual(got['fnv'], 'advanced_suit_2 4b403cd4')

    def test_probe_agrees_when_built(self):
        probe = find_probe()
        if not probe:
            self.skipTest('spidy_mp_probe not built')
        out = subprocess.run([str(probe), 'golden'], capture_output=True, text=True, timeout=30).stdout
        cpp = dict(line.split(' ', 1) for line in out.splitlines() if ' ' in line)
        for k, v in GOLDEN.items():
            self.assertEqual(cpp[k], v, k)

    def test_look_tail(self):
        # 0.0.73 MPSUIT: a buddy pose with a look decodes it; test 6's 112-byte pose (no tail) still decodes.
        p = mb.decode(bytes.fromhex(GOLDEN['pose']))
        self.assertIsNone(p['look'])
        pose = dict(hero=0, suit=255, actions=0, suit_hash=0, root=(0, 0, 0), yaw=0, head=((0, 1.6, 0), (0, 0, 0, 1)),
                    hands=(((-.2, 1, 0), (0, 0, 0, 1)), ((.2, 1, 0), (0, 0, 0, 1))), curl=(0, 0), eye=1.65,
                    look=(True, 0x101010, 0xc01010, 0xffffff))
        q = mb.decode(mb.encode(mb.POSE, mb.ROLE_BUDDY, 1, 2, mb.pose_payload(pose)))
        self.assertEqual(q['look'], (True, 0x101010, 0xc01010, 0xffffff))
        old = mb.encode(mb.POSE, mb.ROLE_BUDDY, 1, 2, mb.pose_payload(pose)[:112])
        self.assertIsNone(mb.decode(old)['look'])

    def test_decode_round_trip(self):
        p = mb.decode(bytes.fromhex(GOLDEN['pose']))
        self.assertEqual((p['type'], p['role'], p['seq'], p['sent_ms'], p['hero'], p['suit']), (mb.POSE, mb.ROLE_BUDDY, 42, 1000, 0, 3))
        self.assertEqual(p['actions'], mb.SWINGING | mb.HAND_R)
        self.assertAlmostEqual(p['yaw'], 0.5)
        self.assertAlmostEqual(p['eye'], 1.65)
        h = mb.decode(bytes.fromhex(GOLDEN['hello']))
        self.assertEqual((h['name'], h['protocol']), ('TEST BUDDY', 1))
        self.assertEqual(mb.decode(bytes.fromhex(GOLDEN['bye']))['type'], mb.BYE)

    def test_flight_lean_0074(self):
        """0.0.74 FLIGHT POSE: the lean rides in the POSE; an old 112-byte POSE reads upright; a bad lean is upright."""
        p = mb.decode(bytes.fromhex(GOLDEN['pose']))
        self.assertEqual(p['lean_up'], (0.0, 1.0, 0.0))
        pose = dict(hero=0, suit=3, actions=mb.FLYING, suit_hash=1, root=(0, 0, 0), yaw=0, head=((0, 1.6, 0), (0, 0, 0, 1)),
                    hands=(((0, 1, 0), (0, 0, 0, 1)), ((0, 1, 0), (0, 0, 0, 1))), curl=(0, 0), eye=1.65, lean_up=(0, 0, -2))
        body = mb.pose_payload(pose)
        self.assertEqual(len(body), 136)  # 0.0.74 test 1: 112 + lean 12 + LOOK 12
        up = mb.decode(mb.encode(mb.POSE, mb.ROLE_BUDDY, 1, 0, body))['lean_up']
        self.assertAlmostEqual(up[2], -1.0, places=6)
        old = mb.decode(mb.encode(mb.POSE, mb.ROLE_BUDDY, 1, 0, body[:112]))
        self.assertEqual(old['lean_up'], (0.0, 1.0, 0.0))
        with self.assertRaises(mb.BadPacket):
            mb.decode(mb.encode(mb.POSE, mb.ROLE_BUDDY, 1, 0, body[:100]))
        for bad in ((float('nan'), 1, 0), (0, -1, 0), (0, 0, 0), (0, 3, 0)):
            self.assertEqual(mb.safe_lean_up(bad), (0.0, 1.0, 0.0), bad)

    def test_puppet_states_encode_and_decode(self):
        pup = mb.Puppet()
        for i in range(400):
            pup.step(1 / 30)
            if i % 13 == 0:
                pup.next_suit()
            if i % 57 == 0:
                pup.wave = not pup.wave
            st = pup.state()
            d = mb.decode(mb.encode(mb.POSE, mb.ROLE_BUDDY, i + 1, i * 33, mb.pose_payload(st)))
            self.assertEqual(d['suit'], st['suit'])
            self.assertEqual(d['hero'], st['hero'])
            self.assertLess(abs(d['root'][0]), 0.81)
        # Every suit's hero is the folder's.
        self.assertEqual(mb.hero_of_suit('Suit_thor_1'), 9)
        self.assertEqual(mb.hero_of_suit('Suit_greenLantern_3'), 10)
        self.assertEqual(len(mb.SUITS), 23)


class Malformed(unittest.TestCase):
    def bad(self, data, why=None):
        with self.assertRaises(mb.BadPacket) as cm:
            mb.decode(data)
        if why:
            self.assertEqual(str(cm.exception), why)

    def test_short_long_and_flips(self):
        good = bytes.fromhex(GOLDEN['pose'])
        for n in range(len(good)):
            self.bad(good[:n])
        self.bad(good + b'\0' * 400, 'too long')
        for i in range(len(good) * 8):
            b = bytearray(good)
            b[i // 8] ^= 1 << (i % 8)
            self.bad(bytes(b))

    def test_version_and_values(self):
        good = bytearray(bytes.fromhex(GOLDEN['hello']))
        good[4] = 2
        self.bad(bytes(good), 'another protocol version')
        nan = dict(hero=0, suit=255, actions=0, suit_hash=0, root=(float('nan'), 0, 0), yaw=0,
                   head=((0, 1.6, 0), (0, 0, 0, 1)), hands=(((0, 1, 0), (0, 0, 0, 1)), ((0, 1, 0), (0, 0, 0, 1))),
                   curl=(0, 0), eye=1.6)
        self.bad(mb.encode(mb.POSE, 2, 1, 0, mb.pose_payload(nan)), 'bad value')
        zero = dict(nan, root=(0, 0, 0), head=((0, 1.6, 0), (0, 0, 0, 0)))
        self.bad(mb.encode(mb.POSE, 2, 1, 0, mb.pose_payload(zero)), 'bad value')
        self.bad(b'GARBAGE!' * 5, 'not a HERO VR packet')


@unittest.skipIf(find_probe() is None, 'spidy_mp_probe not built')
class Live(unittest.TestCase):
    """The probe runs a real game-side Session (with its loopback echo); the buddy connects, then leaves."""

    def test_buddy_takes_over_then_leaves(self):
        port = 39600
        probe = subprocess.Popen([str(find_probe()), 'run', str(port), '9'], stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True)
        lines = []
        reader = threading.Thread(target=lambda: lines.extend(iter(probe.stdout.readline, '')), daemon=True)
        reader.start()
        try:
            time.sleep(2.2)  # loopback first
            log = []
            args = mb.argparse.Namespace(host='127.0.0.1', port=port, name='Live Buddy', suit='Suit_thor_1',
                                         seconds=4.0, switch_every=0, no_keys=True)
            self.assertEqual(mb.run(args, out=log.append), 0)
            self.assertTrue(any(s.startswith('Connected') for s in log), log)
            probe.wait(timeout=20)
        finally:
            if probe.poll() is None:
                probe.kill()
        reader.join(timeout=5)
        probe.stdout.close()
        rows = [json.loads(l) for l in lines if l.startswith('{')]
        self.assertTrue(rows, ''.join(lines))
        loop = [r for r in rows if r['t'] < 2 and r['visible'] and not r['buddy']]
        buddy = [r for r in rows if r['buddy']]
        self.assertTrue(loop, 'no loopback ghost before the buddy:\n' + ''.join(lines))
        self.assertTrue(buddy, 'the buddy never took over:\n' + ''.join(lines))
        self.assertEqual(buddy[-1]['name'], 'LIVE BUDDY')
        self.assertEqual(buddy[-1]['hero'], 9)
        self.assertEqual(buddy[-1]['suit'], mb.SUITS.index('Suit_thor_1'))
        self.assertTrue(any(r['visible'] for r in buddy))
        after = [r for r in rows if r['t'] >= 7.5]
        self.assertTrue(after and not after[-1]['buddy'], 'not back to loopback after BYE:\n' + ''.join(lines))


if __name__ == '__main__':
    unittest.main()
