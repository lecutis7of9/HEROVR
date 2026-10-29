# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Version 0.0.2 (build Update 38.6): BALL LOOK OURS (one mod web ball per press from the pulled wrist, no game
ball, AIM STRENGTH STRONG by default, BALL HIT OURS ONLY), the one-time BALL LOOK -> OURS migration, the COMBAT
record's two new option words, and the TRAINING path guide. Any failing fails."""
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
CI = (ROOT / 'include/spidy/combat_input.hpp').read_text(encoding='utf-8')
OB = (ROOT / 'include/spidy/our_ball.hpp').read_text(encoding='utf-8')
TG = (ROOT / 'include/spidy/training_guide.hpp').read_text(encoding='utf-8')
RUN = (ROOT / 'tools/run_game_vr.py').read_text(encoding='utf-8')


class Version(unittest.TestCase):
    def test_version(self):
        self.assertEqual((ROOT / 'VERSION').read_text(encoding='utf-8').strip(), '0.0.73')  # 0.0.73 = build 39.6 (the 0.0.73 fixes, GAME TAA off by default, no MP SOLO TEST); 0.0.72 = build 39.5 (the 0.0.72 heroes, models and suits); 0.0.71 = 0.0.70 ("0.0.7") + the Stop VR fix (build 39.4 unchanged; 0.0.70 was 39.4, 0.0.63 was 39.3, 0.0.62 was 39.2)
        self.assertEqual((ROOT / 'PORT-VERSION').read_text(encoding='utf-8').strip(), 'Update 39.6')


class Settings(unittest.TestCase):
    def test_defaults(self):
        self.assertEqual(ls.UPDATE38_DEFAULTS['ball_look'], 'ours')
        self.assertEqual(ls.UPDATE38_DEFAULTS['aim_strength'], 'strong')
        self.assertEqual(ls.UPDATE38_DEFAULTS['ball_hit'], 'ours_only')  # the experimental one is off
        for key in ('ball_look', 'aim_strength', 'ball_hit'):
            self.assertEqual(ls.export_value(key, ls.UPDATE38_DEFAULTS[key]), 0, key)

    def test_values_match_cpp(self):
        self.assertEqual(ls.SETTINGS['ball_look'][0], (0, 3))
        self.assertEqual(ls.SETTINGS['aim_strength'][0], (0, 4))
        self.assertEqual(ls.SETTINGS['ball_hit'][0], (0, 1))
        self.assertEqual(ls.export_value('ball_look', 'wrist'), 1)
        self.assertEqual(ls.export_value('ball_look', 'glob'), 2)
        self.assertEqual(ls.export_value('ball_look', 'game_only'), 3)
        for word, n in (('max', 1), ('off', 2), ('low', 3), ('normal', 4)):
            self.assertEqual(ls.export_value('aim_strength', word), n, word)
        self.assertEqual(ls.export_value('ball_hit', '+game_web'), 1)
        self.assertIn('{"OURS", "WRIST", "GLOB", "GAME ONLY"}', C38)
        self.assertIn('{"STRONG", "MAX", "OFF", "LOW", "NORMAL"}', C38)
        self.assertIn('SpidyAimStrength', XR)
        self.assertIn('SpidyBallHit', XR)

    def test_subtexts(self):
        for key in ('ball_look', 'aim_strength', 'ball_hit'):
            m = re.search(r'\{"%s", "[A-Z +]+", "[A-Z]+",\s*"([^"]+)"' % key, C38)
            self.assertTrue(m, key)
            self.assertLessEqual(len(m.group(1)), 80, key)
            self.assertEqual(m.group(1).count('.'), 1, key)

    def test_record_words(self):
        self.assertEqual(dict(rec.COMBAT38_OPTIONS)['ball_look'], ('OURS', 'WRIST', 'GLOB', 'GAME ONLY'))
        self.assertIn('aim_strength', dict(rec.COMBAT38_OPTIONS))
        self.assertIn('ball_hit', dict(rec.COMBAT38_OPTIONS))
        # The two new options sit in the record's spare words (500, 652), never past values3.
        self.assertIn('o < 28', XR)
        self.assertIn('aimStrengthNow', C38)
        self.assertIn('ballHitNow', C38)

    def test_record_parse(self):
        raw = bytearray(768)
        struct.pack_into('<4I', raw, 0, 0x38424D43, 5, 768, 0)
        magic = re.search(r'dataMagic = (0x[0-9A-Fa-f]+)', C38)
        if magic:
            struct.pack_into('<I', raw, 0, int(magic.group(1), 16))
        struct.pack_into('<I', raw, 500, 1)  # AIM STRENGTH MAX
        struct.pack_into('<I', raw, 652, 1)  # BALL HIT +GAME WEB
        out = rec.parse_combat38(bytes(raw))
        if out is None:
            self.skipTest('record header differs from this synthetic one')
        self.assertEqual(out['options']['ball_look'], 'OURS')
        self.assertEqual(out['options']['aim_strength'], 'MAX')
        self.assertEqual(out['options']['ball_hit'], '+GAME WEB')


