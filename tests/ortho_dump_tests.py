# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 32: ORTHO CAPTURE's readable dump (tools/ortho_dump.py) and its native rows."""
import json
import math
import pathlib
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import ortho_dump as od  # noqa: E402
import address_finder as af  # noqa: E402


def box(half, light_pitch_deg=-60.0, centre=(10.0, 0.0, 20.0), row_form=True):
    """An ortho light box: world -> clip, axes scaled by 1/half, centred on centre."""
    p = math.radians(light_pitch_deg)
    z = [0.0, math.sin(p), math.cos(p)]           # depth axis (toward the light or away)
    x = [1.0, 0.0, 0.0]
    y = [0.0, math.cos(p), -math.sin(p)]
    ax = [[v / half[0] for v in x], [v / half[1] for v in y], [v / half[2] for v in z]]
    t = [-sum(a[k] * centre[k] for k in range(3)) for a in ax]
    if row_form:  # columns are the axes, translation in row 3
        m = [ax[0][0], ax[1][0], ax[2][0], 0, ax[0][1], ax[1][1], ax[2][1], 0,
             ax[0][2], ax[1][2], ax[2][2], 0, t[0], t[1], t[2], 1]
    else:
        m = [*ax[0], t[0], *ax[1], t[1], *ax[2], t[2], 0, 0, 0, 1]
    return m


def capture(group, half, head_pitch, ahead, window_extra=(), row_form=True):
    hp = math.radians(head_pitch)
    head = [0.0, 1.7, 0.0, 0.0, math.sin(hp), math.cos(hp)]
    m = box(half, centre=(0.0, 0.0, ahead), row_form=row_form)
    window = [0.0] * 16 + list(m) + list(window_extra) + [0.0] * (64 - len(window_extra))
    return dict(type='ortho_capture', group=group, ms=group * 2500, frame=group * 600, gpu_va=0x12345678,
                copy_bytes=8192, matrix_offset=2624, window_offset=2560,
                row_form=row_form, chain=[0x2ccc294, 0x2cc0000], half_m=list(half), matrix=m, window=window,
                head=head, flat=[0.0, 2.5, -4.0, 0.0, 0.0, 1.0])


class Dump(unittest.TestCase):
    def test_box_size_light_centre(self):
        for rf in (True, False):
            r = od.analyse(capture(1, (32, 32, 512), 0, 30, row_form=rf))
            self.assertEqual([round(v, 2) for v in r['size']], [64, 64, 1024])
            self.assertAlmostEqual(abs(r['light'][1]), math.sin(math.radians(60)), places=4)
            self.assertAlmostEqual(r['centre'][2], 30, places=3)
            self.assertAlmostEqual(r['head_ahead'], 30, places=3)

    def test_split_scan(self):
        found = od.split_scan([0, 0, 1, 2, 3] + [9] * 16 + [8, 32, 64.26, 146.8, 0, 5, 6], 5, sizes=[64, 128.5])
        self.assertEqual(len(found), 1)
        self.assertEqual(found[0]['values'], [8, 32, 64.26, 146.8])
        self.assertEqual(found[0]['matches_box'], [32, 64.26])  # 32 = 64 / 2, 64.26 ~ 64 within 2%
        self.assertEqual(od.split_scan([1, 2, 3], None), [])     # ascending but not 4x

    def test_dump_groups_suns_pitch_chains(self):
        lines = [dict(type='ortho_capture_summary', captures=6, cap=144, groups=3, every_ms=2500)]
        for g, pitch in ((1, -40), (2, 0), (3, 30)):
            lines.append(capture(g, (32, 32, 512), pitch, 20 + pitch * .5, window_extra=[8, 32, 64, 147]))
            lines.append(capture(g, (64.26, 64.26, 342.7), pitch, 40))
        text = od.dump_text(lines)
        self.assertIn('=== group 2', text)
        self.assertIn('size 64.00 x 64.00 x 1024.00 m', text)
        self.assertIn('shared sun direction', text)
        self.assertIn('page 0x12345000 +0x678', text)
        self.assertIn('ascending floats at window+128 (copy+2688): 8, 32, 64, 147 (matches box sizes: 32, 64)', text)
        self.assertIn('64x64x1.02e+03: 3 captures, head pitch -40..30 deg', text)
        self.assertIn('correlation head 1.00', text)
        self.assertIn('6x 0x2ccc294 <- 0x2cc0000', text)
        self.assertIn('ORTHO CAPTURE (Update 32', '\n'.join(od.summary_lines(lines)))

    def test_empty_says_so(self):
        self.assertIn('No ortho matrices were captured', od.dump_text([]))
        self.assertEqual(od.summary_lines([]), [])

    def test_finder_writes_the_dump(self):
        with tempfile.TemporaryDirectory() as d:
            text = json.dumps(capture(1, (32, 32, 512), 0, 30))
            af.write_files(d, {'status_name': 'done'}, text + '\n')
            dump = (pathlib.Path(d) / od.LATEST_NAME).read_text(encoding='utf-8')
            self.assertIn('=== group 1', dump)
            self.assertIn('ORTHO CAPTURE', (pathlib.Path(d) / 'address-finder-latest.txt').read_text(encoding='utf-8'))
        self.assertEqual(af.REPORT_BYTES, 1024 * 1024)

    def test_native_row_keys(self):
        s = (ROOT / 'src/native_address_finder.cpp').read_text(encoding='utf-8')
        for key in ('ortho_capture', 'group', 'ms', 'frame', 'gpu_va', 'copy_bytes', 'matrix_offset',
                    'window_offset', 'row_form', 'chain', 'half_m', 'ortho_capture_summary', 'every_ms'):
            self.assertIn(f'\\"{key}\\"', s)
        for key in ('"matrix"', '"window"', '"head"', '"flat"'):
            self.assertIn(f'floatsOut({key}', s)
        h = (ROOT / 'include/spidy/native_address_finder.hpp').read_text(encoding='utf-8')
        self.assertIn('reportBytes = 1024 * 1024', h)

    def test_collect_reports_takes_the_dump(self):
        ps = (ROOT / 'tools/collect-reports.ps1').read_text(encoding='utf-8')
        self.assertIn("-Filter 'address-finder-latest*'", ps)


if __name__ == '__main__':
    unittest.main()
