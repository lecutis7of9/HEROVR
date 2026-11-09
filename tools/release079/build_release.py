#!/usr/bin/env python3
"""Assemble HERO VR 0.0.79 directly from the public 0.0.78 player zip.
Keep the same clean folder structure, game modules and embedded Python; overlay
all tracked 0.0.79 source files, tested compiled launcher and 0.0.79 mod pack.
Only run on the pinned, verified release build in GitHub Actions.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

RELEASE = "0.0.79"
SOURCE_ZIP = f"HERO-VR-Launcher-{RELEASE}.zip"
MOD_ZIP = f"HERO-VR-{RELEASE}-mod-win64.zip"
REPO = "lecutis7of9/HEROVR"


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def entries_with_root(archive, marker):
    names = [n for n in archive.namelist() if n.endswith(marker) and not n.endswith("/")]
    if len(names) != 1:
        raise RuntimeError(f"Expected one {marker} in archive; found {len(names)}")
    return names[0][:-len(marker)]


def overlay_zip(archive, destination, prefix, *, include=None):
    prefix = prefix.replace("\\", "/")
    for f in archive.infolist():
        if f.is_dir() or not f.filename.startswith(prefix):
            continue
        relative = f.filename[len(prefix):]
        if not relative:
            continue
        safe = Path(relative)
        if safe.is_absolute() or ".." in safe.parts:
            raise RuntimeError("Unsafe archive path: " + f.filename)
        if include and not any(relative.startswith(s) for s in include):
            continue
        target = destination / safe
        target.parent.mkdir(parents=True, exist_ok=True)
        with archive.open(f) as src, target.open("wb") as dst:
            shutil.copyfileobj(src, dst)


def build(previous, mod, exe, output, old_manifest):
    root = Path(__file__).resolve().parents[2]
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="herovr079-") as td:
        stage = Path(td) / "HV079"
        stage.mkdir()
        with zipfile.ZipFile(previous) as prev:
            prefix = entries_with_root(prev, "HERO VR Launcher.cmd")
            overlay_zip(prev, stage, prefix)

        # Distribute the exact git tree for the matching release version.
        tracked = subprocess.check_output(["git", "ls-files", "-z"], cwd=root)
        for raw in filter(None, tracked.split(b"\x00")):
            rel = Path(os.fsdecode(raw))
            if rel.is_absolute() or ".." in rel.parts:
                raise RuntimeError("Invalid source filename")
            source = root / rel
            if source.is_file():
                dst = stage / rel
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, dst)

        # From the tested full 0.0.79 package, not the previous DLLs.
        with zipfile.ZipFile(mod) as package:
            prefix = entries_with_root(package, "build/windows-ninja/spidy_stereo_probe.dll")
            overlay_zip(package, stage, prefix,
                        include=("build/windows-ninja/", "python/", "docs/licenses/"))

        built_launcher = stage / "tools/launcher/SM2VRLauncher.exe"
        built_launcher.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(exe, built_launcher)

        # Do not persist one player's settings or installation reports.
        for rel in ["live.txt", "next-launch.txt"]:
            (stage / rel).unlink(missing_ok=True)
        for d in ["reports", "launcher-data"]:
            if (stage / d).exists():
                shutil.rmtree(stage / d)

        for rel in ["HERO VR Launcher.cmd",
                    "tools/launcher/SM2VRLauncher.exe",
                    "tools/launcher/src/RendererModes.cs",
                    "build/windows-ninja/spidy_stereo_probe.dll",
                    "build/windows-ninja/renderer-modes/original/spidy_stereo_probe.dll",
                    "build/windows-ninja/renderer-modes/revamped/spidy_stereo_probe.dll",
                    "python/python.exe", "VERSION", "PORT-VERSION"]:
            if not (stage / rel).is_file():
                raise RuntimeError("Full player ZIP incomplete: " + rel)
        if (stage / "VERSION").read_text(encoding="utf-8-sig").strip() != RELEASE:
            raise RuntimeError("Wrong full-zip VERSION")
        if digest(stage / "build/windows-ninja/spidy_stereo_probe.dll") != digest(
                stage / "build/windows-ninja/renderer-modes/revamped/spidy_stereo_probe.dll"):
            raise RuntimeError("The default active DLL is not the Revamped renderer")
        if digest(stage / "build/windows-ninja/renderer-modes/original/spidy_stereo_probe.dll") == digest(
                stage / "build/windows-ninja/renderer-modes/revamped/spidy_stereo_probe.dll"):
            raise RuntimeError("Original and Revamped renderers are identical")

        full_zip = output / SOURCE_ZIP
        with zipfile.ZipFile(full_zip, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6,
                             allowZip64=True) as z:
            for path in sorted(stage.rglob("*")):
                if path.is_file():
                    z.write(path, arcname=(Path("HV079") / path.relative_to(stage)).as_posix())

    # The mod ZIP is constructed by tools/package.ps1 and checked separately.
    # The CI invokes this assembler with --mod dist/<name>.zip and --out dist,
    # where source and destination are the same file. CopyFile2 rejects
    # self-copies on Windows (WinError 32).
    if mod.resolve() != (output / MOD_ZIP).resolve():
        shutil.copy2(mod, output / MOD_ZIP)
    existing = json.loads(Path(old_manifest).read_text(encoding="utf-8-sig"))
    existing.update({
        "schema": 1,
        "name": "HERO VR",
        "version": f"Version {RELEASE}",
        "version_number": RELEASE,
        "tag": "v" + RELEASE,
        "release_date": datetime.datetime.now(datetime.timezone.utc).date().isoformat(),
        "internal_build": "39.12",
        "launcher_build": 22,
        "changelog_url": f"https://github.com/{REPO}/blob/v{RELEASE}/RELEASE-NOTES-v{RELEASE}.md",
        "source_url": f"https://github.com/{REPO}/tree/v{RELEASE}",
    })
    base = f"https://github.com/{REPO}/releases/download/v{RELEASE}/"
    existing["files"] = [
        {"kind": kind, "name": name, "url": base + name,
         "sha256": digest(output / name), "size": (output / name).stat().st_size}
        for kind, name in (("source", SOURCE_ZIP), ("prebuilt", MOD_ZIP))
    ]
    existing["whats_new"] = [
        "Original / Revamped mode switch on the launcher's first screen",
        "Revamped 70% per-eye temporal upscaling for higher frame rate",
        "Both rendering modes bundled, with Original v0.0.78 as fallback",
        "Special Thanks: Venom Cash, Gogeta, Tim The KoolAid Lover (testers)",
        "Window lighting and story-mode fixes remain ongoing",
    ]
    (output / "version.json").write_text(json.dumps(existing, indent=2) + "\n", encoding="utf-8")
    sums = []
    for f in (output / SOURCE_ZIP, output / MOD_ZIP):
        line = f"{digest(f)}  {f.name}"
        (output / (f.name + ".sha256")).write_text(line + "\n", encoding="ascii")
        sums.append(line)
    (output / "SHA256SUMS.txt").write_text("\n".join(sums) + "\n", encoding="ascii")
    print(f"Public {RELEASE} source/player zip: {full_zip} ({full_zip.stat().st_size:,} bytes)")
    print(f"Public mod zip: {mod} ({mod.stat().st_size:,} bytes)")
    print("Public manifest and ZIP checksums written.")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--previous", required=True, type=Path)
    p.add_argument("--mod", required=True, type=Path)
    p.add_argument("--launcher", required=True, type=Path)
    p.add_argument("--old-manifest", required=True, type=Path)
    p.add_argument("--out", required=True, type=Path)
    args = p.parse_args()
    build(args.previous, args.mod, args.launcher, args.out, args.old_manifest)


if __name__ == "__main__":
    main()
