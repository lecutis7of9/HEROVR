"""HERO VR 0.0.73 private test build N: the payload from the CI mod zip (upload=false), the 0.0.72 Models plus the
0.0.73 claws and JACKET AND JEANS suit, the 0.0.72 Sounds plus the van's, then 4 part zips (each < 25 MB), ONE
top folder HERO-VR-0.0.73-testN/. Usage: make_test.py <N> <commit7> <ci mod zip> <out dir>"""
import hashlib, io, os, re, shutil, sys, zipfile, zlib
N, COMMIT, MODZIP, OUT = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
T12 = '/workspace/r72work/t12/HERO-VR-0.0.72-test12'
INST72 = '/workspace/r72work/e2e72/runs/live-reup/inst/HERO-VR-Launcher-0.0.72'
MOD72 = '/workspace/r72work/dl72/HERO-VR-0.0.72-mod-win64.zip'
REPO = '/workspace/hv72-repo'
NEWMODELS = '/workspace/hero-0.0.73/models'
TOP = f'HERO-VR-0.0.73-test{N}'
stage = os.path.join(OUT, 'stage', TOP); pay = os.path.join(stage, 'payload')
shutil.rmtree(os.path.join(OUT, 'stage'), ignore_errors=True); os.makedirs(pay)
sha = lambda b: hashlib.sha256(b).hexdigest()

def members(z):
    out = {}
    for i in z.infolist():
        if i.is_dir(): continue
        n = i.filename.replace('\\', '/'); out[n.split('/', 1)[1]] = i
    return out
new = zipfile.ZipFile(MODZIP); newm = members(new)
old = zipfile.ZipFile(MOD72); oldm = members(old)
t12 = [l.split('  ', 1)[1].strip() for l in open(os.path.join(T12, 'payload/SHA256SUMS.txt')) if '  ' in l]
want = set(p for p in t12 if not p.startswith(('Models/', 'Sounds/')) and p not in ('HERO-VR-TEST-BUILD.txt', 'SHA256SUMS.txt'))
for p, i in newm.items():  # the runtime files 0.0.73 changed or added
    if p.startswith(('build/windows-ninja/', 'tools/', 'python/', 'docs/')) and not p.endswith(('.pdb', '.lib', '.ilk')):
        if p not in oldm or sha(new.read(i)) != sha(old.read(oldm[p])):
            if p.startswith('build/') and not p.endswith(('.dll', '.exe')): continue
            want.add(p)
def put(rel, data):
    d = os.path.join(pay, rel); os.makedirs(os.path.dirname(d), exist_ok=True); open(d, 'wb').write(data)
for p in sorted(want):
    if p in newm: put(p, new.read(newm[p]))
    elif os.path.exists(os.path.join(INST72, p)): put(p, open(os.path.join(INST72, p), 'rb').read())
    else: put(p, open(os.path.join(T12, 'payload', p), 'rb').read())
shutil.copytree(os.path.join(INST72, 'Models'), os.path.join(pay, 'Models'))
shutil.copytree(os.path.join(NEWMODELS, 'Claws'), os.path.join(pay, 'Models/Claws'))
shutil.copytree(os.path.join(NEWMODELS, 'Suits/Suit_wolverine_3'), os.path.join(pay, 'Models/Suits/Suit_wolverine_3'), dirs_exist_ok=True)
shutil.copytree(os.path.join(INST72, 'Sounds'), os.path.join(pay, 'Sounds'))
for f in ('user-van-enter-start-engine.mp3',):
    shutil.copy(os.path.join(REPO, 'assets/sfx/source', f), os.path.join(pay, 'Sounds', f))
# 0.0.73 test 2 (fix 20): the desktop shortcut's own icon (make_hero_icon.py), installed at the mod folder's top.
ICON = os.path.join(REPO, 'tools/test073/HERO-VR.ico')
if os.path.exists(ICON):
    shutil.copy(ICON, os.path.join(pay, 'HERO-VR.ico'))
for f in ('THIRD-PARTY-MODELS.md', 'CREDITS.md'):
    shutil.copy(os.path.join(REPO, f), os.path.join(pay, f))
os.makedirs(os.path.join(pay, 'Music'), exist_ok=True)
# the marker the console reads (tools/run_game_vr.py test_build_lines)
dlls = sorted(p for p in want if p.endswith('.dll') and p.startswith(('build/', 'python/')))
marker = [f'HERO VR 0.0.73 test {COMMIT} - private test build {N} (not a release)', 'version=0.0.73',
          f'commit={COMMIT}', f'build={N}'] + [f'sha256 {p}=' + sha(open(os.path.join(pay, p), 'rb').read()) for p in dlls]
open(os.path.join(pay, 'HERO-VR-TEST-BUILD.txt'), 'w', newline='\r\n').write('\n'.join(marker) + '\n')
files = sorted(os.path.relpath(os.path.join(r, f), pay).replace(os.sep, '/') for r, _, fs in os.walk(pay) for f in fs)
open(os.path.join(pay, 'SHA256SUMS.txt'), 'w', newline='\n').write(
    ''.join(f"{sha(open(os.path.join(pay, p), 'rb').read())}  {p}\n" for p in files))
# installer: tools/test073's (from test 12's), renamed
for f in ('INSTALL-TEST-BUILD.bat', 'INSTALL-TEST-BUILD.ps1'):
    own = os.path.join(REPO, 'tools/test073', f)  # 0.0.73 test 2: the installer with the shortcut popup (fix 20)
    s = open(own if os.path.exists(own) else os.path.join(T12, f), encoding='utf-8').read().replace('0.0.72', '0.0.73')
    s = s.replace("extract part 2 and part 3 into this same folder", "extract ALL 4 part zips into this same folder")
    open(os.path.join(stage, f), 'w', encoding='utf-8', newline='\r\n').write(s.replace('\r\n', '\n'))
shutil.copy(os.path.join(os.path.dirname(os.path.abspath(__file__)), f'READ-ME-FIRST-test{N}.txt'), os.path.join(stage, 'READ-ME-FIRST.txt'))
# 4 parts, each < 25 MB: non-model files in part 1, models spread by their deflated size
allf = ['INSTALL-TEST-BUILD.bat', 'INSTALL-TEST-BUILD.ps1', 'READ-ME-FIRST.txt'] + ['payload/' + p for p in files + ['SHA256SUMS.txt']]
size = {p: len(zlib.compress(open(os.path.join(stage, p), 'rb').read(), 9)) for p in allf}
bins = [[], [], [], []]; tot = [0] * 4
for p in allf:
    if not p.startswith('payload/Models/'): bins[0].append(p); tot[0] += size[p]
for p in sorted((p for p in allf if p.startswith('payload/Models/')), key=lambda p: -size[p]):
    k = min(range(4), key=lambda k: tot[k]); bins[k].append(p); tot[k] += size[p]
names = ['part1-install', 'part2-models', 'part3-models', 'part4-models']
for k in range(4):
    zp = os.path.join(OUT, f'{TOP}-{names[k]}.zip')
    with zipfile.ZipFile(zp, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in sorted(bins[k]): z.write(os.path.join(stage, p), f'{TOP}/{p}')
    s = os.path.getsize(zp); assert s < 25 * 1024 * 1024, (zp, s)
    print(f'{sha(open(zp, "rb").read())}  {os.path.basename(zp)}  {s}')
print(len(files), 'payload files;', len(want), 'runtime files')
