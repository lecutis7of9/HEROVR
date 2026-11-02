"""0.0.76 LIGHTS LOCK (owner's Batman report): the game's lens flares stay off in VR for every hero and every
live.txt value, so headlights and street lights never shine through buildings again (the 0.0.62 fix)."""
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import live_settings as L  # noqa: E402


def src(rel):
    return (ROOT/rel).read_text(encoding='utf-8', errors='replace')


class LightsLock(unittest.TestCase):
    def test_every_live_txt_value_keeps_flares_off(self):
        for text in (None, '', 'lens_flares = game\n', 'lens_flares = 0\n', 'lens_flares = off_in_vr\n',
                     'lens_flares = 1\n', L.MIGRATED_073T5 + '\nlens_flares = game\n', 'garbage = = =\n'):
            with self.subTest(text=text):
                self.assertTrue(L.lens_flares_off(text))

    def test_no_hero_or_vehicle_gate(self):
        # One launch-time decision with no hero, suit or vehicle input: Batman, the Batmobile and every hero
        # switch get the same OFF.
        import inspect
        self.assertEqual(list(inspect.signature(L.lens_flares_off).parameters), ['text'])
        r = src('tools/run_game_vr.py')
        i = r.index('if live_settings.lens_flares_off(live_text):')
        block = r[i:r.index('a.session_off = session_off', i)]
        self.assertNotIn('hero', block.split('print(')[0].lower())
        self.assertIn("session_off = ('lens_flares',)", block)

    def test_game_value_is_reported_not_used(self):
        self.assertTrue(L.lens_flares_game_ignored('lens_flares = game\n'))
        self.assertIn('lens_flares_game_ignored(live_text)', src('tools/run_game_vr.py'))
        self.assertNotIn("LENS FLARES: GAME this session", src('tools/run_game_vr.py'))

    def test_wrist_row_says_so_and_needs_no_restart(self):
        c = src('include/spidy/combat38.hpp')
        self.assertIn('"Flares stay off in VR for every hero, GAME is for flat play only, no restart."', c)
        w = src('src/wrist_hud.cpp')
        tag = w[w.index('bool restartTag(Control c) {'):]
        tag = tag[:tag.index('}')]
        self.assertNotIn('combatLensFlares', tag)
        self.assertNotIn('takes effect the next time', L.RESTART['lens_flares'])

    def test_vr_display_still_turns_the_game_setting_off(self):
        import vr_display
        self.assertEqual(vr_display.SESSION_OFF['lens_flares'], {'LensFlares': 0})


if __name__ == '__main__':
    unittest.main()
