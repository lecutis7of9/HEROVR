"""Version 0.0.2 hotfix: no function in the VR thread's file has a stack frame that can overflow a thread.

The 0.0.2 crash (0xc00000fd about 12 s in) was game_xr.cpp's run() holding the 725 KB eye history in its
frame (about 915 KB of the VR thread's 1 MB). mingw's -fstack-usage measures each frame; run() must stay
under 256 KB and every other function under 64 KB. Skips without the mingw compiler."""
import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
CXX = shutil.which('x86_64-w64-mingw32-g++')


@unittest.skipUnless(CXX, 'x86_64-w64-mingw32-g++ is not installed')
class StackFrames(unittest.TestCase):
    def frames(self, name):
        src = (ROOT / 'src' / name).read_text(encoding='utf-8')
        src = re.sub(r'__try', 'if (true)', src)
        src = re.sub(r'__except *\([^)]*\)', 'else', src)
        src = src.replace('__declspec(dllexport)', '__attribute__((dllexport))')
        with tempfile.TemporaryDirectory() as d:
            cpp = pathlib.Path(d) / name
            cpp.write_text(src, encoding='utf-8')
            inc = [f'-I{ROOT / "include"}', f'-I{ROOT / "src"}']
            for extra in ('/tmp/mh/include', str(ROOT.parent / 'port/tp/OpenXR-SDK/include')):
                if pathlib.Path(extra).exists():
                    inc.append('-I' + extra)
            r = subprocess.run([CXX, '-std=c++20', '-c', '-O1', '-fstack-usage', '-w', *inc, '-DWIN32_LEAN_AND_MEAN',
                                '-DNOMINMAX', '-DSM2_PORT=1', '-DXR_USE_PLATFORM_WIN32',
                                '-DXR_USE_GRAPHICS_API_D3D12', str(cpp), '-o', str(cpp.with_suffix('.o'))],
                               capture_output=True, text=True)
            if r.returncode:
                self.skipTest('mingw cannot build %s here: %s' % (name, r.stderr[-300:]))
            rows = []
            for line in cpp.with_suffix('.su').read_text().splitlines():
                parts = line.split('\t')
                rows.append((parts[0], int(parts[1])))
            return rows

    def test_game_xr_frames(self):
        rows = self.frames('game_xr.cpp')
        run = [b for f, b in rows if f.endswith('{anonymous}::run(void*)')]
        self.assertTrue(run, 'run() not measured')
        self.assertLess(run[0], 256 * 1024, 'run() frame %d bytes' % run[0])
        big = [(f, b) for f, b in rows if not f.endswith('{anonymous}::run(void*)') and b >= 64 * 1024]
        self.assertFalse(big, big)

    def test_heap_and_reserve_stay(self):
        src = (ROOT / 'src/game_xr.cpp').read_text(encoding='utf-8')
        self.assertIn('std::make_unique<NativeEyeHistory>()', src)
        self.assertNotRegex(src, r'\n\s*NativeEyeHistory history;')
        self.assertIn('CreateThread(nullptr, xrThreadStackReserve, run, nullptr, STACK_SIZE_PARAM_IS_A_RESERVATION', src)


if __name__ == '__main__':
    unittest.main()
