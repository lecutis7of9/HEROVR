# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 24: tools/player_check.py, the unzip-and-play checks (portable parts; Windows' registry is not used)."""
import io, contextlib, os, pathlib, struct, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import player_check as pc


def module(dlls, pe32=False):
    """A tiny PE file importing `dlls`, as tests/launcher_tests.cpp builds for pe_imports.hpp."""
    data = bytearray(0x400)
    data[0:2] = b'MZ'
    struct.pack_into('<I', data, 0x3C, 0x40)
    data[0x40:0x44] = b'PE\0\0'
    optional_size = 224 if pe32 else 240
    struct.pack_into('<H', data, 0x46, 1)
    struct.pack_into('<H', data, 0x54, optional_size)
    struct.pack_into('<H', data, 0x58, 0x10B if pe32 else 0x20B)
    directories = 0x58 + (96 if pe32 else 112)
    struct.pack_into('<I', data, directories - 4, 16)
    struct.pack_into('<I', data, directories + 8, 0x1000)
    section = 0x58 + optional_size
    struct.pack_into('<IIII', data, section + 8, 0x200, 0x1000, 0x200, 0x200)
    name = 0x300
    for i, dll in enumerate(dlls):
        struct.pack_into('<I', data, 0x200 + 20 * i + 12, 0x1000 + name - 0x200)
        data[name:name + len(dll)] = dll.encode()
        name += len(dll) + 1
    return bytes(data)


class Runtime(unittest.TestCase):
    def test_imports(self):
        self.assertEqual(pc.pe_imports(module(['KERNEL32.dll', 'MSVCP140.dll'])), ['kernel32.dll', 'msvcp140.dll'])
        self.assertEqual(pc.pe_imports(module(['USER32.dll'], pe32=True)), ['user32.dll'])
        self.assertEqual(pc.pe_imports(b'not a module'), [])
        self.assertEqual(pc.pe_imports(module(['KERNEL32.dll'])[:0x250]), [])

    def test_redist_or_not(self):
        self.assertTrue(pc.needs_vc_redist(module(['KERNEL32.dll', 'VCRUNTIME140_1.dll'])))
        self.assertTrue(pc.needs_vc_redist(module(['msvcp140_atomic_wait.dll'])))
        self.assertFalse(pc.needs_vc_redist(module(['KERNEL32.dll', 'api-ms-win-crt-heap-l1-1-0.dll'])))

    def test_folder_users_and_problem(self):
        with tempfile.TemporaryDirectory() as d:
            d = pathlib.Path(d)
            (d / 'spidy_stereo_probe.dll').write_bytes(module(['KERNEL32.dll']))
            (d / 'bhaptics_library.dll').write_bytes(module(['VCRUNTIME140.dll']))   # bHaptics' own: the game's
            self.assertEqual(pc.runtime_users(d), {})
            self.assertIsNone(pc.runtime_problem(d, version=lambda: None))
            (d / 'spidy_bridge.dll').write_bytes(module(['MSVCP140.dll']))           # a build before Update 24
            self.assertEqual(list(pc.runtime_users(d)), ['spidy_bridge.dll'])
            self.assertIsNone(pc.runtime_problem(d, version=lambda: (14, 44, 35211)))
            problem = pc.runtime_problem(d, version=lambda: (14, 29))
            self.assertIn('vc_redist.x64.exe', problem)
            self.assertIn('build again', problem)
            self.assertIn('14.29', problem)
            with contextlib.redirect_stdout(io.StringIO()) as out:
                self.assertEqual(pc.main(['runtime', str(d)]), 3)
            self.assertIn('spidy_bridge.dll', out.getvalue())


