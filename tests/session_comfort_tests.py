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
        self.assertNotIn('Hz:', even)
        timing = dict(pair_to_copy=dict(count=3, mean_ms=21.4, max_ms=40))
        self.assertIn('21 ms', '\n'.join(comfort.comfort_advice(None, {}, None, timing)))


    def test_performance_advice_0_0_6(self):
        # rep0050 (0.0.5, October 25): 37 new pictures a second at 90 Hz, RenderScale 100, the copy 6.82 ms
        # of which 0.19 ms is its own work: GPU-bound, 72 Hz divides evenly, 90/80% are estimates.
        rate = dict(new_pairs_fps=37.0, headset_fps=90.0, lowest_new_pairs_fps=29.0, headset_frames_per_pair=2.43)
        timing = dict(copy_gpu=dict(count=38612, mean_ms=6.82), sharpen=dict(gpu_ms_mean=.19))
        text = '\n'.join(comfort.performance_advice(rate, 100, timing))
        self.assertIn('graphics card is the limit', text)
        self.assertIn('to 72 Hz', text)
        self.assertIn('-RenderScale 90', text)
        self.assertIn('-RenderScale 80', text)
        self.assertIn('ESTIMATE', text)
        # The model reproduces its own measurement: 45 at 100% -> 33 at 125%.
        self.assertAlmostEqual(comfort.estimated_fps(45, 100, 125), 33, delta=.1)
        self.assertAlmostEqual(comfort.estimated_fps(33, 125, 100), 45, delta=.1)
        # 40 a second at 90 Hz: 80 Hz is the even rate; 46 at 90: nothing to say; never a raise in scale.
        self.assertIn('to 80 Hz', '\n'.join(comfort.performance_advice(dict(rate, new_pairs_fps=40.0), 100)))
        self.assertEqual(comfort.performance_advice(dict(rate, new_pairs_fps=46.0), 100, timing)[1:], [])
        self.assertNotIn('RenderScale', '\n'.join(comfort.performance_advice(rate, 80)))
        self.assertEqual(comfort.performance_advice(None), [])
        # 120 Hz at 50: 90 Hz (45 a second) is even.
        self.assertIn('to 90 Hz', '\n'.join(comfort.performance_advice(dict(rate, headset_fps=120.0,
                                                                               new_pairs_fps=50.0), 100)))
        # The session's advice carries it.
        self.assertIn('72 Hz', '\n'.join(comfort.comfort_advice(rate | dict(windows=1), {}, 100, timing)))


if __name__ == '__main__':
    unittest.main()
