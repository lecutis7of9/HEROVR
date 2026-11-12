"""ULTIMATE ALLIANCE EXPANSION v4 (owner's second headset test): the hub's entity props placed (the doors, the lift, the
waterfall with its statue and pond: tools/ua/build_stark_tower.py), the bulkhead between the north wall and the ceiling, the
two points that say where the lift and the waterfall lead, the background people stood up straight (tools/ua/ua_cast.py)
and MUA's body gone for good (include/spidy/ua_body.hpp)."""
import os, sys, unittest
import numpy as np
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'ua'))
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()


class Room(unittest.TestCase):
    def setUp(self):
        self.b = src('tools/ua/build_stark_tower.py')

    def test_entities_placed_where_the_engb_puts_them(self):
        for etype in ("'door'", "'lift'", "'waterfall'", "'bridge_railing'", "'doors_logo_stk_d_01'"):
            self.assertIn(etype + ': [(', self.b)  # (the hub's table; v5's lab table is ENTITY_PARTS_LAB)
        self.assertIn('entity_notes = place_entities()', self.b)
        self.assertIn("inst_all.get(etype", self.b)  # v5: every instance of a type (pillars, machines)
        self.assertIn('PLACED_PROTOS', self.b)  # the prototypes under the hub are removed once copied

    def test_north_wall_bulkhead(self):
        self.assertIn("ceiling.append(box('ua_wall_bulkhead'", self.b)
        self.assertIn('WARNING north wall not found', self.b)

    def test_points_say_where_they_lead(self):
        # v5: the lift and the waterfall are teleports now (tests/ua_fix5_tests.py); only an unknown action word is COMING SOON
        self.assertIn("insts.get('lift')", self.b)
        self.assertIn("insts.get('link_stark2')", self.b)
        h = src('src/hideout074.cpp')
        self.assertIn('return Action::comingSoon;', h)  # any other action word: COMING SOON on the press


class Cast(unittest.TestCase):
    def frame(self, lean_deg):
        """A stick figure of 60 points, 1.8 m tall, its head pitched forward (+x) about its feet by lean_deg."""
        t = np.radians(lean_deg)
        ys = np.linspace(0, 1.8, 60)
        P = np.stack([ys * np.sin(t), ys * np.cos(t), np.zeros_like(ys) + .05 * np.sin(ys * 9)], 1)
        return P.astype(np.float32)

    def test_hunched_loop_stands_up(self):
        import ua_cast
        frames = [self.frame(a) for a in (17, 19, 21, 18, 20, 23)]
        out, was = ua_cast.straighten(frames)
        self.assertGreater(was, ua_cast.LEAN_ABOVE)
        for f in out:
            self.assertLess(ua_cast.lean_of(f)[0], 8.0)
            self.assertAlmostEqual(float(f[:, 1].min()), 0.0, places=5)
            self.assertGreater(float(f[:, 1].max()), 1.7)  # still a person's height

    def test_natural_loop_untouched(self):
        import ua_cast
        frames = [self.frame(a) for a in (4, 6, 7, 5, 6, 8)]
        out, was = ua_cast.straighten(frames)
        self.assertLessEqual(was, ua_cast.LEAN_ABOVE)
        self.assertTrue(all(np.array_equal(a, b) for a, b in zip(frames, out)))

    def test_only_background_looks(self):
        s = src('tools/ua/ua_cast.py')
        self.assertIn("if not look.get('principal'):", s)


class Body(unittest.TestCase):
    def test_only_his_own_sm2_hero(self):
        b = src('include/spidy/ua_body.hpp')
        self.assertIn('enum class Source : uint8_t { none, wornSuit, gameHero, standInSuit };', b)
        self.assertNotIn('uaBody', b)
        c = src('include/spidy/ua_body_cut.hpp')
        self.assertIn('enum class Why : uint8_t', c)
        self.assertIn('heroNotPosed', c)
        self.assertIn('noDepth', c)


if __name__ == '__main__':
    unittest.main()