class Folder(unittest.TestCase):
    env = {'ProgramFiles': r'C:\Program Files', 'OneDrive': r'C:\Users\Ann\OneDrive'}

    def problems(self, path, writable=True):
        return pc.folder_problems(path, environ=self.env, writable=lambda _: writable)

    def test_a_good_folder(self):
        self.assertEqual(self.problems(r'C:\Games\SM2 VR Update 24'), ([], []))

    def test_unwritable_is_an_error(self):
        errors, _ = self.problems(r'C:\Games\SM2', writable=False)
        self.assertEqual(len(errors), 1)
        self.assertIn("can't write", errors[0])

    def test_warnings(self):
        self.assertIn('Program Files', self.problems(r'C:\Program Files\SM2')[1][0])
        self.assertIn('OneDrive', self.problems(r'C:\Users\Ann\OneDrive\Desktop\SM2')[1][0])
        self.assertIn('OneDrive', self.problems(r'D:\OneDrive - Work\SM2')[1][0])
        self.assertEqual(self.problems(r'C:\Program Files Backup\SM2'), ([], []))
        long_path = 'C:\\' + 'x' * 200
        self.assertIn('long', self.problems(long_path)[1][0])
        self.assertTrue(all(len(w) < 400 for w in self.problems(long_path)[1]))

    def test_real_write_check(self):
        with tempfile.TemporaryDirectory() as d:
            self.assertTrue(pc._can_write(d))
            self.assertEqual(list((pathlib.Path(d) / 'reports').iterdir()), [])
        with contextlib.redirect_stdout(io.StringIO()) as out, tempfile.TemporaryDirectory() as d:
            self.assertEqual(pc.main(['folder', d]), 0)
        self.assertEqual(out.getvalue().splitlines()[-1], 'ok')


class Game(unittest.TestCase):
    def test_every_steam_library(self):
        with tempfile.TemporaryDirectory() as d:
            d = pathlib.Path(d)
            steam, library = d / 'Steam', d / 'Games' / 'SteamLibrary'
            (steam / 'steamapps').mkdir(parents=True)
            (steam / 'steamapps' / 'libraryfolders.vdf').write_text(
                '"libraryfolders"\n{\n "0"\n {\n  "path"  "%s"\n }\n "1"\n {\n  "path"  "%s"\n }\n}\n'
                % (str(steam).replace('\\', '\\\\'), str(library).replace('\\', '\\\\')))
            self.assertEqual(pc.steam_libraries([steam]), [steam, library])
            self.assertIsNone(pc.steam_game(pc.steam_libraries([steam])))
            game = library / 'steamapps' / 'common' / 'MSM2' / 'Spider-Man2.exe'
            game.parent.mkdir(parents=True)
            game.write_bytes(b'MZ')
            (library / 'steamapps' / 'appmanifest_2651280.acf').write_text('"AppState"\n{\n "installdir" "MSM2"\n}\n')
            self.assertEqual(pc.find_game([steam], epic=lambda: None), (game, [steam, library]))

    def test_not_found_is_a_note_with_the_libraries(self):
        error, note = pc.game_problem(None, [pathlib.Path('D:/SteamLibrary')])
        self.assertIsNone(error)
        self.assertIn('SteamLibrary', note)
        self.assertIn('not found', note)

    def test_verdicts(self):
        self.assertEqual(pc.game_verdict('a', '2.810.0.0', 'a', '2.810.0.0'), ('steam', None))
        kind, message = pc.game_verdict('b', '2.810.0.0', 'a', '2.810.0.0')
        self.assertEqual(kind, 'other_copy')
        self.assertEqual(pc.game_verdict('b', None, 'a', '2.810.0.0')[0], 'other_copy')   # unreadable: not blocked
        kind, message = pc.game_verdict('b', '2.900.0.0', 'a', '2.810.0.0')
        self.assertEqual(kind, 'unsupported')
        self.assertIn('2.900.0.0', message)
        self.assertIn('2.810.0.0', message)


class OpenXr(unittest.TestCase):
    def test_lines(self):
        vd = 'C:\\Program Files\\Virtual Desktop Streamer\\OpenXR\\virtualdesktop-openxr.json'
        self.assertIn('Virtual Desktop', pc.runtime_line(vd, []))
        self.assertIn('no active OpenXR runtime', pc.runtime_line(None, [{'manifest': 'x'}]))
        self.assertIn('No OpenXR runtime is installed', pc.runtime_line(None, []))


class BeforeSession(unittest.TestCase):
    def test_unwritable_stops_with_plain_words(self):
        saved = pc.folder_problems
        pc.folder_problems = lambda root: (['Spidy can\'t write in X.'], ['W'])
        lines = []
        try:
            with self.assertRaises(RuntimeError) as raised:
                pc.before_session(ROOT, launching=False, say=lines.append)
        finally:
            pc.folder_problems = saved
        self.assertIn("can't write", str(raised.exception))
        self.assertIn('WARNING: W', lines)


if __name__ == '__main__':
    unittest.main()
