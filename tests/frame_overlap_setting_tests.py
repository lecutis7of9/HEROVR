# FRAME OVERLAP test setting: off by default, live, and the launcher switch writes live.txt.
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402


class FrameOverlapSetting(unittest.TestCase):
    def test_off_by_default_and_live(self):
        self.assertEqual(L.SETTINGS['frame_overlap'][:2], ((0, 1), 'SpidyFrameOverlap'))
        self.assertIn('0 off, the default', L.SETTINGS['frame_overlap'][2])
        self.assertNotIn('frame_overlap', L.RESTART)
        self.assertIn('frame_overlap', L.START_ONLY)
        values, errors = L.parse('frame_overlap = on\n')
        self.assertEqual(errors, [])
        self.assertEqual(L.export_value('frame_overlap', values['frame_overlap']), 1)
        values, errors = L.parse('frame_overlap = off\n')
        self.assertEqual(errors, [])
        self.assertEqual(L.export_value('frame_overlap', values['frame_overlap']), 0)
        values, errors = L.parse('')
        self.assertNotIn('frame_overlap', values)

    def test_launcher_switch_and_native_export(self):
        opts = (ROOT / 'tools/launcher/src/LaunchOptions.cs').read_text(encoding='utf-8')
        self.assertIn('Param = "frame_overlap"', opts)
        self.assertIn('FRAME OVERLAP (TEST)', opts)
        self.assertIn('Default = "0"', opts)
        native = (ROOT / 'src/game_xr.cpp').read_text(encoding='utf-8')
        self.assertIn('DWORD WINAPI SpidyFrameOverlap', native)
        self.assertIn('frameOverlapWant{0}', native)
        renderer = (ROOT / 'src/d3d12_renderer.cpp').read_text(encoding='utf-8')
        self.assertIn('beginOverlap', renderer)
        self.assertIn('endOverlap', renderer)
        self.assertIn('frame_overlap::slots', renderer)


if __name__ == '__main__':
    unittest.main()
