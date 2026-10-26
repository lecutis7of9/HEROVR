"""0.0.6 hotfix (Update 39.1.1, Bear's rep0060): Virtual Desktop answered "Create eye swapchain XrResult=-2"
(XR_ERROR_RUNTIME_FAILURE) at VR start and VR ended with the game on its flat screen. The eyes are now tried
again (the 0.0.5 request, then fewer flags, the other format, smaller eyes), every try is said, and an OpenXR
refusal at the start is retried with a new session instead of ending VR."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
XR = (ROOT/'src/xr_session.cpp').read_text(encoding='utf-8')
GAME = (ROOT/'src/game_xr.cpp').read_text(encoding='utf-8')
LADDER = (ROOT/'include/spidy/swapchain_ladder.hpp').read_text(encoding='utf-8')
CORE = (ROOT/'tests/core_tests.cpp').read_text(encoding='utf-8')


def init_loop():
    start = GAME.index('for (unsigned startTries = 1;; ++startTries)')
    return GAME[start:GAME.index('D3D12Renderer overlay;', start)]


class SwapchainLadder(unittest.TestCase):
    def test_every_eye_swapchain_goes_through_the_ladder(self):
        self.assertEqual(XR.count('xrCreateSwapchain('), 1, 'an eye swapchain made outside the ladder')
        self.assertIn('swapchainLadder(format_, usable, fullUsage, lessUsage', XR)
        self.assertNotIn('xr(xrCreateSwapchain', XR, 'a swapchain failure still throws on the first try')

    def test_the_first_try_is_the_0_0_5_request(self):
        # 0.0.5 (v005) asked for color | transfer dst | transfer src, 1 sample, 1 mip, 1 layer, 1 face.
        self.assertRegex(XR, r'fullUsage \|= XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT \| XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT')
        self.assertIn('sc.sampleCount = 1;', XR)
        self.assertIn('sc.faceCount = sc.arraySize = sc.mipCount = 1;', XR)
        self.assertRegex(LADDER, r'add\(format, fullUsage, width, height, 0, "the 0\.0\.5 parameters"\);')

    def test_every_try_is_logged_and_said(self):
        self.assertIn('swapchainTries_.push_back(said + ": made")', XR)
        self.assertIn('swapchainTries_.push_back(said + ": " + where + " XrResult="', XR)
        loop = GAME[GAME.index('const auto sayTries'):GAME.index('D3D12Renderer overlay;')]
        self.assertIn('runtime.swapchainTries()', loop)
        self.assertIn('message(2, 0, text.c_str());', loop)
        self.assertIn('Sleep(150)', loop, 'quick tries would overwrite each other before the launcher reads them')

    def test_a_failed_try_leaves_nothing_behind(self):
        fail = XR[XR.index('last = result;'):XR.index('throw XrError(lastWhere')]
        self.assertIn('xrDestroySwapchain(e.swapchain)', fail)
        self.assertIn('e.images.clear()', fail)

    def test_a_rebuilt_session_keeps_the_game_eyes(self):
        self.assertIn('forced_[0] != 0);', XR)
        self.assertIn('if (!fixedSize)', LADDER)

    def test_the_unit_test_exists(self):
        self.assertIn('0.0.6 HOTFIX SWAPCHAIN LADDER', CORE)


class StartRetry(unittest.TestCase):
    def test_any_openxr_refusal_at_the_start_is_tried_again(self):
        loop = init_loop()
        self.assertIn('catch (const XrError& e)', loop)
        # The only ways out of the loop with an error: the launcher asked to stop or is gone, or no
        # headset after 2 minutes (39.0.3's wait, unchanged).
        throws = loop.count('throw;')
        self.assertEqual(throws, 2, loop)
        self.assertIn('if (stopRequested || launcherGone)\n                        throw;', loop)
        self.assertIn('XR_ERROR_FORM_FACTOR_UNAVAILABLE', loop)
        self.assertNotIn('e.result != XR_ERROR_FORM_FACTOR_UNAVAILABLE ||', loop,
                         'other refusals still end VR at once (rep0060)')
        self.assertIn('Sleep(waited < 120000 ? 2000 : 5000);', loop)

    def test_the_player_is_told_and_told_when_to_restart(self):
        loop = init_loop()
        self.assertIn('"VR is starting: the runtime refused ("', loop)
        self.assertIn('". If it goes on: restart Virtual Desktop and the game"', loop)
        self.assertIn('runtime.swapchainNote()', loop)

    def test_a_dead_launcher_ends_the_wait(self):
        self.assertIn('const bool launcherGone = !config.durationMs && GetTickCount64() >= keepAliveDeadline;',
                      init_loop())

    def test_no_new_game_address(self):
        # The hotfix touches OpenXR calls only: no hex constant in the new code.
        for text in (LADDER, init_loop()):
            self.assertIsNone(re.search(r'0x[0-9a-fA-F]{5,}', text))


if __name__ == '__main__':
    unittest.main()
