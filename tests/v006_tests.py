#!/usr/bin/env python3
"""Version 0.0.6 (build Update 39.0): version, the credit, the hero settings, the V006 record and its log lines."""
import re
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))

GAME = (ROOT / 'src/game_xr.cpp').read_text(encoding='utf-8')


def tracked():
    out = subprocess.run(['git', 'ls-files'], cwd=ROOT, capture_output=True, text=True).stdout.split('\n')
    return [ROOT / f for f in out if f]


class Version(unittest.TestCase):
    def test_version(self):
        self.assertEqual((ROOT / 'VERSION').read_text(encoding='utf-8').strip(), '0.0.70')  # 0.0.70 = "0.0.7" (build 39.4; 0.0.63 was 39.3, 0.0.62 was 39.2)
        self.assertEqual((ROOT / 'PORT-VERSION').read_text(encoding='utf-8').strip(), 'Update 39.4')


class Credit(unittest.TestCase):
    """The on-screen text names this build's maker only; SpidyVR's author stays in LICENSE, CREDITS.md and headers."""

    def test_made_by_on_screen(self):
        version = (ROOT / 'include/spidy/version.hpp').read_text(encoding='utf-8')
        self.assertIn('madeBy = "MADE BY AGENT MULDER"', version)
        self.assertIn('spidy::version::madeBy', (ROOT / 'src/wrist_hud.cpp').read_text(encoding='utf-8'))
        self.assertIn('" - Made by Agent Mulder: starting OpenXR"', GAME)
        self.assertIn('SPIDY VR SETTINGS - MADE BY AGENT MULDER', (ROOT / 'src/game_menu.cpp').read_text(encoding='utf-8'))
        run = (ROOT / 'tools/run_game_vr.py').read_text(encoding='utf-8')
        self.assertIn("MADE_BY = 'Made by Agent Mulder'", run)
        self.assertIn("print(f'SM2 VR {version_line()} - {MADE_BY}'", run)
        for ps in ('tools/build.ps1', 'tools/launch-game-vr.ps1', 'tools/collect-reports.ps1'):
            self.assertIn('Made by Agent Mulder', (ROOT / ps).read_text(encoding='utf-8'), ps)
        # The in-repo C++ launcher (apps/launcher) as in the HERO VR release: MADE BY AGENT MULDER on screen.
        self.assertIn('kAuthor = "Agent Mulder"', (ROOT / 'apps/launcher/system.hpp').read_text(encoding='utf-8'))
        ui = (ROOT / 'apps/launcher/ui.cpp').read_text(encoding='utf-8')
        self.assertIn('MADE BY " + upper(kAuthor)', ui)
        self.assertNotIn('by Ilya Mezerowsky) puts you', ui)

    def test_spidyvr_credit_kept(self):
        credits = (ROOT / 'CREDITS.md').read_text(encoding='utf-8')
        self.assertIn('Ilya Mezerowsky', credits)
        self.assertIn('Agent Mulder', credits)
        self.assertIn('GNU GENERAL PUBLIC LICENSE', (ROOT / 'LICENSE').read_text(encoding='utf-8'))
        self.assertIn('CREDITS.md', (ROOT / 'tools/package.ps1').read_text(encoding='utf-8'))

    def test_no_old_name_anywhere(self):
        bad = re.compile(rb'jim\s*jam', re.I)
        hits = [str(p.relative_to(ROOT)) for p in tracked() if p.is_file() and bad.search(p.read_bytes())]
        self.assertEqual(hits, [])



class Heroes(unittest.TestCase):
    """DAREDEVIL and HULK on the CONFIRMED damage path; no webs for any hero but SPIDER-MAN."""

    def test_daredevil_batons(self):
        self.assertIn('hero_modes::batonAim(hand, aimDir, batonList, foes.count)', GAME)
        self.assertIn('sm2_damage::queue(who, sm2_damage::batonHit())', GAME)
        self.assertIn('hero_modes::batonSounds(batonThrowsSeen[i], b, hit);', GAME)
        self.assertIn('appendBaton(vertices, saved->batonGrip[i] + travel', GAME)

    def test_punch_blows(self):
        self.assertIn('combat38::daredevilPunchK(c38(combat38::daredevilPunch))', GAME)
        self.assertIn('blow = {hero_modes::hulkPunchHp(most), 5, hero_modes::hulkFlingKnock};', GAME)
        self.assertIn('hero_modes::punchVictim(cf.hands[i].position, punchFeet, pn)', GAME)

    def test_hulk_leap_and_sonar(self):
        self.assertIn('input.asks |= hero_modes::leapAsks(heroNow());', GAME)
        self.assertIn('(c.asks & 16u) ? 2.f : 1.f', (ROOT / 'include/spidy/game_swing.hpp').read_text(encoding='utf-8'))
        self.assertIn('heroNow() == hero_modes::daredevil', GAME)
        gpu = (ROOT / 'src/native_eye_gpu.cpp').read_text(encoding='utf-8')
        self.assertIn('sonarRings', gpu)

    def test_no_webs_but_spiderman(self):
        self.assertIn('for (unsigned i = 0; i < 2 && heroWas == hero_modes::spiderMan; ++i)', GAME)
        self.assertIn('"nothing (no webs for this hero)"', GAME)



