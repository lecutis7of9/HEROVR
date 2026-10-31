#!/usr/bin/env python3
"""0.0.7 MIRROR PERSIST FIX (t670u: "I turn it back on and it's just off").

MIRROR VIEW (and the other RESTART-tagged wrist settings) must survive the restart the wrist asks for.
A simulated session: the wrist press sets the mod's value, the launcher's loop reads it back
(SpidyHudSettings 3 and 10, decoded by live_settings) and keeps it in live.txt; the session ends; the
next session reads live.txt and sends MIRROR VIEW to the mod BEFORE SpidyXrStart (the view is only made
with the eyes). The simulated mod follows src/game_xr.cpp, whose lines are checked below too."""
import pathlib
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402

GAME = (ROOT / 'src/game_xr.cpp').read_text(encoding='utf-8')
RUN = (ROOT / 'tools/run_game_vr.py').read_text(encoding='utf-8')
MARKER = 1 << 30


class FakeMod:
    """The parts of game_xr.cpp / stereo_probe.cpp on this path, for one game process."""
    COMBAT = {'mirror_view': 'test', 'lens_flares': 'off_in_vr'}  # the wrist row's next value from 0 (lens_flares
    # starts at 1, OFF IN VR, from the launcher's default since 0.0.73 lights: a press gives GAME)

    def __init__(self):
        self.values = {'mirror_view': 0, 'lens_flares': 0}
        self.changed = 0
        self.mirror_wanted = 0  # stereo_probe's mirrorWanted
        self.eyes_made = False
        self.mirror_view_made = False
        self.next_supersample = 0

    # The wrist's COMBAT row press (game_xr.cpp result.combat38).
    def press(self, key):
        self.values[key] = (self.values[key] + 1) % 2
        if key == 'lens_flares':
            self.changed |= 1 << 29
        if key == 'mirror_view':
            self.changed |= 1 << 21

    def press_supersample(self, value):  # Control::supersample (10), restart: next-launch.txt
        self.next_supersample = value
        self.changed |= 1 << 10

    def hud_settings(self, selector):  # SpidyHudSettings
        if selector == 2:
            return MARKER | self.next_supersample
        if selector == 3:
            code, self.changed = self.changed, 0
            return MARKER | (code & (0xfffff | 1 << 20 | 1 << 21 | 1 << 27 | 1 << 28 | 1 << 29))
        if selector == 10:
            return MARKER | min(self.values['lens_flares'], 1) | min(self.values['mirror_view'], 1) << 1
        return 3970

    def export(self, name, value):  # SpidyMirrorView / SpidyLensFlares
        self.values[name] = value
        if name == 'mirror_view':
            self.mirror_wanted = value  # 0.0.7: forwarded at once (SpidyMirrorViewWant)
        return 0

    def test_ran(self):  # MIRROR TEST: it stopped by itself (mirror_view::finished, failed): back to OFF, saved
        if self.mirror_view_made and self.values['mirror_view']:
            self.values['mirror_view'] = 0
            self.mirror_wanted = 0
            self.changed |= 1 << 21

    def xr_start(self):  # the eyes, and the mirror test's view with them, are made at once
        self.eyes_made = True
        self.mirror_view_made = bool(self.mirror_wanted)


def hud_changes(mod):
    """run_game_vr.py's hud_changes(), on the fake mod."""
    changed = L.decode_hud_settings(3, mod.hud_settings(3))
    if not changed:
        return {}
    values = {}
    for selector in sorted({L.SELECTOR[k] for k in changed}):
        values.update(L.decode_hud_settings(selector, mod.hud_settings(selector)) or {})
    return {k: values[k] for k in changed if k in values and (k != 'supersample' or values[k])}


def session(folder, wrist=(), test_runs=False):
    """One launch: before-start settings, start, kept settings and defaults, the wrist presses, the loop."""
    live = folder / 'live.txt'
    mod = FakeMod()
    for name, value in L.vr_start_settings(live).items():  # run_game_vr.py, before SpidyXrStart
        mod.export(name, L.export_value(name, value))
    mod.xr_start()
    live_file = L.LiveFile(live, {})
    kept, _ = L.persisted(live)
    for name, value in kept.items():
        if name in mod.values:
            mod.export(name, L.export_value(name, value))
            live_file.applied(name, value)
    for name, value in L.UPDATE38_DEFAULTS.items():  # what live.txt does not keep goes to the default
        if name not in kept and name in mod.values:
            mod.export(name, L.export_value(name, value))
    for press in wrist:
        if isinstance(press, tuple):
            mod.press_supersample(press[1])
        else:
            mod.press(press)
    if test_runs:
        mod.test_ran()
    pending = hud_changes(mod)
    live_now = {k: v for k, v in pending.items() if k != 'supersample'}
    if live_now:
        assert live_file.adopt(live_now)
    if 'supersample' in pending:
        assert L.write_next_launch(folder / 'next-launch.txt', pending['supersample'])
    return mod


