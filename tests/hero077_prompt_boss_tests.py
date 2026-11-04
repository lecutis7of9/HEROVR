# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""HERO VR 0.0.77 PROMPT WINDOW (debug only now), PROMPT CARDS and BOSS DAMAGE (all CANDIDATE).

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

NEW = ('include/spidy/story077c.hpp', 'tests/story077c_tests.cpp', 'tests/hero077_prompt_boss_tests.py',
       'include/spidy/story077d.hpp', 'tests/story077d_tests.cpp', 'include/spidy/prompt_cards.hpp',
       'src/prompt_cards.cpp')
# HERO VR 0.0.77 PROMPT CARDS: prompt_window 0 off (default) / 1 debug_full; prompt_cards 0 on (default) / 1 off.
KEYS = {'prompt_window': ('SpidyPromptWindow', 5566, ('off', 'debug_full')),
        'prompt_cards': ('SpidyPromptCards', 5568, ('on', 'off')),
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
        self.assertIn('std::atomic<uint32_t> promptWindowNow{story077d::windowOff};', self.game)
        self.assertIn('std::atomic<uint32_t> promptCardsNow{story077d::cardsOn};', self.game)
        self.assertIn('std::atomic<uint32_t> bossDamageNow{0};', self.game)  # 0 = on by default
        # CUT7: the boss is read for every hero (1 = off); the stand-in only for the others.
        self.assertIn('const bool bossHero = heroNow() != hero_modes::spiderMan;', self.game)
        self.assertIn('if (bossMode != 1) {', self.game)
        self.assertIn('bossId = bossHero ? boss.id : 0;', self.game)

    def test_prompt_window(self):
        g = self.game
        # The whole game frame only with prompt_window = debug_full (b8b33ce showed it by default).
        self.assertIn('promptWindowNow.load() == story077d::windowDebugFull ||', g)
        self.assertNotIn('promptWindowNow.load() != 2', g)
        at = g.index('HERO VR 0.0.77 PROMPT WINDOW (live.txt prompt_window; story077c.hpp), now DEBUG ONLY')
        self.assertLess(at, g.index('overlay.renderViews(overlayViews, vertices, true, false, translucent', at))
        body = g[at:g.index('overlay.renderViews(overlayViews', at)]
        self.assertIn('if (!promptSet && promptMode == story077d::windowDebugFull && !saved->flatScreen', body)
        self.assertEqual(body.count('overlay.setInset('), 2)  # CUT5: the wrist map's, then the debug window's
        self.assertLess(body.index('HERO VR 0.0.77 WRIST MAP'), body.index('if (!promptSet && promptMode'))
        self.assertIn('overlay.clearInset();', body)
        self.assertIn('catch (...)', body)
        self.assertIn('!saved->stereoScreen', body)
        # Elsewhere only HERO VR 0.0.77 CUTSCENES' two (the scene's words, on the screen or the scene's panel),
        # both in the screen's own branches (showScreen), never in play.
        self.assertEqual(g.count('overlay.setInset('), 4)
        for at in (g.index('if (showScreen && cutImmersive'), g.index('if (strip3d && !overlayFault)')):
            self.assertLess(at, g.index('overlay.setInset(', at))
        r = src('src/d3d12_renderer.cpp')
        self.assertIn('insetFault_ = std::string("PROMPT WINDOW off: ")', r)
        self.assertIn('Constants.Num32BitValues = 12;', r)  # 0.0.77 CUTSCENES: the crop and the key
        self.assertIn('float2 targetOrigin', r)

    def test_old_prompt_window_lines(self):
        # The player's live.txt (b8b33ce's 'prompt_window = off'), and the old on / large / 2: no full frame,
        # and none of them turns the cards off.
        for line in ('prompt_window = off', 'prompt_window = on', 'prompt_window = large', 'prompt_window = 2',
                     'prompt_window = 0'):
            values, notes = L.parse(line + '\n')
            self.assertEqual(values, {'prompt_window': 'off'}, line)
            self.assertEqual(notes, [], line)
            self.assertNotIn('prompt_cards', values)
        values, notes = L.parse('prompt_window = banana\nprompt_cards = 7\n')
        self.assertEqual(values, {})
        self.assertEqual(len(notes), 2)
        self.assertEqual(L.parse('prompt_window = debug_full\n')[0], {'prompt_window': 'debug_full'})

    def test_prompt_cards(self):
        g = self.game
        at = g.index('HERO VR 0.0.77 PROMPT CARDS (CANDIDATE, story077d.hpp, live.txt prompt_cards)')
        body = g[at:g.index('// Update 37: the web shooter and the gadgets by gesture', at)]
        # only in VR gameplay: never at the menus, never flat
        # HERO VR 0.0.77 WAYPOINT: the same scan serves the cards and the waypoint (tests/hero077_waypoint_tests.py).
        self.assertIn('const bool vrGameplay = vrPlay && mapping == game_pad::Mapping::gameplay && !flatScreen;', body)
        self.assertIn('promptCardsNow.load() == story077d::cardsOn && vrGameplay;', body)
        self.assertIn('world_trace::setPromptsWanted(cardsWanted || waypointWanted);', body)
        self.assertIn('catch (...)', body)
        self.assertIn('HERO VR 0.0.77 PROMPT CARDS (CANDIDATE) %s', body)
        self.assertIn('HERO VR 0.0.77 PROMPT CARDS: first seen %s', body)
        draw = g.index('HERO VR 0.0.77 PROMPT CARDS (story077d.hpp): the object cards')
        dbody = g[draw:g.index('// The T-pose calibration', draw)]
        self.assertIn('prompt_cards::append(translucent', dbody)
        self.assertNotIn('Inset', dbody)
        self.assertNotIn('latestScreen', dbody)
        self.assertIn('translucent.resize(from);', dbody)
        w = src('src/world_trace.cpp')
        self.assertIn('if (!any && !enemiesWanted() && !promptsOn) {', w)
        self.assertIn('c.bits |= promptBit;', w)
        self.assertIn('promptBit = 8192', src('include/spidy/world_trace.hpp'))
        cm = src('CMakeLists.txt')
        self.assertIn('src/prompt_cards.cpp', cm)
        self.assertIn('add_test(NAME story077d COMMAND spidy_story077d_tests)', cm)

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
