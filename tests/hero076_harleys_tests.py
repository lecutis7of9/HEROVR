"""HERO VR 0.0.76 HARLEYS: CAPTAIN AMERICA's "EL OHV V-TWIN" (matias_romero) and WOLVERINE's "2016 Harley-Davidson
FXDLS Dyna Low Rider S" (everhard), both CC BY 4.0, on the 0.0.75 bike system (X tap, MODEL MISSING, handlebar
steering, driver only); the dash screen and the level crime tracker between the bars; CAP's chest-draw grapple; the
PUNISHER's dual-wield holo guard. Made by Agent Mulder."""
import os, sys, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()
CREDITS = (('EL OHV V-TWIN', 'matias_romero', '9923aaa519334f48b8dd44fd792475ed'),
           ('2016 Harley-Davidson FXDLS Dyna Low Rider S', 'everhard', 'ad65bcb9282a46459a9a42f787bb5cbd'))

class Hero076Harleys(unittest.TestCase):
    def test_each_hero_his_own_bike_slots(self):
        m = src('include/spidy/mesh_model.hpp')
        for f in ('"Vehicle/Motorcycle"', '"Vehicle/Motorcycle/Handlebars"', '"Vehicle/Motorcycle/Cap"',
                  '"Vehicle/Motorcycle/Cap/Handlebars"', '"Vehicle/Motorcycle/Wolverine"', '"Vehicle/Motorcycle/Wolverine/Handlebars"'):
            self.assertIn(f, m)
        self.assertIn('bikeTriangles = 110000, bikeBarsTriangles = 40000', m)
        b = src('include/spidy/bike075.hpp')
        self.assertIn('return ownReady ? own : wolverineHero ? wolverine : cap;', b)
        self.assertIn('inline constexpr const char* overrideMarker = "override.txt";', b)
        g = src('src/game_xr.cpp')
        self.assertIn('return h == hero_modes::captainAmerica || h == hero_modes::wolverine;', g)
        self.assertIn('bikeTap.step(bikeX, frame.seconds)', g)
        self.assertIn('"MOTORCYCLE: MODEL MISSING: "', g)
    def test_screen_and_level_tracker_between_the_bars(self):
        b = src('include/spidy/bike075.hpp')
        self.assertIn('Vec3 screen{};', b)
        self.assertIn('Vec3 tracker{};', b)
        g = src('src/game_xr.cpp')
        self.assertIn('van_screen::mountTracker(V, bikeLayouts[bikeNowWhich].tracker, bikeLayouts[bikeNowWhich].screen,', g)
    def test_credits_and_the_old_bike_gone(self):
        for p in ('THIRD-PARTY-MODELS.md', 'CREDITS.md', 'include/spidy/bike075.hpp'):
            t = src(p)
            self.assertIn('CC BY 4.0', t, p)
            for title, artist, uid in CREDITS:
                self.assertIn(artist, t, p)
        for title, artist, uid in CREDITS:
            self.assertIn(uid, src('THIRD-PARTY-MODELS.md'))
            self.assertIn(uid, src('include/spidy/bike075.hpp'))
        for p in ('THIRD-PARTY-MODELS.md', 'CREDITS.md', 'include/spidy/bike075.hpp', 'src/bike075.cpp', 'README-HERO-VR.md'):
            self.assertNotIn('Mert Tetik', src(p), p)
            self.assertNotIn('50c38758e5134a0b8ffa49780e7fe228', src(p), p)
    def test_packaged_per_hero(self):
        sys.path.insert(0, os.path.join(ROOT, 'tools', 'release072'))
        import required_assets as ra
        for h in ('Cap', 'Wolverine'):
            for f in ('model.glb', 'layout.json', 'license.txt', 'Handlebars/model.glb', 'Handlebars/license.txt'):
                self.assertIn('Models/Vehicle/Motorcycle/%s/%s' % (h, f), ra.RELEASE076_BIKE_FILES)
        self.assertFalse(any(f.startswith('Models/Vehicle/Motorcycle/model.glb') for f in ra.RELEASE076_BIKE_FILES))
        self.assertTrue(os.path.exists(os.path.join(ROOT, 'tools/bike076/convert.py')))
        self.assertFalse(os.path.exists(os.path.join(ROOT, 'tools/bike075/convert.py')))
    def test_cap_grapple_draws_at_the_chest(self):
        p = src('include/spidy/hero007_play.hpp')
        self.assertIn('const bool atRig = punisher_rig::at(in.hands[i].position, in.head, shieldRack.body);', p)
        self.assertIn('gunGrip(in, i, !rackHas && !atBack && atRig, out);', p)
    def test_punisher_dual_wield_holo_guard(self):
        h = src('include/spidy/holo_guard076.hpp')
        self.assertIn('inline constexpr float combatHold = 2.0f;', h)
        self.assertIn('armed = hero == hero_modes::punisher && (gunLeft || gunRight || sinceShot < combatHold);', h)
        self.assertIn('const bool shut = hudPlay && armed && !holoOn;', h)
        g = src('src/game_xr.cpp')
        i = g.index('const bool gesturePlay = holoGuard.gesturePlay(hudPlay, wristToggle.on());')
        self.assertLess(i, g.index('if (wristToggle.update(frame.seconds, gesturePlay, hudHand,'))

if __name__ == '__main__':
    unittest.main()
