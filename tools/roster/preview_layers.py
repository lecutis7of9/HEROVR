#!/usr/bin/env python3
"""0.0.73 test 3 PORTRAITS: the Roster model's images for spidy_hud_preview's "roster" sheet.

Reads a shipped Models/Roster/scene.glb and writes its images (in the glb's order, the order
mesh_model reads them) decoded and scaled to one size, as "RGBA w h n\\n" then the pixels, top row
first. The preview stacks them into one texture (SPIDY_ROSTER_LAYERS); nothing new is drawn here.

    python3 tools/roster/preview_layers.py <scene.glb> <out.rgba> [size]
"""
import io
import json
import struct
import sys

from PIL import Image


def images(glb: bytes):
    magic, _version, _length = struct.unpack_from("<4sII", glb, 0)
    if magic != b"glTF":
        raise ValueError("not a glb")
    at, doc, binary = 12, None, b""
    while at + 8 <= len(glb):
        size, kind = struct.unpack_from("<II", glb, at)
        chunk = glb[at + 8 : at + 8 + size]
        if kind == 0x4E4F534A:
            doc = json.loads(chunk)
        elif kind == 0x004E4942:
            binary = chunk
        at += 8 + size
    for im in doc.get("images", []):
        view = doc["bufferViews"][im["bufferView"]]
        start = view.get("byteOffset", 0)
        yield Image.open(io.BytesIO(binary[start : start + view["byteLength"]])).convert("RGBA")


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    size = int(sys.argv[3]) if len(sys.argv) > 3 else 512
    with open(sys.argv[1], "rb") as f:
        found = [im.resize((size, size), Image.LANCZOS) for im in images(f.read())]
    with open(sys.argv[2], "wb") as out:
        out.write(b"RGBA %d %d %d\n" % (size, size, len(found)))
        for im in found:
            out.write(im.tobytes())
    print(f"{len(found)} images, {size} px")
    return 0


if __name__ == "__main__":
    sys.exit(main())
