"""HERO VR 0.0.78 PBR MATERIALS: decodes pbr_preview's cache misses (<hash>.enc -> <hash>.rgba: width, height, RGBA8)."""
import pathlib
import struct
import sys

from PIL import Image


def main(cache):
    for enc in pathlib.Path(cache).glob('*.enc'):
        im = Image.open(enc).convert('RGBA')
        enc.with_suffix('.rgba').write_bytes(struct.pack('<II', *im.size) + im.tobytes())
        enc.unlink()


if __name__ == '__main__':
    main(sys.argv[1])
