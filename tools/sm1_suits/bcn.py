"""HERO VR SM1 suits: single-colour block-compressed textures (a flat normal map in the game's own format).
Block layouts are the public DirectX ones (BC1/BC3/BC4/BC5/BC7 mode 6)."""
import struct

SRGB = {29, 72, 75, 78, 91, 93, 99}
BC1 = {70, 71, 72}
BC2 = {73, 74, 75}
BC3 = {76, 77, 78}
BC4 = {79, 80, 81}
BC5 = {82, 83, 84}
BC7 = {97, 98, 99}
BLOCK_BYTES = {}
for _f in BC1 | BC4:
    BLOCK_BYTES[_f] = 8
for _f in BC2 | BC3 | BC5 | BC7:
    BLOCK_BYTES[_f] = 16


def _bc1(r, g, b):
    c = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    return struct.pack('<HHI', c, c, 0)


def _bc4(v):
    return struct.pack('<BB', v & 0xFF, v & 0xFF) + bytes(6)


def _bc7_mode6(r, g, b, a):
    bits = []

    def put(value, n):
        for i in range(n):
            bits.append((value >> i) & 1)
    put(1 << 6, 7)  # mode 6
    for c in (r, g, b, a):
        put(c >> 1, 7)
        put(c >> 1, 7)
    put(r & 1, 1)  # p-bits (one shared lsb; the colour channels then round to within 1)
    put(r & 1, 1)
    put(0, 63)  # every texel index 0 (anchor index 3 bits, the rest 4)
    out = bytearray(16)
    for i, bit in enumerate(bits):
        out[i >> 3] |= bit << (i & 7)
    return bytes(out)


def constant_block(fmt, rgba):
    """One 4x4 block of `rgba` (0..255 each) in DXGI format `fmt`, or None if not supported."""
    r, g, b, a = rgba
    if fmt in BC1:
        return _bc1(r, g, b)
    if fmt in BC2:
        return bytes([0xFF if a >= 128 else 0x00]) * 8 + _bc1(r, g, b)
    if fmt in BC3:
        return _bc4(a) + _bc1(r, g, b)
    if fmt == 81:  # BC4_SNORM
        return _bc4(0 if r == 128 else (r - 128) & 0xFF)
    if fmt in BC4:
        return _bc4(r)
    if fmt == 84:  # BC5_SNORM: 0 is the middle
        return _bc4(0) + _bc4(0) if (r, g) == (128, 128) else _bc4((r - 128) & 0xFF) + _bc4((g - 128) & 0xFF)
    if fmt in BC5:
        return _bc4(r) + _bc4(g)
    if fmt in BC7:
        return _bc7_mode6(r, g, b, a)
    return None


def flat_normal(fmt):
    """A flat (straight-up) normal map texel block for format `fmt`: x = y = 0.5, z = 1, alpha 0.5 so a
    swizzled (x in alpha) map reads flat too."""
    return constant_block(fmt, (128, 128, 255, 128))


def decode_bc1_block(blk):
    """For tests: the 16 RGBA texels of one BC1 block."""
    c0, c1, idx = struct.unpack('<HHI', blk)

    def rgb(c):
        return ((c >> 11) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31)
    p0, p1 = rgb(c0), rgb(c1)
    if c0 > c1:
        pal = [p0, p1, tuple((2 * x + y) // 3 for x, y in zip(p0, p1)), tuple((x + 2 * y) // 3 for x, y in zip(p0, p1))]
        pal = [p + (255,) for p in pal]
    else:
        pal = [p0 + (255,), p1 + (255,), tuple((x + y) // 2 for x, y in zip(p0, p1)) + (255,), (0, 0, 0, 0)]
    return [pal[(idx >> (2 * i)) & 3] for i in range(16)]


def decode_bc7_mode6_block(blk):
    """For tests: texel 0 of a BC7 mode-6 block."""
    v = int.from_bytes(blk, 'little')
    assert v & 0x7F == 0x40, 'not mode 6'
    p = 7
    e = []
    for _ in range(8):
        e.append((v >> p) & 0x7F)
        p += 7
    p0 = (v >> p) & 1
    return tuple(((e[2 * c] << 1) | p0) for c in range(4))
