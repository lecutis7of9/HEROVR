"""HERO VR 0.0.73 release: GAME TAA (image_style) starts OFF (the owner on test 6: the picture shook under GAME TAA
SPIDYVR and was steady with it OFF), a live.txt at the test builds' SPIDYVR moves to OFF once, GAME AA TAA follows
GAME TAA, and the MP SOLO TEST is not in the public build."""
import os, pathlib, sys, tempfile, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import live_settings as L  # noqa: E402


def src(p):
    return open(os.path.join(ROOT, p), encoding='utf-8').read()


class GameTaaOff(unittest.TestCase):
    def test_defaults(self):
        self.assertEqual(L.UPDATE38_DEFAULTS['image_style'], 'off')
        self.assertEqual(L.export_value('image_style', 'spidyvr'), 0)  # index 0 keeps its meaning
        self.assertEqual(L.export_value('image_style', 'off'), 1)
        self.assertEqual(L.image_style_of({}), 'off')
        self.assertEqual(L.image_style_in_file('/nonexistent/live.txt'), 'off')
        h = src('include/spidy/image_style.hpp')
        self.assertIn('inline constexpr uint32_t defaultStyle = gameTaaOff;', h)
        self.assertIn('return style == spidyvr;', h)
        g = src('src/game_xr.cpp')
        self.assertIn('combat38Values[combat38::imageStyle].store(image_style::defaultStyle);', g)
        self.assertIn('std::atomic<uint32_t> imageStyle{spidy::image_style::defaultStyle};', src('src/stereo_probe.cpp'))

    def test_game_aa_taa_follows_game_taa(self):
        self.assertFalse(L.game_aa_taa(True, L.image_style_of({}), False))  # the release default: the game's own AA
        self.assertTrue(L.game_aa_taa(True, 'spidyvr', False))             # switched on: as before
        r = src('tools/run_game_vr.py')
        self.assertLess(r.index('live_settings.migrate_0073(ROOT/'), r.index('game_taa_look = live_settings.image_style_in_file'))

    def run_migration(self, body):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d, 'live.txt')
            p.write_text(body, encoding='utf-8')
            line = L.migrate_0073(p)
            text = p.read_text(encoding='utf-8')
            again = L.migrate_0073(p)
            return line, L.parse(text)[0].get('image_style'), text, again, p.read_text(encoding='utf-8')

    def test_migration_moves_the_old_default_once(self):
        for old in ('spidyvr', 'SPIDYVR', '0', 'spidy', 'on'):
            line, value, text, again, after = self.run_migration(f'image_style = {old}\nhistory = 80\n')
            self.assertEqual(value, 'off', old)
            self.assertIn('GAME TAA OFF', line)
            self.assertIn(L.MIGRATED_0073, text)
            self.assertIsNone(again)
            self.assertEqual(text, after)  # once only
            self.assertIn('history = 80', text)

    def test_migration_keeps_off_and_choices_after_the_marker(self):
        line, value, text, _, _ = self.run_migration('image_style = off\n')
        self.assertIsNone(line)
        self.assertEqual(value, 'off')
        self.assertIn(L.MIGRATED_0073, text)
        # SPIDYVR chosen after the marker (the wrist writes it back): stays
        line, value, _, _, _ = self.run_migration('image_style = off\n' + L.MIGRATED_0073 + '\nimage_style = spidyvr\n')
        self.assertIsNone(line)
        self.assertEqual(value, 'spidyvr')

    def test_migration_0072_live_txt_and_none(self):
        line, value, text, _, _ = self.run_migration('history = 80\n')  # 0.0.72 never wrote image_style
        self.assertIsNone(line)
        self.assertIsNone(value)
        self.assertIn(L.MIGRATED_0073, text)
        self.assertEqual(L.image_style_of(L.parse(text)[0]), 'off')
        self.assertIsNone(L.migrate_0073('/nonexistent/dir/live.txt'))
        self.assertIn(L.MIGRATED_0073, L.HEADER)  # a new live.txt starts migrated


class NoMultiplayerTest(unittest.TestCase):
    def test_mp_solo_test_left_out(self):
        self.assertNotIn('mp_solo_test', L.SETTINGS)
        self.assertNotIn('mp_solo_test', L.UPDATE38_DEFAULTS)
        self.assertNotIn('mp_solo_test', L.UPDATE38_KEYS)
        for f in ('include/spidy/combat38.hpp', 'include/spidy/wrist_hud.hpp', 'src/wrist_hud.cpp', 'src/game_xr.cpp',
                  'CMakeLists.txt', 'tools/update37_records.py'):
            s = src(f)
            for bad in ('mpSoloTest', 'MpSoloTest', 'mp_solo_test', 'mp_session', 'mp_ghost'):
                self.assertNotIn(bad, s, (f, bad))
        for f in ('src/mp_session.cpp', 'src/mp_net.cpp', 'include/spidy/mp_session.hpp', 'tools/mp_test_buddy.py',
                  'docs/MP-SOLO-TEST.md'):
            self.assertFalse(os.path.exists(os.path.join(ROOT, f)), f)


class ReleaseLabels(unittest.TestCase):
    """The public 0.0.73 release: no test labels, HERO VR 0.0.73 on the wrist and the console."""
    def test_version_and_labels(self):
        import re
        self.assertEqual(src('VERSION').strip(), '0.0.73')
        self.assertRegex(src('CMakeLists.txt'), r'project\(Spidy VERSION 0\.0\.73 ')
        self.assertRegex(src('CMakeLists.txt'), r'option\(HERO_TEST_BUILD "[^"]*" OFF\)')
        self.assertIn('message(1, 0, "HERO VR " SPIDY_PUBLIC_VERSION " - Made by Agent Mulder: starting OpenXR");', src('src/game_xr.cpp'))
        self.assertNotIn('"SM2 VR " SPIDY_PUBLIC_VERSION', src('src/game_xr.cpp'))
        self.assertIn('f.text = "HERO VR " SPIDY_PUBLIC_VERSION " - MADE BY AGENT MULDER";', src('src/wrist_hud.cpp'))
        for fn in ('include/spidy/combat38.hpp', 'src/holo_roster.cpp', 'src/roster_page.cpp'):
            for line in src(fn).splitlines():
                code = line.split('//')[0]
                self.assertIsNone(re.search(r'"[^"]*\b(?:TEST|test) [0-9][^"]*"', code), (fn, line))
                self.assertNotIn('this test build', code, fn)
        self.assertTrue(os.path.exists(os.path.join(ROOT, 'RELEASE-NOTES-v0.0.73.md')))


if __name__ == '__main__':
    unittest.main()
