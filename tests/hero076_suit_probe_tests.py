"""HERO VR 0.0.76 SUIT PROBE (pt1572u: "It should be the existing in game model with whatever suit he's wearing";
"It should just read the files"): the first step of drawing PLAYER 2 from each PC's own installed game files
in his worn suit - a READ-ONLY probe that logs which .model assets the local Spider-Man wears, live.txt only
(the wrist HUD is full), id 5553, OFF by default. No game address."""
import os, re, sys, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import live_settings  # noqa: E402
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()

KEY, EXPORT, IDENT, ATOMIC = 'suit_probe', 'SpidySuitProbe', 5553, 'suitProbeNow'

class SuitProbe(unittest.TestCase):
    def test_live_txt_option(self):
        (lo, hi), exp, text = live_settings.SETTINGS[KEY]
        self.assertEqual((lo, hi, exp), (0, 1, EXPORT))
        self.assertIn('live, no restart', text)
        self.assertIn('never writes', text)
        self.assertEqual(live_settings.CHOICE_NAMES[KEY], ('off', 'on'))
        self.assertEqual(live_settings.export_value(KEY, 'off'), 0)  # OFF is the default
        self.assertEqual(live_settings.export_value(KEY, 'on'), 1)
        self.assertEqual(live_settings.parse('%s = on\n' % KEY), ({KEY: 'on'}, []))
        self.assertIn(KEY, live_settings.START_ONLY)
        self.assertNotIn(KEY, live_settings.UPDATE38_KEYS)
    def test_export_is_ours_alone(self):
        g = src('src/game_xr.cpp')
        self.assertIn('WINAPI %s(void* v) {\n    return setLive(v, 0, 1, %d, %s, asU32);' % (EXPORT, IDENT, ATOMIC), g)
        self.assertEqual(len(re.findall(r'\b%d\b' % IDENT, g)), 1)
        self.assertIn('std::atomic<uint32_t> %s{0};' % ATOMIC, g)
    def test_wiring(self):
        g = src('src/game_xr.cpp')
        call = g.index('suit_probe::frame(player.hero, player.record, suitProbeNow.load() == 1,')
        self.assertLess(call, g.index('std::lock_guard lock(telemetry);', call))  # never under the lock
        self.assertIn('src/suit_probe.cpp) # 0.0.76 SUIT PROBE', src('CMakeLists.txt'))
        self.assertIn('add_test(NAME suit_probe COMMAND spidy_suit_probe_tests)', src('CMakeLists.txt'))
    def test_read_only(self):
        c = re.sub(r'//.*', '', src('src/suit_probe.cpp'))
        for bad in ('VirtualProtect', 'WriteProcessMemory', 'MH_', 'memset(reinterpret_cast', '= *reinterpret_cast'):
            self.assertNotIn(bad, c)
        self.assertEqual(c.count('std::memcpy('), 1)  # the one SEH-guarded read
        self.assertIn('__except (EXCEPTION_EXECUTE_HANDLER)', c)
        for bad in ('ofstream', 'WriteFile', 'DeleteFile', 'CreateFile', 'std::filesystem::remove', 'fopen'):
            self.assertNotIn(bad, c)  # sm2scan.json is only read (ifstream)
        self.assertIn('std::ifstream in(place, std::ios::binary);', c)
    def test_probe2_throttled(self):
        h = src('include/spidy/suit_probe.hpp')
        for line in ('uint32_t words = 0x40;', 'uint32_t modelWords = 0x80;', 'uint32_t depth = 6;',
                     'uint32_t modelExtra = 2;', 'uint32_t objects = 10000;', 'uint32_t perStep = 24;',
                     'uint32_t faultsPerStep = 48;'):
            self.assertIn(line, h)  # PROBE 3: a little wider and deeper, still a few objects a frame
        c = src('src/suit_probe.cpp')
        self.assertIn('now - passEnded < 2000', c)  # a pause between passes
        self.assertIn('now - lastLook >= 15000', c)  # a missing sm2scan.json is looked for every 15 s, no more
        self.assertIn('size > (16u << 20)', c)  # never a huge file
    def test_probe3_run4(self):
        # run 4 (pt1611u: "advanced suit 2.0, classic suit, hybrid suit"): every pass hit its cap, list ids
        # flickered, none of the three suits' ids were seen. PROBE 3: the same memory each pass, two passes agree,
        # changes only, lists hidden, loaded models looked inside, the three suits watched in every place.
        h = src('include/spidy/suit_probe.hpp')
        self.assertIn('watchNames[3] = {"i30_Advanced_Suit", "SUIT_CLASSIC", "SUIT_NWH_HYBRID"}', h)
        for s in ('const bool agree = havePrev && ids == prev;', 'uint8_t complete() const', 'if (list) // the unlocked suits',
                  'std::vector<Found> settled() const', 'std::vector<Place> watchedPlaces() const'):
            self.assertIn(s, h)
        self.assertNotIn('if (++shown > 12)', h)  # no more 12-line cap hiding the rest
        c = src('src/suit_probe.cpp')
        self.assertIn('watch2.pass(scanTable, deep)', c)
        self.assertIn('watch2.watch(scanTable, deep)', c)
        self.assertIn('watch2.newPlayer();', c)  # a load: changes still told against the last set
    def test_probe2_reads_the_real_scan(self):
        # tools/sm2_suits/sm2scan.py's suits block, in the shape the C++ Table::fromScan reads (tests/suit_probe_tests.cpp)
        h = src('include/spidy/suit_probe.hpp')
        for key in ('s.str("name")', 's.str("item")', 's.str("id")', 's.get("models")', 's.get("mask")', 'm.str("path")'):
            self.assertIn(key, h)
        py = src('tools/sm2_suits/sm2scan.py')
        for key in ("'item'", "'models'", "'mask'", "'path'"):
            self.assertIn(key, py)
    def test_pure_header(self):
        h = src('include/spidy/suit_probe.hpp')
        self.assertNotIn('#include "', h)
        m = re.search(r'help =\s*((?:"[^"]*"\s*)+);', h)
        text = ''.join(re.findall(r'"([^"]*)"', m.group(1)))
        self.assertIn('restart', text)
        self.assertEqual(text.count('. '), 0)  # one sentence
        self.assertIn('CANDIDATE', h)
        self.assertIn('CONFIRMED', h)
    def test_no_forbidden_name(self):
        for p in ('include/spidy/suit_probe.hpp', 'src/suit_probe.cpp', 'tests/suit_probe_tests.cpp',
                  'tests/hero076_suit_probe_tests.py', 'tools/sm2_suits/sm2scan.py', 'tools/live_settings.py'):
            t = src(p).lower()
            self.assertNotIn('jim' + 'jam', t)
            if p != 'tools/live_settings.py':  # older text there names other things
                self.assertNotIn('mil' + 'es', t)

if __name__ == '__main__':
    unittest.main()
