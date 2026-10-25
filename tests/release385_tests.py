# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 38.5 RELEASE GATE: the owner's release bar ("get through the tutorial and switch between Miles and
Peter") and blockers 1-8. Each blocker names the C++ core and replay tests that must PASS (read from the last
ctest log, or by running the built tests), plus static checks: every experimental feature OFF by default and
behind a try/catch, the hero switch probe read-only, PAD MODE present. Any failing fails the gates."""
import pathlib
import re
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
XR = (ROOT / 'src/game_xr.cpp').read_text(encoding='utf-8', errors='replace')
HUD = (ROOT / 'src/wrist_hud.cpp').read_text(encoding='utf-8', errors='replace')
HS = (ROOT / 'include/spidy/hero_switch.hpp').read_text(encoding='utf-8')
SETTINGS = (ROOT / 'include/spidy/vr_settings.hpp').read_text(encoding='utf-8')
BUILD = ROOT / 'build/gate'

# blocker -> (label, [core or replay PASS lines that must be present])
BLOCKERS = {
    1: ('MIRROR shows the suit', [
        'Update 37.2: the MIRROR - body only, stays put until dismissed',
        'Update 38.3: MIRROR SUIT colours',
    ]),
    2: ('WEB SHOOTERS: one ball per pull, the pulled wrist, the game ball, aim assist', [
        'Update 38 SpidyVR: BALL FIRE PRESS shoots one ball',
        'Update 38: web-ball aim assist',
        'Update 38.3: the defaults are the fixes',
        'ONE BALL: every pull fires exactly one ball (100%)',
        "ONE BALL: every ball is the pulled hand's (100%)",
        'ONE BALL: one RB press per ball',
        "BALL WRIST: every ball starts at the pulled hand's web shooter",
    ]),
    3: ('TUTORIAL: prompts pass through, the pad fallback', [
        'every control of the pad can be sent in VR play, by gameplay or the pad layer',
        'the pad layer: a click of both sticks latches it',
        'every button the game can name is sent in VR play',
        "the game's icon ids say what a prompt asks of the pad",
        'a prompt text says its buttons, and is rewritten with the VR controls',
        'PROMPTS: AUTO never latches the pad in a recorded fight',
    ]),
    4: ('PETER / MILES switch', [
        "Update 38.5: HERO SWITCH PROBE times a real switch's re-attach, read-only",
        'Update 38.3: MIRROR SUIT colours',
    ]),
    5: ('INTERACTION BUTTONS: Y and two-button prompts, PAD MODE', [
        "both web buttons on a thing no web holds on, and a yank, are the game's LB + RB",
        'every button the game can name is sent in VR play',
        'the pad layer: a click of both sticks latches it',
    ]),
    6: ('GLIDING: Web Wings on Y, A boost, steering', [
        'flip: off unless FLIPS is on, A in the air does nothing',
        'Update 38.5 GESTURE MOVES: help',
        'wings yield to the stick',
    ]),
    7: ('GADGETS: gestures and the RB combos', [
        'Update 37.2: SLOT TEST sends each of the nine inputs',
        'Update 37: combat - a tap is the web shooter, a hold does nothing, gestures fire their gadget',
        'Update 38.5 GESTURE REMAP on the hologram',
    ]),
    8: ('COMBAT: far punch first try, mid-air, left = right, aim assist, no kicks from waving, zip kick', [
        'Update 38: punches - the gesture',
        'FAR PUNCH 38.5: >= 90% of first throws at an enemy 4-18 m zip',
        'FAR PUNCH 38.5: each hand >= 85%',
        'FAR PUNCH 38.5: mid-air throws >= 85%',
        'FAR PUNCH 38.5: a throw away from the enemy zips <= 2% of the time',
        'run381: arm waving with no enemy - 38.1 attacked, 38.2 never',
        'fast short hand twitch does not zip',
        'GRAB WEB ASSIST: every enemy within 12 degrees is caught',
    ]),
}

_LOG = None


def results():
    """The PASS/FAIL lines of the core and replay tests: the last ctest log, else the built binaries."""
    global _LOG
    if _LOG is not None:
        return _LOG
    text = ''
    log = BUILD / 'Testing/Temporary/LastTest.log'
    if log.exists():
        text = log.read_text(encoding='utf-8', errors='replace')
    if 'REPLAY PASSED' not in text or 'tests passed' not in text:
        text = ''
        for exe in ('spidy_tests', 'spidy_replay_tests'):
            p = BUILD / exe
            if p.exists():
                text += subprocess.run([str(p)], cwd=ROOT, capture_output=True, text=True, timeout=900).stdout
    _LOG = text
    return text


def _code(text):
    text = re.sub(r'"(?:[^"\\\n]|\\.)*"', '""', text)
    return re.sub(r'//[^\n]*', '', text)


class Blockers(unittest.TestCase):
    def setUp(self):
        if not results():
            self.skipTest('no built tests (build/gate) - the gates build them first')

    def test_each_blocker_has_its_tests_passing(self):
        text = results()
        passed = [ln.strip()[5:] for ln in text.splitlines() if ln.strip().startswith('PASS ')]
        failed = [ln.strip() for ln in text.splitlines() if ln.strip().startswith('FAIL ')]
        self.assertFalse(failed, failed[:5])
        for n, (label, needs) in BLOCKERS.items():
            for need in needs:
                with self.subTest(blocker=n, label=label, test=need):
                    self.assertTrue(any(p.startswith(need) or need in p for p in passed),
                                    f'blocker {n} ({label}): no PASS for "{need}"')

    def test_replay_and_core_totals(self):
        text = results()
        self.assertIn('REPLAY PASSED: 0 failure(s)', text)
        m = re.findall(r'^(\d+)/(\d+) tests passed', text, re.M)
        self.assertTrue(m and all(a == b for a, b in m), m)


class ExperimentalOffByDefault(unittest.TestCase):
    """Anything not solid can't break the tutorial run: OFF at start, a fault turns it off, never a crash."""

    def test_atomics_start_off(self):
        for decl in ('invincibleOn{false}', 'flyVignetteOn{false}', 'symbioteOn{false}', 'flyModeNow{0}',
                     'promptHandlingNow{0}', 'gestureMovesNow{0}', 'bloodVisor{false}'):
            self.assertIn(decl, XR, decl)

    def test_flips_off(self):
        self.assertIn('bool realisticFlips = false', SETTINGS)

    def test_gesture_moves_and_remap_guarded(self):
        code = XR
        self.assertIn('GESTURE MOVES failed and turned itself off', code)
        self.assertIn('GESTURE REMAP failed and turned itself off', code)
        self.assertRegex(code, r'gi\.play && gestureMode != gestures385::modeOff')
        # the remap file loads in a try/catch and a bad file means the built-in moves
        self.assertRegex(code, r'gesture_remap::Store loadGestures\(\) \{\s*try \{')
        # saved whole, then put in place (a crash mid-write keeps the last)
        self.assertIn('MOVEFILE_REPLACE_EXISTING', code)

    def test_remap_never_blocks_safety(self):
        self.assertIn('BLOCKED: ', HUD)
        sess = (ROOT / 'include/spidy/gesture_session.hpp').read_text(encoding='utf-8')
        self.assertIn('keepAnyway', sess)