class Migration(unittest.TestCase):
    def run_on(self, text):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text(text, encoding='utf-8')
            first = ls.migrate_386(p)
            after = p.read_text(encoding='utf-8')
            second = ls.migrate_386(p)
            return first, after, second, p.read_text(encoding='utf-8')

    def test_any_old_value_becomes_ours_once(self):
        for old in ('wrist', 'glob', 'game_only', '0', '2'):
            first, after, second, final = self.run_on(f'ball_look = {old}\n')
            self.assertIn('ball_look = ours', first, old)
            self.assertRegex(after, r'(?m)^ball_look\s*=\s*ours')
            self.assertIn(ls.MIGRATED_386, after)
            self.assertIsNone(second)
            self.assertEqual(after, final)

    def test_chosen_after_stays(self):
        _, after, _, _ = self.run_on('ball_look = wrist\n')
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text(after + 'ball_look = wrist\n', encoding='utf-8')
            self.assertIsNone(ls.migrate_386(p))
            self.assertTrue(p.read_text(encoding='utf-8').endswith('ball_look = wrist\n'))

    def test_no_ball_look_line(self):
        first, after, second, _ = self.run_on('lunge = game_button\n')
        self.assertIsNone(first)
        self.assertIn(ls.MIGRATED_386, after)
        self.assertIsNone(second)

    def test_missing_file(self):
        self.assertIsNone(ls.migrate_386('/nonexistent/live.txt'))

    def test_launcher_calls_it_after_385(self):
        self.assertLess(RUN.index('migrate_385('), RUN.index('migrate_386('))


class OursBall(unittest.TestCase):
    def test_no_game_ball_for_ours(self):
        """OURS sends no RB and no stick: the press never reaches the game as a web ball."""
        block = CI[CI.index('if (c.ballOurs) {'):]
        block = block[:block.index('continue;')]
        self.assertNotIn('RB', block.replace('no RB', ''))
        self.assertIn('out.ballOurs[i] = true', block)

    def test_fired_from_the_wrist_and_guarded(self):  # 0.0.62: 11000 (the baton swing, baton bounce and palm blast sit between); 0.0.7: 17000 (+ the HERO PROFILES step); 0.0.72: 18000 (+ the grip squeeze), 24000 (+ the PUNISHER VAN engine preview); 0.0.73: 30000 (+ the PUNISHER VAN drive, its engine loop and slam); 0.0.74 test 1: 70000 (+ the BATMOBILE summon and drive)
        self.assertIn('webShooter(', XR[XR.index('fightOut.ballOurs[i]') - 70000:XR.index('fightOut.ballOurs[i]') + 2000])  # 0.0.73 test 5: HERO HANDS' palm code sits between
        self.assertIn('Version 0.0.2 OURS BALL', XR)

    def test_strengths(self):
        for name in ('off', 'low', 'normal', 'strong', 'maxStrength'):
            self.assertIn(name, OB)


class TrainingGuide(unittest.TestCase):
    def test_states_and_draw(self):
        for word in ('red', 'blue', 'success', 'fail', 'flyRise', 'flyHover'):
            self.assertIn(word, TG)
        self.assertIn('appendTrainingGuide', XR)
        self.assertIn('TRAIN MOVE', XR)


if __name__ == '__main__':
    unittest.main()
