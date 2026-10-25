#!/usr/bin/env python3
"""Writes version.json (release manifest, schema 1) plus a <zip>.sha256 file for each zip, for a GitHub Release.

    python make_version_json.py --repo owner/sm2-vr --port-root C:\\sm2-port ^
        --update-zip dist\\sm2-vr-0.0.1.zip --source-zip dist\\sm2-full-0.0.1.zip --out dist\\version.json

Versions are semver. The first published release is Version 0.0.1 (internal build 38.5), tag v0.0.1; each later
release bumps it (0.0.2, 0.0.3, ...; see PUBLISHING.md). The version is read from the mod folder:
    VERSION       "0.0.1"                        read first
    PORT-VERSION  "Version 0.0.1 (build 38.5)"   used when there is no VERSION; the semver is parsed out
A legacy "Update NN.N" PORT-VERSION (installs from before 0.0.1) is refused: add a VERSION file.

Upload version.json, the zips and the .sha256 files to the release (see PUBLISHING.md). The SM2 VR Launcher and
third-party launchers read https://github.com/<repo>/releases/latest/download/version.json.
Python 3.8+, standard library only. tools/make-version-json.ps1 does the same on Windows without Python.
"""
import argparse, datetime, hashlib, json, os, re, sys, zipfile

LAUNCH_ARGS = ["-RenderScale", "100", "-OverlayWebs", "-EngineTaa", "-SmallerDesktopView"]
CREDITS = [
    {"name": "Ilya Mezerowsky", "role": "Spidy (SpidyVR), the VR mod this is built on",
     "url": "https://github.com/IlyaMez/SpidyVR", "support": "https://ko-fi.com/ilyamezerowsky"},
    {"name": "Sebi (sjrl2000)", "role": "SpidyVR Linux (Steam Proton) guide, pull request 7", "url": "https://github.com/sjrl2000"},
    {"name": "iplusdvr", "role": "SpidyVR pull requests 21, 22 and 23", "url": "https://github.com/iplusdvr"},
    {"name": "Adrian (AdrianGzzEsc)", "role": "SpidyVR pull request 24", "url": "https://github.com/AdrianGzzEsc"},
    {"name": "Blazeventura and ANTIOPSVR", "role": "Blaze's Spider-Man for Blade & Sorcery, swinging inspiration",
     "url": "https://www.nexusmods.com/bladeandsorcery/mods/12564"},
    {"name": "SM2VR 0.0.27", "role": "Marvel's Spider-Man 2 game addresses used by the port"},
]


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


SEMVER = re.compile(r"(?<![\d.])v?(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
                    r"(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?![\d.])")
BUILD_NOTE = re.compile(r"\(\s*build\s+([0-9][0-9A-Za-z.\-]*)\s*\)", re.I)


def first_line(text):
    for line in text.splitlines():
        line = line.strip().lstrip("\ufeff")
        if line:
            return line
    return None


def read_first_line(path):
    if not os.path.isfile(path):
        return None
    with open(path, encoding="utf-8-sig") as f:
        return first_line(f.read())


def parse_semver(text):
    """'0.0.1', 'v0.0.1', 'Version 0.0.1 (build 38.5)' -> ('0.0.1', '38.5' or None). None for legacy 'Update 38.5'."""
    if not text:
        return None
    note = BUILD_NOTE.search(text)
    rest = BUILD_NOTE.sub("", text).strip()
    if re.match(r"^\s*update\b", rest, re.I):
        return None
    m = SEMVER.search(rest)
    if not m:
        return None
    number = "%s.%s.%s" % m.group(1, 2, 3) + ("-" + m.group(4) if m.group(4) else "")
    return number, (note.group(1) if note else None)


def mod_version(version_line, port_line, where):
    """VERSION first, then the semver in PORT-VERSION. Returns (number, build note)."""
    v = parse_semver(version_line)
    p = parse_semver(port_line)
    if v:
        return v[0], v[1] or (p[1] if p else None)
    if p:
        return p
    if version_line or port_line:
        raise SystemExit("%s: %r is not a semver version (a legacy \"Update NN.N\" from before 0.0.1?). "
                         "Put the release version in VERSION, e.g. 0.0.1." % (where, version_line or port_line))
    raise SystemExit("%s: no VERSION or PORT-VERSION" % where)


