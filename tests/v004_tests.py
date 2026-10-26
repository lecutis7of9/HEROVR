#!/usr/bin/env python3
"""Version 0.0.4 (build Update 38.8): version, the new settings' defaults and subtexts, docs."""
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402
import update37_records as rec  # noqa: E402

COMBAT38 = (ROOT / 'include/spidy/combat38.hpp').read_text(encoding='utf-8')
GAME = (ROOT / 'src/game_xr.cpp').read_text(encoding='utf-8')


def info(key):
    m = re.search(r'\{"%s",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)"' % key, COMBAT38)
    assert m, key
    return m.group(1), m.group(3)


class V004(unittest.TestCase):
    def test_version(self):
        # 0.0.4 was build Update 38.8 (0.0.5's bump moved VERSION on; the changelog keeps it).
        self.assertIn('**Version 0.0.4 (build Update 38.8', (ROOT / 'docs/CHANGELOG.md').read_text(encoding='utf-8'))

    def test_new_settings_default_web_and_off(self):
        self.assertEqual(rec.V004_OPTIONS, (('iron_man', ('OFF', 'ON')), ('mirror_view', ('OFF', 'TEST'))))
        self.assertEqual(L.export_value('iron_man', L.UPDATE38_DEFAULTS['iron_man']), 0)
        self.assertEqual(L.export_value('mirror_view', L.UPDATE38_DEFAULTS['mirror_view']), 0)

    def test_subtexts(self):
        for key in ('iron_man', 'mirror_view', 'rope_grab'):
            _, help_text = info(key)
            self.assertLessEqual(len(help_text), 80, key)
            self.assertEqual(help_text.count('.'), 1, key)
        self.assertIn('restart', info('mirror_view')[1])

    def test_repulsor_never_webs(self):
        # makeConfig: REPULSOR forces our own shot at the press, no trigger gadgets (no RB, no Sonic).
        self.assertRegex(GAME, r'heroNow\(\) != hero_modes::spiderMan\) \{\s*c\.ballOurs = true;\s*c\.ballOnPress = true;'
                               r'\s*c\.noTriggerGadgets = true;')
        # 0.0.6: every hero but SPIDER-MAN: no wrist muzzle (game ball), no web ball, no glob
        self.assertIn("heroNow() == hero_modes::spiderMan; // 0.0.6: every hero but SPIDER-MAN", GAME)
        self.assertIn("for (unsigned i = 0; i < 2 && heroWas == hero_modes::spiderMan; ++i)", GAME)
        self.assertIn("heroNow() == hero_modes::spiderMan) { // 0.0.4/0.0.6: no glob for any other hero", GAME)
        self.assertIn("if (!plan.webSplash) { // REPULSOR", GAME)  # no splash, cocoon or game web
        self.assertIn("real damage (CONFIRMED path)", GAME)  # 0.0.6: rep0050 CONFIRMED the blows

    def test_hero_abilities_strip_every_web_path_in_game(self):
        # One choke point for the game's inputs, plus the mod's own web paths, all gated on the hero.
        self.assertIn('hero_modes::stripPad(heroNow(), pad.buttons, pad.rightTrigger,', GAME)
        self.assertLess(GAME.index('hero_modes::stripPad(heroNow()'), GAME.index('game_pad::submit(pad, 200);'))
        self.assertIn('const uint32_t grabMode = heroWebs ? c38(combat38::grabWeb) : 2u;', GAME)
        self.assertIn('c38(combat38::ropeGrab) == 1 && heroWebs;', GAME)
        self.assertIn('hero_modes::webPaths(heroNow()).gadgetWheel;', GAME)
        label, help_text = info('iron_man')
        self.assertEqual(label, 'IRON MAN ABILITIES')
        self.assertIn('no webs', help_text)

    def test_mirror_view_test_wired(self):
        self.assertIn('SpidyMirrorViewWant(mirrorWant);', GAME)
        self.assertIn('d.testHeroDraws = mirrorStats.heroDraws;', GAME)
        self.assertIn('RESTART THE GAME TO RUN MIRROR VIEW TEST', GAME)
        doc = (ROOT / 'docs/mirror.md').read_text(encoding='utf-8')
        self.assertIn('single headset test', doc)


if __name__ == '__main__':
    unittest.main()
