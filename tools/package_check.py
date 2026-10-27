"""Version 0.0.5 packaging gate: a fresh folder (the zip only + what bootstrap.ps1 fetches) has every
file the Python tools/tests import and every file CMake names."""
import ast, os, re, sys
root = sys.argv[1]
missing = []
std = set(sys.stdlib_module_names)
optional = set()
pyfiles = []
for d, _, fs in os.walk(root):
    if '/build' in d.replace(root, '') or '/third_party' in d.replace(root, ''):
        continue
    pyfiles += [os.path.join(d, f) for f in fs if f.endswith('.py')]
localmods = {}
for p in pyfiles:
    localmods.setdefault(os.path.splitext(os.path.basename(p))[0], []).append(p)
third = {}
for p in pyfiles:
    try:
        tree = ast.parse(open(p, encoding='utf-8').read())
    except SyntaxError as e:
        missing.append(f'{p}: syntax {e}')
        continue
    # imports guarded by try/except ImportError are optional
    guarded = set()
    for n in ast.walk(tree):
        if isinstance(n, ast.Try) and any(isinstance(h.type, ast.Name) and h.type.id in ('ImportError', 'ModuleNotFoundError', 'Exception') or (isinstance(h.type, ast.Tuple)) or h.type is None for h in n.handlers):
            for m in ast.walk(n):
                if isinstance(m, (ast.Import, ast.ImportFrom)):
                    guarded.add(id(m))
    for n in ast.walk(tree):
        names = []
        if isinstance(n, ast.Import):
            names = [a.name.split('.')[0] for a in n.names]
        elif isinstance(n, ast.ImportFrom) and n.level == 0 and n.module:
            names = [n.module.split('.')[0]]
        for m in names:
            if m in std or m in localmods or m == '__future__':
                continue
            if id(n) in guarded:
                continue
            third.setdefault(m, []).append(os.path.relpath(p, root))
cm = open(os.path.join(root, 'CMakeLists.txt')).read()
for f in sorted(set(re.findall(r'(?:^|[\s"(]|\$\{CMAKE_CURRENT_SOURCE_DIR\}/)((?:src|apps|tests|include|tools|assets)/[A-Za-z0-9_./-]+\.(?:cpp|c|hpp|h|rc|manifest|def|in|cmd|ps1|hlsl|json|png|ico|txt|cmake))(?![\w.])', cm, re.M))):
    if not os.path.exists(os.path.join(root, f)):
        missing.append(f'CMakeLists.txt names {f}')
for f in sorted(set(re.findall(r'add_subdirectory\(\s*([^\s)]+)', cm))):
    if not os.path.exists(os.path.join(root, f, 'CMakeLists.txt')):
        missing.append(f'add_subdirectory {f}')
for p in pyfiles:
    s = open(p, encoding='utf-8', errors='replace').read()
    for f in set(re.findall(r"""['"]((?:tools|tests|src|include)/[A-Za-z0-9_./-]+\.(?:py|cpp|hpp|ps1|json))['"]""", s)):
        if not os.path.exists(os.path.join(root, f)):
            missing.append(f'{os.path.relpath(p, root)} names {f}')
allowed = {'pefile', 'capstone', 'numpy', 'PIL', 'scipy', 'pyfqmr'}  # research, art, sfx-generator and model-converter tools only (the WAVs ship in assets/sfx, converted models never ship in git; the docs say pip install)
# Version 0.0.7 CAPTAIN AMERICA: modules that exist only where a dev tool runs, allowed ONLY for those
# files - Blender's own (bpy, mathutils, bmesh) for the suit scripts run inside Blender on the PC, and
# matplotlib for the preview diagram (a box-side preview tool; nothing ships its output).
scoped = {'bpy': 'tools/cap_suit/blender/', 'mathutils': 'tools/cap_suit/blender/', 'bmesh': 'tools/cap_suit/blender/',
          'matplotlib': 'tools/models007/'}
for k in list(third):
    if k in scoped:
        outside = [f for f in third[k] if not f.replace(os.sep, '/').startswith(scoped[k])]
        if outside:
            third[k] = outside
        else:
            del third[k]
for k, v in third.items():
    if k not in allowed:
        missing.append(f'python import {k} (in {v[:3]}) is neither stdlib nor in the zip')
print('third-party python imports (not stdlib, not in the zip):', {k: v[:3] for k, v in third.items()})
if missing:
    print('MISSING:'); print('\n'.join(sorted(set(missing))))
    sys.exit(1)
print(f'packaging: {len(pyfiles)} python files import only the stdlib, each other or the listed packages; every file CMake names is there')
