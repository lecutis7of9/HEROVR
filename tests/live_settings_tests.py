# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
import os, pathlib, sys, tempfile, time, unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
import live_settings as live


class Parse(unittest.TestCase):
    def test_values_aliases_and_comments(self):
        v, p = live.parse('reach = 100\n# note\nMonitorRate=15 # every\n-BuildingReach 120%\n')
        self.assertEqual(v, {'reach': 120, 'monitor': 15})
        self.assertEqual(p, [])

    def test_eye_aa(self):
        self.assertEqual(live.parse('aa = 0\n'), ({'aa': 0}, []))
        self.assertEqual(live.parse('EyeAA=1\n'), ({'aa': 1}, []))
        self.assertEqual(live.parse('aa=2\n')[1], ['line 1: aa must be 0-1'])

    def test_eye_history(self):
        self.assertEqual(live.parse('history = 50\n'), ({'history': 50}, []))
        self.assertEqual(live.parse('EyeHistory=0\n'), ({'history': 0}, []))
        self.assertEqual(live.parse('history=90\n'), ({'history': 90}, []))
        self.assertEqual(live.parse('history=99\n')[1], ['line 1: history must be 0-95'])
        self.assertEqual(live.SETTINGS['history'][1], 'SpidyEyeHistory')

    def test_web_width(self):
        self.assertEqual(live.parse('web_width = 1.3\n'), ({'web_width': 1.3}, []))
        self.assertEqual(live.parse('WebWidth=2 cm\n'), ({'web_width': 2.0}, []))
        self.assertEqual(live.parse('web_width=5\n')[1], ['line 1: web_width must be 0.4-4'])
        self.assertEqual(live.parse('web_width=x\n')[1], ['line 1: web_width needs a number'])
        self.assertEqual(live.SETTINGS['web_width'][1], 'SpidyWebWidth')
        self.assertEqual(live.export_value('web_width', 1.3), 130)

    def test_wrist_hud(self):
        self.assertEqual(live.parse('hud = 0\n'), ({'hud': 0}, []))
        self.assertEqual(live.parse('wrist_hud=1\n'), ({'hud': 1}, []))
        self.assertEqual(live.parse('hud=2\n')[1], ['line 1: hud must be 0-1'])
        self.assertEqual(live.SETTINGS['hud'][1], 'SpidyWristHud')

    def test_wrist_hud_hand(self):
        self.assertEqual(live.parse('hud_hand = right\n'), ({'hud_hand': 'right'}, []))
        self.assertEqual(live.parse('hud_hand=Left\n'), ({'hud_hand': 'left'}, []))
        self.assertEqual(live.parse('hud_hand=1\n'), ({'hud_hand': 'right'}, []))
        self.assertEqual(live.parse('hud_hand=up\n')[1], ['line 1: hud_hand must be left or right'])
        self.assertEqual(live.export_value('hud_hand', 'left'), 0)
        self.assertEqual(live.export_value('hud_hand', 'right'), 1)
        self.assertEqual(live.SETTINGS['hud_hand'][1], 'SpidyWristHudHand')

    def test_wrist_hud_scale(self):
        self.assertEqual(live.parse('hud_scale = 1.2\n'), ({'hud_scale': 1.2}, []))
        self.assertEqual(live.parse('hud_scale = 150%\n'), ({'hud_scale': 1.5}, []))
        self.assertEqual(live.parse('hud_scale = 3\n')[1], ['line 1: hud_scale must be 0.5-2'])
        self.assertEqual(live.parse('hud_scale = x\n')[1], ['line 1: hud_scale needs a number'])
        self.assertEqual(live.export_value('hud_scale', 1.2), 120)
        self.assertEqual(live.SETTINGS['hud_scale'][1], 'SpidyWristHudScale')

    def test_web_travel(self):
        self.assertEqual(live.parse('web_travel = 0\n'), ({'web_travel': 0}, []))
        self.assertEqual(live.parse('web_shot = 1\n'), ({'web_travel': 1}, []))
        self.assertEqual(live.parse('web_travel = 2\n')[1], ['line 1: web_travel must be 0-1'])
        self.assertEqual(live.SETTINGS['web_travel'][1], 'SpidyWebTravel')

    def test_web_speed(self):
        self.assertEqual(live.parse('web_speed = 120\n'), ({'web_speed': 120}, []))
        self.assertEqual(live.parse('shot_speed = 400\n'), ({'web_speed': 400}, []))
        self.assertEqual(live.parse('web_speed = 5\n')[1], ['line 1: web_speed must be 20-2000'])
        self.assertEqual(live.export_value('web_speed', 120), 120)
        self.assertEqual(live.SETTINGS['web_speed'][1], 'SpidyWebSpeed')

    def test_unchanged_hud_not_reapplied(self):
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / 'live.txt'
            f = live.LiveFile(path, {'hud': 1, 'hud_hand': 'left', 'hud_scale': 1.0})
            path.write_text('hud = 1\nhud_hand = right\nhud_scale = 1\n')
            self.assertEqual(f.poll(), ({'hud_hand': 'right'}, []))
        self.assertEqual(live.export_value('history', 50), 50)

    def test_problems(self):
        v, p = live.parse('reach=20\nmonitor=x\nrenderscale=80\nfoo=1\n')
        self.assertEqual(v, {})
        self.assertEqual(len(p), 4)
        self.assertIn('relaunch', p[2])


