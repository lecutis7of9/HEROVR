"""Build 7 (test 7 of 0.0.72): the user's test 6 - lag from calibration on, Thor's hammer not thrown, no
lightning seen, Green Lantern's controls unclear. Source-level checks of the fixes (the behaviour is in
core_tests.cpp: 'Build 7: THOR ...' and 'Build 7: GREEN LANTERN ...')."""
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))


def read(*p):
    with open(os.path.join(ROOT, *p), encoding='utf-8') as f:
        return f.read()


class Build7(unittest.TestCase):
    def test_textures_512_at_768_mb(self):
        s = read('src', 'game_xr.cpp')
        self.assertIn('commitFree >= (768ull << 20) ? 512u : 256u', s)

    def test_model_vertices_buffer_kept(self):
        s = read('src', 'game_xr.cpp')
        self.assertIn('static thread_local std::vector<Vertex> modelVerticesKept;', s)

    def test_sheathed_hammer_not_drawn(self):
        s = read('src', 'hero072_visual.cpp')
        self.assertIn('l.hammer && !l.hammerSheathed && l.hammerModel', s)

    def test_hammer_triangles_kept(self):
        # the real models are never flattened: the hammer keeps its 50,000 triangles
        self.assertIn('hammerTriangles = 50000', read('include', 'spidy', 'mesh_model.hpp'))

    def test_thor_hand_never_rises_and_gl_ring_flight(self):
        s = read('src', 'game_xr.cpp')
        self.assertIn('hero072Play.hammerInHandOrOut()', s)
        self.assertIn('glRingFlight.gate(', s)
        self.assertIn('VERSION 0.0.72 THOR: %s', s)

    def test_readme(self):
        s = read('tools', 'release072', 'READ-ME-FIRST.txt')
        for want in ('back to 90 Hz', 'GREEN LANTERN CONTROLS', 'BOTH arms straight up', 'RING BEAM: hold the RIGHT trigger',
                     'BUILD 7 THOR CONTROLS', 'THROW: swing your arm forward', 'RECALL / CATCH', 'LIGHTNING BLAST', 'STORM'):
            self.assertIn(want, s)

    def test_comfort_never_says_72(self):
        import session_comfort
        src = read('tools', 'session_comfort.py')
        self.assertNotIn('(72 Hz)', src)
        self.assertIn('back to 90 Hz', src)


if __name__ == '__main__':
    unittest.main()
