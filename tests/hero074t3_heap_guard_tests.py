"""HERO VR 0.0.74 test 3 HEAP GUARD: when the game rebuilds its render heaps and aliases (the public 0.0.73
player's "[Mem12] Aliased resource does not fit in the original heap ... Recreating heap and aliases."), the eye
bridge lets go of the eye images, forgets their states, takes them again from the next frame and copies no pair
begun before. On by default, switchable (live.txt heap_guard, VR SETTINGS HEAP GUARD). Made by Agent Mulder."""
import ast, os, struct, sys, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import live_settings  # noqa: E402
def src(p):
    with open(os.path.join(ROOT, p), encoding='utf-8') as f: return f.read()
def probe_snapshot():
    # probe_stereo_gpu.py needs Windows to import: its snapshot() alone, from its source.
    tree = ast.parse(src('tools/probe_stereo_gpu.py'))
    fn = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'snapshot')
    scope = {'struct': struct}
    exec(compile(ast.Module([fn], []), 'probe_stereo_gpu.py', 'exec'), scope)
    return scope['snapshot']


class Settings(unittest.TestCase):
    def test_default_on_and_switchable(self):
        self.assertEqual(live_settings.UPDATE38_DEFAULTS['heap_guard'], 'on')
        self.assertEqual(live_settings.export_value('heap_guard', 'on'), 0)
        self.assertEqual(live_settings.export_value('heap_guard', 'off'), 1)
        self.assertEqual(live_settings.CHOICE_NAMES['heap_guard'][0], 'on')
        self.assertEqual(live_settings.SETTINGS['heap_guard'][1], 'SpidyHeapGuard')

    def test_option_row(self):
        self.assertIn('{"heap_guard", "HEAP GUARD", "HEAP",', src('include/spidy/combat38.hpp'))
        self.assertIn('{"ON", "OFF", nullptr, nullptr}}', src('include/spidy/combat38.hpp'))
        self.assertIn('combat38::heapGuard, 5502', src('src/game_xr.cpp'))


class Native(unittest.TestCase):
    s = src('src/native_eye_gpu.cpp')

    def test_create_heap_watched_not_changed(self):
        self.assertIn('dvt[heap_guard::createHeapSlot]', self.s)
        body = self.s[self.s.index('HRESULT STDMETHODCALLTYPE createHeap('):]
        body = body[:body.index('\n}\n')]
        self.assertIn('originalCreateHeap(d, desc, riid, heap)', body)
        self.assertIn('return result;', body)
        self.assertIn('MH_DisableHook(heapHook)', self.s)

    def test_guard_before_copies(self):
        ex = self.s[self.s.index('void STDMETHODCALLTYPE execute('):]
        ex = ex[:ex.index('\n}\n')]
        self.assertLess(ex.index('guardHeaps();'), ex.index('if (!relevant)'))
        self.assertEqual(ex.count('heapGuard.mayCopy(ready.generation)'), 2)
        g = self.s[self.s.index('void guardHeaps()'):]
        g = g[:g.index('\n}\n')]
        self.assertIn('retiredEyes.push_back({resources[i], fenceValue})', g)
        self.assertNotIn('ExecuteCommandLists', g)

    def test_retake_same_kind_only_and_export(self):
        r = self.s[self.s.index('void registerEye('):]
        r = r[:r.index('\n}\n')]
        self.assertIn('eye_swap::compatible(heapRetakeDesc[eye]', r)
        self.assertIn('eyesLostCode = eye_swap::lostCode', r)
        self.assertRegex(self.s, r'SpidyEyeHeapGuard\(void\* on\) \{\n.*\n\s+if \(value > 1\)\n\s+return 3993;')


class Telemetry(unittest.TestCase):
    def test_probe_reads_heap_rebuilds(self):
        raw = bytearray(208)
        struct.pack_into('<3I', raw, 0, 0x53475044, 2, 208)
        struct.pack_into('<I', raw, 28, 3)
        class Game:
            def read(self, address, size):
                return bytes(raw[address - 0x1000:address - 0x1000 + size])
        self.assertEqual(probe_snapshot()(Game(), 0x1000)['heap_rebuilds'], 3)
        self.assertIn('heapRebuilds', src('include/spidy/native_eye_gpu.hpp'))
        self.assertIn("get('heap_rebuilds')", src('tools/run_game_vr.py'))


if __name__ == '__main__':
    unittest.main()