def zip_version(path):
    """(number, note) from VERSION / PORT-VERSION inside a zip (top folder or one wrapper folder), or None."""
    with zipfile.ZipFile(path) as z:
        names = {n.replace("\\", "/"): n for n in z.namelist()}
        for prefix in [""] + sorted({n.split("/")[0] + "/" for n in names if "/" in n}):
            lines = {}
            for fname in ("VERSION", "PORT-VERSION"):
                n = names.get(prefix + fname)
                if n is not None:
                    lines[fname] = first_line(z.read(n).decode("utf-8-sig", "replace"))
            if lines:
                v, p = parse_semver(lines.get("VERSION")), parse_semver(lines.get("PORT-VERSION"))
                return v or p or ("legacy", lines.get("PORT-VERSION") or lines.get("VERSION"))
    return None


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--repo", required=True, help="owner/name of the GitHub repository")
    p.add_argument("--tag", help="release tag (default and required form: v<version>, e.g. v0.0.1)")
    p.add_argument("--version", help='semver, e.g. 0.0.1 or "Version 0.0.1" (default: VERSION, then PORT-VERSION)')
    p.add_argument("--build-note", help='internal build, e.g. 38.5 (default: "(build NN.N)" in PORT-VERSION)')
    p.add_argument("--port-root", default=".", help="mod folder with VERSION / PORT-VERSION (default: current folder)")
    p.add_argument("--update-zip", help="sm2-vr-<version>.zip (expand over an install, then build)")
    p.add_argument("--source-zip", help="full source zip (a complete, buildable install; best for reverting)")
    p.add_argument("--package-zip", help="prebuilt SM2-VR-<ver>-win64.zip from tools/package.ps1 (play only, no build)")
    p.add_argument("--date", help="release date YYYY-MM-DD (default: today, UTC)")
    p.add_argument("--min-game-version", default="2.810.0.0")
    p.add_argument("--install-subfolder", default="sm2-port", help="folder name launchers install into")
    p.add_argument("--changelog-url", help="default: the release page")
    p.add_argument("--source-url", help="default: https://github.com/<repo>")
    p.add_argument("--out", default="version.json")
    p.add_argument("--no-sha-files", action="store_true", help="do not write <zip>.sha256 files")
    a = p.parse_args(argv)

    repo = a.repo.strip().strip("/")
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repo):
        raise SystemExit("--repo must be owner/name")
    if a.version:
        parsed = parse_semver(a.version)
        if not parsed:
            raise SystemExit("--version %r is not semver (MAJOR.MINOR.PATCH, e.g. 0.0.1)" % a.version)
        number, note = parsed
    else:
        root = a.port_root
        number, note = mod_version(read_first_line(os.path.join(root, "VERSION")),
                                   read_first_line(os.path.join(root, "PORT-VERSION")), root)
    note = a.build_note or note
    version = "Version " + number
    tag = a.tag or "v" + number
    if tag != "v" + number:
        raise SystemExit("--tag %s does not match the version: use v%s" % (tag, number))
    base = "https://github.com/%s/releases/download/%s/" % (repo, tag)

    files = []
    for kind, path in (("update", a.update_zip), ("source", a.source_zip), ("package", a.package_zip)):
        if not path:
            continue
        if not os.path.isfile(path):
            raise SystemExit("not found: " + path)
        name = os.path.basename(path)
        if " " in name:
            raise SystemExit("rename %r: GitHub turns spaces in asset names into dots" % name)
        zv = zip_version(path) if zipfile.is_zipfile(path) else None
        if zv and zv[0] == "legacy":
            raise SystemExit("%s carries the legacy version %r; it must say %s (VERSION / PORT-VERSION)" % (name, zv[1], number))
        if zv and zv[0] != number:
            raise SystemExit("%s says version %s, not %s; rebuild the zip from the right folder" % (name, zv[0], number))
        if kind in ("update", "source") and number not in name:
            print("warning: %s does not have %s in its name (expected sm2-vr-%s.zip / sm2-full-%s.zip)"
                  % (name, number, number, number), file=sys.stderr)
        digest = sha256(path)
        files.append({"kind": kind, "name": name, "url": base + name, "sha256": digest, "size": os.path.getsize(path)})
        if not a.no_sha_files:
            with open(path + ".sha256", "w", encoding="ascii", newline="\n") as f:
                f.write("%s  %s\n" % (digest, name))
        print("%-8s %s  %s" % (kind, digest, name))
    if not any(f["kind"] in ("update", "source") for f in files):
        raise SystemExit("give --update-zip and/or --source-zip (launchers install those)")

    manifest = {
        "schema": 1,
        "name": "SM2 VR",
        "version": version,
        "version_number": number,
        "tag": tag,
        "release_date": a.date or datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d"),
        "min_game_version": a.min_game_version,
        "install_subfolder": a.install_subfolder,
        "files": files,
        "build": {"script": "tools/build.ps1", "args": ["-Observer"]},
        "launch": {"script": "tools/launch-game-vr.ps1", "args": LAUNCH_ARGS},
        "changelog_url": a.changelog_url or "https://github.com/%s/releases/tag/%s" % (repo, tag),
        "source_url": a.source_url or "https://github.com/" + repo,
        "license": "GPL-3.0",
        "credits": CREDITS,
    }
    if note:
        manifest["internal_build"] = note   # informational; launchers order by version_number
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print("wrote " + a.out)


if __name__ == "__main__":
    main()
