"""79.8 CLASSIC PICTURE: the public v0.0.78 render defaults, the gameplay changes kept."""
import pathlib
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402


def src(path):
    return (ROOT / path).read_text(encoding='utf-8', errors='replace')


class Classic(unittest.TestCase):
    def test_defaults_are_0_0_78(self):
        self.assertEqual(L.vr_aa_choice(''), 'ours')
        self.assertEqual(L.vr_aa_choice(None), 'ours')
        self.assertEqual(L.vr_aa_choice('vr_aa = game\n'), 'game')  # a deliberate choice still works
        self.assertIn(L.MIGRATED_798_RENDER, L.HEADER)

    def test_launcher_writes_ours_for_both_modes(self):
        r = src('tools/launcher/src/RendererModes.cs')
        self.assertIn('string aa = "ours";', r)
        self.assertNotIn('"ours" : "game"', r)
        m = src('tools/launcher/src/MainForm.cs')
        self.assertNotIn('RendererModes.Key, RendererModes.Revamped)', m)
        self.assertIn('renderer_798_classic', m)
        self.assertIn('migrate_798_render', src('tools/run_game_vr.py'))

    def test_migration(self):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text('hud = 1\nvr_aa = game\nvr_jitter = off\nvr_render = native\n', encoding='utf-8')
            said = L.migrate_798_render(p)
            self.assertIn('vr_aa game set once to ours', said)
            self.assertIn('vr_jitter off set once to med', said)
            v = L.parse(p.read_text())[0]
            self.assertEqual((v['vr_aa'], v['vr_jitter']), ('ours', 'med'))
            # a choice made after the marker stays
            p.write_text(p.read_text().replace('\nvr_aa = ours', '\nvr_aa = game'), encoding='utf-8')
            self.assertIsNone(L.migrate_798_render(p))
            self.assertEqual(L.parse(p.read_text())[0]['vr_aa'], 'game')
            self.assertIsNone(L.migrate_798_render(pathlib.Path(d) / 'none.txt'))
            q = pathlib.Path(d) / 'b.txt'  # smaa / keep / low are the player's: kept, marker added
            q.write_text('vr_aa = smaa\nvr_jitter = low\n', encoding='utf-8')
            self.assertIsNone(L.migrate_798_render(q))
            v = L.parse(q.read_text())[0]
            self.assertEqual((v['vr_aa'], v['vr_jitter']), ('smaa', 'low'))
            self.assertIn(L.MIGRATED_798_RENDER, q.read_text())

    def test_tag(self):
        c = src('CMakeLists.txt')
        self.assertIn('set(HERO_TEST_VERSION "0.0.79.8"', c)
        self.assertIn('set(HERO_TEST_NUMBER "8"', c)
        self.assertIn('HERO VR 79.8 TEST (commit', src('.github/workflows/dynamic-body-0791.yml'))
        self.assertIn('HERO VR 79.8 TEST (includes 79.7)', src('src/game_xr.cpp'))

    def test_gameplay_kept(self):
        g = src('src/game_xr.cpp')
        self.assertIn('appendWeblineMarker', src('src/game_xr.cpp') + src('src/stereo_probe.cpp') + src('src/native_body.cpp'))
        self.assertIn('crawlNormal', g + src('src/native_body.cpp') + src('src/body_ik.cpp'))


if __name__ == '__main__':
    unittest.main()
