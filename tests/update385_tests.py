# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 38.5: BALL LOOK WRIST (the game's ball from the pulled hand's wrist, a one-time GAME ONLY -> WRIST
migration), the far-punch fix, MIRROR PROBE v2 and the read-only HERO SWITCH PROBE (HSWP). No setting that does
nothing; every setting has its subtext. Any failing fails."""
import pathlib
import re
import struct
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as ls  # noqa: E402
import update37_records as rec  # noqa: E402

HS = (ROOT / 'include/spidy/hero_switch.hpp').read_text(encoding='utf-8')
XR = (ROOT / 'src/game_xr.cpp').read_text(encoding='utf-8', errors='replace')
C38 = (ROOT / 'include/spidy/combat38.hpp').read_text(encoding='utf-8')
VERSION = (ROOT / 'PORT-VERSION').read_text(encoding='utf-8')


def _code(text):
    text = re.sub(r'"(?:[^"\\\n]|\\.)*"', '""', text)
    return re.sub(r'//[^\n]*', '', text)


class Version(unittest.TestCase):
    def test_version(self):
        self.assertRegex(VERSION, r'^Update 38\.[5-9]')  # 38.6 is Version 0.0.2


class Migration(unittest.TestCase):
    def run_on(self, text):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text(text, encoding='utf-8')
            first = ls.migrate_385(p)
            after = p.read_text(encoding='utf-8')
            second = ls.migrate_385(p)
            return first, after, second, p.read_text(encoding='utf-8')

    def test_game_only_becomes_wrist_once(self):
        first, after, second, final = self.run_on('ball_look = game_only\n')
        self.assertIn('ball_look = wrist', first)
        self.assertRegex(after, r'(?m)^ball_look\s*=\s*wrist')
        self.assertIn(ls.MIGRATED_385, after)
        self.assertIsNone(second)
        self.assertEqual(after, final)

    def test_glob_stays(self):
        first, after, _, _ = self.run_on('ball_look = glob\n')
        self.assertIsNone(first)
        self.assertRegex(after, r'(?m)^ball_look\s*=\s*glob')

    def test_chosen_after_stays(self):
        _, after, _, _ = self.run_on('ball_look = game_only\n')
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text(after + 'ball_look = game_only\n', encoding='utf-8')
            self.assertIsNone(ls.migrate_385(p))

    def test_missing_file(self):
        self.assertIsNone(ls.migrate_385('/nonexistent/live.txt'))


class Settings(unittest.TestCase):
    def test_ball_look_values_and_subtext(self):
        self.assertEqual(ls.SETTINGS['ball_look'][0], (0, 3))  # Version 0.0.2 added OURS
        m = re.search(r'"(OURS fires [^"]*)"', C38)  # Version 0.0.2's BALL LOOK subtext
        self.assertTrue(m, 'BALL LOOK subtext')
        self.assertLessEqual(len(m.group(1)), 100)


class HeroSwitch(unittest.TestCase):
    def test_read_only_and_no_game_calls(self):
        code = _code(HS)
        for banned in ('VirtualProtect', 'WriteProcessMemory', 'MH_', 'reinterpret_cast', 'SendInput', '(*)('):
            self.assertNotIn(banned, code, banned)
        self.assertIn('static_assert(sizeof(Data) == 112', HS)
        self.assertIn('0x50575348', HS)

    def test_exported_and_fed(self):
        self.assertIn('__declspec(dllexport) spidy::hero_switch::Data SpidyHeroSwitchData;', XR)
        self.assertIn('heroSwitch.frame(player.hero, player.record', XR)
        self.assertNotRegex(_code(XR), r'HeroCharacterSwitch|0x54294f8|0x5429a90')  # nothing called

    def test_record_parse_and_console(self):
        self.assertEqual(rec.RECORDS['hero_switch'], ('SpidyHeroSwitchData', 0x50575348, 1, 112))
        raw = bytearray(112)
        struct.pack_into('<III', raw, 0, 0x50575348, 1, 112)
        struct.pack_into('<5I', raw, 12, 1, 1, 1, 0, 0)
        struct.pack_into('<4Q', raw, 40, 0x2000, 0x2008, 0x1000, 0x1008)
        struct.pack_into('<5f', raw, 72, 800, 300, 310, 300, 310)
        d = rec.parse_hero_switch(bytes(raw))
        self.assertEqual((d['swaps'], d['last_gap_ms'], d['last_view_ms'], d['last_hero']), (1, 800, 300, '0x1000'))
        lines = rec.console_lines({'hero_switch': d})
        self.assertTrue(any('HERO SWITCH: 1 hero replacements' in l for l in lines), lines)
        d0 = rec.parse_hero_switch(bytes(bytearray(struct.pack('<III', 0x50575348, 1, 112)) + bytes(100)))
        self.assertTrue(any('no hero switch this session' in l for l in rec.console_lines({'hero_switch': d0})))


class WristMuzzle(unittest.TestCase):
    def test_record(self):
        self.assertEqual(rec.RECORDS['wrist_muzzle'], ('SpidyWristMuzzleData', 0x5a554d57, 1, 144))


class YankYHold(unittest.TestCase):
    def test_setting_and_migration(self):
        self.assertEqual(ls.SETTINGS['yank_slot'][0], (0, 7))
        self.assertIn('7 Y hold', ls.SETTINGS['yank_slot'][2])
        self.assertIn('Y HOLD', rec.SLOTS)
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text('yank_slot = 5\n', encoding='utf-8')
            self.assertIn('yank_slot = 7', ls.migrate_385(p))
            self.assertRegex(p.read_text(encoding='utf-8'), r'(?m)^yank_slot\s*=\s*7')
            p.write_text(p.read_text(encoding='utf-8') + 'yank_slot = 5\n', encoding='utf-8')
            self.assertIsNone(ls.migrate_385(p))  # chosen after the marker stays

    def test_default_y_hold(self):
        cin = (ROOT / 'include/spidy/combat_input.hpp').read_text(encoding='utf-8')
        self.assertRegex(cin, r'uint32_t yankSlot = yHold;')


class AirWatch(unittest.TestCase):
    def test_record_parse_and_console(self):
        self.assertEqual(rec.RECORDS['air_watch'], ('SpidyAirWatchData', 0x57524941, 1, 104))
        raw = bytearray(104)
        struct.pack_into('<IIII', raw, 0, 0x57524941, 1, 104, 1)
        struct.pack_into('<9Q', raw, 16, 4, 3, 2, 1, 5, 2, 1, 0, 0)
        struct.pack_into('<2I', raw, 88, 7000, 4000)
        d = rec.parse_air_watch(bytes(raw))
        self.assertEqual((d['y_sent'], d['y_in_air'], d['past_3s'], d['longest_ms']), (4, 3, 2, 7000))
        lines = rec.console_lines({'air_watch': d})
        self.assertTrue(any('Y IN THE AIR' in l for l in lines), lines)



GPU = (ROOT / 'src/native_eye_gpu.cpp').read_text(encoding='utf-8', errors='replace')
EC = (ROOT / 'include/spidy/eye_color.hpp').read_text(encoding='utf-8')


class Animated90s(unittest.TestCase):
    """The COLOR FILTER 90s ANIMATED: default OFF, live, its own three settings, falls back to NONE."""

    def test_live_settings(self):
        self.assertEqual(ls.export_value('color_filter', 'animated'), 6)
        self.assertEqual(ls.export_value('color_filter', '90s'), 6)
        for key, export in (('color_cartoon', 'SpidyColorCartoon'), ('color_outlines', 'SpidyColorOutlines'),
                            ('color_bands', 'SpidyColorBands')):
            self.assertEqual(ls.SETTINGS[key][1], export)
            self.assertIn('90s ANIMATED', ls.SETTINGS[key][2])
            self.assertIn(f'WINAPI {export}(', XR)
            self.assertIn(key, ls.START_ONLY)
        self.assertEqual(ls.SETTINGS['color_bands'][0], (3, 5))
        self.assertNotIn('color_filter', getattr(ls, 'UPDATE37_DEFAULTS', {}))  # no filter forced on

    def test_record_v2(self):
        _, magic, version, size = rec.RECORDS['color']
        self.assertEqual((version, size), (2, 80))
        b = bytearray(struct.pack('<3I', magic, version, size) + bytes(size - 12))
        struct.pack_into('<5I', b, 12, 1, 6, 100, 100, 100)
        struct.pack_into('<2I', b, 56, 60, 1 | 4 << 8)
        struct.pack_into('<Q', b, 64, 12)
        r = rec.parse_color(bytes(b))
        self.assertEqual((r['filter'], r['cartoon'], r['outlines'], r['bands'], r['toon_fault'], r['graded_pairs']),
                         ('90s ANIMATED', 60, True, 4, 'NONE', 12))
        line = '\n'.join(rec.console_lines({'color': r}))
        if line:
            self.assertIn('90s ANIMATED', line)

    def test_fallback_not_flatscreen(self):
        code = _code(GPU)
        self.assertIn('toonFault', code)
        # the toon only replaces the pass's pipeline when it was built; else the plain sharpen runs
        self.assertRegex(GPU, r'cel = tp\.on > \.5f && toonOn\.load\(\) && sharp\.toon && sharp\.toonSrgb')
        self.assertIn('FILTER set to NONE', XR)
        self.assertIn('eyeToonFault()', XR)

    def test_default_off(self):
        self.assertRegex(EC, r'bool on = false;')
        self.assertRegex(EC, r'uint32_t filter = none;')


G385 = (ROOT / 'include/spidy/gestures385.hpp').read_text(encoding='utf-8')
X385 = (ROOT / 'include/spidy/extras385.hpp').read_text(encoding='utf-8')
BS = (ROOT / 'include/spidy/blood_splatter.hpp').read_text(encoding='utf-8')
VD = (ROOT / 'tools/vr_display.py').read_text(encoding='utf-8')
RG = (ROOT / 'tools/run_game_vr.py').read_text(encoding='utf-8')


def _helps(text):
    return re.findall(r'inline constexpr const char\* (?:help|\w+Help) = "([^"]*)";', text)


class HelpLines(unittest.TestCase):
    """Every new 38.5 setting's subtext: one line, <= 80 characters, one '.', ending in '.'."""

    def test_rule(self):
        lines = _helps(G385) + _helps(X385) + _helps(BS)
        self.assertGreaterEqual(len(lines), 8)
        for h in lines:
            self.assertLessEqual(len(h), 80, h)
            self.assertTrue(h.endswith('.') and h.count('.') == 1, h)


