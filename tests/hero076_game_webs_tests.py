"""HERO VR 0.0.76 GAME WEBS ONLY (the user: get rid of HERO VR's custom web visuals for Spider-Man in SM2, use only
the game's own web lines from the wrist; also our web balls and webbed-up enemies, the game's own instead, with the
ball's aim and firing from the wrist kept). live.txt game_webs, id 5556, ON by default. Only the picture changes."""
import os, re, sys, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import live_settings  # noqa: E402
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()

KEY, EXPORT, IDENT, ATOMIC = 'game_webs', 'SpidyGameWebs', 5556, 'gameWebsNow'

def render_webs(g):
    """The overlay's web drawing: from the GAME WEBS ONLY gate to the aim markers."""
    a = g.index('const bool gameWebsDrawn = gameWebsOnly();')
    return g[a:g.index('std::vector<AimMarker> marks;', a)]

class GameWebs(unittest.TestCase):
    def test_live_txt_option(self):
        (lo, hi), exp, text = live_settings.SETTINGS[KEY]
        self.assertEqual((lo, hi, exp), (0, 1, EXPORT))
        self.assertIn('restart', text)  # the subtext says what needs one
        self.assertEqual(live_settings.CHOICE_NAMES[KEY], ('on', 'off'))
        self.assertEqual(live_settings.export_value(KEY, 'on'), 0)  # ON is the default (0)
        self.assertEqual(live_settings.export_value(KEY, 'off'), 1)
        self.assertEqual(live_settings.parse('%s = off\n' % KEY), ({KEY: 'off'}, []))
        self.assertIn(KEY, live_settings.START_ONLY)
        self.assertIn(KEY, live_settings.VR_START_KEYS)  # sent before SpidyXrStart, which hooks the game's webs
        self.assertNotIn(KEY, live_settings.UPDATE38_KEYS)
    def test_export_is_ours_alone(self):
        g = src('src/game_xr.cpp')
        self.assertIn('WINAPI %s(void* v) {\n    return setLive(v, 0, 1, %d, %s, asU32);' % (EXPORT, IDENT, ATOMIC), g)
        self.assertEqual(len(re.findall(r'\b%d\b' % IDENT, g)), 1)
        self.assertIn('std::atomic<uint32_t> %s{0};' % ATOMIC, g)
    def test_spider_man_only(self):
        g = src('src/game_xr.cpp')
        self.assertIn('bool gameWebsOnly() { return gameWebsNow.load() == 0 && heroNow() == hero_modes::spiderMan; }', g)
    def test_game_lines_from_the_wrists(self):
        g = src('src/game_xr.cpp')
        # The game's web lines are hooked even when the launcher asks for overlay webs.
        self.assertIn('websStarted = (!(config.options & 2) || gameWebsNow.load() == 0) &&\n'
                      '                                      !native_webs::start(config.base, player.record);', g)
        # The wrist anchoring is still worked out and submitted.
        self.assertIn('native_webs::submit(webs, 200, bent);', g)
        self.assertIn('hand.wrist = webStart(motion.grips[from], from, webStartSpot.load());', g)
        r = render_webs(g)
        self.assertIn('native_eyes::renderWebStart(imageGeneration, hand, ropeStarts[hand])', r)
        self.assertIn('ropeHands |= 1u << hand;', r)
    def test_no_overlay_web_drawn(self):
        r = render_webs(src('src/game_xr.cpp'))
        # Every web line goes through one of the two gates; nothing else draws a web.
        self.assertEqual(r.count('appendWeb(vertices,'), 2)
        self.assertIn('if (!(gameWebsDrawn && gameLinesRunning))\n                                appendWeb(', r)
        self.assertIn('if (!gameWebsDrawn)\n                                appendWeb(', r)
        self.assertEqual(r.count('drawWeb(line);'), 5)     # the swing's, zip's, held and let-go lines
        self.assertEqual(r.count('drawOurWeb(line);'), 2)  # GRAB WEB's rope, WALL STICK's nets
        self.assertIn('if (!c0.on || gameWebsDrawn)', r)   # the webbed-up cocoons
        self.assertIn('!saved->repulsorLook && !gameWebsDrawn; ++b)', r)  # OURS balls and splashes
        self.assertIn('for (unsigned g = 0; g < 2 && !gameWebsDrawn; ++g)', r)  # GLOB
        self.assertEqual(r.count('appendWebThread('), 1)
        self.assertEqual(r.count('appendWebBall('), 2)
    def test_balls_are_the_games_from_the_wrist(self):
        g = src('src/game_xr.cpp')
        self.assertIn('return gameWebsOnly() && (look == combat38::lookOurs || look == combat38::lookGlob) ? '
                      'combat38::lookWrist : look;', g)
        self.assertIn('c.ballOurs = ballLookNow() == combat38::lookOurs;', g)
        self.assertIn('combat38::wristMuzzle(ballLookNow())', g)
        self.assertIn('ballLookNow() == combat38::lookGlob', g)
        self.assertEqual(len(re.findall(r'c38\(combat38::ballLook\)', g)), 1)  # only ballLookNow reads it
        # The player's own BALL LOOK default is unchanged (older tests pin it).
        self.assertEqual(live_settings.UPDATE38_DEFAULTS['ball_look'], 'ours')
    def test_logic_kept(self):
        g = src('src/game_xr.cpp')
        # Only the drawing goes: the cocoons' hits, the nets' detector and the swing are as they were.
        self.assertIn('cocoons.update(frame.seconds, ids, footAt, static_cast<int>(foes.count));', g)
        self.assertIn('wallStick.frame(dt, wallMode);', g)
    def test_forbidden_names(self):
        t = src('tests/hero076_game_webs_tests.py') + src('tools/live_settings.py')
        self.assertNotIn('jim' + 'jam', t.lower())

if __name__ == '__main__':
    unittest.main()