class Watch(unittest.TestCase):
    def test_changes_only(self):
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / 'live.txt'
            w = live.LiveFile(path, {'reach': 150, 'monitor': 30})
            self.assertEqual(w.poll(), ({}, []))
            path.write_text('reach=150\nmonitor=10\n')
            self.assertEqual(w.poll(), ({'monitor': 10}, []))
            w.applied('monitor', 10)
            self.assertEqual(w.poll(), ({}, []))  # unchanged file
            path.write_text('reach=90\nmonitor=10\n')
            os.utime(path, ns=(time.time_ns() + 10**9,) * 2)
            self.assertEqual(w.poll(), ({'reach': 90}, []))



class HologramPersistence(unittest.TestCase):
    """Update 15: what the wrist hologram's pages set is kept across a relaunch."""
    M = live.MARKER

    def test_decode_hud_settings(self):
        self.assertEqual(live.decode_hud_settings(0, self.M | 130 | 180 << 10), {'web_width': 1.3, 'web_speed': 180})
        self.assertEqual(live.decode_hud_settings(1, self.M | 1 | 50 << 1 | 200 << 8 | 15 << 17),
                         {'aa': 1, 'history': 50, 'reach': 200, 'monitor': 15})
        self.assertEqual(live.decode_hud_settings(2, self.M | 150), {'supersample': 150})
        self.assertEqual(live.decode_hud_settings(3, self.M | 1 << 4 | 1 << 10),
                         {'web_width': True, 'supersample': True})
        self.assertEqual(live.decode_hud_settings(3, self.M), {})
        for refused in (None, -1, 0, 3970, 130):  # no marker: an old mod, a refusal or a failed call
            self.assertIsNone(live.decode_hud_settings(0, refused))
        self.assertIsNone(live.decode_hud_settings(11, self.M))
        # Version 0.0.62: selector 10 is the hologram's LENS FLARES
        self.assertEqual(live.decode_hud_settings(10, self.M), {'lens_flares': 'game'})
        self.assertEqual(live.decode_hud_settings(10, self.M | 1), {'lens_flares': 'off_in_vr'})
        self.assertIsNone(live.decode_hud_settings(10, 1))

    def test_sharpen_and_reticles(self):
        """Update 16: the headset sharpening and the aim reticles, live and kept."""
        values, problems = live.parse('sharpen = 30\nreticles = 0\n')
        self.assertEqual((values, problems), ({'sharpen': 30, 'reticles': 0}, []))
        self.assertEqual(live.parse('sharpness = 100\nreticle = 1')[0], {'sharpen': 100, 'reticles': 1})
        self.assertEqual(len(live.parse('sharpen = 201\nreticles = 2\nsharpen = -1')[1]), 3)
        self.assertEqual(live.SETTINGS['sharpen'][1], 'SpidyEyeSharpen')
        self.assertEqual(live.SETTINGS['reticles'][1], 'SpidyReticles')
        self.assertIn('sharpen', live.PERSISTED)
        self.assertIn('reticles', live.PERSISTED)
        self.assertEqual(live.decode_hud_settings(4, self.M | 30 | 1 << 8), {'sharpen': 30, 'reticles': 1})
        # Update 21: sharpening up to 200 (strong CAS), in 8 bits.
        self.assertEqual(live.decode_hud_settings(4, self.M | 180), {'sharpen': 180, 'reticles': 0})
        self.assertEqual(live.decode_hud_settings(4, self.M | 255 | 1 << 8), {'sharpen': 200, 'reticles': 1})
        self.assertEqual(live.parse('sharpen = 200\n'), ({'sharpen': 200}, []))
        self.assertEqual(live.decode_hud_settings(4, self.M), {'sharpen': 0, 'reticles': 0})
        self.assertEqual(live.decode_hud_settings(3, self.M | 1 << 11 | 1 << 12), {'sharpen': True, 'reticles': True})
        self.assertEqual(live.SELECTOR['sharpen'], 4)
        self.assertEqual(live.SELECTOR['reticles'], 4)
        self.assertEqual(live.write_back('# mine\nreach = 100\n', {'sharpen': 25, 'reticles': 0}).count('\n'), 4)

    def test_hologram_page(self):
        """Update 17: the HOLOGRAM page's height, size, text size, theme and text colour, live and kept."""
        values, problems = live.parse('hud_height = 12 cm\nhud_text = 150%\nhud_theme = Deutan\n'
                                      'hud_text_color = yellow\nhud_scale = 120%\n')
        self.assertEqual(problems, [])
        self.assertEqual(values, {'hud_height': 12, 'hud_text': 150, 'hud_theme': 'deutan',
                                  'hud_text_color': 'yellow', 'hud_scale': 1.2})
        self.assertEqual(live.parse('theme = 4\ntext_size = 75\nhud_height = 0')[0],
                         {'hud_theme': 'mono', 'hud_text': 75, 'hud_height': 0})
        self.assertEqual(len(live.parse('hud_height = 26\nhud_text = 70\nhud_text = 180\nhud_theme = pink\n'
                                        'hud_text_color = 4\nhud_theme = 6')[1]), 6)
        for name, export in (('hud_height', 'SpidyWristHudHeight'), ('hud_text', 'SpidyWristHudText'),
                             ('hud_theme', 'SpidyWristHudTheme'), ('hud_text_color', 'SpidyWristHudTextColor'),
                             ('hud_scale', 'SpidyWristHudScale')):
            self.assertEqual(live.SETTINGS[name][1], export)
            self.assertIn(name, live.PERSISTED)
            self.assertEqual(live.SELECTOR[name], 5)
        self.assertEqual(live.export_value('hud_theme', 'tritan'), 3)
        self.assertEqual(live.export_value('hud_text_color', 'cyan'), 3)
        self.assertEqual(live.export_value('hud_scale', 1.25), 125)
        code = self.M | 12 | 125 << 5 | 150 << 13 | 2 << 21 | 1 << 24
        self.assertEqual(live.decode_hud_settings(5, code), {'hud_height': 12, 'hud_scale': 1.25, 'hud_text': 150,
                                                             'hud_theme': 'deutan', 'hud_text_color': 'white'})
        # Out of range from an odd mod: held to the ranges, never an IndexError.
        self.assertEqual(live.decode_hud_settings(5, self.M | 31 | 7 << 21)['hud_theme'], 'raimi')
        # Update 36: 2000s Raimi.
        self.assertEqual(live.export_value('hud_theme', 'raimi'), 5)
        self.assertEqual(live.parse('hud_theme = 2000s_raimi')[0], {'hud_theme': 'raimi'})
        self.assertEqual(live.decode_hud_settings(5, self.M)['hud_scale'], 0.5)
        self.assertEqual(live.decode_hud_settings(3, self.M | 0x1f << 13),
                         {'hud_height': True, 'hud_scale': True, 'hud_text': True, 'hud_theme': True,
                          'hud_text_color': True})
        out = live.write_back('hud_theme = spiderman  # mine\n', {'hud_theme': 'tritan', 'hud_text': 125})
        self.assertEqual(out, 'hud_theme = tritan  # mine\nhud_text = 125\n')
        self.assertEqual(live.parse(out)[0], {'hud_theme': 'tritan', 'hud_text': 125})

    def test_write_back_keeps_comments_and_other_lines(self):
        text = '# mine\nreach = 100  # far ones\nWebWidth=2 cm\nhud_hand = right\nweb_width = 3\n'
        out = live.write_back(text, {'reach': 220, 'web_width': 1.5, 'aa': 0})
        self.assertEqual(out, '# mine\nreach = 220  # far ones\nweb_width = 1.5\nhud_hand = right\n'
                              'web_width = 1.5\naa = 0\n')
        self.assertEqual(live.parse(out), ({'reach': 220, 'web_width': 1.5, 'hud_hand': 'right', 'aa': 0}, []))
        self.assertEqual(live.write_back('', {'web_speed': 240}), 'web_speed = 240\n')

    def test_adopt_does_not_fight_poll(self):
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / 'live.txt'
            w = live.LiveFile(path, {'web_width': 1.3, 'reach': 150})
            self.assertTrue(w.adopt({'web_width': 2.5}))  # no file yet: made
            self.assertEqual(live.parse(path.read_text())[0], {'web_width': 2.5})
            self.assertEqual(w.poll(), ({}, []))  # its own write is not applied again
            self.assertEqual(w.current['web_width'], 2.5)
            path.write_text('web_width = 2.5\nreach = 90\n')
            os.utime(path, ns=(time.time_ns() + 10**9,) * 2)
            self.assertFalse(w.adopt({'web_width': 3.0}))  # the player's save is read first
            self.assertEqual(w.poll(), ({'reach': 90}, []))
            w.applied('reach', 90)
            self.assertTrue(w.adopt({'web_width': 3.0}))
            self.assertEqual(live.parse(path.read_text())[0], {'web_width': 3.0, 'reach': 90})
            self.assertFalse(list(pathlib.Path(d).glob('*.tmp')))  # written whole, in one step

    def test_persisted_values_are_applied_at_the_start(self):
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / 'live.txt'
            self.assertEqual(live.persisted(path), ({}, []))
            path.write_text('web_width = 2\nweb_speed = 300\naa = 0\nhistory = 40\nreach = 250\n'
                            'monitor = 20\nhud = 0\nhud_scale = 1.5\nbogus = 3\n')
            self.assertEqual(live.persisted(path)[0], {'web_width': 2.0, 'web_speed': 300, 'aa': 0, 'history': 40,
                                                       'reach': 250, 'monitor': 20, 'hud_scale': 1.5})
            # Update 18: WEB COLOR (control 19) is kept per style, as web_color_<style>
            self.assertEqual((set(live.PERSISTED) | {'supersample', 'web_color'}) - set(live.WEB_COLOR_KEYS),
                             set(live.CONTROL_KEYS.values()))

    def test_next_launch_supersample(self):
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / 'next-launch.txt'
            self.assertIsNone(live.next_launch_supersample(path))
            self.assertTrue(live.write_next_launch(path, 150))
            self.assertIn('RESTART REQUIRED', path.read_text())
            self.assertEqual(live.next_launch_supersample(path), 150)
            self.assertFalse(live.write_next_launch(path, 400))  # out of range: not written
            self.assertEqual(live.next_launch_supersample(path), 150)
            for bad in ('supersample = 999\n', 'supersample = x\n', 'supersample = 50\n'):
                path.write_text(bad)
                self.assertIsNone(live.next_launch_supersample(path))
            self.assertNotIn('supersample', live.SETTINGS)  # never a live setting

    def test_update23_next_launch_supersample_reset_once(self):
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / 'next-launch.txt'
            self.assertIsNone(live.migrate_next_launch(path))  # no file: nothing made
            self.assertFalse(path.exists())
            # An Update 15-22 file asking for 150: back to OFF once, with a line to show.
            path.write_text('# Next launch\nsupersample = 150\n')
            line = live.migrate_next_launch(path)
            self.assertIn('150%', line)
            self.assertIn('OFF', line)
            self.assertEqual(live.next_launch_supersample(path), 100)
            self.assertIn(live.SUPERSAMPLE_MIGRATED, path.read_text())
            # Chosen again from Update 23 on: it stays.
            self.assertTrue(live.write_next_launch(path, 170))
            self.assertIsNone(live.migrate_next_launch(path))
            self.assertEqual(live.next_launch_supersample(path), 170)
            # An old file already at 100 (or unreadable) only gets the marker, quietly.
            path.write_text('supersample = 100\n')
            self.assertIsNone(live.migrate_next_launch(path))
            self.assertEqual(live.next_launch_supersample(path), 100)
            self.assertIn(live.SUPERSAMPLE_MIGRATED, path.read_text())

    def test_update23_web_start(self):
        self.assertEqual(live.SETTINGS['web_start'][1], 'SpidyWebStart')
        for word, number in (('wrist', 0), ('palm', 1), ('0', 0), ('1', 1), ('Palm', 1)):
            values, problems = live.parse(f'web_start = {word}\n')
            self.assertEqual(problems, [])
            self.assertEqual(live.export_value('web_start', values['web_start']), number)
        self.assertEqual(live.parse('webstart = palm\n')[0], {'web_start': 'palm'})
        self.assertTrue(live.parse('web_start = 2\n')[1])  # refused, with a line saying why
        with tempfile.TemporaryDirectory() as d:  # kept: applied when the next session starts
            path = pathlib.Path(d) / 'live.txt'
            path.write_text('web_start = palm\n')
            self.assertEqual(live.persisted(path)[0], {'web_start': 'palm'})



