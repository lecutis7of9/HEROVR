# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 28.1: EYE LOD's record (tools/eye_lod.py) and tools/collect-reports.ps1."""
import pathlib
import re
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import eye_lod  # noqa: E402


def raw(state=0, reason=0, frames=0, writes=0, held=0, settling=0, values=(0, 0, 0, 0, 0), eye=0, seq=2):
    return (struct.pack('<4I', 0x444f4c45, 1, 88, 0x1b24) + struct.pack('<q', seq) +
            struct.pack('<2I', state, reason) + struct.pack('<4Q', frames, writes, held, settling) +
            struct.pack('<5f', *values) + struct.pack('<I', eye))


class EyeLodRecord(unittest.TestCase):
    def test_layout_matches_native(self):
        header = (ROOT / 'include/spidy/native_view.hpp').read_text()
        self.assertIn('static_assert(sizeof(EyeLodData) == 88);', header)
        self.assertIn('0x444f4c45', header)
        self.assertEqual(len(raw()), eye_lod.SIZE)

    def test_reasons_match_native(self):
        header = (ROOT / 'include/spidy/native_view.hpp').read_text()
        native = re.findall(r'case EyeLodReason::\w+:\s*return "([^"]+)";', header)
        self.assertEqual(native, [eye_lod.REASONS[i] for i in range(1, len(eye_lod.REASONS))])

    def test_failed_has_status_reason_and_one_line(self):
        d = eye_lod.parse(raw(state=2, reason=3, frames=130, settling=130, values=(0, 0, 0, 1.06, .629), eye=1))
        self.assertEqual(d['status'], 'failed')
        self.assertEqual(d['reason'], "the eye view's value was out of range")
        self.assertEqual(d['failed_eye'], 1)
        f = eye_lod.report_fields(d)
        self.assertEqual((f['eye_lod_status'], f['eye_lod_reason']), ('failed', d['reason']))
        line = eye_lod.console_line(d)
        self.assertIn('EYE LOD: off for this session', line)
        self.assertIn(d['reason'], line)
        self.assertNotIn('\n', line)

    def test_on_and_writing(self):
        d = eye_lod.parse(raw(state=1, frames=40, writes=11, settling=29, values=(.4667, .4667, .742, 1.06, .629)))
        self.assertEqual((d['status'], d['reason']), ('on', None))
        self.assertIn('raised to 0.742', eye_lod.console_line(d))
        # Values moving frame to frame never print a new line: one per state and reason.
        e = eye_lod.parse(raw(state=1, frames=41, writes=12, settling=29, values=(.742, .4668, .7421, 1.06, .629)))
        self.assertEqual(eye_lod.said_key(d), eye_lod.said_key(e))

    def test_on_waiting_says_why(self):
        self.assertEqual(eye_lod.parse(raw(state=1))['reason'], 'no eye view reached EYE LOD yet')
        self.assertIn('steady', eye_lod.parse(raw(state=1, frames=5, settling=5))['reason'])
        self.assertIsNone(eye_lod.console_line(eye_lod.parse(raw(state=1, frames=5, settling=5))))

    def test_off_and_unreadable(self):
        self.assertEqual(eye_lod.report_fields(eye_lod.parse(raw()))['eye_lod_status'], 'off')
        self.assertIsNone(eye_lod.parse(raw(seq=3)))  # being written
        self.assertEqual(eye_lod.report_fields(None)['eye_lod_status'], 'unknown')
        with self.assertRaises(RuntimeError):
            eye_lod.parse(b'\0' * 88)

    def test_launcher_reads_and_reports_it(self):
        text = (ROOT / 'tools/run_game_vr.py').read_text()
        self.assertIn("'SpidyEyeLodData'", text)
        self.assertIn('eye_lod.report_fields(', text)
        self.assertIn('eye_lod.console_line(', text)
        probe = (ROOT / 'src/stereo_probe.cpp').read_text()
        self.assertIn('__declspec(dllexport) native_view::EyeLodData SpidyEyeLodData;', probe)
        self.assertNotIn('eyeLodStep(current, lens, slot.ours)', probe)  # Update 28's exact match is gone


class CollectReports(unittest.TestCase):
    def test_script(self):
        ps = (ROOT / 'tools/collect-reports.ps1').read_text()
        for needed in ("'game-vr-*.json'", "'address-finder-latest*'", "'send-me.zip'", 'Compress-Archive',
                       "Join-Path $root 'reports'"):
            self.assertIn(needed, ps)
        self.assertNotIn('USERPROFILE', ps)

    def test_no_userprofile_desktop_in_docs_or_tools(self):
        for path in list((ROOT / 'tools').glob('*.ps1')) + list(ROOT.glob('*.md')) + list((ROOT / 'docs').glob('*.md')):
            self.assertNotRegex(path.read_text(errors='ignore'), r'USERPROFILE[^\n]{0,4}\\Desktop', str(path))


class EyeLodUpdate29(unittest.TestCase):
    """Update 29: an off line after it ran, an on/off timeline, and eye_lod in every report."""

    def test_off_after_running_says_so_once(self):
        never = eye_lod.parse(raw())
        self.assertIsNone(eye_lod.console_line(never))
        off = eye_lod.parse(raw(state=0, frames=900, writes=400, held=500))
        self.assertIn('EYE LOD: off', eye_lod.console_line(off))
        on = eye_lod.parse(raw(state=1, frames=901, writes=401, values=(.6, .6089, .968, .4667, .629)))
        self.assertNotEqual(eye_lod.said_key(off), eye_lod.said_key(on))

    def test_timeline(self):
        t = []
        for s, state in ((30, 0), (31, 0), (49.04, 1), (50, 1), (78, 0), (108, 1)):
            eye_lod.timeline_step(t, s, eye_lod.parse(raw(state=state, frames=5, writes=5)))
        eye_lod.timeline_step(t, 120, None)
        self.assertEqual(t, [(30, 'off'), (49.0, 'on'), (78, 'off'), (108, 'on')])

    def test_every_report_has_eye_lod(self):
        text = (ROOT / 'tools/run_game_vr.py').read_text(encoding='utf-8')
        body = text[text.index('def session_report('):]
        body = body[:body.index('lost_device = {}')]
        self.assertIn('**eye_lod.report_fields(eye_lod_last)', body)
        self.assertIn('eye_lod_timeline=list(eye_lod_timeline)', body)
        self.assertEqual(text.count('eye_lod_timeline=list(eye_lod_timeline)'), 2)


if __name__ == '__main__':
    unittest.main()
