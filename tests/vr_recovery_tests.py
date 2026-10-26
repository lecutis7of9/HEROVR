"""Update 39.0.3 (B3ARVR, Virtual Desktop): a lost headset never ends VR; the session comes back by itself."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SESSION = (ROOT/'src/xr_session.cpp').read_text(encoding='utf-8')
HEADER = (ROOT/'include/spidy/xr_session.hpp').read_text(encoding='utf-8')
GAME = (ROOT/'src/game_xr.cpp').read_text(encoding='utf-8')


def body(text, start):
    i = text.index(start)
    j = text.index('{', i)
    depth = 0
    for k in range(j, len(text)):
        depth += {'{': 1, '}': -1}.get(text[k], 0)
        if not depth:
            return text[j:k + 1]
    raise AssertionError(start)


class HeadsetLostNeverFlat(unittest.TestCase):
    def test_xr_errors_carry_their_result(self):
        self.assertIn('class XrError : public std::runtime_error', HEADER)
        self.assertIn('throw XrError(std::string(where) + " XrResult=" + std::to_string(r), r);', SESSION)

    def test_the_game_runtime_never_ends_vr_for_a_lost_session(self):
        stereo = body(SESSION, 'bool XrRuntime::frameStereo(')
        self.assertIn('if (!recoverable_)\n        return frameOnce(prepare, draw);', stereo)
        self.assertIn('catch (const XrError& e)', stereo)
        self.assertNotIn('return false', stereo)
        self.assertIn('lose(', stereo)
        # Only the device initialize (the game's) recovers: probes and the lab end as before.
        device = body(SESSION, 'void XrRuntime::initialize(ID3D12Device* device')
        self.assertIn('recoverable_ = true;', device)
        self.assertIn('forced_ = {eyes_[0].width, eyes_[0].height};', device)
        self.assertIn('teardown();\n        throw;', device)

    def test_recovery_is_cheap_and_keeps_the_eye_size(self):
        recover = body(SESSION, 'bool XrRuntime::recover()')
        self.assertIn('std::chrono::seconds(2)', recover)
        self.assertIn('if (now < retryAt_)\n        return false;', recover)
        self.assertIn('XR_ERROR_FORM_FACTOR_UNAVAILABLE', recover)
        self.assertIn('if (forced_[0]) {', SESSION)
        self.assertIn('restartNeeded_ = true;', SESSION)
        lose = body(SESSION, 'void XrRuntime::lose(')
        self.assertIn('drainQueue();', lose)
        self.assertIn('teardown(!instanceLost);', lose)

    def test_a_lost_session_late_events_are_ignored(self):
        poll = body(SESSION, 'void XrRuntime::poll()')
        self.assertIn('->session == session_', poll)
        self.assertIn('exit_ = instanceLost_ = true;', poll)

    def test_teardown_leaves_no_handle(self):
        tear = body(SESSION, 'void XrRuntime::teardown(bool keepInstance)')
        for h in ('session_', 'space_', 'head_', 'actionSet_', 'instance_'):
            self.assertRegex(tear, r'std::exchange\(%s,' % re.escape(h))
        self.assertIn('e.swapchain = XR_NULL_HANDLE;', tear)
        self.assertIn('running_ = exit_ = lastFrameSubmitted_ = false;', tear)

    def test_the_game_says_so_and_never_throws_for_paused_eyes(self):
        self.assertNotIn('throw std::runtime_error("No native eye images reached OpenXR within 3 seconds")', GAME)
        self.assertNotIn('throw std::runtime_error("Native eye presentation stalled for 3 seconds")', GAME)
        self.assertIn('"Headset lost: reopen Virtual Desktop in your headset; VR comes back by itself"', GAME)
        self.assertIn('"Headset not connected: open Virtual Desktop in your headset first"', GAME)
        # 0.0.6 hotfix (39.1.1): every start refusal waits now, not only a missing headset
        # (tests/swapchain_retry_hotfix_tests.py); no headset still waits 2 minutes.
        self.assertIn('if (e.result == XR_ERROR_FORM_FACTOR_UNAVAILABLE) {', GAME)
        self.assertIn('restart the game to play in VR again', GAME)
        self.assertIn('runtime.recoveries() != recoveriesSeen', GAME)


if __name__ == '__main__':
    unittest.main()
