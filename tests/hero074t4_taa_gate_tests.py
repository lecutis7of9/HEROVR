"""0.0.74 test 4 AA fixes 1-3 (include/spidy/taa_gate074.hpp): TAA GATE LOG, EYE TAA REQUEUE and MAIN VIEW SMALL -
their options (TAA GATE LOG ON, the other two OFF by default), subtexts, launch bit and wiring; AA MODE CLEAN
(ON by default). Made by Agent Mulder."""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402

KEYS = {'taa_gate_log': ('TAA GATE LOG', 'TGLG', 'SpidyTaaGateLog'),
        'eye_taa_requeue': ('EYE TAA REQUEUE', 'ETRQ', 'SpidyEyeTaaRequeue'),
        'main_view_small': ('MAIN VIEW SMALL', 'MVSM', 'SpidyMainViewSmall')}


def src(rel):
    return (ROOT / rel).read_text(encoding='utf-8')


class Options(unittest.TestCase):
    def test_defaults(self):
        for key, (_, _, export) in KEYS.items():
            self.assertEqual(L.UPDATE38_DEFAULTS[key], 'on' if key == 'taa_gate_log' else 'off', key)
            self.assertEqual(L.CHOICES[key]['on'], 0, key)
            self.assertEqual(L.CHOICES[key]['off'], 1, key)
            self.assertEqual(L.SETTINGS[key][1], export, key)
            self.assertIn(key, L.UPDATE38_KEYS)
        self.assertFalse(L.main_view_small_of({}))
        self.assertFalse(L.main_view_small_of({'main_view_small': 'on'}))  # AA MODE CLEAN wins
        self.assertTrue(L.main_view_small_of({'aa_mode': 'off', 'main_view_small': 'on'}))
        self.assertFalse(L.main_view_small_of({'aa_mode': 'off', 'main_view_small': 'off'}))
        self.assertFalse(L.main_view_small_in_file('/nonexistent/live.txt'))
        g = src('src/game_xr.cpp')
        self.assertIn('combat38Values[combat38::eyeTaaRequeue].store(1u);', g)
        self.assertIn('combat38Values[combat38::mainViewSmall].store(1u);', g)
        self.assertIn('requeueOn{false}', src('src/stereo_probe.cpp'))

    def test_subtexts_one_sentence_with_restart(self):
        h = src('include/spidy/combat38.hpp')
        for key, (label, short, _) in KEYS.items():
            m = re.search(r'\{"%s", "%s", "%s", "([^"]+)", 2, \{"ON", "OFF"' % (key, label, short), h)
            self.assertIsNotNone(m, key)
            self.assertLessEqual(len(m.group(1)), 80, key)
            self.assertEqual(m.group(1).count('.'), 1, key)
            self.assertIn('restart', m.group(1), key)

    def test_records(self):
        r = src('tools/update37_records.py')
        for key in KEYS:
            self.assertIn("('%s', ('ON', 'OFF'))" % key, r)
        self.assertEqual(L.UPDATE38_DEFAULTS['heap_guard'], 'off')  # unchanged


class Wiring(unittest.TestCase):
    def test_launch_bit(self):
        r = src('tools/run_game_vr.py')
        self.assertIn('MAIN_VIEW_SMALL_OPTION = 0x2000000', r)
        self.assertIn("main_small = live_settings.main_view_small_in_file(ROOT/'live.txt')", r)
        g = src('src/game_xr.cpp')
        self.assertIn('constexpr uint32_t knownOptions = 0x3ffffff;', g)
        self.assertIn('(config.options & taa_gate::mainStartOption) ? taa_gate::mainEyeTemporalBit << 8', g)

    def test_exports_extern_c(self):
        g = src('src/game_xr.cpp')
        for code, (name, enum) in {5514: ('SpidyTaaGateLog', 'taaGateLog'), 5515: ('SpidyEyeTaaRequeue', 'eyeTaaRequeue'),
                                   5516: ('SpidyMainViewSmall', 'mainViewSmall')}.items():
            self.assertIn('%s(void* v) { return setHero007(v, combat38::%s, %d); }' % (name, enum, code), g)
        p = src('src/stereo_probe.cpp')
        self.assertIn('extern "C" {\n__declspec(dllexport) spidy::taa_gate::Data SpidyTaaGateData;', p)
        for name in ('SpidyTaaGateSet', 'SpidyTaaGateNow'):
            self.assertRegex(p, r'extern "C"[^\n]*' + name)

    def test_log_lines_short(self):
        g = src('src/game_xr.cpp')
        self.assertIn('char g1[taa_gate::lineMax]', g)
        self.assertGreaterEqual(g.count('[taa_gate::lineMax]'), 4)

    def test_fallbacks(self):
        p = src('src/stereo_probe.cpp')
        for why in ('requeueFault', 'requeueFailed', 'mainGameAa'):
            self.assertRegex(p, r'gateFallBack\((spidy::taa_gate|tg)::%s\)' % why)
        self.assertIn('__try', p)
        h = src('include/spidy/taa_gate074.hpp')
        self.assertIn('CONFIRMED', h)
        self.assertIn('CANDIDATE', h)

    def test_never_view_flag_32(self):
        h = src('include/spidy/taa_gate074.hpp')
        self.assertNotRegex(h, r'=\s*(0x20|32)\s*[;,]\s*//[^\n]*flag')


