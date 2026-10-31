# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.74 test 10 grapple fixes: QUICK HOOK, SURE GRAPPLE, GRAPPLE YANK (CANDIDATE).

The owner: "punishers grapple takes so long to fire", "half the time i press it multiple times to get it to
shoot", "grapples still dont grab enemies to reel them in". Three live settings, each ON by default.
"""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402

NEW = {'punisher_quick_hook': ('SpidyPunisherQuickHook', 'punisherQuickHook', 5530),
       'grapple_yank': ('SpidyGrappleYank', 'grappleYank', 5531),
       'sure_grapple': ('SpidySureGrapple', 'sureGrapple', 5532)}


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Settings(unittest.TestCase):
    def test_live_settings(self):
        for key, (export, _, _) in NEW.items():
            self.assertEqual(L.SETTINGS[key][0], (0, 1), key)
            self.assertEqual(L.SETTINGS[key][1], export, key)
            self.assertEqual(L.UPDATE38_DEFAULTS[key], 'on', key)
            self.assertIn(key, L.UPDATE38_KEYS)
            self.assertEqual(L.export_value(key, 'on'), 0)
            self.assertEqual(L.export_value(key, 'off'), 1)
            values, problems = L.parse(f'{key} = off\n')
            self.assertEqual((values.get(key), problems), ('off', []), key)

    def test_quick_hook_text(self):
        text = L.SETTINGS['punisher_quick_hook'][2]
        self.assertIn('instant you pull the trigger', text)
        self.assertIn('no restart', text)
        self.assertIn('CANDIDATE', L.SETTINGS['grapple_yank'][2])

    def test_dll_option_rows_and_exports(self):
        c = src('include/spidy/combat38.hpp')
        g = src('src/game_xr.cpp')
        for key, (export, enum, code) in NEW.items():
            row = re.search(r'\{"' + key + r'", "[^"]+", "[^"]+", "([^"]+)", 2, \{"ON", "OFF"', c)
            self.assertIsNotNone(row, key)
            help_text = row.group(1)
            self.assertLessEqual(len(help_text), 80, key)
            self.assertTrue(help_text.endswith('.') and help_text.count('.') == 1, key)
            self.assertIn(f'WINAPI {export}(void* v) {{ return setHero007(v, combat38::{enum}, {code}); }}', g)
            self.assertEqual(g.count(f'combat38::{enum}, '), 1, export)


class Wiring(unittest.TestCase):
    def test_yank_never_a_web(self):
        h = src('include/spidy/grapple074.hpp')
        self.assertNotIn('WebYankBot', h)
        g = src('src/game_xr.cpp')
        block = g[g.index('0.0.74 test 10 GRAPPLE YANK (CANDIDATE, grapple074.hpp)'):]
        block = block[:block.index('capeGrapplesSeen = capeNow.grapples;')]
        self.assertIn('sm2_damage::queue(', block)
        self.assertIn('grapple074::yankBlow()', block)
        self.assertNotIn('WebYank', block)
        self.assertIn('moved', block)

    def test_bridge_reports_reels(self):
        s = src('src/game_swing.cpp')
        self.assertIn('++capeData.reels;', s)
        self.assertIn('capeData.reelId = reelId;', s)
        self.assertIn('blow.toward', src('src/sm2_damage.cpp'))

    def test_latch_every_grappler(self):
        g = src('src/game_xr.cpp')
        self.assertIn('grappleLatch.ask(', g)
        self.assertIn('grappleLatch.answer(capeNow.grapples, capeNow.misses)', g)


if __name__ == '__main__':
    unittest.main()
