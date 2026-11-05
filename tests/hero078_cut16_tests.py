# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.78 cut16 SANDMAN: the boss pick (the boss hit last, else the nearest living), the gate on his HP after
the debris throws, the plain Y in a boss fight and the boss_gate switch (5644); cut15's punch change reverted."""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Cut16(unittest.TestCase):
    def test_boss_gate_switch(self):
        g = src('src/game_xr.cpp')
        self.assertIn('WINAPI SpidyBossGate(void* v)', g)
        self.assertEqual(len(re.findall(r'\b5644\b', g)), 1)
        key = 'boss_gate'
        self.assertEqual(live_settings.SETTINGS[key][1], 'SpidyBossGate')
        self.assertTrue(live_settings.SETTINGS[key][2].endswith('- live, no restart needed'))
        self.assertIn(key, live_settings.START_ONLY)
        self.assertEqual(live_settings.CHOICE_NAMES[key], ('on', 'off'))
        self.assertIn('boss_gate', src('apps/launcher/ui.cpp'))

    def test_wiring(self):
        g = src('src/game_xr.cpp')
        self.assertIn('!motion.standing && !yPlain16', g)
        self.assertIn('bossGate16.thrown(r.have0, r.hp0, r.lowest, r.most0, !cut)', g)
        self.assertIn('HERO VR 0.0.78 BOSS GATE', g)
        self.assertIn('HERO VR 0.0.78 BOSS Y #%u', g)
        self.assertIn('HERO VR 0.0.78 BOSS LOST', g)
        self.assertIn('bossLastName[0] = 0;', g)
        w = src('src/world_trace.cpp')
        self.assertIn('bossPick.pick(in.data(), bossN, bossNowSeconds)', w)
        self.assertNotIn('bossMetres = metres;', w)

    def test_cut15_punch_reverted(self):
        g = src('src/game_xr.cpp')
        self.assertNotIn('bossFight78', g)
        self.assertIn('cf.enemyCount > 0 || bossNear77', g)
        self.assertIn('PROP SKIP', g)


if __name__ == '__main__':
    unittest.main()
