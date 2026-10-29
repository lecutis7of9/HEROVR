"""A session's game settings return when the game closes, except those the player changed in the game meanwhile."""
import json
import pathlib
import sys
import tempfile
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
import vr_display as display


class Registry:
    def __init__(self, graphics):
        self.keys = {display.KEY: dict(graphics), display.INPUT_KEY: {}}

    def read(self, names=display.NAMES, key=display.KEY):
        return {n: v for n, v in self.keys.get(key, {}).items() if n in names}

    def read_all(self, key):
        return dict(self.keys.get(key, {}))

    def keys_under(self, root):
        return [k for k in self.keys if k == root or k.startswith(root + '\\')]

    def write(self, values, key=display.KEY):
        stored = self.keys.setdefault(key, {})
        for n, v in values.items():
            if v is None: stored.pop(n, None)
            else: stored[n] = v


USER = dict(Fullscreen=1, ExclusiveFullscreen=0, WindowMaximized=0, WindowLeft=0, WindowTop=0, WindowWidth=2560,
            WindowHeight=1440, UserWindowWidth=2560, UserWindowHeight=1440, VSync=1, Sharpness=500, DOFQuality=2,
            MotionBlurSomething=7)


class InGameChanges(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.backup = pathlib.Path(self.folder.name)/'reports'/'backup.json'

    def tearDown(self):
        self.folder.cleanup()

    def test_setting_changed_in_game_is_kept_others_return(self):
        r = Registry(USER)
        display.prepare_launch(registry=r, backup=self.backup, screen=(2560, 1440))
        self.assertEqual((r.keys[display.KEY]['Sharpness'], r.keys[display.KEY]['VSync']), (1, 0))
        self.assertEqual(json.loads(self.backup.read_text())['written'][display.KEY]['Sharpness'], 1)
        r.keys[display.KEY]['Sharpness'] = 300  # the player's slider, in the game, during VR
        self.assertEqual(display.kept_in_game(r, self.backup), {'Sharpness': 300})
        self.assertTrue(display.restore(r, self.backup))
        g = r.keys[display.KEY]
        self.assertEqual((g['Sharpness'], g['VSync'], g['DOFQuality'], g['WindowWidth'], g['Fullscreen']),
                         (300, 1, 2, 2560, 1))
        self.assertFalse(self.backup.exists())

    def test_old_backup_without_written_restores_everything(self):
        r = Registry(USER)
        display.prepare_launch(registry=r, backup=self.backup, screen=(2560, 1440))
        saved = json.loads(self.backup.read_text()); del saved['written']
        self.backup.write_text(json.dumps(saved))
        r.keys[display.KEY]['Sharpness'] = 300
        display.restore(r, self.backup)
        self.assertEqual(r.keys[display.KEY]['Sharpness'], 500)

    def test_kept_picture_is_not_written_or_recorded(self):
        r = Registry(USER)
        display.prepare_launch(registry=r, backup=self.backup, screen=(2560, 1440),
                               keep=('depth_of_field', 'sharpening'))
        self.assertEqual((r.keys[display.KEY]['Sharpness'], r.keys[display.KEY]['DOFQuality']), (500, 2))
        self.assertNotIn('Sharpness', json.loads(self.backup.read_text())['written'][display.KEY])

    def test_camera_effects_off_for_the_session_and_back_after(self):
        # The October 22 report's registry: MotionBlurScale 1000 is the log's "Motion Blur 1.000000".
        r = Registry(dict(USER, MotionBlurScale=1000, FilmGrainStrength=1000, ChromaticAberration=1, Vignette=1))
        display.prepare_launch(registry=r, backup=self.backup, screen=(2560, 1440))
        g = r.keys[display.KEY]
        self.assertEqual((g['MotionBlurScale'], g['FilmGrainStrength'], g['ChromaticAberration'], g['Vignette']),
                         (0, 0, 0, 0))
        self.assertTrue(display.restore(r, self.backup))
        self.assertEqual((g['MotionBlurScale'], g['FilmGrainStrength'], g['ChromaticAberration'], g['Vignette']),
                         (1000, 1000, 1, 1))

    def test_kept_camera_effects_and_missing_ones_are_left_alone(self):
        r = Registry(dict(USER, MotionBlurScale=1000, Vignette=1))
        display.prepare_launch(registry=r, backup=self.backup, screen=(2560, 1440), keep=('comfort',))
        self.assertEqual((r.keys[display.KEY]['MotionBlurScale'], r.keys[display.KEY]['Vignette']), (1000, 1))
        r2 = Registry(USER)  # a game that never saved them: nothing is created
        backup2 = self.backup.with_name('other.json')
        display.prepare_launch(registry=r2, backup=backup2, screen=(2560, 1440))
        self.assertNotIn('FilmGrainStrength', r2.keys[display.KEY])

    def test_game_aa_taa_for_the_session_and_back_after(self):
        # 0.0.73 AA GAME AA TAA: test 5's DLAA/DLSS Quality (AA 2, UpscaleMethod 2) is TAA with no upscaler
        # while VR runs (AA 2, UpscaleMethod 0: "Anti aliasing: TAA / Upscale method: None" in 23 of the 31 kept reports),
        # and the player's own after; without 'game_aa' (KEEP MY GAME AA) nothing is written.
        r = Registry(dict(USER, AA=3, UpscaleMethod=2, UpscaleQuality=3))
        display.prepare_launch(registry=r, backup=self.backup, screen=(2560, 1440), off=('game_aa',))
        g = r.keys[display.KEY]
        self.assertEqual((g['AA'], g['UpscaleMethod'], g['UpscaleQuality']), (2, 0, 3))
        self.assertTrue(display.restore(r, self.backup))
        self.assertEqual((g['AA'], g['UpscaleMethod']), (3, 2))
        r2 = Registry(dict(USER, AA=2, UpscaleMethod=2))
        display.prepare_launch(registry=r2, backup=self.backup.with_name('kept.json'), screen=(2560, 1440))
        self.assertEqual(r2.keys[display.KEY]['UpscaleMethod'], 2)

    def test_hud_option_probe_only_reads(self):
        # 38.5: the game's own Hide HUD options are looked for, by the exe's names, and never written.
        r = Registry(USER)
        self.assertEqual(display.hud_option_probe(r), {})
        r.keys[display.GAME_KEY + '\\UI'] = dict(UIHideHUDEnabled=0, Other=3)
        before = json.dumps(r.keys, sort_keys=True)
        self.assertEqual(display.hud_option_probe(r), {display.GAME_KEY + '\\UI': {'UIHideHUDEnabled': 0}})
        self.assertEqual(json.dumps(r.keys, sort_keys=True), before)

    def test_snapshot_lists_every_value(self):
        snap = display.snapshot(Registry(USER))
        self.assertEqual(snap[display.KEY]['MotionBlurSomething'], 7)

    def test_smaller_desktop_view_and_back(self):
        # Update 33: -SmallerDesktopView writes a 1280 x 720 window; the user's own returns afterwards.
        r = Registry(USER)
        window = display.prepare_launch(registry=r, backup=self.backup, screen=(2560, 1440),
                                        height=display.SMALLER_HEIGHT)
        self.assertEqual((window['WindowWidth'], window['WindowHeight']), (1280, 720))
        self.assertEqual(r.keys[display.KEY]['WindowHeight'], 720)
        display.restore(r, self.backup)
        self.assertEqual(r.keys[display.KEY]['WindowHeight'], 1440)
        r2 = Registry(USER)
        window = display.prepare_launch(registry=r2, backup=self.backup.with_name('b2.json'), screen=(2560, 1440))
        self.assertEqual(window['WindowHeight'], display.HEIGHT)


if __name__ == '__main__':
    unittest.main()