if __name__ == '__main__':
    unittest.main()


class AaModeClean(unittest.TestCase):
    """0.0.74 test 4 AA MODE CLEAN: ON by default, OFF gives the player's own AA options back."""

    def test_option(self):
        self.assertEqual(L.UPDATE38_DEFAULTS['aa_mode'], 'clean')
        self.assertEqual((L.CHOICES['aa_mode']['clean'], L.CHOICES['aa_mode']['off']), (0, 1))
        self.assertEqual(L.SETTINGS['aa_mode'][1], 'SpidyAaMode')
        self.assertIn('aa_mode', L.UPDATE38_KEYS)
        self.assertTrue(L.aa_mode_of({}) and L.aa_mode_in_file('/nonexistent/live.txt'))
        self.assertFalse(L.aa_mode_of({'aa_mode': 'off'}))
        self.assertIn("('aa_mode', ('CLEAN', 'OFF'))", src('tools/update37_records.py'))
        h = src('include/spidy/combat38.hpp')
        m = re.search(r'\{"aa_mode", "AA MODE", "AAMD", "([^"]+)", 2, \{"CLEAN", "OFF"', h)
        self.assertIsNotNone(m)
        self.assertLessEqual(len(m.group(1)), 80)
        self.assertEqual(m.group(1).count('.'), 1)
        self.assertIn('restart', m.group(1))

    def test_preset(self):
        p = L.CLEAN_PRESET
        self.assertEqual((p['aa'], p['history'], p['trail'], p['sharpen'], p['sharpen_match'], p['image'],
                          p['image_style'], p['eye_temporal'], p['taa_jitter'], p['reach'], p['impostors']),
                         (1, 80, 0, 40, 'off', 'classic', 'off', 1, 3, 150, 1))
        for name, value in p.items():
            L.export_value(name, value)  # every value sends
        self.assertEqual(L.render_preset_in_file('/nonexistent/live.txt'), (p, 'AA MODE CLEAN'))
        self.assertFalse(L.stock_of({}) or L.picture_preset_of({}) or L.miles_taa_of({}))
        self.assertEqual(L.image_style_of({'image_style': 'spidyvr'}), 'off')
        self.assertEqual(L.CLEAN_MIP_BIAS, '0.5')

    def test_session_off(self):
        d = src('tools/vr_display.py')  # vr_display needs winreg (Windows only)
        self.assertIn('screen_shake=dict(ScreenShake=0)', d)
        self.assertIn('lens_flares=dict(LensFlares=0)', d)
        self.assertTrue(L.screen_shake_off(None) and L.screen_shake_off(''))
        self.assertFalse(L.screen_shake_off('aa_mode = off\n'))
        self.assertTrue(L.lens_flares_off(''))

    def test_launcher(self):
        r = src('tools/run_game_vr.py')
        self.assertIn("if render_preset_name == 'AA MODE CLEAN':", r)
        self.assertIn('live_settings.CLEAN_MIP_BIAS', r)
        self.assertIn("session_off = tuple(session_off) + ('screen_shake',)", r)
        self.assertIn("game_taa_look = 'off' if clean else", r)

    def test_dll(self):
        g = src('src/game_xr.cpp')
        self.assertIn('combat38::aaModeClean(combat38::clampValue(combat38::aaMode, combat38Values[combat38::aaMode].load()))', g)
        self.assertIn('return combat38::cleanValue(option, own);', g)
        self.assertLess(g.index('return combat38::cleanValue(option, own);'), g.index('return combat38::stockValue(option, own);'))
        self.assertIn('SpidyAaMode(void* v) { return setHero007(v, combat38::aaMode, 5517); }', g)
        h = src('include/spidy/combat38.hpp')
        for line in ('case spidyvrStock: return 1;', 'case imageStyle: return 1;', 'case milesTaa: return 1;',
                     'case eyeTaaRequeue: return 1;', 'case mainViewSmall: return 1;', 'case lensFlares: return 1;'):
            self.assertIn(line, h)

    def test_no_copied_names(self):
        bad = re.compile(r'jim\s*jam', re.I)
        for rel in ('include/spidy/combat38.hpp', 'src/game_xr.cpp', 'tools/live_settings.py', 'tools/run_game_vr.py',
                    'tools/vr_display.py', 'docs/AA-MODE-CLEAN-074.md'):
            self.assertIsNone(bad.search(src(rel)), rel)
