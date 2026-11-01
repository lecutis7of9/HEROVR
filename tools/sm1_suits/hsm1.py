"""HERO VR SM1 suit pack (.hsm1): one hero's suit, converted ahead of time on the build box from
Models/Suits/<suit>/scene.glb, so the launcher (embedded Python, no numpy) only has to fit it to the
game's own suit model on the player's PC.

Layout: b'HSM1', u32 version, u32 header length, the header (JSON, UTF-8), then the blobs.
Each blob is zlib-compressed and named in header['blobs'] as name -> [offset from the blobs' start, length].

Header: hero (index 0..10), name, suit (source folder), ends (19 x [x,y,z], the 16 suit joints then the
head top and both fingertips; the canonical frame: Y up, floor at 0, facing +Z, its left at +X), groups:
one per texture, each {vertices, indices, pos, nrm, uv, jnt, wgt, idx, tex} where pos/nrm are f32 x3,
uv f32 x2, jnt/wgt u8 x4 (bones of tools/suits/suit_skeleton.BONES, weights summing to 255), idx u32 x3
per triangle and tex = {width, height, mips, blob} - a BC1 (DXGI 71/72 blocks) mip chain, largest first."""
import json
import struct
import zlib

MAGIC = b'HSM1'
VERSION = 1


def write(path, header, blobs):
    out = bytearray()
    table = {}
    for name, data in blobs.items():
        z = zlib.compress(bytes(data), 9)
        table[name] = [len(out), len(z)]
        out += z
    h = dict(header)
    h['blobs'] = table
    hj = json.dumps(h, separators=(',', ':'), sort_keys=True).encode('utf-8')
    with open(path, 'wb') as f:
        f.write(MAGIC + struct.pack('<II', VERSION, len(hj)) + hj + bytes(out))


class Pack(object):
    def __init__(self, path):
        with open(path, 'rb') as f:
            data = f.read()
        if data[:4] != MAGIC:
            raise ValueError('not a HERO VR SM1 suit pack: ' + path)
        ver, hl = struct.unpack('<II', data[4:12])
        if ver != VERSION:
            raise ValueError('unsupported suit pack version %d' % ver)
        self.header = json.loads(data[12:12 + hl].decode('utf-8'))
        self._blobs = data[12 + hl:]

    def blob(self, name):
        off, ln = self.header['blobs'][name]
        return zlib.decompress(self._blobs[off:off + ln])

    def floats(self, name):
        b = self.blob(name)
        return struct.unpack('<%df' % (len(b) // 4), b)

    def u32s(self, name):
        b = self.blob(name)
        return struct.unpack('<%dI' % (len(b) // 4), b)

    def u8s(self, name):
        return bytes(self.blob(name))
