"""HERO VR 0.0.74 FLIGHT POSE (fix 27, the third try): the lean reaches every body the player can see - his own worn
suit hangs from the neck below the eyes (test 3 swung it about the eyes, level with the camera), Player 2 leans
too (0.0.73's pose packet had no lean), LEAN is still the default with UPRIGHT to go back, and the view never tilts.
Made by Agent Mulder."""
import os, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()

class Hero074FlightPose(unittest.TestCase):
    def test_own_suit_hangs_from_the_neck(self):
        g = src('src/game_xr.cpp')
        j = g.index('fly_lean::leanSuit(saved->flyLean, t.eye, t.up, t.feet);')
        self.assertIn('t.lean = saved->flyLean;', g[j:j + 600])
        self.assertLess(j, g.index('suit_wear::estimate(*suitRig, t, live)', j))
        w = src('src/suit_wear.cpp')
        self.assertIn('const Vec3 neckAt = headAt + B0(r.at[neck] - r.at[head]) * s;', w)
        self.assertIn('out.at[e] = neckAt + B(r.at[e] - r.at[neck]) * s;', w)
        self.assertIn('Quat lean{};', src('include/spidy/suit_wear.hpp'))
    def test_player_two_leans(self):
        g = src('src/game_xr.cpp')
        self.assertIn('lp.leanUp = back.rotate(fly_lean::upOf(saved->flyLean));', g)
        h = src('include/spidy/mp_net.hpp')
        self.assertIn('poseBytes = 124, poseBytesV1 = 112', h)
        n = src('src/mp_net.cpp')
        self.assertIn('w.vec(safeLeanUp(s.leanUp));', n)
        self.assertIn('body >= poseBytes ? safeLeanUp(r.vec())', n)
        self.assertIn('g.up = turn(safeLeanUp(s.leanUp));', n)
        self.assertIn('fly_lean::leanSuit(lean, t.eye, t.up, t.feet);', src('src/mp_ghost.cpp'))
    def test_the_view_never_tilts(self):
        g = src('src/game_xr.cpp')
        # the lean only ever turns the body: never the eyes' poses sent to the headset
        self.assertNotIn('flyTiltNow * motion.headPose', g)
        self.assertNotIn('flyLean * rendered.headPose', g)
        self.assertNotIn('eyes.pose', g[g.index('Quat flyTiltNow{};'):g.index('Quat flyTiltNow{};') + 900])
    def test_on_by_default_with_a_switch(self):
        ls = src('tools/live_settings.py')
        self.assertIn("'fly_pose': ('lean', 'upright')", ls)
        self.assertIn("'fly_pose': 'lean'", ls)
        c = src('include/spidy/combat38.hpp')
        self.assertIn('"LEAN tips the body with flight speed, upright to hover, no restart."', c)
    def test_old_buddy_still_reads(self):
        # the test buddy (and 0.0.73) send 112-byte poses and accept longer ones of the same version
        b = src('tools/mp_test_buddy.py')
        self.assertIn('PAYLOAD = {POSE: 112', b)
        self.assertIn('if length < PAYLOAD[ptype]', b)

if __name__ == '__main__':
    unittest.main()
