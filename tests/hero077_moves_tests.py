# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.77 STORY MOVES: abilities from VR X held, the throw's aim, the dodge everywhere.

live.txt story_abilities (SpidyStoryAbilities 5561), story_throw_aim (5562), story_dodge (5563), head_dodge (5564);
include/spidy/story077b.hpp (tests/story077b_tests.cpp), the STRB record.
"""
import pathlib
import re
import struct
import subprocess
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402
import update37_records as R  # noqa: E402

NEW = ('include/spidy/story077b.hpp', 'tests/story077b_tests.cpp', 'tests/hero077_moves_tests.py')
KEYS = {'story_abilities': ('SpidyStoryAbilities', 5561, ('on', 'off')),
        'story_throw_aim': ('SpidyStoryThrowAim', 5562, ('look', 'point', 'off')),
        'story_dodge': ('SpidyStoryDodge', 5563, ('on', 'off')),
        'head_dodge': ('SpidyHeadDodge', 5564, ('fight', 'always', 'off'))}


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class MovesSettings(unittest.TestCase):
    def test_settings(self):
        for key, (export, _code, names) in KEYS.items():
            rng, exp, text = L.SETTINGS[key]
            self.assertEqual(exp, export)
            self.assertEqual(rng, (0, len(names) - 1))
            self.assertEqual(L.CHOICE_NAMES[key], names)
            self.assertEqual([L.CHOICES[key][n] for n in names], list(range(len(names))))
            self.assertIn(key, L.START_ONLY)
            self.assertIn('live, no restart needed', text)


class MovesWiring(unittest.TestCase):
    def setUp(self):
        self.game = src('src/game_xr.cpp')

    def test_exports(self):
        for key, (export, code, names) in KEYS.items():
            self.assertRegex(self.game, export + r'\(void\* v\) \{ return setLive\(v, 0, ' + str(len(names) - 1) +
                             ', ' + str(code) + r', \w+, asU32\); \}')
            self.assertEqual(len(re.findall(r'\b%d\b' % code, self.game)), 1, code)
        self.assertIn('__declspec(dllexport) spidy::story077b::Data SpidyStoryMovesData;', self.game)

    def test_dodge_everywhere(self):
        self.assertIn('storyDodge.update(frame.seconds, canDodge, (origins.play() & buttonB) != 0, side)', self.game)
        self.assertIn('(storyDodgeNow.load() == 0 && walk.dodge && (inFlight || onWall))', self.game)
        self.assertIn('mark->nowCued = true;', self.game)

    def test_abilities_before_submit(self):
        at = self.game.index('storyAbility.update(ai)')
        self.assertLess(at, self.game.index('game_pad::submit(pad, 200);'))
        end = self.game.index('// Version 0.0.4 HERO ABILITIES', at)
        self.assertLess(end, self.game.index('hero_modes::stripPad', at))
        body = self.game[at:end]
        self.assertNotIn('game_grab', body)
        self.assertNotIn('web_grab', body)

    def test_phone_card(self):
        phone = src('src/phone.cpp')
        self.assertIn('"STORY MOVES"', phone)
        self.assertIn('movesScreen(out, p);', phone)

    def test_no_forbidden_words(self):
        for p in NEW:
            text = src(p).lower()
            self.assertNotIn('jim' + 'jam', text, p)
            self.assertNotIn('mil' + 'es', text, p)
        added = subprocess.run(['git', 'diff', '081c102', '--', 'src', 'include', 'tools', 'CMakeLists.txt'],
                               cwd=ROOT, capture_output=True, text=True).stdout
        plus = '\n'.join(l for l in added.splitlines() if l.startswith('+')).lower()
        self.assertNotIn('jim' + 'jam', plus)
        self.assertNotRegex(plus, r'\bmil' + r'es\w*\b')


class MovesRecord(unittest.TestCase):
    def test_record(self):
        self.assertEqual(R.RECORDS['story_moves'], ('SpidyStoryMovesData', 0x42525453, 1, 128))
        vals = [1, 0, 1, 0, 3, 1, 0, 0, 2, 1, 5, 1, 400, 1, 4, 30, 2]
        raw = struct.pack('<3I', 0x42525453, 1, 128) + struct.pack('<17If6I3I', *vals, 12.5, 7, 3, 2, 1, 9, 4,
                                                                     0, 0, 0) + struct.pack('<Q', 2)
        self.assertEqual(len(raw), 128)
        d = R.parse_story_moves(raw)
        self.assertTrue(d['abilities_on'])
        self.assertEqual(d['aim'], 'LOOK')
        self.assertEqual(d['fired']['LB+X'], 3)
        self.assertEqual(d['last_ability'], 'LB+X')
        self.assertEqual(d['aimed_throws'], 4)
        self.assertEqual(d['last_aim_degrees'], 12.5)
        self.assertEqual(d['dodges_in_air'], 3)
        self.assertEqual(d['sense_now_cues'], 9)
        self.assertIn('story_moves', R.PARSERS)
        self.assertIn('static_assert(sizeof(Data) == 128);', src('include/spidy/story077b.hpp'))
        lines = R.console_lines({'story_moves': d})
        if lines is not None:
            self.assertTrue(any('STORY MOVES' in l for l in lines))


if __name__ == '__main__':
    unittest.main()
