# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 38.1 (DIAGNOSIS run38): the eight new COMBAT settings are exported with their own codes
(5312-5319), read, live and start at the fix; HOLO OPEN has no hands-apart guard, its zone swallows the
pointing hand's trigger and grip and never fires a ball, F8 is kept; the fight aims along AIM AXIS; BLOOD
stays OFF; every 38 graphics default is unchanged; the COMBAT record is version 3. Any failing fails."""
import pathlib
import re
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as ls  # noqa: E402
import update37_records as rec  # noqa: E402

XR = (ROOT / 'src/game_xr.cpp').read_text(encoding='utf-8', errors='replace')
C38 = (ROOT / 'include/spidy/combat38.hpp').read_text(encoding='utf-8')
VERSION = (ROOT / 'PORT-VERSION').read_text(encoding='utf-8')
NEW = dict(aim_axis=('SpidyAimAxis', 5312, 'hand_-y'), ring_layer=('SpidyRingLayer', 5313, 'hold'),
           lunge_from=('SpidyLungeFrom', 5314, '2.5_m'), lunge_speed=('SpidyLungeSpeed', 5315, '2.2'),
           lunge_aim=('SpidyLungeAim', 5316, 'steady'), punch_angle=('SpidyPunchAngle', 5317, 'wide_.2'),
           ball_lead=('SpidyBallLead', 5318, 'on_target'), blood_look=('SpidyBloodLook', 5319, 'brutal'))
# Update 38.2: ring_layer HOLD and blood_look BRUTAL are first now (update382_tests).


class Settings(unittest.TestCase):
    def test_version(self):
        self.assertRegex(VERSION, r'^Update (38\.[1-9]|39\.\d)')

    def test_exports_codes_defaults(self):
        for key, (export, code, default) in NEW.items():
            m = re.search(rf'DWORD WINAPI {export}\(void\* v\) \{{\s*return setLive\(v, 0, [^,]+, (\d+),', XR)
            self.assertIsNotNone(m, export)
            self.assertEqual(int(m.group(1)), code, export)
            self.assertEqual(ls.SETTINGS[key][1], export)
            self.assertEqual(ls.UPDATE38_DEFAULTS[key], default, key)
            self.assertEqual(ls.export_value(key, default), 0, key)
        codes = [int(c) for c in re.findall(r'combat38::\w+\]\.count - 1, (\d+),', XR)]
        self.assertEqual(len(codes), len(set(codes)), 'two COMBAT settings share a refusal code')

    def test_holo_open_default_tap(self):
        self.assertEqual(ls.UPDATE38_DEFAULTS['holo_open'], 'forearm')  # Update 38.2: the forearm tap
        self.assertEqual(ls.export_value('holo_open', 'tap'), 0)
        self.assertEqual(ls.export_value('holo_open', 'safe'), 0)  # a 38 live.txt keeps working

    def test_no_gold_switch(self):
        """GOLD WEB EFFECT was left out (no confirmed runtime-only method): nothing half-built ships."""
        self.assertNotIn('gold', ' '.join(ls.SETTINGS).lower())
        self.assertNotIn('0x2c98ed0', XR.lower())


class Holo(unittest.TestCase):
    def test_no_hands_apart_guard(self):
        self.assertNotIn('wristToggle.guard(', XR)
        self.assertIn('wristToggle.setMode(combat38::toggleMode(c38(combat38::holoOpen)));', XR)
        self.assertIn('wristToggle.look(frame.head', XR)

    def test_zone_swallows(self):
        self.assertIn('hudZone = wristToggle.zoneHands();', XR)
        self.assertIn('controller.hands[i].squeeze = 0;', XR)
        self.assertIn('u37.ball[i] = u37.other[i] = 0;', XR)

    def test_f8_kept(self):
        self.assertIn('GetAsyncKeyState(VK_F8)', XR)
        self.assertIn('wristToggle.flip()', XR)


class Fight(unittest.TestCase):
    def test_aim_axis(self):
        self.assertIn('h.aim = combat38::aimOf(grip, frame.hands[i].aim, c38(combat38::aimAxis));', XR)
        self.assertNotIn('h.aim = grip.orientation.rotate({0, 0, -1});', XR)

    def test_blood_off_by_default(self):
        self.assertIn('std::atomic<uint32_t> bloodIntensity{0};', XR)
        self.assertEqual(ls.UPDATE38_DEFAULTS['blood_look'], 'brutal')


class Graphics(unittest.TestCase):
    def test_38_defaults_unchanged(self):
        self.assertEqual(ls.UPDATE37_DEFAULTS, {
            'history': 80, 'sharpen': 140, 'trail': 0, 'taa_jitter': 3, 'hud_theme': 'raimi', 'web_width': 1.8,
            'web_speed': 70, 'reticles': 0, 'aa': 1, 'eye_temporal': 1, 'far_shadows': 2, 'eye_bloom': 0,  # 0.0.73 lights: EYE BLOOM OFF
            'shared_lod': 2, 'bloom_fx': 0, 'aim_steer': 0, 'ring_face': 0, 'wall_crawl': 0, 'web_bend': 100,
            'wall_body': 0, 'zip_strike': 1, 'web_yank': 1, 'npc_fade': 'off', 'color_fx': 0, 'foveated': 'off',
            'lod_fade': 0})  # 0.0.74 test 10 PICTURE 72: aa 1, sharpen 140 (0.0.72's). test build 12 WINDOW LIGHTS: LOD FADE off by default
        self.assertEqual(ls.STARTUP_ON, {'eye_temporal': 1, 'far_shadows': 2, 'sharpen': 140})  # 0.0.74 test 10 PICTURE 72: sharpen 140 again


class Record(unittest.TestCase):
    def test_v3_parses(self):
        _, magic, version, size = rec.RECORDS['combat38']
        self.assertEqual((version, size), (5, 768))  # Update 38.3: version 5, version 3's fields in place
        raw = bytearray(size)
        struct.pack_into('<4I', raw, 0, magic, version, size, 0)
        struct.pack_into('<I', raw, 16 + 4 * 11, 2)  # option 11 (aim_axis) in values[11]
        struct.pack_into('<8I', raw, 416, 2, 0, 0, 1, 0, 1, 1, 0)  # options 12-18 in values2
        struct.pack_into('<6Q', raw, 448, 7, 3, 2, 1, 40, 5)
        struct.pack_into('<I', raw, 496, 4)
        d = rec.parse_combat38(bytes(raw))
        self.assertEqual(d['options']['aim_axis'], 'OLD -Z')
        self.assertEqual(d['options']['ring_layer'], 'IMAGE')
        self.assertEqual(d['options']['lunge_aim'], '38')
        self.assertEqual(d['options']['ball_lead'], 'ALWAYS')
        self.assertEqual(d['options']['blood_look'], 'BIG')
        self.assertEqual(d['options']['melee_anim'], 'EVERY HIT')
        self.assertEqual((d['swing_rejects'], d['holo_zone_swallowed'], d['holo_taps'], d['holo_look_opens'],
                          d['ring_hidden_frames'], d['lunge_held_off'], d['lock_held']), (7, 3, 2, 1, 40, 5, 4))
        lines = rec.console_lines({'combat38': d})
        self.assertTrue(any(l.startswith('Update 38.1: 7 swings not sent') for l in lines), lines)
        self.assertIn('offsetof(Data, swingRejects) == 448', C38)


if __name__ == '__main__':
    unittest.main()
