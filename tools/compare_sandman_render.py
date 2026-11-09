"""Compare HEROVR render-command traces with and without Sandman crystal clouds.

In the same VR session and near the same scene, capture using the existing tool:
  python tools/probe_render_commands.py --seconds 5 --output reports/sandman-clear.json
  python tools/probe_render_commands.py --seconds 5 --output reports/sandman-cloud.json
  python tools/compare_sandman_render.py reports/sandman-clear.json reports/sandman-cloud.json

This is a passive command-family comparison, NOT an effect-material identifier.
Camera direction, simulation, LOD, and occlusion may also change command counts.
"""
import argparse
import json
import math
from pathlib import Path

OWNERS = ("other", "game_view", "left_eye", "right_eye")


def load(path):
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or type(data.get("frames")) is not int or data["frames"] <= 0:
        raise ValueError(f"{path}: invalid frame count")
    commands = data.get("commands")
    if not isinstance(commands, dict):
        raise ValueError(f"{path}: missing command table")
    for name, row in commands.items():
        if not isinstance(name, str) or not isinstance(row, list) or len(row) != 4:
            raise ValueError(f"{path}: invalid command record")
        if any(type(n) not in (int, float) or not math.isfinite(n) or n < 0 for n in row):
            raise ValueError(f"{path}: invalid command counts")
    return data


def compare(clear, cloud, threshold=0.5):
    results = []
    for name in sorted(set(clear["commands"]) | set(cloud["commands"])):
        before = clear["commands"].get(name, [0.0] * 4)
        after = cloud["commands"].get(name, [0.0] * 4)
        delta = [round(b - a, 2) for a, b in zip(before, after)]
        if max(map(abs, delta)) < threshold:
            continue
        results.append({"command": name, "without_cloud": before,
                        "with_cloud": after, "delta_per_frame": delta})
    return sorted(results, key=lambda r: (-max(map(abs, r["delta_per_frame"])), r["command"]))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("without_cloud", type=Path)
    p.add_argument("with_cloud", type=Path)
    p.add_argument("--threshold", type=float, default=0.5)
    p.add_argument("--output", type=Path)
    args = p.parse_args()
    if not math.isfinite(args.threshold) or args.threshold < 0:
        p.error("--threshold must be nonnegative and finite")
    try:
        clear, cloud = load(args.without_cloud), load(args.with_cloud)
    except (OSError, ValueError) as error:
        p.error(str(error))
    changes = compare(clear, cloud, args.threshold)
    print(f"Clear: {clear['frames']} frames; cloud: {cloud['frames']} frames")
    print("Command differences (cloud minus clear; commands per frame):")
    print(f"{'command':46s} " + " ".join(f"{owner:>11}" for owner in OWNERS))
    for item in changes:
        print(f"{item['command'][:46]:46s} " +
              " ".join(f"{n:+11.2f}" for n in item["delta_per_frame"]))
    if not changes:
        print("No command-family differences exceed the threshold.")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps({"without_cloud_frames": clear["frames"],
            "with_cloud_frames": cloud["frames"], "threshold": args.threshold,
            "changes": changes}, indent=2) + "\n", encoding="utf-8")
        print(f"Report: {args.output}")
    print("Counts cannot identify an effect's shader, material or orientation transform.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
