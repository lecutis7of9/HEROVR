# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 38.2 (run381/DIAGNOSIS.md): the five new COMBAT settings are exported with their own codes
(5320-5324), every 38.2 fix is the first value (0) of its setting and live, a 38.1 live.txt's numbers keep
their meaning (migrate_382, once), GADGET KEYS 38.2 puts the Grabber on RB+B, the COMBAT record is version 4
(664 bytes) and its console line names the 38.2 counters, the replay tests are in the gates, the graphics
defaults are untouched and no GOLD switch ships (the gold squares are the NG+ Golden Gadgets cosmetic, a
game setting). Any failing fails."""
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

XR = (ROOT / 'src/game_xr.cpp').read_text(encoding='utf-8', errors='replace')
C38 = (ROOT / 'include/spidy/combat38.hpp').read_text(encoding='utf-8')
CMAKE = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
VERSION = (ROOT / 'PORT-VERSION').read_text(encoding='utf-8')
NEW = dict(melee_anim=('SpidyMeleeAnim', 5320, 'every_hit'), gadget_keys=('SpidyGadgetKeys', 5321, '38.2'),
           grab_web=('SpidyGrabWeb', 5322, 'assist')  # Update 38.3: ASSIST first
           , mirror_look=('SpidyMirrorLook', 5323, 'suit'),
           gadget_feel=('SpidyGadgetFeel', 5324, 'easy'))
FIXES = dict(aim_assist='smart', punch_mode='targeted', ring_anchor='wrist', holo_open='forearm',
             ring_layer='hold', blood_look='brutal')


class Settings(unittest.TestCase):
    def test_version(self):
        self.assertRegex(VERSION, r'^Update (38\.[2-9]|39\.\d)')

    def test_exports_codes_defaults(self):
        for key, (export, code, default) in NEW.items():
            m = re.search(rf'DWORD WINAPI {export}\(void\* v\) \{{\s*return setLive\(v, 0, [^,]+, (\d+),', XR)
            self.assertIsNotNone(m, export)
            self.assertEqual(int(m.group(1)), code, export)
            self.assertEqual(ls.SETTINGS[key][1], export)
            self.assertEqual(ls.UPDATE38_DEFAULTS[key], default, key)
            self.assertEqual(ls.export_value(key, default), 0, key)
        codes = [int(c) for c in re.findall(r'combat38::\w+\]\.count - 1, (\d+),', XR)]
        self.assertEqual(len(codes), len(set(codes)), 'two COMBAT settings share a refusal code')

    def test_every_fix_is_value_0(self):
        for key, word in FIXES.items():
            self.assertEqual(ls.UPDATE38_DEFAULTS[key], word, key)
            self.assertEqual(ls.export_value(key, word), 0, key)
        for key, word in ls.UPDATE38_DEFAULTS.items():
            self.assertEqual(ls.export_value(key, word), 0, key)

    def test_plan_b_exists(self):
        """Every 38.2 setting has a second value (its plan B), so no switch is a one-way door."""
        for key in list(NEW) + list(FIXES):
            low, high = ls.SETTINGS[key][0]
            self.assertGreaterEqual(high, 1, key)

    def test_help_matches_spacing(self):
        self.assertIn('meleeSpacing = .65f', C38)
        self.assertIn('0.65 s apart', ls.SETTINGS['melee_anim'][2])

    def test_no_gold_switch(self):
        self.assertNotIn('gold', ' '.join(ls.SETTINGS).lower())
        self.assertNotIn('0x2c98ed0', XR.lower())


class Migrate(unittest.TestCase):
    def test_numbers_keep_meaning_once(self):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text('ring_layer = 1\naim_assist = 2\nhistory = 35\n', encoding='utf-8')
            line = ls.migrate_382(p)
            self.assertIsNotNone(line)
            text = p.read_text(encoding='utf-8')
            self.assertIn(ls.MIGRATED_382, text)
            self.assertIn('history = 35', text)  # graphics untouched
            self.assertNotRegex(text, r'(?m)^ring_layer = 1$')
            again = p.read_text(encoding='utf-8')
            self.assertIsNone(ls.migrate_382(p))
            self.assertEqual(p.read_text(encoding='utf-8'), again)

    def test_missing_file_is_fine(self):
        self.assertIsNone(ls.migrate_382('/nonexistent/live.txt'))


class Gadgets(unittest.TestCase):
    def test_grabber_on_b(self):
        self.assertIn('Grabber RB+B', C38)
        self.assertIn('inline GadgetSlots gadgetSlots(uint32_t keys, GadgetSlots custom)', C38)


class Record(unittest.TestCase):
    def test_v4_parses_and_console(self):
        _, magic, version, size = rec.RECORDS['combat38']
        self.assertEqual((version, size), (5, 768))  # Update 38.3: version 5, 38.2's fields in place
        raw = bytearray(size)
        struct.pack_into('<4I', raw, 0, magic, version, size, 0)
        struct.pack_into('<13Q', raw, 536, 11, 900, 40, 30, 3, 1, 9, 2, 1, 0, 25, 6, 5000)
        struct.pack_into('<2f', raw, 640, .5, 1.5)
        struct.pack_into('<I', raw, 648, 21)
        d = rec.parse_combat38(bytes(raw))
        self.assertEqual(d['holo_forearm_opens'], 11)
        self.assertEqual(d['ring_held_frames'], 900)
        self.assertEqual(d['targeted_held_off'], 40)
        self.assertEqual(d['mirror_suit_frames'], 5000)
        self.assertEqual(d['mirror_joints'], 21)
        self.assertEqual((d['ring_wrist_cm'], d['ring_wrist_peak_cm']), (0.5, 1.5))
        lines = rec.console_lines({'combat38': d})
        self.assertTrue(any(l.startswith('Update 38.2: holo forearm opens 11') for l in lines), lines)
        self.assertIn('offsetof(Data, sequence) == 760', C38)  # Update 38.3


class Gates(unittest.TestCase):
    def test_replay_in_ctest(self):
        self.assertIn('add_executable(spidy_replay_tests tests/replay_tests.cpp)', CMAKE)
        self.assertIn('add_test(NAME replay COMMAND spidy_replay_tests)', CMAKE)
        for run in ('run36', 'run371', 'run372', 'run38', 'run381'):
            f = ROOT / 'tests/replay' / f'{run}.csv'
            self.assertTrue(f.exists(), run)
            self.assertEqual(len(f.read_text(encoding='utf-8').splitlines()[1].split(',')), 32, run)


class Docs(unittest.TestCase):
    GOLDEN = ('Pause > Gadgets > select Web Shooters > highlight the LAST upgrade (Golden) > toggle it off; '
              'repeat for each gadget')

    def test_golden_steps(self):
        for doc in ('docs/combat38.md', 'docs/CHANGELOG.md', 'BUILD-SM2.md'):
            self.assertIn(self.GOLDEN, (ROOT / doc).read_text(encoding='utf-8'), doc)

    def test_ball_waits_for_a_forming_gadget(self):
        ci = (ROOT / 'include/spidy/combat_input.hpp').read_text(encoding='utf-8')
        self.assertIn('easyBallWait', ci)
        self.assertIn('!gadgetForming', ci)

    def test_yellow_check_in_replay(self):
        rt = (ROOT / 'tests/replay_tests.cpp').read_text(encoding='utf-8')
        self.assertIn('yellowMarks', rt)
        self.assertIn('WRIST is within 2 cm', rt)


class Graphics(unittest.TestCase):
    def test_graphics_defaults_unchanged(self):
        self.assertEqual(ls.UPDATE37_DEFAULTS, {
            'history': 80, 'sharpen': 140, 'trail': 0, 'taa_jitter': 3, 'hud_theme': 'raimi', 'web_width': 1.8,
            'web_speed': 70, 'reticles': 0, 'aa': 1, 'eye_temporal': 1, 'far_shadows': 2, 'eye_bloom': 1,
            'shared_lod': 2, 'bloom_fx': 0, 'aim_steer': 0, 'ring_face': 0, 'wall_crawl': 0, 'web_bend': 100,
            'wall_body': 0, 'zip_strike': 1, 'web_yank': 1, 'npc_fade': 'off', 'color_fx': 0, 'foveated': 'off'})
        self.assertEqual(ls.STARTUP_ON, {'eye_temporal': 1, 'far_shadows': 2, 'sharpen': 140})


if __name__ == '__main__':
    unittest.main()
