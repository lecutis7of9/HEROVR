# HERO VR 0.0.76 ROLL FIX (run flight-roll-1): the roll's locked axis, the body turned about the head, the near plane
# stepped out when rolled far over, the right stick's roll or climb, and the roll line. Made by Agent Mulder.
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
XR = (ROOT / 'src' / 'game_xr.cpp').read_text(encoding='utf-8')
TRACK = (ROOT / 'src' / 'game_tracking.cpp').read_text(encoding='utf-8')
TRACK_H = (ROOT / 'include' / 'spidy' / 'game_tracking.hpp').read_text(encoding='utf-8')
FLY = (ROOT / 'include' / 'spidy' / 'fly076.hpp').read_text(encoding='utf-8')
STEREO = (ROOT / 'src' / 'stereo_probe.cpp').read_text(encoding='utf-8')


class RollFix(unittest.TestCase):
    def test_the_axis_is_locked_by_roll_frame(self):
        self.assertIn('fly076::RollFrame fly076Frame;', XR)
        self.assertIn('rig.flyRollAxis(fly076Frame.axis());', XR)
        # no longer the raw velocity each frame
        self.assertNotIn('axis = normalized(swingState.velocity);', XR)

    def test_the_turn_reaches_the_body_the_same_frame(self):
        self.assertIn('Quat flyRollTurn{};', TRACK_H)
        self.assertIn('out.flyRollTurn = roll;', TRACK)
        # the suit body (COMFORT's level view: only the body turns; FULL: the feet about the head) and the mp pose
        self.assertEqual(XR.count('rendered.flyRollTurn'), 3)
        self.assertIn('fly076::rollBody(rendered.flyRollTurn, t.eye, t.up, t.feet);', XR)
        self.assertIn('t.feet = fly076::RollFrame::aboutHead(t.eye, t.feet, rendered.flyRollTurn);', XR)

    def test_near_plane(self):
        # 0.0.76 LEVEL VIEW: FLY ROLL COMFORT never rolls the view, so no near-plane step there
        self.assertIn('near_clip::boost(fly076::levelView(rollMode) ? 0.f : fly076Frame.nearClip());', XR)
        self.assertGreaterEqual(XR.count('near_clip::boost(0.f);'), 2)  # failure and not new flight
        self.assertIn('std::max(nearClip.load(std::memory_order_relaxed), nearClipBoost.load', STEREO)

    def test_roll_or_climb(self):
        self.assertIn('fly076::rollStick(frame.hands[1].stickX,', XR)
        self.assertIn('fly076::climbStick(frame.hands[1].stickX,', XR)

    def test_landing_and_snap(self):
        self.assertIn('snapDegrees = 8.f', FLY)
        self.assertIn('landed (or the flight ended): level at once', FLY)

    def test_the_line(self):
        self.assertIn('VERSION 0.0.76 FLY ROLL: %.0f deg about', XR)
        self.assertIn('VERSION 0.0.76 FLY ROLL: landed - level at once', XR)

    def test_no_forbidden_words(self):
        for text in (XR, FLY, TRACK, TRACK_H, STEREO):
            self.assertNotIn('jim' + 'jam', text.lower())


if __name__ == '__main__':
    unittest.main()
