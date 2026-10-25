#!/usr/bin/env python3
"""Gate for the 0.0.2 hotfix Windows build break (MSVC C2589 on std::min/std::max).

windows.h defines min/max macros unless NOMINMAX is set first. The Linux and
MinGW gates can't see this, so check it by rule:
  1. every test source that includes <windows.h> defines NOMINMAX before it;
  2. every test executable that includes it (or compiles src/ files on Windows)
     gets NOMINMAX from CMake on Windows too;
  3. every non-test target in CMakeLists.txt that compiles src/ gets NOMINMAX.
"""
import pathlib, re, sys
root = pathlib.Path(__file__).resolve().parent.parent
cm = (root / "CMakeLists.txt").read_text()
bad = []
inc = re.compile(r'^\s*#\s*include\s*<windows\.h>', re.M)
for p in sorted((root / "tests").glob("*.cpp")):
    s = p.read_text(errors="replace")
    m = inc.search(s)
    if not m:
        continue
    d = re.search(r'^\s*#\s*define\s+NOMINMAX\b', s[:m.start()], re.M)
    if not d:
        bad.append(f"{p.name}: includes <windows.h> without #define NOMINMAX before it")
    for t in re.findall(r'add_executable\((\w+)\s+tests/' + re.escape(p.name), cm):
        if not re.search(r'target_compile_definitions\(\s*' + t + r'\s[^)]*NOMINMAX', cm):
            bad.append(f"CMakeLists.txt: {t} has no NOMINMAX for Windows")
for t in ("spidy_tests", "spidy_replay_tests"):
    if not re.search(r'target_compile_definitions\(\s*' + t + r'\s[^)]*NOMINMAX', cm):
        bad.append(f"CMakeLists.txt: {t} has no NOMINMAX for Windows")
if not re.search(r"if\(WIN32\)\s*\n\s*add_compile_definitions\([^)]*NOMINMAX", cm):
    bad.append("CMakeLists.txt: no project-wide add_compile_definitions(NOMINMAX) for Windows")
if "NOMINMAX" not in cm:
    bad.append("CMakeLists.txt: NOMINMAX is never defined")
if bad:
    print("FAIL windows header gate:"); [print("  " + b) for b in bad]; sys.exit(1)
print("OK windows header gate: every <windows.h> user is NOMINMAX-safe")