class MirrorPersist(unittest.TestCase):
    def test_mirror_view_survives_the_restart_and_runs(self):
        with tempfile.TemporaryDirectory() as d:
            folder = pathlib.Path(d)
            first = session(folder, wrist=['mirror_view'])
            self.assertEqual(first.values['mirror_view'], 1)
            self.assertFalse(first.mirror_view_made)  # turned on after the eyes: RESTART THE GAME ...
            self.assertIn('mirror_view = test', (folder / 'live.txt').read_text())
            second = session(folder)  # the restart
            self.assertEqual(second.values['mirror_view'], 1, 'MIRROR VIEW is back OFF after the restart')
            self.assertTrue(second.mirror_view_made, 'the wish reached the mod after the eyes were made')
            third = session(folder, wrist=['mirror_view'])  # turned off on the wrist: stays off
            self.assertEqual(third.values['mirror_view'], 0)
            self.assertEqual(session(folder).values['mirror_view'], 0)

    def test_working_mirror_stays_on(self):
        """MIRROR TEST (private): a mirror that works stays ON while the player keeps it ON, restarts too."""
        with tempfile.TemporaryDirectory() as d:
            folder = pathlib.Path(d)
            session(folder, wrist=['mirror_view'])
            second = session(folder)  # the restart: the view is made and shown; nothing stops it
            self.assertTrue(second.mirror_view_made)
            self.assertEqual(second.values['mirror_view'], 1)
            self.assertTrue(session(folder).mirror_view_made, 'a working mirror was not made at the next start')

    def test_mirror_view_runs_once_then_turns_itself_off(self):
        """After it stopped by itself, MIRROR VIEW goes back to OFF and stays OFF (no rerun at every launch)."""
        with tempfile.TemporaryDirectory() as d:
            folder = pathlib.Path(d)
            session(folder, wrist=['mirror_view'])
            second = session(folder, test_runs=True)  # the restart: the view is made, then stops by itself
            self.assertTrue(second.mirror_view_made)
            self.assertEqual(second.values['mirror_view'], 0, 'MIRROR VIEW stayed on after its test')
            self.assertIn('mirror_view = off', (folder / 'live.txt').read_text())
            third = session(folder)  # the next launch: no rerun
            self.assertEqual(third.values['mirror_view'], 0)
            self.assertFalse(third.mirror_view_made, 'the mirror test ran again at the next launch')

    def test_game_xr_turns_it_off_when_finished(self):
        self.assertIn('inline bool finished(const Test& t)',
                      (ROOT / 'include/spidy/mirror_view.hpp').read_text(encoding='utf-8'))
        i = GAME.index('mirror_view::finished(mirrorStats.test)')
        body = GAME[i:i + 400]
        self.assertIn('combat38Values[combat38::mirrorView] = 0;', body)
        self.assertIn('SpidyMirrorViewWant(0);', body)
        self.assertIn('wristChanged |= 1u << 21;', body)

    def test_other_restart_settings_survive_too(self):
        with tempfile.TemporaryDirectory() as d:
            folder = pathlib.Path(d)
            session(folder, wrist=['lens_flares', ('supersample', 200)])  # 0.0.73 lights: OFF IN VR -> GAME
            self.assertEqual(L.persisted(folder / 'live.txt')[0].get('lens_flares'), 'game')
            self.assertEqual(L.next_launch_supersample(folder / 'next-launch.txt'), 200)
            self.assertEqual(session(folder).values['lens_flares'], 0)

    def test_a_hero_toggle_does_not_clear_it(self):
        self.assertNotIn('mirror_view', L.HERO_KEYS)
        text = L.write_back('mirror_view = test\n', {'captain_america': 'on'})
        self.assertEqual(L.parse(text)[0]['mirror_view'], 'test')

    def test_decode_is_compatible(self):
        self.assertEqual(L.decode_hud_settings(10, MARKER | 2), {'lens_flares': 'game', 'mirror_view': 'test'})
        self.assertEqual(L.decode_hud_settings(3, MARKER | 1 << 21), {'mirror_view': True})
        self.assertEqual(L.SELECTOR['mirror_view'], 10)
        self.assertIn('mirror_view', L.PERSISTED)

    def test_game_xr_matches_the_simulation(self):
        self.assertRegex(GAME, r'if \(o == combat38::mirrorView\)\s*wristChanged \|= 1u << 21;')
        self.assertIn('1u << 20 | 1u << 21 | 1u << 27 | 1u << 28 | 1u << 29', GAME)  # 0.0.74 test 10 PICTURE 72: IMAGE (22) gone
        self.assertIn('std::min<DWORD>(c38(combat38::mirrorView), 1) << 1', GAME)
        body = GAME[GAME.index('WINAPI SpidyMirrorView(void* v)'):][:900]
        self.assertIn('SpidyMirrorViewWant(c38(combat38::mirrorView));', body)
        # Bit 21 (MUSIC PREVIOUS) is a press: the pages' apply() never takes it, so never marks it changed.
        apply = GAME[GAME.index('const auto apply = '):GAME.index('wristApply.due(wristClock')]
        self.assertNotIn('Control::musicPrevious', apply)

    def test_run_game_vr_sends_it_before_vr_starts(self):
        before = RUN.index("live_settings.vr_start_settings(ROOT/'live.txt')")
        self.assertLess(before, RUN.index("call_with_payload(process, xr['SpidyXrStart'], config)"))


if __name__ == '__main__':
    unittest.main()
