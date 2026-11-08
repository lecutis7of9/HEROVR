#!/usr/bin/env python3
"""Analyze HEROVR's bounded per-eye camera-history CSV without a headset.

Usage: py -3 tools/analyze_camera_history.py herovr-camera-history-1234.csv
This inspects CPU view-descriptor camera positions; it does not inspect GPU b0.
"""
import csv
import math
import sys


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def main(path):
    with open(path, newline="", encoding="utf-8-sig") as f:
        rows = list(csv.DictReader(f))
    by_eye = {0: {}, 1: {}}
    for r in rows:
        eye = int(r["eye"])
        if eye not in by_eye:
            continue
        frame = int(r["frame"])
        by_eye[eye][frame] = {
            "now": tuple(float(r[f"current_{a}"]) for a in "xyz"),
            "prev": tuple(float(r[f"previous_{a}"]) for a in "xyz"),
            "lens": tuple(float(r[f"lens_{a}"]) for a in ("left", "right", "top", "bottom")),
            "jitter": (float(r["jitter_x"]), float(r["jitter_y"])),
        }
    issues = []
    for eye, frames in by_eye.items():
        previous = None
        checked = 0
        for frame, sample in sorted(frames.items()):
            if previous and previous[0] == frame - 1:
                checked += 1
                error = dist(sample["prev"], previous[1]["now"])
                if error > 0.005:
                    issues.append(f"eye {eye}, frame {frame}: previous camera differs from preceding current by {error:.5f} (game units)")
            previous = frame, sample
        print(f"Eye {eye}: {len(frames)} frames; {checked} consecutive-frame comparisons")
    paired = sorted(set(by_eye[0]) & set(by_eye[1]))
    for frame in paired:
        left, right = by_eye[0][frame], by_eye[1][frame]
        current = dist(left["now"], right["now"])
        old = dist(left["prev"], right["prev"])
        if current > 0.01 and old < 0.001:
            issues.append(f"frame {frame}: previous eye baseline collapsed ({old:.5f} vs current {current:.5f})")
    print(f"Paired frames: {len(paired)}; warnings: {len(issues)}")
    for warning in issues[:30]:
        print("WARNING:", warning)
    if len(issues) > 30:
        print(f"... and {len(issues) - 30} additional warnings")
    if not issues:
        print("No CPU camera-position mismatch found. Shader motion vectors and other causes remain unverified.")
    return 1 if issues else 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Usage: py -3 tools/analyze_camera_history.py herovr-camera-history-1234.csv")
    try:
        sys.exit(main(sys.argv[1]))
    except (OSError, ValueError, KeyError) as exc:
        sys.exit(f"Cannot analyze CSV: {exc}")