class WebStyle(unittest.TestCase):
    """Update 18: web_style and web_color (each style's own shades, kept per style)."""

    def test_names_match_the_mod(self):
        self.assertEqual(live.WEB_STYLES, ('90s', 'messy', 'modern', 'symbiote', 'energy'))
        self.assertEqual(live.WEB_SHADES['energy'], ('blue', 'red', 'green', 'orange', 'purple', 'gold'))
        words = [w for shades in live.WEB_SHADES.values() for w in shades]
        self.assertEqual(len(words), len(set(words)))  # a shade's name is one style's only
        self.assertEqual([len(live.WEB_SHADES[s]) for s in live.WEB_STYLES], [4, 4, 4, 4, 6])
        # the names the mod's web_style::key / shadeKey give (src/web_visual.cpp)
        src = (pathlib.Path(__file__).parents[1] / 'src' / 'web_visual.cpp').read_text()
        for word in words + list(live.WEB_STYLES):
            self.assertIn(f'"{word}"', src)

    def test_parse_words_numbers_and_spellings(self):
        values, problems = live.parse('web_style = Messy\nweb_color = warm grey\nweb_color_energy = 5\n'
                                      'web_colour_modern = BRIGHT-WHITE\nweb_color_90s = gold\nwebstyle = 7\n')
        self.assertEqual(values, {'web_style': 'messy', 'web_color': 'warm_grey', 'web_color_energy': 'gold',
                                  'web_color_modern': 'bright_white'})
        self.assertEqual(len(problems), 2)  # gold is not a 90s shade; there is no style 7
        self.assertEqual(live.parse('web_style = nineties\nweb_color = 3\n')[0], {'web_style': '90s', 'web_color': 3})

    def test_export_values(self):
        self.assertEqual(live.export_value('web_style', 'symbiote'), 3)
        self.assertEqual(live.export_value('web_color', 'gold'), (5 << 4) | 5)   # ENERGY's gold
        self.assertEqual(live.export_value('web_color', 'frost'), (2 << 4) | 3)  # MESSY's frost
        self.assertEqual(live.export_value('web_color', 2), 2)                     # the style chosen now's
        self.assertEqual(live.export_value('web_color_90s', 'parchment'), (1 << 4) | 2)
        self.assertEqual(live.export_value('web_color_symbiote', 'matte'), (4 << 4) | 3)

    def test_hologram_readback_and_persistence(self):
        self.assertEqual(live.decode_hud_settings(3, live.MARKER | 1 << 18 | 1 << 19),
                         {'web_style': True, **{k: True for k in live.WEB_COLOR_KEYS}})
        for key in ('web_style', *live.WEB_COLOR_KEYS):
            self.assertEqual(live.SELECTOR[key], 6)
            self.assertIn(key, live.PERSISTED)
        code = 4 | 1 << 3 | 3 << 6 | 2 << 9 | 3 << 12 | 5 << 15
        self.assertEqual(live.decode_hud_settings(6, live.MARKER | code),
                         {'web_style': 'energy', 'web_color_90s': 'ivory', 'web_color_messy': 'frost',
                          'web_color_modern': 'ice', 'web_color_symbiote': 'matte', 'web_color_energy': 'gold'})
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / 'live.txt'
            path.write_text('web_style = modern\nweb_color_messy = smoke\n')
            w = live.LiveFile(path, {'web_style': '90s', 'web_color_messy': 'silver'})
            self.assertTrue(w.adopt({'web_style': 'energy', 'web_color_energy': 'orange'}))
            self.assertEqual(live.persisted(path)[0], {'web_style': 'energy', 'web_color_messy': 'smoke',
                                                       'web_color_energy': 'orange'})
            path.write_text('web_style = messy\nweb_color = 1\n')  # a number: the style's own list
            self.assertEqual(live.persisted(path)[0], {'web_style': 'messy', 'web_color_messy': 'pearl'})
            path.write_text('web_color = crimson_sheen\n')
            self.assertEqual(live.persisted(path)[0], {'web_color_symbiote': 'crimson_sheen'})


