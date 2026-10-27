"""Version 0.0.72: the private test zip (never a release).

  python tools/release072/make_test_zip.py <CI prebuilt-mod zip> <Models folder> <out zip> [commit] [build] [part MB]

Test build 5 (the user ran build 4 from a backup copy of the mod, so none of it was where he played): the zip
has ONE top folder, HERO-VR-0.0.72-test<build>/, with INSTALL-TEST-BUILD.bat (+ .ps1) and READ-ME-FIRST.txt
at its top and the mod files under payload/ (the CI zip's top folder stripped; Models/, Sounds/ - the player's
five mp3s, checked against their sha256 - THIRD-PARTY-MODELS.md, HERO-VR-TEST-BUILD.txt with the commit and
every DLL's sha256, and SHA256SUMS.txt). The installer finds the mod folder he played from, copies, checks
every file and starts that folder's own launcher. With a part size, part zips under it are written too
(<out>-partN-*.zip, the same top folder, so extracting all of them into one place makes the whole).
Fails if a required file is missing."""
import hashlib, os, sys, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import required_assets as ra  # noqa: E402


def build(ci_zip, models, out, commit='unknown', build_no='5', part_mb=0):
    files = {}
    with zipfile.ZipFile(ci_zip) as z:
        for n in z.namelist():
            if n.endswith('/'):
                continue
            rel = n.split('/', 1)[1] if '/' in n else n
            files[rel] = z.read(n)
    for root, _, names in os.walk(models):
        for f in names:
            p = os.path.join(root, f)
            files['Models/' + os.path.relpath(p, models).replace(os.sep, '/')] = open(p, 'rb').read()
    for name, (sha, _, _) in ra.USER_SOUNDS.items():
        b = open(ra.source_mp3(name), 'rb').read()
        if hashlib.sha256(b).hexdigest() != sha:
            raise SystemExit('sound changed: ' + name)
        files['Sounds/' + name] = b
    files['THIRD-PARTY-MODELS.md'] = open(os.path.join(ra.REPO, 'THIRD-PARTY-MODELS.md'), 'rb').read()
    title = 'HERO VR 0.0.72 test %s - private test build %s (not a release)' % (commit, build_no)
    marker = [title, 'version=0.0.72', 'commit=%s' % commit, 'build=%s' % build_no]
    marker += ['sha256 %s=%s' % (k, hashlib.sha256(files[k]).hexdigest()) for k in sorted(files)
               if k.lower().endswith('.dll')]
    files[ra.TEST_MARKER] = ('\r\n'.join(marker) + '\r\n').encode()
    sums = ''.join('%s  %s\r\n' % (hashlib.sha256(files[k]).hexdigest(), k) for k in sorted(files))
    files['SHA256SUMS.txt'] = sums.encode()
    top = {}
    for name in ra.INSTALLER_FILES:
        top[name] = crlf(open(os.path.join(HERE, name), 'rb').read())
    top['READ-ME-FIRST.txt'] = crlf(open(os.path.join(HERE, 'READ-ME-FIRST.txt'), 'rb').read()
                                    .replace(b'@@COMMIT@@', commit.encode()).replace(b'@@BUILD@@', str(build_no).encode()))
    entries = {**{k: v for k, v in top.items()}, **{'payload/' + k: v for k, v in files.items()}}
    missing = [f for f in ra.required_zip_files() if f not in entries]
    if missing:
        raise SystemExit('missing: %s' % missing)
    folder = ra.top_folder(build_no)
    write(out, folder, entries, sorted(entries))
    parts = []
    if part_mb:
        limit = int(float(part_mb) * 1024 * 1024)
        # part 1: the installer, the notes, the sums and the mod; then the models, biggest first, packed in
        first = [k for k in entries if not k.startswith('payload/Models/')]
        rest = sorted((k for k in entries if k.startswith('payload/Models/')), key=lambda k: -len(entries[k]))
        groups = [first]
        sizes = [packed(entries, first)]
        for k in rest:
            one = packed(entries, [k])
            for g in range(1, len(groups)):
                if sizes[g] + one <= limit * .97:
                    groups[g].append(k); sizes[g] += one
                    break
            else:
                groups.append([k]); sizes.append(one)
        base = out[:-4] if out.endswith('.zip') else out
        for g, keys in enumerate(groups):
            name = '%s-part%d-%s.zip' % (base, g + 1, 'install' if g == 0 else 'models')
            write(name, folder, entries, sorted(keys))
            if os.path.getsize(name) > limit:
                raise SystemExit('%s is %d bytes, over %s MB' % (name, os.path.getsize(name), part_mb))
            parts.append(name)
    return sorted(entries), parts


def crlf(b):
    return b.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')


def packed(entries, keys):
    import zlib
    return sum(len(zlib.compress(entries[k], 6)) + 120 + 2 * len(k) for k in keys)


def write(out, folder, entries, keys):
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        for k in keys:
            info = zipfile.ZipInfo(folder + '/' + k, date_time=(2026, 10, 27, 12, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, entries[k])


if __name__ == '__main__':
    names, parts = build(*sys.argv[1:7])
    for f in names:
        print(f)
    for p in parts:
        print('part:', p, os.path.getsize(p))
