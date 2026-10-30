# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 38 (combat): the COMBAT settings' defaults (each the recommended fix, all live), the launcher's
and the hologram's option tables agree with combat38.hpp, every setting has its export and is read
(no switch that does nothing), the 37.2 start-up defaults are kept, the COMBAT and LUNGE records parse,
and the swing Command v6 / Data v4 layouts. Any of these failing fails the build."""
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
SWING = (ROOT / 'src/game_swing.cpp').read_text(encoding='utf-8', errors='replace')
C38 = (ROOT / 'include/spidy/combat38.hpp').read_text(encoding='utf-8')
SWING_H = (ROOT / 'include/spidy/game_swing.hpp').read_text(encoding='utf-8')
LAUNCHER = (ROOT / 'tools/run_game_vr.py').read_text(encoding='utf-8')
VERSION = (ROOT / 'PORT-VERSION').read_text(encoding='utf-8')

# The C++ option table: {"key", "TITLE", "SHORT", "subtext", count, {"A", "B", ...}}
OPTION = re.compile(r'\{"(\w+)", "([A-Z0-9 ()]+)", "([A-Z0-9]+)",\s*"([^"]+)",\s*(\d),\s*\{([^}]*)\}\}')  # 0.0.73 MPSUIT: digits (PLAYER 2 BODY, P2B)
EXPORTS = dict(crawl_cap='SpidyCrawlCap', zip_mode='SpidyZipMode', aim_assist='SpidyAimAssist',
               ball_look='SpidyBallLook', punch_mode='SpidyPunchMode', ring_anchor='SpidyRingAnchor',
               holo_open='SpidyHoloOpen', wall_tilt='SpidyWallTilt', web_rope='SpidyWebRope',
               lunge='SpidyLunge', ball_fire='SpidyBallFire',
               # Update 38.1
               aim_axis='SpidyAimAxis', ring_layer='SpidyRingLayer', lunge_from='SpidyLungeFrom',
               lunge_speed='SpidyLungeSpeed', lunge_aim='SpidyLungeAim', punch_angle='SpidyPunchAngle',
               ball_lead='SpidyBallLead', blood_look='SpidyBloodLook',
               # Update 38.3
               look_lock='SpidyLookLock', strike_steer='SpidyStrikeSteer', far_windows='SpidyFarWindowsOption',
               gadget_hint='SpidyGadgetHint',
               # Update 38.2
               melee_anim='SpidyMeleeAnim', gadget_keys='SpidyGadgetKeys', grab_web='SpidyGrabWeb',
               mirror_look='SpidyMirrorLook', gadget_feel='SpidyGadgetFeel',
               # Version 0.0.2
               aim_strength='SpidyAimStrength', ball_hit='SpidyBallHit',
               # Version 0.0.3
               rope_grab='SpidyRopeGrab', rope_strength='SpidyRopeStrength', wall_stick='SpidyWallStick',
               auto_perf='SpidyAutoPerf', perf_target='SpidyPerfTarget', perf_floor='SpidyPerfFloor',
               perf_status='SpidyPerfStatus',
               # Version 0.0.4
               iron_man='SpidyShooterType', mirror_view='SpidyMirrorView',
               # Version 0.0.5
               hero_height='SpidyHeroHeight', hero_damage='SpidyHeroDamage',
               punch_sense='SpidyPunchSense', sfx_volume='SpidySfxVolume', supersonic='SpidySupersonic',
               supersonic_speed='SpidySupersonicSpeed',
               # Version 0.0.6
               daredevil='SpidyDaredevil', hulk='SpidyHulk', daredevil_punch='SpidyDaredevilPunch',
               # Version 0.0.62
               hit_indicator='SpidyHitIndicator', hand_flips='SpidyHandFlips', lens_flares='SpidyLensFlares',
               punch_hits='SpidyPunchHits', foe_track='SpidyFoeTrack', beacons_vision='SpidyBeaconsVision',
               # Version 0.0.7
               batman='SpidyBatman', wolverine='SpidyWolverine', superman='SpidySuperman', punisher='SpidyPunisher',
               batarang_chain='SpidyBatarangChain', easy_reload='SpidyEasyReload', holster_hands='SpidyHolsterHands',
               baton_holsters='SpidyBatonHolsters', baton_look='SpidyBatonLook', batarang_style='SpidyBatarangStyle',
               gun_models='SpidyGunModels',
               # Version 0.0.7 CAPTAIN AMERICA
               captain_america='SpidyCaptainAmerica', shield_arm='SpidyShieldArm', shield_damage='SpidyShieldDamage',
               shield_chain='SpidyShieldChain', shield_block='SpidyShieldBlock', shield_look='SpidyShieldLook',
               # Version 0.0.72 (TEST)
               thor='SpidyThor', thor_lightning='SpidyThorLightning', thor_storm='SpidyThorStorm', thor_chain='SpidyThorChain', green_lantern='SpidyGreenLantern', gl_flight='SpidyGlFlight', gl_fist='SpidyGlFist', gl_beam='SpidyGlBeam', gl_bubble='SpidyGlBubble', gl_chain='SpidyGlChain', gl_charge='SpidyGlCharge', spider_climb_only='SpidySpiderClimbOnly', uppercut='SpidyUppercut', smoke_blind='SpidySmokeBlind', dd_grapple='SpidyDdGrapple', dd_parry='SpidyDdParry', iron_hud='SpidyIronHud', punisher_rage='SpidyPunisherRage', reputation='SpidyReputation', survival='SpidySurvival', punisher_suit='SpidyPunisherSuit', natural_invincible='SpidyNaturalInvincible', hulk_rage='SpidyHulkRage', rage_blood='SpidyRageBlood', rage_tint='SpidyRageTint', fire_douse='SpidyFireDouse', shield_slam='SpidyShieldSlam', punisher_hook='SpidyPunisherHook', claw_climb='SpidyClawClimb', npc_shove='SpidyNpcShove', faction='SpidyFaction', hero_suit='SpidyHeroSuit', test_unlocks='SpidyTestUnlocks', ultimates='SpidyUltimates', perk_branch='SpidyPerkBranch', own_suit='SpidyOwnSuit', perch_takedown='SpidyPerchTakedown', takedown_light='SpidyTakedownLight', punisher_van='SpidyPunisherVan', thor_hammer='SpidyThorHammer', game_body='SpidyGameBody', thor_hammer_model='SpidyThorHammerModel', thor_flight='SpidyThorFlight', suit_body='SpidySuitBody', cap_sprint='SpidyCapSprint', cull_margin='SpidyCullMargin', shadow_match='SpidyShadowMatch', calib_look='SpidyCalibLook', fly_pose='SpidyFlyPose', weapon_collision='SpidyWeaponCollision', light_hold='SpidyLightHold', van_screen='SpidyVanScreen', palm_blast='SpidyPalmBlast', hero_menu='SpidyHeroMenu', rep_log='SpidyRepLog', van_pedals='SpidyVanPedals', image='SpidyImage', image_style='SpidyImageStyleOption', dd_baton_tip='SpidyDdBatonTip', streamer_mode='SpidyStreamerMode', streamer_smooth='SpidyStreamerSmooth', streamer_eye='SpidyStreamerEye', iron_hands='SpidyIronHands', dd_grapple_aim='SpidyDdGrappleAim', wolverine_claws='SpidyWolverineClaws', light_checks='SpidyLightChecks', glow_check='SpidyGlowCheck', sl_watch='SpidySlWatch', mp_solo_test='SpidyMpSoloTest', sharpen_match='SpidySharpenMatch', claw_level='SpidyClawLevel', window_pop='SpidyWindowPop', mp_player2_body='SpidyMpPlayer2Body', coming_soon='SpidyComingSoon', hideouts='SpidyHideouts', car_music='SpidyCarMusic', seated_driving='SpidySeatedDriving', batmobile='SpidyBatmobile', batmobile_choice='SpidyBatmobileChoice', spidyvr_picture='SpidySpidyvrPicture', flare_block='SpidyFlareBlock', cabin_clear='SpidyCabinClear', car_paint='SpidyCarPaint', seated_tools='SpidySeatedTools', dash_pointer='SpidyDashPointer', taa_diagnostics='SpidyTaaDiagnostics', crime_gps='SpidyCrimeGps', taa_hold='SpidyTaaHold', heap_guard='SpidyHeapGuard', jitter_guard='SpidyJitterGuard', wheel_first='SpidyWheelFirst', cave_quiet='SpidyCaveQuiet', finger_ray='SpidyFingerRay', dash_inside='SpidyDashInside', cockpit_glow='SpidyCockpitGlow', spidyvr_stock='SpidySpidyvrStock', miles_taa='SpidyMilesTaa', taa_gate_log='SpidyTaaGateLog', eye_taa_requeue='SpidyEyeTaaRequeue', main_view_small='SpidyMainViewSmall', aa_mode='SpidyAaMode', impostor_eye='SpidyImpostorEye', clean_jitter='SpidyCleanJitterOn')
# Where each setting is read (combat38 option name in C++).
READ_AS = dict(crawl_cap='crawlCap', zip_mode='zipMode', aim_assist='aimAssist', ball_look='ballLook',
               punch_mode='punchMode', ring_anchor='ringAnchor', holo_open='holoOpen', wall_tilt='wallTilt',
               web_rope='webRope', lunge='lunge', ball_fire='ballFire',
               aim_axis='aimAxis', ring_layer='ringLayer', lunge_from='lungeFrom', lunge_speed='lungeSpeed',
               lunge_aim='lungeAim', punch_angle='punchAngle', ball_lead='ballLead', blood_look='bloodLook',
               melee_anim='meleeAnim', gadget_keys='gadgetKeys', grab_web='grabWeb', mirror_look='mirrorLook',
               gadget_feel='gadgetFeel', look_lock='lookLock', strike_steer='strikeSteer',
               far_windows='farWindows', gadget_hint='gadgetHint',  # Update 38.3
               aim_strength='aimStrength', ball_hit='ballHit',  # Version 0.0.2
               rope_grab='ropeGrab', rope_strength='ropeStrength', wall_stick='wallStick', auto_perf='autoPerf',
               perf_target='perfTarget', perf_floor='perfFloor', perf_status='perfStatus',  # Version 0.0.3
               iron_man='shooterType', mirror_view='mirrorView',  # Version 0.0.4
               hero_height='heroHeight', hero_damage='heroDamage',
               punch_sense='punchSense', sfx_volume='sfxVolume', supersonic='supersonic', supersonic_speed='supersonicSpeed',  # Version 0.0.5
               daredevil='daredevil', hulk='hulk', daredevil_punch='daredevilPunch',  # Version 0.0.6
               hit_indicator='hitIndicator', hand_flips='handFlips', lens_flares='lensFlares',
               punch_hits='punchHits', foe_track='foeTrack', beacons_vision='beaconsVision',  # Version 0.0.62
               batman='batman', wolverine='wolverine', superman='superman', punisher='punisher',
               batarang_chain='batarangChain', easy_reload='easyReload', holster_hands='holsterHands',
               baton_holsters='batonHolsters', baton_look='batonLook', batarang_style='batarangStyle', gun_models='gunModels',  # Version 0.0.7
               captain_america='captainAmerica', shield_arm='shieldArm', shield_damage='shieldDamage',
               shield_chain='shieldChain', shield_block='shieldBlock', shield_look='shieldLook',  # CAPTAIN AMERICA
               thor='thor', thor_lightning='thorLightning', thor_storm='thorStorm', thor_chain='thorChain', green_lantern='greenLantern', gl_flight='glFlight', gl_fist='glFist', gl_beam='glBeam', gl_bubble='glBubble', gl_chain='glChain', gl_charge='glCharge', spider_climb_only='spiderClimbOnly', uppercut='uppercut', smoke_blind='smokeBlind', dd_grapple='ddGrapple', dd_parry='ddParry', iron_hud='ironHud', punisher_rage='punisherRage', reputation='reputation', survival='survival', punisher_suit='punisherSuit', natural_invincible='naturalInvincible', hulk_rage='hulkRage', rage_blood='rageBlood', rage_tint='rageTint', fire_douse='fireDouse', shield_slam='shieldSlam', punisher_hook='punisherHook', claw_climb='clawClimb', npc_shove='npcShove', faction='faction', hero_suit='heroSuit', test_unlocks='testUnlocks', ultimates='ultimates', perk_branch='perkBranch', own_suit='ownSuit', perch_takedown='perchTakedown', takedown_light='takedownLight', punisher_van='punisherVan', thor_hammer='thorHammer', game_body='gameBody', thor_hammer_model='thorHammerModel', thor_flight='thorFlight', suit_body='suitBody', cap_sprint='capSprint', cull_margin='cullMargin', shadow_match='shadowMatch', calib_look='calibLook', fly_pose='flyPose', weapon_collision='weaponCollision', light_hold='lightHold', van_screen='vanScreen', palm_blast='palmBlast', hero_menu='heroMenu', rep_log='repLog', van_pedals='vanPedals', image='image', image_style='imageStyle', dd_baton_tip='ddBatonTip', streamer_mode='streamerMode', streamer_smooth='streamerSmooth', streamer_eye='streamerEye', iron_hands='ironHands', dd_grapple_aim='ddGrappleAim', wolverine_claws='wolverineClaws', light_checks='lightChecks', glow_check='glowCheck', sl_watch='slWatch', mp_solo_test='mpSoloTest', sharpen_match='sharpenMatch', claw_level='clawLevel', window_pop='windowPop', mp_player2_body='mpPlayer2Body', coming_soon='comingSoon', hideouts='hideouts', car_music='carMusic', seated_driving='seatedDriving', batmobile='batmobile', batmobile_choice='batmobileChoice', spidyvr_picture='spidyvrPicture', flare_block='flareBlock', cabin_clear='cabinClear', car_paint='carPaint', seated_tools='seatedTools', dash_pointer='dashPointer', taa_diagnostics='taaDiagnostics', crime_gps='crimeGps', taa_hold='taaHold', heap_guard='heapGuard', jitter_guard='jitterGuard', wheel_first='wheelFirst', cave_quiet='caveQuiet', finger_ray='fingerRay', dash_inside='dashInside', cockpit_glow='cockpitGlow', spidyvr_stock='spidyvrStock', miles_taa='milesTaa', taa_gate_log='taaGateLog', eye_taa_requeue='eyeTaaRequeue', main_view_small='mainViewSmall', aa_mode='aaMode', impostor_eye='impostorEye', clean_jitter='cleanJitter')  # Version 0.0.72


def cpp_options():
    out = []
    for m in OPTION.finditer(C38):
        names = tuple(re.findall(r'"([^"]+)"', m.group(6)))
        out.append((m.group(1), m.group(2), m.group(4), int(m.group(5)), names))
    return out


class Defaults(unittest.TestCase):
    def test_recommended_first(self):
        """Each default is the recommended fix: value 0 of its option.

        Version 0.0.72: the four model looks default to their MODEL value instead (the
        user's 0.0.71 test: index 0 drew the built-in stand-ins, never the bundled models)."""
        for key, value in ls.UPDATE38_DEFAULTS.items():
            if key == 'lens_flares':  # 0.0.73 lights: OFF IN VR (1) by default; index 0 stays GAME
                self.assertEqual(value, 'off_in_vr')
                self.assertEqual(ls.export_value(key, value), 1)
                continue
            if key == 'image_style':  # 0.0.73 release: GAME TAA OFF (1) by default; index 0 stays SPIDYVR
                self.assertEqual(value, 'off')
                self.assertEqual(ls.export_value(key, value), 1)
                continue
            if key in ('miles_taa', 'eye_taa_requeue', 'main_view_small'):  # 0.0.74 test 4: OFF (1) by default
                self.assertEqual(value, 'off')
                self.assertEqual(ls.export_value(key, value), 1)
                continue
            if key in ls.MODEL_LOOKS_072:
                self.assertEqual(value, ls.MODEL_LOOKS_072[key], key)
                self.assertNotEqual(ls.export_value(key, value), 0, key)
                continue
            self.assertEqual(ls.export_value(key, value), 0, key)

    def test_keys(self):
        self.assertEqual(set(ls.UPDATE38_DEFAULTS), set(ls.UPDATE38_KEYS))
        self.assertEqual(set(ls.UPDATE38_KEYS), set(EXPORTS))
        self.assertEqual(ls.UPDATE38_DEFAULTS['lunge'], 'game_button')
        self.assertEqual(ls.UPDATE38_DEFAULTS['zip_mode'], 'spidyvr')
        self.assertEqual(ls.UPDATE38_DEFAULTS['aim_assist'], 'smart')  # Update 38.2: SMART first

    def test_aim_steer_stays_off_and_372_kept(self):
        self.assertEqual(ls.UPDATE37_DEFAULTS['aim_steer'], 0)
        self.assertEqual(ls.STARTUP_ON, {'eye_temporal': 1, 'far_shadows': 2})  # 0.0.73 IMAGE: sharpen follows the look (IMAGE_LOOKS)
        self.assertNotIn('NOT ON', ls.startup_line(dict(ls.STARTUP_ON)))

    def test_sent_at_start_no_restart(self):
        self.assertIn('for name, value in live_settings.UPDATE38_DEFAULTS.items()', LAUNCHER)
        self.assertIn('Update 38 COMBAT defaults (all live, no restart)', LAUNCHER)

    def test_version(self):
        self.assertRegex(VERSION, r'Update 3[89]')  # 0.0.6 is Update 39.0.2


class Options(unittest.TestCase):
    def test_tables_agree(self):
        cpp = cpp_options()
        self.assertEqual(len(cpp), len(rec.COMBAT38_ALL_OPTIONS))  # Version 0.0.3: + V003_OPTIONS
        for (key, title, sub, count, names), (rkey, rnames) in zip(cpp, rec.COMBAT38_ALL_OPTIONS):
            self.assertEqual(key, rkey)
            self.assertEqual(names[:count], rnames, key)
            self.assertEqual(ls.SETTINGS[key][0], (0, count - 1), key)
            self.assertEqual(ls.export_value(key, rnames[-1].lower().replace(' ', '_')), count - 1, key)

    def test_one_sentence_subtext(self):
        for key, title, sub, count, names in cpp_options():
            self.assertTrue(sub.endswith('.'), key)
            self.assertEqual(sub.count('. '), 0, f'{key}: subtext is more than one sentence')
            self.assertLessEqual(len(sub), 80, key)  # 0.0.72 rule: <= 80 characters, exactly one '.'
            self.assertEqual(sub.count('.'), 1, key)
            self.assertRegex(sub, r'restart|live', key)  # says whether a restart is needed

    def test_every_switch_does_something(self):
        for key, export in EXPORTS.items():
            self.assertEqual(ls.SETTINGS[key][1], export)
            self.assertIn(f'DWORD WINAPI {export}(void* v)', XR, export)
            uses = len(re.findall(rf'c38\(combat38::{READ_AS[key]}\)', XR))
            self.assertGreaterEqual(uses, 1, f'{key} is never read')

    def test_live_not_restart(self):
        """All COMBAT settings are live; the hologram cycles them in place."""
        self.assertIn('combat38Values[o] = (c38Own(o) + 1) % combat38::optionInfo[o].count', XR)  # 0.0.74 test 3: the player's own (SPIDYVR STOCK)


class Records(unittest.TestCase):
    def test_sizes_match_cpp(self):
        self.assertIn('static_assert(sizeof(Data) == 768)', C38)  # Update 38.3: version 5
        self.assertEqual(rec.RECORDS['combat38'][2:], (5, 768))
        self.assertEqual(rec.RAY_RECORDS['lunge'][3], 64)
        self.assertIn('static_assert(sizeof(LungeData) == 64)',
                      (ROOT / 'include/spidy/combat_spidy.hpp').read_text(encoding='utf-8'))

    def test_combat38_parses(self):
        _, magic, version, size = rec.RECORDS['combat38']
        raw = bytearray(size)
        struct.pack_into('<4I', raw, 0, magic, version, size, 1)
        struct.pack_into('<12I', raw, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0)
        struct.pack_into('<8Q', raw, 352, 5, 3, 2, 1, 4, 90, 9, 1)
        struct.pack_into('<6f', raw, 320, 3.5, 7.0, .02, 10.0, .05, 3.0)
        d = rec.parse_combat38(bytes(raw))
        self.assertEqual(d['options']['crawl_cap'], 'AUTO')
        self.assertEqual(d['options']['lunge'], 'SPIDYVR MOVE')
        self.assertEqual(d['options']['ball_fire'], 'PRESS')
        self.assertEqual((d['lunges'], d['lunges_landed'], d['lunges_missed'], d['sight_rays']), (5, 3, 2, 90))
        self.assertEqual(d['scan_hz'], 10.0)
        self.assertTrue(d['airborne_combat'])
        lines = rec.console_lines({'combat38': d})
        self.assertTrue(any(l.startswith('Update 38 SpidyVR: 5 lunges (3 landed, 2 missed)') for l in lines), lines)

    def test_lunge_parses(self):
        _, magic, version, size = rec.RAY_RECORDS['lunge']
        raw = bytearray(size)
        struct.pack_into('<4I', raw, 0, magic, version, size, 1)
        struct.pack_into('<4Q', raw, 16, 2, 1, 1, 42)
        struct.pack_into('<2i', raw, 48, 0, 1)
        d = rec.parse_lunge(bytes(raw))
        self.assertEqual((d['phase'], d['opened'], d['target'], d['mode']), ('flying', 2, 42, 'SPIDYVR MOVE'))

    def test_vr_module_records_only(self):
        """The launcher requires every RECORDS export of the VR module: the swing module's lunge record
        must not be among them (a missing export would stop the launch)."""
        others = '\n'.join(p.read_text(encoding='utf-8', errors='replace') for p in sorted((ROOT / 'src').glob('*.cpp'))
                            if p.name != 'game_swing.cpp')
        for key, (export, *_rest) in rec.RECORDS.items():
            self.assertRegex(others, rf'\b{export}\b', key)
        self.assertNotIn('lunge', rec.RECORDS)
        self.assertIn('spidy::combat_spidy::LungeData SpidyLungeData', SWING)
        self.assertEqual(set(rec.PARSERS), set(rec.RECORDS))

    def test_lunge_sample_export(self):
        self.assertIn('DWORD WINAPI SpidyLungeSample(void* out)', SWING)
        self.assertIn('GetProcAddress', XR)
        self.assertIn('"SpidyLungeSample"', XR)


class SwingLayouts(unittest.TestCase):
    def test_command_v6_and_v5(self):
        self.assertIn('commandBytesV5 = 240', SWING_H)
        # Version 0.0.73: version 7 adds the van's carry after version 6's 440 bytes
        self.assertIn('sizeof(Command) == 464', SWING_H)
        self.assertIn('commandBytesV6 = 440', SWING_H)
        self.assertIn('commandBytesV5', SWING)

    def test_data_v4_unchanged(self):
        self.assertIn('sizeof(Data) == 272', SWING_H)


if __name__ == '__main__':
    unittest.main()