class Music(unittest.TestCase):
    """Update 19: the MUSIC page's music_volume, music_visualizer and music_track."""

    def test_names_match_the_mod(self):
        self.assertEqual(live.VISUALIZERS, ('original', 'bars', 'fractal_bloom', 'tunnel', 'starburst'))
        src = (pathlib.Path(__file__).parents[1] / 'src' / 'music_player.cpp').read_text()
        for word in live.VISUALIZERS:
            self.assertIn(f'"{word}"', src)
        game = (pathlib.Path(__file__).parents[1] / 'src' / 'game_xr.cpp').read_text()
        for name in ('music_volume', 'music_visualizer', 'music_track'):
            self.assertIn(f'WINAPI {live.SETTINGS[name][1]}(', game)

    def test_parse_and_export(self):
        values, problems = live.parse('music_volume = 40%\nvisualiser = Fractal Bloom\nsong = 3\n'
                                      'music_volume = 101\nmusic_visualizer = 5\nmusic_track = -1\n')
        self.assertEqual(values, {'music_volume': 40, 'music_visualizer': 'fractal_bloom', 'music_track': 3})
        self.assertEqual(len(problems), 3)
        self.assertEqual(live.parse('music_visualizer = 4\n')[0], {'music_visualizer': 'starburst'})
        self.assertEqual(live.export_value('music_visualizer', 'tunnel'), 3)
        self.assertEqual(live.export_value('music_volume', 55), 55)
        self.assertEqual(live.export_value('music_track', 12), 12)

    def test_hologram_readback_and_persistence(self):
        self.assertEqual(live.decode_hud_settings(3, live.MARKER | 1 << 20 | 1 << 27 | 1 << 28),
                         {'music_track': True, 'music_volume': True, 'music_visualizer': True})
        for key in ('music_volume', 'music_visualizer', 'music_track'):
            self.assertEqual(live.SELECTOR[key], 7)
            self.assertIn(key, live.PERSISTED)
        self.assertEqual(live.decode_hud_settings(7, live.MARKER | 85 | 2 << 7 | 9 << 10),
                         {'music_volume': 85, 'music_visualizer': 'fractal_bloom', 'music_track': 9})
        self.assertEqual(live.decode_hud_settings(7, live.MARKER | 127 | 7 << 7),
                         {'music_volume': 100, 'music_visualizer': 'starburst', 'music_track': 0})
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / 'live.txt'
            path.write_text('music_volume = 30  # quiet\n')
            w = live.LiveFile(path, {'music_volume': 70})
            self.assertTrue(w.adopt({'music_volume': 45, 'music_visualizer': 'tunnel', 'music_track': 2}))
            self.assertEqual(live.persisted(path)[0],
                             {'music_volume': 45, 'music_visualizer': 'tunnel', 'music_track': 2})
            self.assertIn('# quiet', path.read_text())

    def test_music_folder(self):
        with tempfile.TemporaryDirectory() as d:
            line = live.ensure_music_folder(d)
            self.assertIn('Music', line)
            readme = pathlib.Path(d) / 'Music' / 'README.txt'
            self.assertIn('MP3', readme.read_text())
            readme.write_text('mine')
            live.ensure_music_folder(d)  # kept, never overwritten
            self.assertEqual(readme.read_text(), 'mine')
            (pathlib.Path(d) / 'blocked').write_text('')
            self.assertIn('could not', live.ensure_music_folder(pathlib.Path(d) / 'blocked'))


