# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.77 STORY INPUTS: the hero switch (the game's own FNSM app, pressed for you) and the web throw.

live.txt story_swap (0 view, 1 dpad_left, 2 dpad_right, 3 off; SpidyStorySwap 5559) and story_throw (0 on, 1 off;
SpidyStoryThrow 5560), both live; include/spidy/story077.hpp (tests/story077_tests.cpp), the STRY record.
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

NEW = ('include/spidy/story077.hpp', 'tests/story077_tests.cpp', 'tests/hero077_story_tests.py')


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class StorySettings(unittest.TestCase):
    def test_swap(self):
        rng, export, text = L.SETTINGS['story_swap']
        self.assertEqual((rng, export), ((0, 3), 'SpidyStorySwap'))
        self.assertIn('no restart needed', text)
        self.assertEqual(L.CHOICE_NAMES['story_swap'], ('view', 'dpad_left', 'dpad_right', 'off'))
        self.assertEqual([L.CHOICES['story_swap'][k] for k in L.CHOICE_NAMES['story_swap']], [0, 1, 2, 3])
        self.assertIn('story_swap', L.START_ONLY)

    def test_throw(self):
        rng, export, text = L.SETTINGS['story_throw']
        self.assertEqual((rng, export), ((0, 1), 'SpidyStoryThrow'))
        self.assertIn('no restart needed', text)
        self.assertEqual(L.CHOICE_NAMES['story_throw'], ('on', 'off'))
        self.assertEqual(L.CHOICES['story_throw']['on'], 0)
        self.assertIn('story_throw', L.START_ONLY)


class StoryWiring(unittest.TestCase):
    def setUp(self):
        self.game = src('src/game_xr.cpp')

    def test_exports(self):
        self.assertRegex(self.game, r'SpidyStorySwap\(void\* v\) \{ return setLive\(v, 0, 3, 5559, storySwapNow, asU32\); \}')
        self.assertRegex(self.game, r'SpidyStoryThrow\(void\* v\) \{ return setLive\(v, 0, 1, 5560, storyThrowNow, asU32\); \}')
        self.assertEqual(len(re.findall(r'\b5559\b', self.game)), 1)
        self.assertEqual(len(re.findall(r'\b5560\b', self.game)), 1)
        self.assertIn('__declspec(dllexport) spidy::story077::Data SpidyStoryData;', self.game)

    def test_throw_in_the_air(self):
        self.assertIn('pullPress.update(frame.seconds, reaching || (storyThrow && inFlight)', self.game)
        self.assertIn('(storyThrow && inFlight && walk.pull)', self.game)  # the swing yields to the game's throw
        # only the game's LB + RB: never the custom grab
        body = self.game[self.game.index('HERO VR 0.0.77 STORY INPUTS: the game\'s web throw'):]
        body = body[:body.index('storyPulling = clickPulled;')]
        self.assertNotIn('game_grab', body)
        self.assertNotIn('web_grab', body)

    def test_swap_before_submit(self):
        at = self.game.index('storySwap.update(frame.seconds')
        self.assertLess(at, self.game.index('game_pad::submit(pad, 200);'))
        self.assertGreater(at, self.game.index('pausePress.update(frame.seconds'))
        self.assertIn('storySwap.cancel(); // the player\'s pause wins', self.game)
        self.assertIn('case PhoneAction::switchHero:', self.game)
        self.assertIn('"SWITCH HERO"', src('src/phone.cpp'))

    def test_swap_gameplay_only(self):
        # 0.0.77 launch audit: the head gesture and the switch's presses never start at the main menu.
        at = self.game.index('const auto touch = storyHead.update(')
        self.assertIn('mapping == game_pad::Mapping::gameplay && steering', self.game[at:at + 600])
        start = self.game.index('storySwap.start(route, marker)')
        self.assertIn('} else if (mapping != game_pad::Mapping::gameplay) {', self.game[start - 900:start])

    def test_no_forbidden_words(self):
        for p in NEW:
            text = src(p).lower()
            self.assertNotIn('jim' + 'jam', text, p)
            self.assertNotIn('mil' + 'es', text, p)
        added = subprocess.run(['git', 'diff', '792386b', '--', 'src', 'include', 'tools', 'CMakeLists.txt'],
                               cwd=ROOT, capture_output=True, text=True).stdout
        plus = '\n'.join(l for l in added.splitlines() if l.startswith('+')).lower()
        self.assertNotIn('jim' + 'jam', plus)
        self.assertNotRegex(plus, r'\bmil' + r'es\w*\b')


class StoryRecord(unittest.TestCase):
    def test_record(self):
        self.assertEqual(R.RECORDS['story'], ('SpidyStoryData', 0x59525453, 1, 96))
        vals = [0, 1, 3, 2, 1, 1, 1, 0, 0, 1, 4, 2, 5, 1, 300, 120, 6, 0, 0]
        raw = struct.pack('<3I19IQ', 0x59525453, 1, 96, *vals, 2)
        d = R.parse_story(raw)
        self.assertEqual(d['route'], 'HOLD VIEW')
        self.assertEqual(d['swap_starts'], 3)
        self.assertEqual(d['swap_last'], 'switched')
        self.assertEqual(d['throw_yanks_in_air'], 2)
        self.assertEqual(d['lb_rb_frames'], 300)
        self.assertIn('story', R.PARSERS)
        h = src('include/spidy/story077.hpp')
        self.assertIn('static_assert(sizeof(Data) == 96);', h)


if __name__ == '__main__':
    unittest.main()
