#!/usr/bin/env python3
"""Offline Spider-Man 2 G-buffer camera constant-buffer validator.

Input: JSON array of captures, each with frame (integer), eye ("left"/"right"),
and b0 (576-byte buffer encoded as hex or base64). This does NOT require VR.
Use a GPU capture to obtain the game's b0 (36 float4) for the motion-vector
G-buffer pass. Do not confuse shader register b0 with a CPU address.

Example:
  py -3 tools/check_motion_vectors.py gbuffer_capture.json
"""
import argparse
import base64
import json
import math
import struct
import sys

ROWS = 36
SIZE = ROWS * 16


def decode(value):
    if isinstance(value, list):
        if len(value) != ROWS * 4:
            raise ValueError("b0 float list must contain 144 numbers")
        return [float(x) for x in value]
    if not isinstance(value, str):
        raise ValueError("b0 must be hex, base64, or 144 floats")
    if value.startswith("hex:"):
        raw = bytes.fromhex(value[4:])
    elif value.startswith("base64:"):
        raw = base64.b64decode(value[7:], validate=True)
    else:
        raise ValueError("prefix encoded b0 with hex: or base64:")
    if len(raw) != SIZE:
        raise ValueError(f"b0 must contain exactly {SIZE} bytes, got {len(raw)}")
    return struct.unpack("<144f", raw)


def row(v, n):
    return tuple(v[n * 4:n * 4 + 4])


def distance(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def matrix(v, start):
    return tuple(v[start * 4:(start + 4) * 4])


def inspect(items, tolerance):
    issues = []
    frames = {}
    previous = {}
    for item in sorted(items, key=lambda x: (int(x["frame"]), x["eye"])):
        frame = int(item["frame"])
        eye = item["eye"].lower()
        if eye not in ("left", "right"):
            raise ValueError(f"unexpected eye: {eye}")
        v = decode(item["b0"])
        if not all(math.isfinite(x) for x in v):
            issues.append(f"frame {frame} {eye}: non-finite b0 values")
            continue
        cam = row(v, 3)[:3]
        prev_cam = row(v, 15)[:3]
        now = matrix(v, 4)
        before = matrix(v, 8)
        inv_size = row(v, 34)
        if inv_size[0] <= 0 or inv_size[1] <= 0:
            issues.append(f"frame {frame} {eye}: invalid inverse render size {inv_size[:2]}")
        if frame in previous.get(eye, {}):
            pass
        old = previous.get(eye)
        if old and old["frame"] == frame - 1:
            drift = distance(prev_cam, old["cam"])
            if drift > tolerance:
                issues.append(f"frame {frame} {eye}: previous camera differs from prior current camera by {drift:.6f}m")
            # Previous camera-relative projection may change coordinate origins;
            # do not compare raw matrices across frames as if their origins match.
        previous[eye] = {"frame": frame, "cam": cam, "now": now}
        frames.setdefault(frame, {})[eye] = {
            "cam": cam, "prev_cam": prev_cam, "now": now,
            "before": before, "inv_size": inv_size[:2],
        }
    for frame, pair in sorted(frames.items()):
        if "left" not in pair or "right" not in pair:
            continue
        a, b = pair["left"], pair["right"]
        baseline = distance(a["cam"], b["cam"])
        if baseline < 0.01:
            issues.append(f"frame {frame}: eyes have near-identical current camera positions ({baseline:.6f}m)")
        if a["before"] == b["before"]:
            issues.append(f"frame {frame}: both eyes share identical previous view-projection matrices")
        if a["now"] == b["now"]:
            issues.append(f"frame {frame}: both eyes share identical current view-projection matrices")
        if a["inv_size"] != b["inv_size"]:
            issues.append(f"frame {frame}: eyes use different inverse render sizes")
        print(f"frame {frame}: eye separation {baseline:.5f}m; previous camera separation "
              f"{distance(a['prev_cam'], b['prev_cam']):.5f}m")
    print(f"Checked {len(items)} G-buffer view buffers across {len(frames)} frames; {len(issues)} warnings.")
    for issue in issues:
        print("WARNING:", issue)
    if not issues:
        print("No tested mismatch detected. This does not prove motion vectors are correct.")
    return bool(issues)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", help="JSON list of frame/eye/b0 captures")
    parser.add_argument("--position-tolerance", type=float, default=0.005,
                        help="metres of tolerated previous/current camera mismatch")
    args = parser.parse_args()
    with open(args.capture, encoding="utf-8") as f:
        items = json.load(f)
    if not isinstance(items, list):
        parser.error("capture must contain a JSON array")
    return int(inspect(items, args.position_tolerance))


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, struct.error) as exc:
        sys.exit(f"error: {exc}")