class WebStyleMigration(unittest.TestCase):
    """Update 20: the default web look is messy; a live.txt left on 90s moves to it once, never again."""

    def test_default(self):
        self.assertEqual(live.DEFAULT_WEB_STYLE, 'messy')
        self.assertIn(live.DEFAULT_WEB_STYLE, live.WEB_STYLES)

    def test_once(self):
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / 'live.txt'
            path.write_text('reach = 120\nweb_style = 90s  # mine\nweb_color_90s = ivory')
            self.assertIn('messy', live.migrate_web_style(path))
            text = path.read_text()
            self.assertEqual(live.parse(text), ({'reach': 120, 'web_style': 'messy', 'web_color_90s': 'ivory'}, []))
            self.assertIn('# mine', text)
            self.assertIn(live.WEB_STYLE_MIGRATED, text)
            # The player picks 90s again: it stays.
            path.write_text(live.write_back(path.read_text(), {'web_style': '90s'}))
            self.assertIsNone(live.migrate_web_style(path))
            self.assertEqual(live.parse(path.read_text())[0]['web_style'], '90s')
            self.assertEqual(live.persisted(path)[0]['web_style'], '90s')

    def test_other_styles_and_no_file(self):
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / 'live.txt'
            self.assertIsNone(live.migrate_web_style(path))  # none: made with the marker only
            self.assertEqual(live.parse(path.read_text()), ({}, []))
            path.write_text(live.write_back(path.read_text(), {'web_style': '90s'}))
            self.assertIsNone(live.migrate_web_style(path))  # a 90s chosen after Update 20 stays
            self.assertEqual(live.parse(path.read_text())[0]['web_style'], '90s')
            other = pathlib.Path(d) / 'other.txt'
            other.write_text('web_style = energy\n')
            self.assertIsNone(live.migrate_web_style(other))
            self.assertEqual(live.parse(other.read_text())[0], {'web_style': 'energy'})
            self.assertIsNone(live.migrate_web_style(pathlib.Path(d) / 'missing' / 'live.txt'))


