"""0.0.6 hotfix: tools/check_bundled_python.py, the gate that a ready-made mod zip carries a runnable Python of its own
(players never install Python; launch-game-vr.ps1 uses python\\python.exe). Synthetic zips: one good, several broken."""
import io, pathlib, struct, sys, tempfile, unittest, zipfile
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import check_bundled_python as cbp

def pe(machine=0x8664):
    b = bytearray(0x200); b[:2] = b'MZ'; struct.pack_into('<I', b, 0x3C, 0x80); b[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<H', b, 0x84, machine); return bytes(b)

def stdlib(drop=()):
    m = io.BytesIO()
    with zipfile.ZipFile(m, 'w') as z:
        for mod in cbp.STDLIB:
            if mod not in drop: z.writestr(mod + ('/__init__.pyc' if mod in ('json', 'ctypes', 'encodings') else '.pyc'), b'x')
    return m.getvalue()

def make(tmp, drop=(), exe=None, minor=12, pth=None, lib=None):
    p = pathlib.Path(tmp) / 'HERO-VR-0.0.6-mod-win64.zip'
    files = {'python.exe': exe or pe(), 'python3%d.dll' % minor: pe(), 'python3%d.zip' % minor: lib or stdlib(),
             'python3%d._pth' % minor: pth or ('python3%d.zip\n.\n..\\tools\n' % minor).encode()}
    for n in cbp.NATIVE: files[n] = pe() if n.endswith('.dll') or n.endswith('.pyd') else b'x'
    with zipfile.ZipFile(p, 'w') as z:
        z.writestr('HERO-VR-0.0.6-mod/build/windows-ninja/spidy_stereo_probe.dll', pe())
        for n, b in files.items():
            if n not in drop: z.writestr('HERO-VR-0.0.6-mod/python/' + n, b)
    return str(p)

class BundledPython(unittest.TestCase):
    def run_check(self, **kw):
        with tempfile.TemporaryDirectory() as t: return cbp.check(make(t, **kw))
    def test_good_zip_passes(self):
        e, v = self.run_check(); self.assertEqual(e, []); self.assertEqual(v, '3.12')
    def test_no_python_folder_fails(self):
        with tempfile.TemporaryDirectory() as t:
            p = pathlib.Path(t) / 'm.zip'
            with zipfile.ZipFile(p, 'w') as z: z.writestr('HERO-VR-0.0.6-mod/build/windows-ninja/a.dll', pe())
            self.assertEqual(cbp.main([str(p)]), 1)
    def test_32_bit_python_fails(self): self.assertTrue(self.run_check(exe=pe(0x14c))[0])
    def test_old_python_fails(self): self.assertTrue(any('older than 3.11' in e for e in self.run_check(minor=10)[0]))
    def test_missing_ctypes_runtime_fails(self): self.assertTrue(self.run_check(drop=('_ctypes.pyd',))[0])
    def test_missing_stdlib_zip_fails(self): self.assertTrue(self.run_check(drop=('python312.zip',))[0])
    def test_stdlib_without_a_needed_module_fails(self): self.assertTrue(self.run_check(lib=stdlib(drop=('subprocess',)))[0])
    def test_pth_must_list_the_stdlib(self): self.assertTrue(self.run_check(pth=b'.\n')[0])
    def test_launch_script_uses_the_bundled_python(self):
        ps = (ROOT / 'tools/launch-game-vr.ps1').read_text(encoding='utf-8-sig')
        self.assertIn("$bundledPython=Join-Path $projectRoot 'python\\python.exe'", ps)
    def test_package_ships_python(self):
        pk = (ROOT / 'tools/package.ps1').read_text(encoding='utf-8-sig')
        self.assertIn('embed-amd64.zip', pk)

if __name__ == '__main__':
    unittest.main()
