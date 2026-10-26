# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 38.4 (mirror-research/MIRROR-REAL-MESH.md, docs/mirror.md): the MIRROR PROBE. It is read-only (no
writes to the game, no hooks of its own, no game calls), cheap (a self-measured budget that stops it), on by
default with no setting, and its record (MPRB, 1624 bytes) reaches the game-vr report and the console. No
GAME MODEL switch ships (nothing confirmed to switch). The graphics defaults are untouched. Any failing fails."""
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as ls  # noqa: E402
import update37_records as rec  # noqa: E402

HPP = (ROOT / 'include/spidy/mirror_probe.hpp').read_text(encoding='utf-8')
CPP = (ROOT / 'src/mirror_probe.cpp').read_text(encoding='utf-8')
PROBE = (ROOT / 'src/stereo_probe.cpp').read_text(encoding='utf-8', errors='replace')
XR = (ROOT / 'src/game_xr.cpp').read_text(encoding='utf-8', errors='replace')
CMAKE = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
VERSION = (ROOT / 'PORT-VERSION').read_text(encoding='utf-8')


def _code(text):
    """The text without // comments and string literals."""
    text = re.sub(r'"(?:[^"\\\n]|\\.)*"', '""', text)
    return re.sub(r'//[^\n]*', '', text)


class Version(unittest.TestCase):
    def test_version(self):
        self.assertRegex(VERSION, r'^Update (38\.[4-9]|39\.\d)')


class ReadOnly(unittest.TestCase):
    def test_no_writes_or_hooks(self):
        code = _code(CPP)
        for banned in ('VirtualProtect', 'WriteProcessMemory', 'MH_', 'CreateThread', 'VirtualAlloc',
                       'LoadLibrary', 'GetProcAddress', 'FlushInstructionCache'):
            self.assertNotIn(banned, code, banned)

    def test_every_game_read_is_peek(self):
        code = _code(CPP)
        # Game memory is only ever touched by peek()'s guarded memcpy (the source is the game, the target ours).
        casts = re.findall(r'reinterpret_cast<([^>]*)>\(', code)
        self.assertEqual(casts, ['const void*'], casts)
        self.assertEqual(code.count('std::memcpy(out, reinterpret_cast<const void*>(p), n)'), 1)
        self.assertEqual(code.count('__try'), 2, 'peek and guarded only')
        self.assertNotRegex(code, r'\*\s*reinterpret_cast|\(\s*\w+\s*\*\s*\)\s*\(?\s*(base|imageBase|p|f\.)')
        self.assertIn('if (p < 0x10000)', code)

    def test_no_game_calls(self):
        code = _code(CPP)
        self.assertNotRegex(code, r'using\s+\w+\s*=\s*\w+\s*\(\s*\*')  # no function-pointer types
        self.assertNotIn('(*)(', code)

    def test_hooks_only_report(self):
        # The four calls from stereo_probe: start once, a frame at the end of maintain, a count in copyJob,
        # and the sampled draw before the hero hiding. Their results are never used. Version 0.0.4:
        # slotDraws reads the MIRROR VIEW TEST's slot counts (read-only, for the V003 record).
        self.assertEqual(sorted(set(re.findall(r'spidy::mirror_probe::(\w+)', PROBE))),
                         ['Frame', 'draw', 'frame', 'job', 'sampling', 'slotDraws', 'start'])
        for name in ('start', 'frame', 'job', 'draw'):
            for line in re.findall(rf'[^\n]*spidy::mirror_probe::{name}\([^\n]*', PROBE):
                self.assertRegex(line.strip(), rf'^spidy::mirror_probe::{name}\(.*\);', line)
        self.assertIn('void start(uintptr_t base);', HPP)
        for decl in ('void frame(const Frame&);', 'void job(uint64_t view);',
                     'void draw(uint64_t view, uint64_t owner, uint64_t instance);'):
            self.assertIn(decl, HPP)
        # draw comes before the latched hide filter in renderActor, so the probe sees what the port drops.
        render = PROBE[PROBE.index('spidy::mirror_probe::draw('):]
        self.assertIn('latchedHide', render[:4000])

    def test_frame_is_guarded_and_budgeted(self):
        self.assertIn('if (!guarded(f)) {\n        stopAs(stoppedFault);', CPP)
        self.assertIn('stopAs(stoppedSlow);', CPP)
        self.assertIn('inline constexpr double budgetUs = 60;', HPP)
        self.assertIn('inline constexpr uint32_t drawCap = 4096;', HPP)
        self.assertIn('>= drawCap)\n        return;', CPP)

    def test_built_into_the_vr_module(self):
        self.assertIn('src/mirror_probe.cpp)', CMAKE)
        self.assertIn('add_test(NAME mirror_probe COMMAND spidy_mirror_probe_tests)', CMAKE)
        self.assertIn('__declspec(dllexport) spidy::mirror_probe::Data SpidyMirrorProbeData', CPP)