class EyeDebugAndShimmer(unittest.TestCase):
    """Update 24: the far-flicker eye test (live, never kept) and the session-start shimmer notes."""

    def test_eye_debug_parse_and_export(self):
        self.assertEqual(live.parse('eye_debug = same\n'), ({'eye_debug': 'same'}, []))
        self.assertEqual(live.parse('eye_mono = on\n')[0], {'eye_debug': 'same'})
        self.assertEqual(live.parse('EyeDebug = off\n')[0], {'eye_debug': 'off'})
        self.assertEqual(live.parse('eye_debug = 2\n')[1], ['line 1: eye_debug must be off or same'])
        # Update 26: Update 25's HEAD SHADOWS is gone (it never took effect in SM2): refused, not applied.
        self.assertEqual(live.parse('eye_debug = shadows\n')[1], ['line 1: eye_debug must be off or same'])
        self.assertEqual(live.SETTINGS['eye_debug'][0], (0, 1))
        self.assertEqual((live.export_value('eye_debug', 'same'), live.export_value('eye_debug', 'off')), (1, 0))
        self.assertEqual(live.SETTINGS['eye_debug'][1], 'SpidyEyeDebug')
        self.assertNotIn('eye_debug', live.PERSISTED)  # a test: never kept for the next session
        self.assertNotIn('eye_debug', live.CONTROL_KEYS.values())
        self.assertNotIn('eye_debug', live.START_ONLY)  # live: applied when live.txt is saved

    def test_export_is_called(self):
        text = (pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'run_game_vr.py').read_text(encoding='utf-8')
        self.assertIn("'SpidyEyeDebug'", text)

    def test_shimmer_notes(self):
        self.assertEqual(live.shimmer_notes({'sharpen': 100}, {}), [])
        notes = live.shimmer_notes({'sharpen': 190}, {})
        self.assertEqual(len(notes), 1)
        self.assertIn('190%', notes[0])
        self.assertEqual(live.shimmer_notes({}, {'eye_debug': 'off'}), [])
        notes = live.shimmer_notes({}, {'eye_debug': 'same'})
        self.assertEqual(len(notes), 1)
        self.assertIn('eye_debug = off', notes[0])

    def test_start_notes(self):
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / 'live.txt'
            self.assertEqual(live.start_notes(path), [])  # no file: nothing, no error
            path.write_text('sharpen = 190\neye_debug = same\n')
            self.assertEqual(len(live.start_notes(path)), 2)


