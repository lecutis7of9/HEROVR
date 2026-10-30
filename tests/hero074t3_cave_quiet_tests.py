# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.74 test 3 CAVE QUIET: the city sound ducked and the engine stopped in the Batcave / safehouse (on, live)."""
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class CaveQuiet(unittest.TestCase):
    def test_setting(self):
        self.assertEqual(live_settings.UPDATE38_DEFAULTS['cave_quiet'], 'on')
        self.assertEqual(live_settings.export_value('cave_quiet', 'on'), 0)
        self.assertEqual(live_settings.export_value('cave_quiet', 'off'), 1)
        self.assertEqual(live_settings.SETTINGS['cave_quiet'][1], 'SpidyCaveQuiet')
        self.assertIn('{"cave_quiet", "CAVE QUIET", "CAVQ",', src('include/spidy/combat38.hpp'))

    def test_wired(self):
        xr = src('src/game_xr.cpp')
        self.assertIn('sfx::loop(sfx::vanEngine, engineSounds);', xr)
        self.assertNotIn('sfx::loop(sfx::vanEngine, vanDriving);', xr)
        self.assertIn('spidy::gameDuck(quietWant);', xr)
        self.assertIn('~CaveQuietEnd() { spidy::gameDuck(false); }', xr)  # back on every way out of VR
        self.assertIn('spidy::gameDuckRecover();', xr)
        self.assertIn('src/game_duck_win.cpp', src('CMakeLists.txt'))
        win = src('src/game_duck_win.cpp')
        self.assertIn('GetSimpleAudioVolume(nullptr, FALSE, &volume)', win)
        # the songs keep their volume: their own session
        self.assertIn('&f.Format, &musicSession)', src('src/music_audio_win.cpp'))


if __name__ == '__main__':
    unittest.main()
