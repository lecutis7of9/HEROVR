"""HERO VR 0.0.74 test 3 HEAP GUARD: when the game rebuilds its render heaps and aliases (the public 0.0.73
player's "[Mem12] Aliased resource does not fit in the original heap ... Recreating heap and aliases."), the eye
bridge lets go of the eye images, forgets their states, takes them again from the next frame and copies no pair
begun before. OFF by default since the test 3 fix (the black headset), switchable (live.txt heap_guard, VR SETTINGS HEAP GUARD). Made by Agent Mulder."""
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
        self.assertEqual(live_settings.UPDATE38_DEFAULTS['heap_guard'], 'off')  # the test 3 fix
        self.assertEqual(live_settings.export_value('heap_guard', 'off'), 0)
        self.assertEqual(live_settings.export_value('heap_guard', 'on'), 1)
        self.assertEqual(live_settings.CHOICE_NAMES['heap_guard'][0], 'off')
        self.assertEqual(live_settings.SETTINGS['heap_guard'][1], 'SpidyHeapGuard')

    def test_option_row(self):
        self.assertIn('{"heap_guard", "HEAP GUARD", "HEAP",', src('include/spidy/combat38.hpp'))
        self.assertIn('{"ON", "OFF", nullptr, nullptr}}', src('include/spidy/combat38.hpp'))
        self.assertIn('combat38::heapGuard, 5505', src('src/game_xr.cpp'))


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



class BlackHeadsetFix(unittest.TestCase):
    """0.0.74 test 3 fa15f5f: the player's headset black (797 eye pairs rendered, 30 copied). Made by Agent Mulder."""
    s = src('src/native_eye_gpu.cpp')
    g = src('src/game_xr.cpp')
    h = src('include/spidy/heap_guard.hpp')

    def test_guard_falls_back_and_rate_limits(self):
        self.assertIn('inline constexpr uint64_t minGapMs = 1000;', self.h)
        self.assertIn('bool fellBack{};', self.h)
        self.assertIn('return fellBack || generation > holdGeneration;', self.h)
        body = self.s[self.s.index('void guardHeaps()'):]
        body = body[:body.index('\n}\n')]
        self.assertIn('newestGeneration.load(), copied,', body)
        self.assertIn('GetTickCount64()', body)

    def test_switches_reach_the_bridge_without_a_picture(self):
        lam = self.g[self.g.index('[&](std::array<XrRuntime::EyeTarget, 2>& targets) {'):]
        head = lam[:lam.index('if (showScreen && stereoScene')]
        self.assertIn('SpidyEyeHeapGuard(', head)
        self.assertIn('native_gpu::setStockImage(', head)
        self.assertIn('native_gpu::heapGuardFellBack()', head)

    def test_off_by_default_everywhere(self):
        # Sessions 014414 and 020500 (fa15f5f): the bridge started with the guard ON and live.txt's off only
        # reached it after a pair reached the headset, which never happened. OFF from the bridge's first frame.
        self.assertIn('std::atomic<bool> heapGuardWanted{false};', self.s)
        self.assertIn('inline bool on(uint32_t value) { return value == 1; }', self.h)
        self.assertIn('"HEAP", "ON lets the eyes go', src('include/spidy/combat38.hpp'))
        self.assertIn('2, {"OFF", "ON", nullptr, nullptr}}', src('include/spidy/combat38.hpp').split('"heap_guard"', 1)[1].split('\n', 1)[0])
        self.assertEqual(live_settings.CHOICE_NAMES['heap_guard'], ('off', 'on'))

    def test_notice_says_test_3(self):
        self.assertIn('set(HERO_TEST_NUMBER "3" CACHE', src('CMakeLists.txt'))
        self.assertIn('" TEST " SPIDY_TEST_NUMBER " " SPIDY_TEST_COMMIT " - Made by Agent Mulder: starting OpenXR"', self.g)


if __name__ == '__main__':
    unittest.main()