class SessionFps(unittest.TestCase):
    """Update 24: the session HIGH / LOW / AVG FPS, SpidyHudSettings(8, 9), in the report and at exit."""
    M = 1 << 30

    def test_decode(self):
        avg = self.M | 452 | 125 << 11
        high_low = self.M | 601 | 388 << 11
        self.assertEqual(live.session_fps(avg, high_low),
                         dict(high=60.1, low=38.8, average=45.2, tracked_seconds=125))
        # Nothing tracked yet, an older mod (3970, no marker), a failed read.
        self.assertIsNone(live.session_fps(self.M | 0, self.M))
        self.assertIsNone(live.session_fps(3970, 3970))
        self.assertIsNone(live.session_fps(None, high_low))
        self.assertNotIn(8, set(live.SELECTOR.values()))
        self.assertNotIn(9, set(live.SELECTOR.values()))

    def test_line(self):
        line = live.session_fps_line(dict(high=60.1, low=38.8, average=45.2, tracked_seconds=125))
        self.assertIn('HIGH 60.1, LOW 38.8, AVG 45.2 over 125 s', line)
        self.assertIn('10 s after loading', line)
        self.assertIn('nothing tracked', live.session_fps_line(None))

    def test_launcher_reads_and_prints_it(self):
        text = (pathlib.Path(__file__).resolve().parents[1]/'tools'/'run_game_vr.py').read_text(encoding='utf-8')
        self.assertIn('session_fps=session_fps_last', text)
        self.assertIn('live_settings.session_fps_line(', text)
        source = (pathlib.Path(__file__).resolve().parents[1]/'src'/'game_xr.cpp').read_text(encoding='utf-8')
        self.assertIn('case 8:', source)
        self.assertIn('wristSession.reset()', source)


