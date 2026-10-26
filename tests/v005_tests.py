#!/usr/bin/env python3
"""Version 0.0.5 (build Update 38.9): version, the new settings, the V005 record v2 and its log lines, docs."""
import re
import struct
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402
import update37_records as rec  # noqa: E402

COMBAT38 = (ROOT / 'include/spidy/combat38.hpp').read_text(encoding='utf-8')
GAME = (ROOT / 'src/game_xr.cpp').read_text(encoding='utf-8')
SWING = (ROOT / 'src/game_swing.cpp').read_text(encoding='utf-8')
EXTRAS = (ROOT / 'include/spidy/extras385.hpp').read_text(encoding='utf-8')
NEW_KEYS = ('hero_height', 'hero_damage', 'punch_sense', 'sfx_volume', 'supersonic', 'supersonic_speed')


def info(key):
    m = re.search(r'\{"%s",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)"' % key, COMBAT38)
    assert m, key
    return m.group(1), m.group(3)


def v005_record(values=(0, 0), more=(0, 0, 0, 0), engages=0, fly=0.0, sfx=(2, 0, 5, 4, 0, 1, .8)):
    raw = bytearray(552)
    struct.pack_into('<4I', raw, 0, 0x35303056, 2, 552, 0)
    struct.pack_into('<2I', raw, 16, *values)
    struct.pack_into('<f', raw, 24, 1.0)
    struct.pack_into('<2I', raw, 472, sfx[0], sfx[1])
    struct.pack_into('<4Q', raw, 480, *sfx[2:6])
    struct.pack_into('<f', raw, 512, sfx[6])
    struct.pack_into('<4I', raw, 520, *more)
    struct.pack_into('<If', raw, 536, engages, fly)
    return bytes(raw)


class V005(unittest.TestCase):
    def test_version(self):
        # 0.0.5 was build Update 38.9 (0.0.6's bump moved VERSION on; the changelog keeps it).
        self.assertIn('**Version 0.0.5 (build Update 38.9', (ROOT / 'docs/CHANGELOG.md').read_text(encoding='utf-8'))

    def test_new_settings_and_defaults(self):
        self.assertEqual([k for k, _ in rec.V005_OPTIONS], list(NEW_KEYS))
        d = L.UPDATE38_DEFAULTS
        self.assertEqual(L.export_value('hero_damage', d['hero_damage']), 0)  # experimental: OFF
        self.assertEqual(d['hero_damage'], 'off')
        self.assertEqual(d['hero_height'], '1.00x')
        self.assertEqual(d['punch_sense'], 'forgiving')
        self.assertEqual(d['sfx_volume'], '100%')
        self.assertEqual(d['supersonic'], 'on')
        self.assertEqual(d['supersonic_speed'], '2x')
        # the tools' labels match the hologram's, value for value
        for key, labels in rec.V005_OPTIONS:
            m = re.search(r'\{"%s",[^{]*\{([^}]*)\}' % key, COMBAT38)
            self.assertTrue(m, key)
            holo = tuple(x.strip().strip('"') for x in m.group(1).split(',') if 'nullptr' not in x and x.strip())
            self.assertEqual(holo, labels, key)

    def test_subtexts(self):
        for key in NEW_KEYS:
            _, help_text = info(key)
            self.assertLessEqual(len(help_text), 80, key)
            self.assertEqual(help_text.count('.'), 1, key)

    def test_record_v2(self):
        self.assertEqual(rec.RECORDS['v005'], ('SpidyV005Data', 0x35303056, 2, 552))
        p = rec.parse_v005(v005_record(values=(2, 0), more=(0, 1, 0, 3), engages=4, fly=79.6))
        self.assertEqual(p['values'], dict(hero_height='1.30X', hero_damage='OFF', punch_sense='FORGIVING',
                                           sfx_volume='75%', supersonic='ON', supersonic_speed='3X'))
        self.assertEqual(p['sfx']['state'], 'playing')
        self.assertEqual((p['sfx']['asked'], p['sfx']['played'], p['sfx']['muted']), (5, 4, 1))
        self.assertEqual(p['supersonic_engages'], 4)
        self.assertEqual(p['fly_speed'], 79.6)
        odd = bytearray(v005_record())
        struct.pack_into('<Q', odd, 544, 1)  # being written: skipped
        self.assertIsNone(rec.parse_v005(bytes(odd)))
        self.assertIsNone(rec.parse_v005(b'\0' * 10))

    def test_log_lines(self):
        p = rec.parse_v005(v005_record(more=(0, 0, 0, 0), engages=2, fly=40.0))
        lines = rec.console_lines({'v005': p})
        text = '\n'.join(lines)
        self.assertIn('VERSION 0.0.5 HERO HEIGHT 1.00X', text)
        self.assertIn('VERSION 0.0.5 DAMAGE PROBE (read-only): CANDIDATE', text)
        self.assertIn('VERSION 0.0.5 DAMAGE TEST OFF (CANDIDATE)', text)
        self.assertIn('VERSION 0.0.5 SFX PLAYING (volume 100%): 5 asked, 4 played, 1 muted, 0 dropped.', text)
        self.assertIn('FLY SUPERSONIC ON 2X: 2 engages, flying 40.0 m/s now.', text)

    def test_fly_fast_and_supersonic_wired(self):
        self.assertIn('speeds[4] = {0, 8, 16, 40}', EXTRAS)
        self.assertIn('supersonicTop = 120', EXTRAS)
        self.assertIn('fi.boost = live ? fly_boost::multiplierOf(c.asks) : 0.f;', SWING)
        self.assertIn('sfx::play(sfx::sonicBoom', GAME)
        self.assertIn('appendShockwave(vertices, viewer, saved->shockWay, saved->shockAge, pixelAngle);', GAME)
        self.assertIn('VERSION 0.0.5 SUPERSONIC', GAME)

    def test_sfx_and_beam_ring_wired(self):
        for name in ('repulsorFire', 'chestBeam', 'beamReady', 'sonicBoom'):
            self.assertIn('sfx::' + name, GAME, name)
        self.assertIn('appendBeamRing(', GAME)
        self.assertNotIn('"BEAM %', GAME)  # the floating countdown is gone

    def test_docs(self):
        hero = (ROOT / 'docs/hero_modes.md').read_text(encoding='utf-8')
        self.assertIn('0.0.5', hero)
        self.assertIn('0x3412ec0', hero)
        log = (ROOT / 'docs/CHANGELOG.md').read_text(encoding='utf-8')
        for word in ('SUPERSONIC', 'BEAM RING', 'SOUND EFFECTS', 'PUNCH SENSITIVITY', 'DAMAGE PROBE', 'HERO HEIGHT'):
            self.assertIn(word, log.split('**Version 0.0.4')[0], word)


if __name__ == '__main__':
    unittest.main()