class Record(unittest.TestCase):
    def test_registered(self):
        export, magic, version, size = rec.RECORDS['mirror_probe']
        self.assertEqual(export, 'SpidyMirrorProbeData')
        self.assertEqual(magic, 0x4252504d)
        self.assertEqual(magic.to_bytes(4, 'little'), b'MPRB')
        self.assertIn(f'dataMagic = {hex(magic)};', HPP)
        self.assertIn(f'uint32_t magic = dataMagic, version = {version}, bytes = {size}, status{{}};', HPP)
        self.assertIn(f'static_assert(sizeof(Data) == {size}', HPP)
        self.assertIn('mirror_probe', rec.PARSERS)
        self.assertEqual(set(rec.PARSERS), set(rec.RECORDS))

    def _dump(self):
        exe = None
        for build in ('build/gate', 'build'):
            for name in ('spidy_mirror_probe_tests', 'spidy_mirror_probe_tests.exe'):
                if (ROOT / build / name).exists():
                    exe = str(ROOT / build / name)
        tmp = tempfile.mkdtemp()
        if exe is None:
            cxx = shutil.which('c++') or shutil.which('g++') or shutil.which('clang++')
            if cxx is None:
                self.skipTest('no C++ compiler to build the dump')
            exe = os.path.join(tmp, 'mpt')
            subprocess.check_call([cxx, '-std=c++20', '-O1', '-I', str(ROOT / 'include'),
                                   str(ROOT / 'tests/mirror_probe_tests.cpp'), '-o', exe])
        out = os.path.join(tmp, 'data.bin')
        subprocess.check_call([exe, '--dump', out])
        return pathlib.Path(out).read_bytes()

    def test_parser_reads_the_native_layout(self):
        raw = self._dump()
        self.assertEqual(len(raw), rec.RECORDS['mirror_probe'][3])  # 38.5: v2, 2752
        d = rec.parse_mirror_probe(raw)
        self.assertEqual(d['status'], 'watching')
        self.assertEqual(d['frames'], 1234)
        self.assertEqual((d['slots']['used'], d['slots']['used_max'], d['slots']['free_min']), (3, 4, 4))
        self.assertEqual(d['slots']['events'][0], dict(frame=10, slot=2, created=True, ours=False, flags='0x40',
                                                         size='512x512', kind=7, ms=4321))
        self.assertEqual(d['view_list']['headroom_min'], 2)
        self.assertEqual(d['jobs']['offscreen'], 7)
        self.assertEqual(d['hero_draws']['eye_left'], 11)
        self.assertEqual(d['piece_draws']['eye_right'], 12)
        self.assertEqual(d['pieces'][0]['draws'], dict(game=1, eye_left=2, eye_right=3, offscreen=4, other=5))
        self.assertTrue(d['pieces'][0]['drawn_off'])
        self.assertEqual(d['hero']['hidden_by_game'], 20)
        self.assertEqual(d['hero']['model_changes'], 2)
        self.assertEqual(d['camera_switch'], dict(now=1, changes=4))
        self.assertEqual(d['capped_frames'], 1)
        self.assertEqual(d['suit']['events'][0]['changed_words'], 9)
        self.assertEqual(d['suit']['fields'][0]['where'], 'record')
        self.assertEqual(d['suit']['fields'][0]['offset'], '0x1f0')
        self.assertEqual(d['cost'], dict(total_ms=2.468, peak_us=12.5, mean_us=2.0))
        lines = rec.console_lines({'mirror_probe': d})
        self.assertEqual(len(lines), 2)  # 38.5: the PROBE v2 line
        self.assertTrue(lines[0].startswith('Update 38.4 MIRROR PROBE: watching;'))

    def test_odd_sequence_is_skipped(self):
        raw = bytearray(self._dump())
        raw[-8] |= 1
        self.assertIsNone(rec.parse_mirror_probe(bytes(raw)))


class Settings(unittest.TestCase):
    def test_no_game_model_switch(self):
        # Nothing is confirmed that a GAME MODEL switch could turn on, so none ships (no switches that do nothing).
        for text in (XR, PROBE, (ROOT / 'tools/live_settings.py').read_text(encoding='utf-8')):
            self.assertNotRegex(text, r'(?i)game[ _]?model')
        self.assertNotIn('mirror_probe', ls.SETTINGS)

    def test_graphics_defaults_unchanged(self):
        d = ls.UPDATE37_DEFAULTS
        self.assertEqual((d['aa'], d['far_shadows'], d['sharpen'], d['shared_lod'], d['eye_bloom']), (1, 2, 140, 2, 1))
        self.assertEqual(ls.STARTUP_ON, {'eye_temporal': 1, 'far_shadows': 2, 'sharpen': 140})


if __name__ == '__main__':
    unittest.main()
