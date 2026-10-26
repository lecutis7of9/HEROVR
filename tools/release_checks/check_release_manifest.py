#!/usr/bin/env python3
"""Regression gate (0.0.62 sweep): a release must ship the ready-made mod (players never build it).

0.0.5's first release asked players to build the mod; since then every release's version.json must name the
prebuilt mod zip for the same tag, with a SHA-256, a size and a download URL on that tag, and the asset folder
must hold that zip with the same hash, the launcher zip and SHA256SUMS.txt.
Usage: check_release_manifest.py ASSET_DIR [--tag vX.Y.Z] [--min-launcher-build N]"""
import argparse, hashlib, json, os, sys


def problems(d, tag=None, min_build=None, allow_launcher_build=False):
    out = []
    vj = os.path.join(d, 'version.json')
    if not os.path.isfile(vj):
        return ['no version.json']
    m = json.load(open(vj, encoding='utf-8'))
    t = m.get('tag', '')
    if tag and t != tag:
        out.append(f'tag {t!r} is not {tag!r}')
    if t != 'v' + str(m.get('version_number', '')):
        out.append(f"tag {t!r} does not match version_number {m.get('version_number')!r}")
    if min_build is not None and int(m.get('launcher_build_in_zip', m.get('launcher_build', 0)) or 0) < min_build:
        out.append(f"launcher build in the zip {m.get('launcher_build_in_zip', m.get('launcher_build'))} < {min_build}")
    # Update-loop gate (0.0.62, launcher 0.0.6.1): every launcher offers the same mod version again while
    # "launcher_build" is above its own build. A launcher run from outside the install is never replaced by the
    # update, so it offered 0.0.62 forever. Only a launcher-only hotfix (same version as before) may set it.
    if int(m.get('launcher_build', 0) or 0) > 0 and not allow_launcher_build:
        out.append(f"version.json sets launcher_build={m.get('launcher_build')}: launchers started from outside the install would "
                   "offer this same version on every start (update loop); a new mod version needs no launcher_build")
    pre = [f for f in m.get('files', []) if f.get('kind') == 'prebuilt']
    if len(pre) != 1:
        out.append(f'{len(pre)} prebuilt mod entries (need exactly 1): players would be asked to build')
    for f in pre:
        name = f.get('name', '')
        if not name.endswith('-mod-win64.zip'):
            out.append(f'prebuilt entry {name!r} is not a -mod-win64.zip')
        if f'/download/{t}/' not in f.get('url', ''):
            out.append(f'prebuilt URL {f.get("url")!r} is not on {t}')
        p = os.path.join(d, name)
        if not os.path.isfile(p):
            out.append(f'{name} missing from the asset folder')
            continue
        h = hashlib.sha256(open(p, 'rb').read()).hexdigest()
        if h.lower() != str(f.get('sha256', '')).lower():
            out.append(f'{name}: sha256 {h[:16]} != manifest {str(f.get("sha256"))[:16]}')
        if int(f.get('size', -1)) != os.path.getsize(p):
            out.append(f'{name}: size differs from the manifest')
    if not any(n.startswith('HERO-VR-Launcher-') and n.endswith('.zip') for n in os.listdir(d)):
        out.append('no HERO-VR-Launcher-*.zip in the asset folder')
    if not os.path.isfile(os.path.join(d, 'SHA256SUMS.txt')):
        out.append('no SHA256SUMS.txt')
    return out


if __name__ == '__main__':
    a = argparse.ArgumentParser()
    a.add_argument('dir'); a.add_argument('--tag'); a.add_argument('--min-launcher-build', type=int)
    a.add_argument('--launcher-only-hotfix', action='store_true', help='allow launcher_build (same mod version as the last release)')
    o = a.parse_args()
    p = problems(o.dir, o.tag, o.min_launcher_build, o.launcher_only_hotfix)
    for l in p:
        print('RELEASE:', l)
    print(f'release assets {o.dir}: ' + ('OK (ready-made mod listed and matching)' if not p else f'{len(p)} problem(s)'))
    sys.exit(1 if p else 0)