class Record(unittest.TestCase):
    def test_v006_record(self):
        import struct
        import update37_records as rec
        self.assertEqual(rec.RECORDS['v006'], ('SpidyV006Data', 0x36303056, 1, 128))
        raw = bytearray(128)
        struct.pack_into('<4I', raw, 0, 0x36303056, 1, 128, 1)
        struct.pack_into('<4I', raw, 16, 1, 0, 0, 2)
        struct.pack_into('<5Q', raw, 32, 6, 4, 6, 1, 4)
        struct.pack_into('<4Q', raw, 72, 3, 0, 2, 9)
        struct.pack_into('<I', raw, 104, 1)
        struct.pack_into('<2f', raw, 112, 63.5, 64.0)
        p = rec.parse_v006(bytes(raw))
        self.assertEqual(p['hero'], 'DAREDEVIL')
        self.assertEqual(p['values'], {'daredevil': 'ON', 'hulk': 'OFF', 'daredevil_punch': '3X'})
        self.assertEqual(p['batons'], dict(throws=6, hits=4, catches=6, refused=1, blows_sent=4))
        self.assertEqual((p['daredevil_punch_blows'], p['punches_out_of_reach'], p['iron_man_blows']), (3, 2, 9))
        self.assertEqual(p['sonar'], 'on')
        self.assertEqual((p['supersonic_reached'], p['supersonic_best']), (63.5, 64.0))
        raw[120] = 1
        self.assertIsNone(rec.parse_v006(bytes(raw)))
        self.assertIn('SpidyV006Data', GAME)
        self.assertIn('VERSION 0.0.6 SUPERSONIC: reached %.1f m/s', GAME)


class MirrorExport(unittest.TestCase):
    def test_mirror_view_test_exported(self):
        # 0.0.6: the v003 record's MIRROR VIEW TEST fields (offsets 784-855) reach the report.
        import struct
        import update37_records as rec
        raw = bytearray(5200)
        struct.pack_into('<3I', raw, 0, 0x33303056, 1, 5200)
        struct.pack_into('<4I', raw, 784, 3, 0, 2, 1)
        struct.pack_into('<3f', raw, 800, 30.0, 11.1, 12.4)
        struct.pack_into('<Q', raw, 816, 2600)
        struct.pack_into('<4I', raw, 824, 320, 480, 7, 5)
        struct.pack_into('<2Q', raw, 840, 41, 900)
        mt = rec.parse_v003(bytes(raw))['mirror_view_test']
        self.assertEqual((mt['phase'], mt['why'], mt['runs'], mt['frames']), ('done', '', 1, 2600))
        self.assertEqual((mt['width'], mt['height'], mt['hero_draws'], mt['other_draws']), (320, 480, 41, 900))
        text = '\n'.join(rec.console_lines({'v003': rec.parse_v003(bytes(raw))}))
        self.assertIn('VERSION 0.0.6 MIRROR VIEW TEST: phase done', text)
        self.assertIn('hero draws 41', text)
        struct.pack_into('<4I', raw, 784, 4, 5, 2, 1)
        mt = rec.parse_v003(bytes(raw))['mirror_view_test']
        self.assertEqual((mt['phase'], mt['why']), ('failed', 'too slow: frames over 1.5x'))


class PrebuiltBridge(unittest.TestCase):
    """0.0.6 keeps 0.0.5 hotfix 2: an older launcher runs tools\\build.ps1 -Observer after every install, and that
    installs the release's ready-made mod files (SHA-256 checked), never a compile on a player's PC."""

    def test_build_ps1_routes_launcher_runs_to_prebuilt(self):
        b = (ROOT / 'tools/build.ps1').read_text(encoding='utf-8')
        self.assertIn('[switch]$FromSource', b.splitlines()[0])
        gate = b.index("if ($env:SM2VR_LAUNCHER -eq '1' -and -not $FromSource -and -not $CoreOnly)")
        self.assertLess(gate, b.index('$vswhere ='))
        self.assertIn("install-prebuilt.ps1", b[gate:gate + 300])

    def test_install_prebuilt_checks_sha(self):
        p = (ROOT / 'tools/install-prebuilt.ps1').read_text(encoding='utf-8')
        self.assertIn("releases/download/$tag/version.json", p)
        self.assertIn("kind -eq 'prebuilt'", p)
        self.assertIn('Get-FileHash', p)
        self.assertIn('checksum mismatch', p)


if __name__ == '__main__':
    unittest.main()