class Extras(unittest.TestCase):
    """EXTRAS: each OFF by default and live; the cheats start OFF every session; no save or story writes."""

    def test_off_and_live(self):
        for key, export in (('invincible', 'SpidyInvincible'), ('fly_mode', 'SpidyFlyMode'),
                            ('symbiote_look', 'SpidySymbioteLook'), ('prompt_handling', 'SpidyPromptHandling')):
            self.assertIn(f'WINAPI {export}(', XR)
            self.assertNotIn(key, getattr(ls, 'UPDATE37_DEFAULTS', {}))
            self.assertNotIn(key, getattr(ls, 'UPDATE38_DEFAULTS', {}))
        for name in ('invincibleOn{false}', 'symbioteOn{false}', 'flyModeNow{0}', 'gestureMovesNow{0}'):
            self.assertIn(name, XR)

    def test_invincible_never_resurrects(self):
        self.assertIn('0x541edd0', X385)  # only the confirmed health table
        self.assertRegex(X385, r'one-shot and scripted deaths still happen')

    def test_no_save_or_story_writes(self):
        code = _code(X385 + G385)
        for word in ('SaveGame', 'StoryFlag', 'WriteSave'):
            self.assertNotIn(word, code)


class GestureMoves(unittest.TestCase):
    """GESTURE MOVES: OFF by default, TRAINING sends nothing, ON's presses go out just before the pad."""

    def test_setting(self):
        self.assertEqual(ls.SETTINGS['gesture_moves'][0], (0, 2))
        self.assertEqual(ls.SETTINGS['gesture_moves'][1], 'SpidyGestureMoves')
        self.assertIn('gesture_moves', ls.START_ONLY)
        self.assertIn('WINAPI SpidyGestureMoves(', XR)

    def test_sent_only_in_on(self):
        i = XR.index('GESTURE MOVES ON:')
        j = XR.index('game_pad::submit(pad, 200);', i)
        block = XR[i:j]
        self.assertIn('gestures385::modeOn', block)
        self.assertIn('comboSender.update', block)
        self.assertIn('!stickHeld', block)
        # TRAINING only names the move
        k = XR.index('if (gestureMode == gestures385::modeTraining)')
        self.assertIn('message(3, 0, line)', XR[k:k + 200])

    def test_fly_owns_the_arms(self):
        self.assertIn('gestureSet.wingArms = flyModeNow.load() == 0', XR)