class AddressFinder(unittest.TestCase):
    def test_eye_lod_parse_and_export(self):
        # Update 28: EYE LOD, live, default off, never kept.
        self.assertEqual(live.parse('eye_lod = on\n'), ({'eye_lod': 'on'}, []))
        self.assertEqual(live.parse('eyelod = off\n')[0], {'eye_lod': 'off'})
        self.assertEqual(live.parse('eye_lod = 2\n')[1], ['line 1: eye_lod must be off or on'])
        self.assertEqual(live.SETTINGS['eye_lod'][1], 'SpidyEyeLod')
        self.assertEqual((live.export_value('eye_lod', 'on'), live.export_value('eye_lod', 'off')), (1, 0))
        self.assertNotIn('eye_lod', live.PERSISTED)
        self.assertNotIn('eye_lod', live.CONTROL_KEYS.values())
        self.assertNotIn('eye_lod', live.START_ONLY)
        runner = (pathlib.Path(__file__).resolve().parents[1] / 'tools/run_game_vr.py').read_text(encoding='utf-8')
        self.assertIn("'SpidyEyeLod'", runner)

    def test_address_finder_parse_and_export(self):
        self.assertEqual(live.parse('address_finder = on\n'), ({'address_finder': 'on'}, []))
        self.assertEqual(live.parse('addressfinder = off\n')[0], {'address_finder': 'off'})
        self.assertEqual(live.parse('address_finder = start\n')[0], {'address_finder': 'on'})
        self.assertEqual(live.parse('address_finder = 2\n')[1],
                         ['line 1: address_finder must be off or on'])
        self.assertEqual(live.SETTINGS['address_finder'][1], 'SpidyAddressFinder')
        self.assertEqual((live.export_value('address_finder', 'on'),
                          live.export_value('address_finder', 'off')), (1, 0))
        self.assertNotIn('address_finder', live.PERSISTED)
        self.assertNotIn('address_finder', live.CONTROL_KEYS.values())
        self.assertNotIn('address_finder', live.START_ONLY)


class Extras385(unittest.TestCase):
    def test_extras_keys(self):
        for key, top, setter in (('invincible', 1, 'SpidyInvincible'), ('fly_mode', 3, 'SpidyFlyMode'),
                                 ('fly_vignette', 1, 'SpidyFlyVignette'), ('symbiote_look', 1, 'SpidySymbioteLook'),
                                 ('prompt_handling', 1, 'SpidyPromptHandling'), ('gesture_moves', 2, 'SpidyGestureMoves'),
                                 ('fly_roll', 2, 'SpidyFlyRoll'), ('fly_auto_level', 1, 'SpidyFlyAutoLevel')):
            self.assertEqual(live.SETTINGS[key][0], (0, top))
            self.assertEqual(live.SETTINGS[key][1], setter)
            self.assertEqual(live.parse('%s = %d\n' % (key, top)), ({key: top}, []))
            self.assertTrue(live.parse('%s = %d\n' % (key, top + 1))[1])

    def test_cheats_start_off_each_session(self):
        self.assertNotIn('invincible', live.START_ONLY)
        self.assertNotIn('fly_mode', live.START_ONLY)
        self.assertIn('fly_vignette', live.START_ONLY)
        self.assertIn('gesture_moves', live.START_ONLY)
        for key in ('fly_roll', 'fly_roll_speed', 'fly_auto_level'):
            self.assertIn(key, live.START_ONLY)

    def test_fly_roll_speed_range(self):
        self.assertEqual(live.SETTINGS['fly_roll_speed'][:2], ((30, 180), 'SpidyFlyRollSpeed'))
        self.assertEqual(live.parse('fly_roll_speed = 90\n'), ({'fly_roll_speed': 90}, []))
        self.assertTrue(live.parse('fly_roll_speed = 181\n')[1])
        self.assertTrue(live.parse('fly_roll_speed = 29\n')[1])


if __name__ == '__main__':
    unittest.main()
