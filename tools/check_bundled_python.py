#!/usr/bin/env python3
"""Checks that a ready-made mod zip (HERO-VR-<version>-mod-win64.zip) carries a complete, runnable Python of its own.

Players never install Python: tools\\launch-game-vr.ps1 runs the session with the package's python\\python.exe.
  python tools/check_bundled_python.py ZIP          structure check (any OS): x64 python.exe, python3XX.dll >= 3.11,
                                                    its stdlib zip with every module the VR session imports, the ._pth
  python tools/check_bundled_python.py ZIP --run    on Windows also unpacks it and runs python\\python.exe
Exit code 0 = OK, 1 = not usable (the reason is printed)."""
import io, os, re, struct, subprocess, sys, tempfile, zipfile

STDLIB = ["encodings", "argparse", "json", "ctypes", "subprocess", "zipfile", "socket", "hashlib", "threading", "uuid", "statistics",
          "datetime", "pathlib", "shutil", "signal", "struct", "bisect", "ast", "collections", "importlib", "re"]
NATIVE = ["_ctypes.pyd", "_socket.pyd", "_hashlib.pyd", "select.pyd", "libffi-8.dll", "libcrypto-3.dll", "vcruntime140.dll", "python3.dll"]
IMPORTS = "import sys, argparse, json, ctypes, subprocess, zipfile, socket, hashlib, threading, uuid, statistics, datetime, pathlib, shutil, signal, struct, msvcrt, winreg"


def pe_machine(data):
    if data[:2] != b"MZ" or len(data) < 0x40: return None
    off = struct.unpack_from("<I", data, 0x3C)[0]
    if data[off:off + 4] != b"PE\0\0": return None
    return struct.unpack_from("<H", data, off + 4)[0]


def check(path, run=False):
    errors = []
    z = zipfile.ZipFile(path)
    names = [n.replace("\\", "/") for n in z.namelist()]
    pys = [n for n in names if n.endswith("/python/python.exe") or n == "python/python.exe"]
    if not pys: return ["no python/python.exe in the zip"], None
    base = pys[0][:-len("python.exe")]
    have = {n[len(base):]: n for n in names if n.startswith(base)}
    m = pe_machine(z.read(pys[0]))
    if m != 0x8664: errors.append("python/python.exe is not a 64-bit Windows program (machine %r)" % m)
    dlls = [k for k in have if re.fullmatch(r"python3(\d+)\.dll", k)]
    if not dlls: return errors + ["no python3XX.dll"], None
    minor = int(re.fullmatch(r"python3(\d+)\.dll", dlls[0]).group(1))
    if minor < 11: errors.append("bundled Python is 3.%d, older than 3.11" % minor)
    if pe_machine(z.read(have[dlls[0]])) != 0x8664: errors.append(dlls[0] + " is not 64-bit")
    for n in NATIVE:
        if n not in have: errors.append("missing python/" + n)
    stdzip = "python3%d.zip" % minor
    pth = "python3%d._pth" % minor
    if stdzip not in have: errors.append("missing python/" + stdzip + " (its standard library)")
    else:
        try:
            lib = set(zipfile.ZipFile(io.BytesIO(z.read(have[stdzip]))).namelist())
            for mod in STDLIB:
                if not any(mod + x in lib for x in (".pyc", ".py", "/__init__.pyc", "/__init__.py")):
                    errors.append("standard library has no " + mod)
        except zipfile.BadZipFile: errors.append(stdzip + " is not a valid zip")
    if pth not in have: errors.append("missing python/" + pth)
    else:
        lines = [l.strip() for l in z.read(have[pth]).decode("utf-8", "replace").splitlines()]
        if stdzip not in lines: errors.append(pth + " does not list " + stdzip)
    version = "3.%d" % minor
    if run and not errors:
        if os.name != "nt": errors.append("--run needs Windows")
        else:
            with tempfile.TemporaryDirectory() as tmp:
                z.extractall(tmp)
                exe = os.path.join(tmp, *pys[0].split("/"))
                p = subprocess.run([exe, "-c", IMPORTS + "; assert sys.version_info >= (3, 11); print(sys.version.split()[0])"],
                                   capture_output=True, text=True, timeout=120)
                if p.returncode: errors.append("python/python.exe did not run: " + (p.stderr or p.stdout).strip()[-400:])
                else: version = p.stdout.strip()
    return errors, version


def main(argv):
    if not argv: print(__doc__); return 2
    errors, version = check(argv[0], "--run" in argv)
    if errors:
        for e in errors: print("FAIL bundled Python: " + e)
        return 1
    print("bundled Python OK: %s in %s%s" % (version, os.path.basename(argv[0]), " (ran it)" if "--run" in argv else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