class GameHud(unittest.TestCase):
    """GAME HUD: no switch that does nothing; a read-only probe and a launcher tip."""

    def test_probe_read_only(self):
        i = VD.index('def hud_option_probe(')
        body = VD[i:VD.index('\ndef ', i + 10)]
        for w in ('SetValueEx', 'CreateKey', 'DeleteValue', 'DeleteKey'):
            self.assertNotIn(w, body)
        self.assertIn('game_hud_options', RG)
        self.assertIn('Hide HUD', RG)

    def test_no_game_hud_setting(self):
        self.assertNotIn('game_hud', ls.SETTINGS)


class BloodExtreme(unittest.TestCase):
    def test_extreme_and_visor(self):
        self.assertEqual(ls.SETTINGS['blood_splatter'][0], (0, 200))
        self.assertEqual(ls.SETTINGS['blood_visor'][0], (0, 1))
        self.assertIn('budgetQuads', BS)


WH = (ROOT / 'src/wrist_hud.cpp').read_text(encoding='utf-8', errors='replace')
WHH = (ROOT / 'include/spidy/wrist_hud.hpp').read_text(encoding='utf-8')


class SubtextScroll(unittest.TestCase):
    """Holo subtext: wrapped to the panel, scrolled (stick, drag, auto after 1.5 s), never cut off silently."""

    def test_constants(self):
        self.assertIn('helpAutoDelay = 1.5f', WHH)
        self.assertIn('helpRowsShown = 3', WHH)

    def test_clipped_not_dropped(self):
        i = WH.index('void helpLines(const Painter&')
        body = WH[i:WH.index('\n}\n', i)]
        self.assertIn('clipBand(', body)
        self.assertIn('p.triangle(', body)  # the arrows

    def test_gate_present(self):
        CT = (ROOT / 'tests/core_tests.cpp').read_text(encoding='utf-8', errors='replace')
        self.assertIn('Update 38.5 SUBTEXT GATE', CT)
        self.assertIn('Update 38.5 SUBTEXT SCROLL', CT)
        self.assertTrue((ROOT / 'docs/images/subtext-385-sheet.png').is_file())


if __name__ == '__main__':
    unittest.main()
