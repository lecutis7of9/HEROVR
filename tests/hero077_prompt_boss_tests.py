# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.77 PROMPT WINDOW and BOSS DAMAGE (both CANDIDATE).

live.txt prompt_window (SpidyPromptWindow 5566), boss_damage (SpidyBossDamage 5567);
include/spidy/story077c.hpp (tests/story077c_tests.cpp), the renderer's inset, world_trace's nearestBoss.
"""
import pathlib
import re
import subprocess
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402

NEW = ('include/spidy/story077c.hpp', 'tests/story077c_tests.cpp', 'tests/hero077_prompt_boss_tests.py')
KEYS = {'prompt_window': ('SpidyPromptWindow', 5566, ('on', 'large', 'off')),
        'boss_damage': ('SpidyBossDamage', 5567, ('on', 'off', 'strong'))}


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Settings(unittest.TestCase):
    def test_settings(self):
        for key, (export, _code, names) in KEYS.items():
            rng, exp, text = L.SETTINGS[key]
            self.assertEqual(exp, export)
            self.assertEqual(rng, (0, len(names) - 1))
            self.assertEqual(L.CHOICE_NAMES[key], names)
            self.assertEqual([L.CHOICES[key][n] for n in names], list(range(len(names))))
            self.assertIn(key, L.START_ONLY)
            self.assertIn('live, no restart needed', text)


class Wiring(unittest.TestCase):
    def setUp(self):
        self.game = src('src/game_xr.cpp')

    def test_exports(self):
        for key, (export, code, names) in KEYS.items():
            self.assertRegex(self.game, export + r'\(void\* v\) \{ return setLive\(v, 0, ' + str(len(names) - 1) +
                             ', ' + str(code) + r', \w+, asU32\); \}')
            self.assertEqual(len(re.findall(r'\b%d\b' % code, self.game)), 1, code)
        self.assertIn('std::atomic<uint32_t> promptWindowNow{0};', self.game)
        self.assertIn('std::atomic<uint32_t> bossDamageNow{0};', self.game)  # 0 = on by default
        self.assertIn('if (bossMode != 1 && heroNow() != hero_modes::spiderMan) {', self.game)  # 1 = off

    def test_prompt_window(self):
        g = self.game
        self.assertIn('promptWindowNow.load() != 2)); // 0.0.77 PROMPT WINDOW', g)
        at = g.index('HERO VR 0.0.77 PROMPT WINDOW (CANDIDATE, live.txt prompt_window')
        self.assertLess(at, g.index('overlay.renderViews(overlayViews, vertices, true, false, translucent', at))
        body = g[at:g.index('overlay.renderViews(overlayViews', at)]
        self.assertIn('overlay.setInset(', body)
        self.assertIn('overlay.clearInset();', body)
        self.assertIn('catch (...)', body)
        self.assertIn('!saved->stereoScreen', body)
        r = src('src/d3d12_renderer.cpp')
        self.assertIn('insetFault_ = std::string("PROMPT WINDOW off: ")', r)
        self.assertIn('Constants.Num32BitValues = 6;', r)
        self.assertIn('float2 targetOrigin', r)

    def test_boss_proxy_weapons_only(self):
        g = self.game
        motion = g.index('oursMotion.update(static_cast<float>(combatClock), foes);')
        proxy = g.index('HERO VR 0.0.77 BOSS DAMAGE (CANDIDATE, live.txt boss_damage)')
        self.assertLess(motion, proxy)  # after the lock / lunge: nothing steers to the stand-in
        self.assertLess(proxy, g.index('oursBalls.update(frame.seconds, foes', proxy))
        body = g[proxy:g.index('const uint32_t strength = c38(combat38::aimStrength);', proxy)]
        self.assertIn('heroNow() != hero_modes::spiderMan', body)
        self.assertIn('sm2_damage::watchBoss(', body)
        self.assertIn('HERO VR 0.0.77 BOSS DAMAGE (CANDIDATE) %s', body)
        self.assertIn('heroNow() == hero_modes::hulk', g)
        d = src('src/sm2_damage.cpp')
        self.assertIn('watchBoss', d)

    def test_no_forbidden_words(self):
        for p in NEW:
            text = src(p).lower()
            self.assertNotIn('jim' + 'jam', text, p)
            self.assertNotRegex(text, r'\bmil' + r'es\w*', p)
        added = subprocess.run(['git', 'diff', 'd1640aa', '--', 'src', 'include', 'tools', 'tests', 'CMakeLists.txt'],
                               cwd=ROOT, capture_output=True, text=True).stdout
        plus = '\n'.join(l for l in added.splitlines() if l.startswith('+')).lower()
        self.assertNotIn('jim' + 'jam', plus)
        self.assertNotRegex(plus, r'\bmil' + r'es\w*\b')


if __name__ == '__main__':
    unittest.main()
