"""Version 0.0.72: the private test zip (never a release).

  python tools/release072/make_test_zip.py <CI prebuilt-mod zip> <Models folder> <out zip>

The CI zip's top folder (HERO-VR-0.0.70-mod/) is stripped so the zip extracts straight INTO the player's
HERO-VR-0.0.70 folder; Models/, Sounds/ (the player's five mp3s, checked against their sha256),
READ-ME-FIRST.txt, THIRD-PARTY-MODELS.md and SHA256SUMS.txt are added. Fails if a required file is missing."""
import hashlib, os, sys, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import required_assets as ra  # noqa: E402


def build(ci_zip, models, out):
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
    files['READ-ME-FIRST.txt'] = open(os.path.join(HERE, 'READ-ME-FIRST.txt'), 'rb').read().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    files['THIRD-PARTY-MODELS.md'] = open(os.path.join(ra.REPO, 'THIRD-PARTY-MODELS.md'), 'rb').read()
    sums = ''.join('%s  %s\r\n' % (hashlib.sha256(files[k]).hexdigest(), k) for k in sorted(files))
    files['SHA256SUMS.txt'] = sums.encode()
    missing = [f for f in ra.required_zip_files() if f not in files]
    if missing:
        raise SystemExit('missing: %s' % missing)
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        for k in sorted(files):
            z.writestr(k, files[k])
    return sorted(files)


if __name__ == '__main__':
    for f in build(*sys.argv[1:4]):
        print(f)
