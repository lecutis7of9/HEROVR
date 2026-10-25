# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
import pathlib, sys, unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
import session_comfort as comfort

LOG = '''17:00:11:081 (00015100) > [*****Display + Graphics Settings*****]
17:00:11:081 (00015100) > [Settings] Window settings:
17:00:11:081 (00015100) > [Settings] \tVSync: Off
17:00:11:081 (00015100) > [Settings] \tShadow Quality: Ultra
17:00:11:081 (00015100) > [Settings] \tAmbient Occlusion: SSAO
17:00:11:081 (00015100) > [Settings] \tVignette: On
17:00:11:081 (00015100) > [Settings] \tMotion Blur: 1.000000
17:00:11:081 (00015100) > [Settings] \tFilm Grain Strength: 0.000000
17:00:11:081 (00015100) > [Settings] \tScreen Shake: Off
17:00:11:081 (00015100) > [*****Display + Graphics Settings*****]
17:00:12:000 (00015100) > [Settings] \tMotion Blur: 0.5
'''


def samples(new_per_second, seconds=10, presentation='immersive'):
    return [dict(status=3, presentation=presentation, seconds=i * .05, unique_submitted=int(i * .05 * new_per_second),
                 frames=int(i * .05 * 90)) for i in range(int(seconds / .05) + 1)]


class Settings(unittest.TestCase):
    def test_newest_block_by_the_games_names(self):
        found = comfort.game_settings(LOG)
        self.assertEqual(found['Motion Blur'], '1.000000')  # the line after the block is not the block's
        self.assertEqual(found['Shadow Quality'], 'Ultra')
        self.assertNotIn('Window settings', found)
        again = comfort.game_settings(LOG + LOG.replace('Ultra', 'High'))
        self.assertEqual(again['Shadow Quality'], 'High')


class Rate(unittest.TestCase):
    def test_immersive_windows_only(self):
        rate = comfort.play_rate(samples(34) + samples(90, presentation='screen'))
        self.assertAlmostEqual(rate['new_pairs_fps'], 34, delta=1)
        self.assertAlmostEqual(rate['headset_fps'], 90, delta=1)
        self.assertEqual(rate['windows'], 5)
        self.assertIsNone(comfort.play_rate(samples(34, presentation='screen')))

    def test_advice(self):
        settings = comfort.game_settings(LOG)
        slow = comfort.comfort_advice(comfort.play_rate(samples(34)), settings, 125)
        text = '\n'.join(slow)
        self.assertIn('-RenderScale 125', text)
        for name in ('Motion Blur', 'Vignette', 'Ambient Occlusion', 'Shadow Quality'):
            self.assertIn(name, text)
        for name in ('Film Grain', 'Screen Shake'):  # already off
            self.assertNotIn(name, text)
        even = '\n'.join(comfort.comfort_advice(comfort.play_rate(samples(45)), settings, 100))
        self.assertNotIn('RenderScale', even)
        timing = dict(pair_to_copy=dict(count=3, mean_ms=21.4, max_ms=40))
        self.assertIn('21 ms', '\n'.join(comfort.comfort_advice(None, {}, None, timing)))


if __name__ == '__main__':
    unittest.main()
