# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 AO and ONE PAGE (BUILD-SM2.md section 78).

live.txt:
  vr_ao = on | off (default)   RESTART: the game's Ambient Occlusion at XeGTAO (3) while VR runs if the player's is
                               Off (0), put back when the game closes; off: the player's as set.
The wrist hologram's VR SETTINGS and GRAPHICS pages are one page (VR SETTINGS); the rows a player does not use are off
the hologram, each still set by live.txt. Neither touches the TAA core, the lens jitter, the headset eye path or
sharpening.
"""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests'))
import live_settings as L  # noqa: E402
import session_comfort as comfort  # noqa: E402
from hero076_shadow_texture_tests import run_game_vr  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class AmbientOcclusion(unittest.TestCase):
    def test_switch(self):
        self.assertEqual(L.SETTINGS['vr_ao'][:2], ((0, 1), None))
        sub = L.SETTINGS['vr_ao'][2]
        self.assertIn('XeGTAO', sub)
        self.assertIn('RESTART NOTICE', sub)
        self.assertIn('vr_ao', L.RESTART)
        self.assertIn('vr_ao', L.START_ONLY)
        self.assertFalse(L.vr_ao(''))  # 0.0.76 HARLEYS fix: off by default (XeGTAO shimmered in the eyes)
        self.assertFalse(L.vr_ao('vr_ao = maybe\n'))
        self.assertTrue(L.vr_ao('vr_ao = 1\n'))
        self.assertTrue(L.vr_ao('vr_ao = on\n'))
        self.assertFalse(L.vr_ao('vr_ao = off\n'))
        self.assertFalse(L.vr_ao('vr_ao = 0\n'))

    def test_plan(self):
        r = run_game_vr()
        names, lines, _ = r.vr_picture_plan(True, False, False, {'AmbientOcclusionQuality': 0}, ao=True)
        self.assertIn('ambient_occlusion', names)
        self.assertTrue(any('XeGTAO' in line and 'vr_ao = off' in line for line in lines))
        for own in ({'AmbientOcclusionQuality': 3}, {'AmbientOcclusionQuality': 1}):  # the player's own kept
            names, _, _ = r.vr_picture_plan(True, False, False, own, ao=True)
            self.assertNotIn('ambient_occlusion', names)
        names, lines, _ = r.vr_picture_plan(True, False, False, {}, ao=True)
        self.assertNotIn('ambient_occlusion', names)
        self.assertTrue(any('not read' in line for line in lines))
        names, lines, _ = r.vr_picture_plan(True, False, False, {'AmbientOcclusionQuality': 0}, ao=False)
        self.assertNotIn('ambient_occlusion', names)
        self.assertIn('AmbientOcclusionQuality', r.PICTURE_NAMES)
        self.assertIn('Ambient Occlusion Off', ' '.join(r.UNCONFIRMED_COMFORT))

    def test_session_value(self):
        d = src('tools/vr_display.py')
        self.assertIn('ambient_occlusion=dict(AmbientOcclusionQuality=3)', d)
        g = src('tools/run_game_vr.py')
        self.assertIn('ao=live_settings.vr_ao(', g)

    def test_advice(self):
        entry = [e for e in comfort.COMFORT_SETTINGS if e[0] == 'Ambient Occlusion'][0]
        self.assertTrue(entry[1]('Off'))
        self.assertFalse(entry[1]('SSAO'))
        self.assertIn('XeGTAO', entry[2])


class OnePage(unittest.TestCase):
    OFF = ('C::aa', 'C::sharpen', 'C::history', 'C::eyeAa', 'C::eyeTrail', 'C::engineTaa', 'C::taaJitter',
           'C::eyeTest', 'C::addressFinder', 'C::webBallTest', 'C::foveaDebug', 'C::foveaStats', 'C::eyeLod',
           'C::sharedLod', 'C::combatLensFlares', 'C::worldLog', 'C::slotTest')

    def vr_sections(self):
        w = src('src/wrist_hud.cpp')
        return w[w.index('constexpr SectionDef vrSections[]'):w.index('constexpr SectionDef graphicsSections[]')]

    def test_merged(self):
        v = self.vr_sections()
        for name in ('"BODY AND VIEW"', '"WEBS"', '"HOLOGRAM"', '"MIRROR"', '"SOUND"', '"GRAPHICS"', '"PERFORMANCE"'):
            self.assertIn(name, v)
        self.assertNotIn('"ADVANCED', v)
        rows = v[v.index('"GRAPHICS", C::farShadows'):]
        for c in ('C::farShadows', 'C::reach', 'C::combatFarWindows', 'C::combatCullMargin', 'C::combatShadowMatch',
                  'C::combatLightHold', 'C::combatLightChecks', 'C::eyeBloom', 'C::monitor', 'C::combatAutoPerf',
                  'C::combatPerfTarget', 'C::combatPerfFloor', 'C::combatPerfStatus', 'C::foveated', 'C::foveaInner'):
            self.assertIn(c, rows)
        code = re.sub(r'//[^\n]*', '', v)
        for c in self.OFF:
            self.assertNotIn(c + ',', code)
            self.assertNotIn(c + '}', code)

    def test_graphics_page_passed(self):
        w = src('src/wrist_hud.cpp')
        self.assertIn('case graphicsPage: // HERO VR 0.0.76 ONE PAGE: merged into VR SETTINGS\n        return {nullptr, 0};', w)
        self.assertIn('return p == graphicsPage || (tiles && p == colorPage);', w)
        self.assertIn('sections(0, sectionCount(vrPage));', w)
        self.assertNotIn('C::supersample,', w[w.index('constexpr Control alwaysShown[]'):w.index('static_assert(std::size(alwaysShown)')])
        h = src('include/spidy/wrist_hud.hpp')
        self.assertIn('page_ = page == graphicsPage ? vrPage : page >= 0 && page < pages ? page : 0;', h)

    def test_off_rows_still_set_by_live_txt(self):
        for key in ('aa', 'sharpen', 'history', 'vr_ss', 'eye_temporal', 'trail', 'taa_jitter', 'eye_debug',
                    'address_finder', 'fovea_debug', 'eye_lod', 'shared_lod', 'lens_flares', 'world_log'):
            self.assertIn(key, L.SETTINGS, key)


if __name__ == '__main__':
    unittest.main()