class FlyControls(unittest.TestCase):
    """FLY MODE's poses, R3 drop, L3+R3 ultimate and FLY ROLL: only while FLY is on, never a crash."""

    def test_poses_guarded_and_gated(self):
        self.assertIn("FLY's poses failed and FLY MODE turned itself off", XR)
        self.assertRegex(XR, r"pi\.live = gi\.play && !gi\.xHeld && \(flyModeNow\.load\(\) != 0 \|\| flyTraining\)")

    def test_roll_mod_side_and_guarded(self):
        self.assertIn('rig.flyRoll(', XR)
        self.assertIn('FLY ROLL failed and turned itself off', XR)
        self.assertRegex(XR, r'flyModeNow\.load\(\) != 0 && flyNow\.flying && flyNow\.moving')

    def test_r3_is_fly_while_flying(self):
        self.assertIn('!flyNow.flying && !flyChord.claims()', XR)
        self.assertIn('game_pad::pauseHoldSeconds', XR)
        self.assertIn('if (flyChord.tapping())', XR)

    def test_roll_exports(self):
        for name, lo, hi, code in (('SpidyFlyRoll', 0, 2, 5350), ('SpidyFlyRollSpeed', 30, 180, 5351),
                                   ('SpidyFlyAutoLevel', 0, 1, 5352)):
            self.assertRegex(XR, r'%s\(void\* \w+\) \{\s*return setLive\(\w+, %d, %d, %d,' % (name, lo, hi, code))


class HeroSwitch(unittest.TestCase):
    def test_probe_is_read_only(self):
        code = _code(HS)
        self.assertNotRegex(code, r'\bWriteProcessMemory\b|\bmemcpy\s*\(|VirtualProtect')

    def test_reattach_relearns_the_suit(self):
        self.assertIn('suitColours.forget()', XR)
        self.assertIn('lastAttachedHero', XR)


class PadFallback(unittest.TestCase):
    def test_pad_mode_on_the_hologram(self):
        self.assertIn('PAD MODE', HUD)
        self.assertIn('padModePressed = true', XR)


if __name__ == '__main__':
    unittest.main()
