# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 37: the new native records for the console and the game-vr report.

VGAT  SpidyViewGatesData   include/spidy/view_gates.hpp    BLOOM FX, LOD FADE, LOD REACH, IMPOSTORS
CMBT  SpidyCombatData      include/spidy/combat_input.hpp  gadgets by gesture, hand hits, lock-on
CRWL  SpidyHandCrawlData   include/spidy/hand_crawl.hpp    hand-over-hand climbing, the game's crawl
HLTH  SpidyHealthData      include/spidy/health_log.hpp    the ring's health (CANDIDATE 0x541edd0 +0xd0/+0xa0)
MIRR  SpidyMirrorData      include/spidy/vr_mirror.hpp     the MIRROR's cost
WTRC  SpidyWorldTraceData  include/spidy/world_trace.hpp   the WORLD LOG: NPC talk, spawns, missions (read-only)
BLDS  SpidyBloodData       include/spidy/blood_splatter.hpp BLOOD SPLATTER: splashes, plan B, cost
PRF7  SpidyPerfData        include/spidy/perf37.hpp        the performance pass: per-part CPU ms, HOLO REUSE, spikes
COLR  SpidyColorData       include/spidy/eye_color.hpp     Update 37.2 COLOR / ATMOSPHERE: the look in force
FOVE  SpidyFoveaData       include/spidy/foveation.hpp     Update 37.2 FOVEATED (experimental): per-level FPS / GPU ms
NPCF  SpidyNpcFadeData     include/spidy/npc_fade.hpp      Update 37.2 NPC NEAR FADE (CANDIDATE addresses)
CMB8  SpidyCombat38Data    include/spidy/combat38.hpp      Update 38 COMBAT: punch rejects, zips, crawl cap, ring error
MPRB  SpidyMirrorProbeData include/spidy/mirror_probe.hpp  Update 38.4 MIRROR PROBE (38.5: v2, preview/menu/first events)
WMUZ  SpidyWristMuzzleData include/spidy/wrist_muzzle.hpp  Update 38.5 BALL LOOK WRIST: web-shooter muzzle moves (CANDIDATE)
HSWP  SpidyHeroSwitchData  include/spidy/hero_switch.hpp   Update 38.5 HERO SWITCH PROBE: a real Peter / Miles switch, re-attached
AIRW  SpidyAirWatchData    include/spidy/air_watch.hpp     Update 38.5 Y IN THE AIR: Y presses, the game's moves out of the swing's flight (a glide's sign), read-only

Each is a seqlock record (an odd sequence: being written; read again).
"""
import json
import struct

GADGETS = ('none', 'Web Shooter', 'Web Grabber', 'Sonic Burst', 'Upshot', 'Ricochet Web', 'Web Yank', 'Zip Strike')
SLOTS = ('off', 'RB+A', 'RB+B', 'RB+X', 'RB+Y', 'RB HOLD', 'Y', 'Y HOLD')
FX = ('OFF', 'LENS', 'LENS+')
FADE = ('OFF', 'LEFT', 'BOTH')
FADE_WHY = ('fine', 'this game build differs (N/A)', 'no eye reached the fade call (back to OFF)')
HEALTH_STATUS = ('no health component found: the ring shows N/A',
                 'found, but not health values: the ring shows N/A',
                 'valid, not yet seen to change (take a hit or heal)',
                 'CONFIRMED: it fell on a hit or rose on a heal')
HEALTH_EVENTS = ('tick', 'hit (fell)', 'rose', 'heal pressed', '1 s after heal', 'invalid')
GRIPS = ('grip', 'trigger', 'grip or trigger')
SPOTS, ANCHORS, STYLES = ('LEFT', 'CENTRE', 'RIGHT'), ('PLAYSPACE', 'WORLD'), ('FIGURE', 'PANEL')

RECORDS = {
    'view_gates': ('SpidyViewGatesData', 0x54414756, 1, 352),
    'combat': ('SpidyCombatData', 0x54424d43, 3, 208),
    'hand_crawl': ('SpidyHandCrawlData', 0x4c575243, 1, 96),
    'health': ('SpidyHealthData', 0x48544c48, 1, 616),
    'mirror': ('SpidyMirrorData', 0x5252494d, 1, 72),
    'world_trace': ('SpidyWorldTraceData', 0x43525457, 1, 11248),
    'blood': ('SpidyBloodData', 0x53444c42, 1, 56),
    'perf': ('SpidyPerfData', 0x37465250, 1, 288),
    # Update 37.2
    'color': ('SpidyColorData', 0x524c4f43, 2, 80),  # Update 38.5: version 2 (90s ANIMATED)
    'fovea': ('SpidyFoveaData', 0x45564f46, 1, 216),
    'npc_fade': ('SpidyNpcFadeData', 0x4643504e, 1, 96),
    # Update 38
    'combat38': ('SpidyCombat38Data', 0x38424d43, 5, 768),
    # Update 38.4
    'mirror_probe': ('SpidyMirrorProbeData', 0x4252504d, 2, 2752),  # Update 38.5: PROBE v2
    # Update 38.5
    'wrist_muzzle': ('SpidyWristMuzzleData', 0x5a554d57, 1, 144),
    'hero_switch': ('SpidyHeroSwitchData', 0x50575348, 1, 112),
    'air_watch': ('SpidyAirWatchData', 0x57524941, 1, 104),
    # Version 0.0.3
    'v003': ('SpidyV003Data', 0x33303056, 1, 5200),
    # Version 0.0.5
    'v005': ('SpidyV005Data', 0x35303056, 2, 552),
    # Version 0.0.6
    'v006': ('SpidyV006Data', 0x36303056, 1, 128),
}
# Update 38: records of the swing (ray) module, not the VR module: never in RECORDS, whose exports the
# launcher requires of the VR module. The COMBAT record already carries the lunge counts.
RAY_RECORDS = {
    'lunge': ('SpidyLungeData', 0x4c4e4753, 1, 64),
}
# Update 38: combat38::optionInfo's keys and values, in order.
COMBAT38_OPTIONS = (('crawl_cap', ('AUTO', 'WITH FLAG', 'OFF')), ('zip_mode', ('SPIDYVR', 'BOTH', 'DISTANCE', 'AIMED')),
                    ('aim_assist', ('SMART', 'SPIDYVR', 'CONE 20', 'OFF')),
                    ('ball_look', ('OURS', 'WRIST', 'GLOB', 'GAME ONLY')),  # Version 0.0.2 order
                    ('punch_mode', ('TARGETED', 'BOTH', 'GESTURE', 'CONTACT')), ('ring_anchor', ('WRIST', 'HAND', 'WORLD')),
                    ('holo_open', ('FOREARM', 'PALM LOOK', 'CLICK', 'F8 ONLY')), ('wall_tilt', ('SAFE', 'OFF')),
                    ('web_rope', ('ROPE', 'FIBRE')), ('lunge', ('GAME BUTTON', 'SPIDYVR MOVE')),
                    ('ball_fire', ('PRESS', 'RELEASE')),
                    # Update 38.1 (record version 3: values2)
                    ('aim_axis', ('HAND -Y', 'AIM POSE', 'OLD -Z')), ('ring_layer', ('HOLD', 'NEWEST', 'IMAGE')),
                    ('lunge_from', ('2.5 M', '1.3 M', '4 M')), ('lunge_speed', ('2.2', '2.6', '1.9')),
                    ('lunge_aim', ('STEADY', '38')), ('punch_angle', ('WIDE .2', '38 .35', 'ANY 0')),
                    ('ball_lead', ('ON TARGET', 'ALWAYS')), ('blood_look', ('BRUTAL', 'BIG', '37 SMALL')),
                    # Update 38.2 (record version 4: melee_anim is values2[7], the rest values3)
                    ('melee_anim', ('EVERY HIT', 'GAME', 'SPACED')), ('gadget_keys', ('38.2', 'GAME DEFAULT', 'CUSTOM')),
                    ('grab_web', ('ASSIST', 'EXACT', 'OFF')), ('mirror_look', ('SUIT', 'FIGURE')),
                    ('gadget_feel', ('EASY', '38.1')),
                    # Update 38.3 (record version 5: values3[4..7])
                    ('look_lock', ('QUICK', 'STEADY')), ('strike_steer', ('TO LOCK', 'OFF')),
                    ('far_windows', ('2X', '3X', '1.5X', 'GAME')), ('gadget_hint', ('ON', 'OFF')),
                    # Version 0.0.2 (the spare words at 500 and 652)
                    ('aim_strength', ('STRONG', 'MAX', 'OFF', 'LOW', 'NORMAL')), ('ball_hit', ('OURS ONLY', '+GAME WEB')))
# Version 0.0.3: options 30-36, in the V003 record (the CMB8 record keeps its 30).
V003_OPTIONS = (('rope_grab', ('OFF', 'ON')), ('rope_strength', ('NORMAL', 'LOW', 'STRONG')),
                ('wall_stick', ('OFF', 'LOW HEALTH ONLY', 'ALWAYS')), ('auto_perf', ('OFF', 'ON')),
                ('perf_target', ('AUTO', '72', '80', '90', '120')), ('perf_floor', ('MEDIUM', 'HIGH', 'LOW')),
                ('perf_status', ('ON', 'OFF')))
# Version 0.0.4: options 37-38, in the V004 record.
V004_OPTIONS = (('iron_man', ('OFF', 'ON')), ('mirror_view', ('OFF', 'ON')))
# Version 0.0.5: options 39-44, in the V005 record.
V005_OPTIONS = (('hero_height', ('1.00X', '1.15X', '1.30X', '1.45X', '1.60X')), ('hero_damage', ('OFF', 'ON')),
                ('punch_sense', ('FORGIVING', 'MAX', '0.0.4', 'STRICT')), ('sfx_volume', ('100%', '75%', '50%', '25%', 'OFF')),
                ('supersonic', ('ON', 'OFF')), ('supersonic_speed', ('2X', '1.5X', '2.5X', '3X')))
# Version 0.0.6: options 45-47, in the V006 record.
V006_OPTIONS = (('daredevil', ('OFF', 'ON')), ('hulk', ('OFF', 'ON')), ('daredevil_punch', ('3X', '1X', '2X', '4X', '5X')))
# Version 0.0.62: HIT INDICATOR, HAND FLIPS, LENS FLARES, PUNCH HITS, ENEMY TRACKING (the records keep
# 0.0.6's layout: these are not in them; listed so the tables agree).
V0062_OPTIONS = (('hit_indicator', ('OFF', 'ON')), ('hand_flips', ('ON', 'OFF')), ('lens_flares', ('GAME', 'OFF IN VR')),
                 ('punch_hits', ('GAME + INSTANT', 'GAME MOVE', 'INSTANT ONLY')), ('foe_track', ('LIVE', '0.0.6')), ('beacons_vision', ('ON', 'OFF')))
# Version 0.0.7 HERO PROFILES (not in the records either: 0.0.6's layout kept).
V007_OPTIONS = (('batman', ('OFF', 'ON')), ('wolverine', ('OFF', 'ON')), ('superman', ('OFF', 'ON')), ('punisher', ('OFF', 'ON')),
                ('batarang_chain', ('25%', '50%', '75%', '100%', 'OFF')), ('easy_reload', ('AUTO', 'REALISTIC')),
                ('holster_hands', ('SAME SIDE', 'EITHER HAND')), ('baton_holsters', ('ON', 'ALWAYS ON')), ('baton_look', ('CLASSIC', 'CLUB', 'MODEL')),
                ('batarang_style', ('DEFAULT', 'EIRA', 'CLOONEY', 'JOKER')), ('gun_models', ('BUILT-IN', 'MODELS')),
                ('captain_america', ('OFF', 'ON')), ('shield_arm', ('RIGHT', 'LEFT')), ('shield_damage', ('35 HP', '20 HP', '50 HP', '70 HP')),
                ('shield_chain', ('ON', 'OFF')), ('shield_block', ('OFF', 'EXPERIMENTAL')), ('shield_look', ('BUILT-IN', 'MODEL')))
# Version 0.0.72 (TEST): THOR, GREEN LANTERN and the 0.0.72 list (not in the records either).
V0072_OPTIONS = (('thor', ('OFF', 'ON')), ('thor_lightning', ('ON', 'OFF')), ('thor_storm', ('ON', 'OFF')), ('thor_chain', ('ON', 'OFF')), ('green_lantern', ('OFF', 'ON')), ('gl_flight', ('ON', 'OFF')), ('gl_fist', ('ON', 'OFF')), ('gl_beam', ('ON', 'OFF')), ('gl_bubble', ('ON', 'OFF')), ('gl_chain', ('ON', 'OFF')), ('gl_charge', ('ON', 'OFF')), ('spider_climb_only', ('ON', 'OFF')), ('uppercut', ('ON', 'OFF')), ('smoke_blind', ('ON', 'OFF')), ('dd_grapple', ('ON', 'OFF')), ('dd_parry', ('ON', 'OFF')), ('iron_hud', ('ON', 'OFF')), ('punisher_rage', ('ON', 'OFF')), ('reputation', ('ON', 'OFF')), ('survival', ('OFF', 'ON')), ('punisher_suit', ('VEST', 'WARZONE')), ('natural_invincible', ('ON', 'OFF')), ('hulk_rage', ('ON', 'OFF')), ('rage_blood', ('EXTREME', 'NORMAL', 'OFF')), ('rage_tint', ('NORMAL', 'STRONG', 'SUBTLE', 'OFF')), ('fire_douse', ('ON', 'OFF')), ('shield_slam', ('ON', 'OFF')), ('punisher_hook', ('ON', 'OFF')), ('claw_climb', ('OFF', 'ON')), ('npc_shove', ('ON', 'OFF')), ('faction', ('AUTO', 'VANILLA', 'THE HAND', 'HYDRA', 'JOKER GANG')), ('hero_suit', ('DEFAULT', 'NEXT')), ('test_unlocks', ('OFF', 'SUITS', 'ULTIMATES', 'ALL')), ('ultimates', ('ON', 'OFF')), ('perk_branch', ('AUTO', 'CHAIN FIRST', 'HOLD POINTS', 'PERKS OFF')), ('own_suit', ('OFF', 'ON')), ('perch_takedown', ('ON', 'OFF')), ('takedown_light', ('ON', 'OFF')), ('punisher_van', ('ON', 'OFF')), ('thor_hammer', ('BOOOOP', 'DEVILSEYE', 'LOVE THUNDER', 'FCCOOPER')), ('game_body', ('OFF', 'ON')), ('thor_hammer_model', ('ON', 'OFF')), ('thor_flight', ('ON', 'OFF')), ('suit_body', ('ARMS', 'FULL BODY', 'OFF')), ('cap_sprint', ('ON', 'OFF')), ('cull_margin', ('10 DEG', '6 DEG', '15 DEG', '20 DEG')), ('shadow_match', ('ON', 'OFF')), ('calib_look', ('HOLO', 'CLASSIC')), ('fly_pose', ('LEAN', 'UPRIGHT')), ('weapon_collision', ('ON', 'OFF')), ('light_hold', ('ON', 'STRONG', 'OFF')), ('van_screen', ('ON', 'OFF')), ('palm_blast', ('PALM', 'AIM')), ('hero_menu', ('HOLO TILES', 'CLASSIC')), ('rep_log', ('ON', 'OFF')), ('van_pedals', ('WHEEL HAND', 'CLASSIC')), ('image', ('SPIDYVR', 'HERO VR CLASSIC')), ('image_style', ('SPIDYVR', 'OFF')), ('dd_baton_tip', ('ON', 'OFF')), ('streamer_mode', ('ON', 'OFF')), ('streamer_smooth', ('LOW', 'OFF', 'HIGH')), ('streamer_eye', ('LEFT EYE', 'RIGHT EYE')), ('iron_hands', ('ON', 'OFF')), ('dd_grapple_aim', ('ON', 'OFF')), ('wolverine_claws', ('ON', 'OFF')))
COMBAT38_ALL_OPTIONS = COMBAT38_OPTIONS + V003_OPTIONS + V004_OPTIONS + V005_OPTIONS + V006_OPTIONS + V0062_OPTIONS + V007_OPTIONS + V0072_OPTIONS
COMBAT38_REJECTS = ('sent', 'hand untracked', 'hand holding or carrying', 'trigger held', 'hand busy', 'too slow',
                    'not forward', 'not extended', 'not re-armed', 'gated (not playing)')
CRAWL_WHY = ('none', 'low ceiling', 'crawl flag (WITH FLAG)', 'held off: combat', 'off')
PERF_SECTIONS = ('phone+holo menu+settings', 'motion+view+calibration', 'slow motion+combat+heal',
                 'swing+wall+gadgets', 'haptics+body', 'screen copies', 'publish (to the end)')
TRACE_STATUS = ('off', 'running', 'stopped (fault)', 'waiting for the registry')
TRACE_FAULTS = ('none', 'an exception (stopped itself)', 'no registry found', 'scans too slow (stopped itself)')
TRACE_KINDS = ('npc', 'spawn', 'mission')
TRACE_EVENTS = ('none', 'dialogue', 'spawned', 'despawned', 'mission start', 'mission stop', 'mission state')
TRACE_BITS = ('dialogue', 'civilian', 'enemy', 'ally', 'companion', 'pedestrian', 'bot', 'spawner', 'mission',
              'objective', 'activity', 'hero')
TRACE_EVENT_KIND = {1: 'npc', 2: 'spawn', 3: 'spawn', 4: 'mission', 5: 'mission', 6: 'mission'}


def _name(names, i):
    return names[i] if 0 <= i < len(names) else f'#{i}'


def _header(raw, key):
    _, magic, version, size = RECORDS[key] if key in RECORDS else RAY_RECORDS[key]
    if len(raw) != size:
        return False
    if struct.unpack_from('<3I', raw, 0) != (magic, version, size):
        raise RuntimeError(f'Native {key} record protocol mismatch')
    return not struct.unpack_from('<Q', raw, size - 8)[0] & 1


def parse_view_gates(raw):
    if not _header(raw, 'view_gates'):
        return None
    fx, fade, why, ready, reach, installed, impostors, dumps, _ = struct.unpack_from('<9I', raw, 12)
    jobs, writes, failed, ask0, ask1, ans0, ans1, game_calls, reach_writes, withheld = struct.unpack_from('<10Q', raw, 48)
    return dict(bloom_fx=_name(FX, fx), lod_fade=_name(FADE, fade), fade_why=_name(FADE_WHY, why),
                fade_hooked=bool(ready), lod_reach=reach, impostors_installed=bool(installed),
                impostors=bool(impostors), dumps=dumps, fx_jobs=jobs, fx_writes=writes, fx_failed=failed,
                fade_asked=[ask0, ask1], fade_answered=[ans0, ans1], fade_game_calls=game_calls,
                reach_writes=reach_writes, impostor_withheld=withheld,
                working=dict(bloom_fx=bool(writes) if fx else None, lod_fade=bool(ans0 or ans1) if fade else None))


def parse_combat(raw):
    if not _header(raw, 'combat'):
        return None
    on, tap, gesture, slots, flags = struct.unpack_from('<5I', raw, 12)
    fired = struct.unpack_from('<8Q', raw, 32)
    punches = struct.unpack_from('<2Q', raw, 96)
    missed, = struct.unpack_from('<Q', raw, 112)
    f = struct.unpack_from('<14f', raw, 120)
    steers, gated, jumps = struct.unpack_from('<3Q', raw, 176)
    return dict(on=bool(on), tap_ms=tap, gesture_ms=gesture,
                slots={g: _name(SLOTS, slots >> s & 15) for g, s in
                       (('Web Grabber', 0), ('Sonic Burst', 4), ('Upshot', 8), ('Ricochet Web', 12),
                        ('Web Yank', 16))},
                punch=bool(flags & 2), sonic_palms=bool(flags & 4), aim_steer=bool(flags & 8),
                zip_strike=bool(flags & 16), web_yank=bool(flags & 32),
                fired={GADGETS[i]: fired[i] for i in range(1, 8)}, punches=list(punches),
                sonic_missed=missed, peak_hand_speed=[round(f[0], 2), round(f[1], 2)],
                last_aim_degrees=round(f[12], 1), steers=steers, gated=gated, speed_jumps=jumps)


def parse_hand_crawl(raw):
    if not _header(raw, 'hand_crawl'):
        return None
    on, pull, grip, wall, crawl = struct.unpack_from('<5I', raw, 12)
    g0, g1, climb, crawl_frames, swing_wall = struct.unpack_from('<5Q', raw, 32)
    sx, sy, peak, _ = struct.unpack_from('<4f', raw, 72)
    return dict(hand_climb=bool(on), pull=pull, grip=_name(GRIPS, grip), game_wall_crawl=bool(wall),
                crawling_now=bool(crawl), grabs=[g0, g1], climb_frames=climb, crawl_frames=crawl_frames,
                swing_wall_frames=swing_wall, last_stick=[round(sx, 2), round(sy, 2)], peak_pull=round(peak, 2))


def parse_health(raw):
    if not _header(raw, 'health'):
        return None
    status, = struct.unpack_from('<I', raw, 12)
    reads, found, invalid, falls, rises, presses, healed = struct.unpack_from('<7Q', raw, 16)
    hp, most, share, seconds = struct.unpack_from('<4f', raw, 72)
    nxt, now = struct.unpack_from('<2I', raw, 88)  # Update 38: pad = this frame's status
    ring = []
    for i in range(32):
        s, h, m, e = struct.unpack_from('<3fI', raw, 96 + 16 * ((nxt + i) % 32))
        if s or h or m or e:
            ring.append(dict(seconds=round(s, 1), hp=round(h, 1), most=round(m, 1), event=_name(HEALTH_EVENTS, e)))
    return dict(status=_name(HEALTH_STATUS, status), status_now=_name(HEALTH_STATUS, now), confirmed=status == 3, valid=status >= 2, reads=reads,
                found=found, invalid_reads=invalid, falls=falls, rises=rises, heal_presses=presses,
                heals_that_rose=healed, hp=round(hp, 1), most=round(most, 1), share=round(share, 3),
                seconds=round(seconds, 1), recent=ring)


def parse_mirror(raw):
    if not _header(raw, 'mirror'):
        return None
    on, anchor, spot, style, recentres = struct.unpack_from('<5I', raw, 12)
    frames, vertices = struct.unpack_from('<2Q', raw, 32)
    last, mean, peak, distance = struct.unpack_from('<4f', raw, 48)
    return dict(used=bool(on), anchor=_name(ANCHORS, anchor), spot=_name(SPOTS, spot), style=_name(STYLES, style),
                recentres=recentres, frames=frames, vertices=vertices, last_ms=round(last, 3),
                mean_ms=round(mean, 3), peak_ms=round(peak, 3), distance_m=round(distance, 2))


def _text(raw, at, n):
    return raw[at:at + n].split(b'\0', 1)[0].decode('ascii', 'replace')


def _bits(b):
    return [name for i, name in enumerate(TRACE_BITS) if b >> i & 1]


def parse_world_trace(raw):
    if not _header(raw, 'world_trace'):
        return None
    status, = struct.unpack_from('<I', raw, 12)
    on = struct.unpack_from('<3I', raw, 16)
    fault, = struct.unpack_from('<I', raw, 28)
    scans, entries, classes, actors = struct.unpack_from('<4Q', raw, 32)
    counts = struct.unpack_from('<7Q', raw, 64)
    dropped = struct.unpack_from('<3Q', raw, 120)
    last, mean, peak, interval = struct.unpack_from('<4f', raw, 144)
    nxt, census_count = struct.unpack_from('<2I', raw, 160)
    events = []
    for i in range(64):
        at = 168 + 152 * ((nxt + i) % 64)
        sec, x, y, z, dist, gaze, ev, bits, changed, count = struct.unpack_from('<6f4I', raw, at)
        if not ev:
            continue
        events.append(dict(seconds=round(sec, 2), kind=TRACE_EVENT_KIND.get(ev, '?'), event=_name(TRACE_EVENTS, ev),
                           at=[round(x, 1), round(y, 1), round(z, 1)], metres=round(dist, 1),
                           looked_at=gaze < 12, gaze_degrees=round(gaze, 1), bits=_bits(bits),
                           changed_words=changed, count=count, name=_text(raw, at + 40, 48),
                           text=_text(raw, at + 88, 64)))
    census = []
    for i in range(min(census_count, 24)):
        at = 9896 + 56 * i
        b, live = struct.unpack_from('<2I', raw, at + 48)
        census.append(dict(name=_text(raw, at, 48), bits=_bits(b), live=live))
    return dict(status=_name(TRACE_STATUS, status), fault=_name(TRACE_FAULTS, fault),
                on={k: bool(on[i]) for i, k in enumerate(TRACE_KINDS)}, scans=scans, registry_entries=entries,
                classes=classes, actors=actors, counts={_name(TRACE_EVENTS, i): counts[i] for i in range(1, 7)},
                dropped={k: dropped[i] for i, k in enumerate(TRACE_KINDS)}, last_scan_ms=round(last, 3),
                mean_scan_ms=round(mean, 3), peak_scan_ms=round(peak, 3), interval_s=round(interval, 2),
                events=events, census=census)


def parse_blood(raw):
    if not _header(raw, 'blood'):
        return None
    intensity, = struct.unpack_from('<I', raw, 12)
    fired, plan_b, confirmed = struct.unpack_from('<3Q', raw, 16)
    mean, peak = struct.unpack_from('<2f', raw, 40)
    return dict(intensity=intensity, on=bool(intensity), fired=fired, plan_b=plan_b, confirmed=confirmed,
                mean_ms=round(mean, 3), peak_ms=round(peak, 3))


def parse_perf(raw):
    if not _header(raw, 'perf'):
        return None
    reuse, = struct.unpack_from('<I', raw, 12)
    built, reused = struct.unpack_from('<2Q', raw, 16)
    build, build_peak, replay, shown = struct.unpack_from('<4f', raw, 32)
    n = len(PERF_SECTIONS)
    means = struct.unpack_from(f'<{n}f', raw, 48)
    peaks = struct.unpack_from(f'<{n}f', raw, 48 + 4 * n)
    spike_ms, spike_section_ms, spike_s = struct.unpack_from('<3f', raw, 104)
    spike_section, = struct.unpack_from('<I', raw, 116)
    spikes, frames = struct.unpack_from('<2Q', raw, 120)
    parts = struct.unpack_from('<8f', raw, 136)
    blood, mirror, scan = struct.unpack_from('<3f', raw, 168)
    r = lambda x: round(x, 3)
    return dict(holo_reuse=bool(reuse), holo_built=built, holo_reused=reused, holo_build_ms=r(build),
                holo_build_peak_ms=r(build_peak), holo_replay_ms=r(replay), holo_mean_ms=r(shown),
                sections={name: dict(mean_ms=r(means[i]), peak_ms=r(peaks[i])) for i, name in enumerate(PERF_SECTIONS)},
                spike=dict(ms=r(spike_ms), section=_name(PERF_SECTIONS, spike_section),
                           section_ms=r(spike_section_ms), at_seconds=r(spike_s), count=spikes),
                frames=frames,
                overlay=dict(world_and_holo_ms=r(parts[0]), world_and_holo_peak_ms=r(parts[1]),
                             wheel_phone_ms=r(parts[2]), wheel_phone_peak_ms=r(parts[3]), draw_ms=r(parts[4]),
                             draw_peak_ms=r(parts[5]), total_ms=r(parts[6]), total_peak_ms=r(parts[7])),
                blood_ms=r(blood), mirror_ms=r(mirror), world_scan_ms=r(scan))


COLOR_FILTERS = ('NONE', 'VIVID', 'GOLDEN HOUR', 'NIGHT NEON', '2000s RAIMI', 'NOIR', '90s ANIMATED')
TOON_FAULTS = ('NONE', 'SHADER FAILED', 'PIPELINE FAILED')
FOVEA_LEVELS = ('OFF', 'LIGHT', 'BALANCED', 'AGGRESSIVE')
FOVEA_FAULTS = ('none', 'NO VRS TIER 2', 'LIST REFUSED', 'IMAGE FAILED', 'OTHER SIZE', 'HOOK FAILED')
NPC_FADE_MODES = ('GAME', 'OFF', 'SWITCH')
NPC_FADE_WHY = ('fine', 'the candidate bytes were not found (left as the game has it)',
                'a write failed (everything put back: GAME)')


def parse_color(raw):
    """Update 37.2: COLOR / ATMOSPHERE (eye_color.hpp Data)."""
    if not _header(raw, 'color'):
        return None
    on, flt, strength, contrast, saturation = struct.unpack_from('<5I', raw, 12)
    exposure, warmth, tint = struct.unpack_from('<3i', raw, 32)
    vibrance, vignette, fault = struct.unpack_from('<3I', raw, 44)
    cartoon, toon_bits = struct.unpack_from('<2I', raw, 56)  # Update 38.5 (version 2)
    pairs, = struct.unpack_from('<Q', raw, 64)
    return dict(on=bool(on), filter=_name(COLOR_FILTERS, flt), strength=strength, exposure_ev=exposure / 10,
                contrast=contrast, saturation=saturation, vibrance=vibrance, warmth=warmth, tint=tint,
                vignette=vignette, fault=fault, graded_pairs=pairs, cartoon=cartoon,
                outlines=bool(toon_bits & 1), bands=toon_bits >> 8 & 0xff,
                toon_fault=_name(TOON_FAULTS, toon_bits >> 16 & 0xff))


def parse_fovea(raw):
    """Update 37.2: FOVEATED (EXPERIMENTAL, foveation.hpp Data), with each level's FPS and eye GPU ms."""
    if not _header(raw, 'fovea'):
        return None
    level, inner, debug, tier, tile, has4x4, fault, width, height = struct.unpack_from('<9I', raw, 12)
    share, cu, cv = struct.unpack_from('<3f', raw, 48)
    tagged, applied, untagged, resets, images, mismatches = struct.unpack_from('<6Q', raw, 64)
    per = {}
    for i, name in enumerate(FOVEA_LEVELS):
        seconds, avg, low, high, gpu = struct.unpack_from('<I4f', raw, 112 + 24 * i)
        per[name] = dict(seconds=seconds, avg_fps=round(avg, 1), low_fps=round(low, 1), high_fps=round(high, 1),
                         gpu_ms=round(gpu, 3))
    return dict(level=_name(FOVEA_LEVELS, level), inner=inner, debug=bool(debug), vrs_tier=tier, tile=tile,
                has_4x4=bool(has4x4), fault=_name(FOVEA_FAULTS, fault), image=[width, height],
                shaded_share=round(share, 3), tagged_passes=tagged, applied_passes=applied,
                untagged_passes=untagged, resets=resets, images=images, mismatches=mismatches, per_level=per)


def parse_npc_fade(raw):
    """Update 37.2: NPC NEAR FADE (npc_fade.hpp Data; CANDIDATE addresses)."""
    if not _header(raw, 'npc_fade'):
        return None
    mode, why, switch_now, ret_now, ready = struct.unpack_from('<5I', raw, 12)
    sw, rw, cleared, near, near_game, near_fixed = struct.unpack_from('<6Q', raw, 32)
    nearest, = struct.unpack_from('<f', raw, 80)
    return dict(mode=_name(NPC_FADE_MODES, mode), why=_name(NPC_FADE_WHY, why), switch_now=switch_now,
                ret_now=ret_now, ready=bool(ready), switch_writes=sw, ret_writes=rw, switch_cleared=cleared,
                near_episodes=near, near_while_game=near_game, near_while_fixed=near_fixed,
                nearest_m=None if nearest > 1e8 else round(nearest, 2))


def parse_combat38(raw):
    """Update 38: the COMBAT record (combat38.hpp Data)."""
    if not _header(raw, 'combat38'):
        return None
    flags, = struct.unpack_from('<I', raw, 12)
    values = struct.unpack_from('<12I', raw, 16) + struct.unpack_from('<8I', raw, 416) + \
        struct.unpack_from('<8I', raw, 504) + struct.unpack_from('<I', raw, 500) + struct.unpack_from('<I', raw, 652)
    rejects = struct.unpack_from('<10Q', raw, 64)
    (contact, gesture, zips, zips_aimed, air, assists, assists_missed, globs, caps, held_off, holo_deferred,
     holo_cancelled, tilt_resets, tilt_capped) = struct.unpack_from('<14Q', raw, 144)
    near = struct.unpack_from('<3Q', raw, 256)
    near_frames = struct.unpack_from('<3Q', raw, 280)
    enemies, crawl_why, last_reject, npc_mode = struct.unpack_from('<4I', raw, 304)
    nearest, assist_deg, ring_error, scan_hz, ring_peak, roof = struct.unpack_from('<6f', raw, 320)
    melee_merged, held_off_far = struct.unpack_from('<2I', raw, 344)  # Version 0.0.3 (was a spare pad)
    (lunges, landed, missed, yanks, lock_changes, sight_rays, sight_blocked,
     sight_unknown) = struct.unpack_from('<8Q', raw, 352)
    swing_rejects, zone_swallowed, holo_taps, holo_look_opens, ring_hidden, lunge_held_off = \
        struct.unpack_from('<6Q', raw, 448)
    lock_held, = struct.unpack_from('<I', raw, 496)
    (forearm_opens, ring_held, targeted_held_off, melee_taps, melee_delayed, melee_dropped, gadgets_recognised,
     grab_attached, grab_yanks, grab_released, smart_assists, blood_coats,
     mirror_suit_frames) = struct.unpack_from('<13Q', raw, 536)
    ring_wrist_cm, ring_wrist_peak_cm = struct.unpack_from('<2f', raw, 640)
    mirror_joints, = struct.unpack_from('<I', raw, 648)
    (look_switches, strike_steers, grab_assisted, gadget_hints, sonic_hints, yank_gestures, window_calls,
     window_writes) = struct.unpack_from('<8Q', raw, 656)
    window_ready = bool(window_calls >> 63)  # bit 63: the far-windows hook is ready (stereo_probe.cpp)
    window_calls &= (1 << 63) - 1
    window_game_m, window_m = struct.unpack_from('<2f', raw, 720)
    suit_samples, suit_snapshots = struct.unpack_from('<2Q', raw, 728)
    suit_primary, suit_secondary, suit_emblem, suit_learned = struct.unpack_from('<4I', raw, 744)
    modes = ('GAME', 'OFF', 'SWITCH')
    return dict(options={key: _name(names, values[i]) for i, (key, names) in enumerate(COMBAT38_OPTIONS)},
                airborne_combat=bool(flags & 1),
                rejects={COMBAT38_REJECTS[i]: n for i, n in enumerate(rejects)},
                contact_hits=contact, gesture_hits=gesture, zips=zips, zips_aimed=zips_aimed, air_punches=air,
                assists=assists, assists_missed=assists_missed, globs=globs, crawl_caps=caps,
                crawl_held_off=held_off, holo_deferred=holo_deferred, holo_cancelled=holo_cancelled,
                tilt_resets=tilt_resets, tilt_capped=tilt_capped,
                enemy_near={modes[i]: near[i] for i in range(3)},
                enemy_near_frames={modes[i]: near_frames[i] for i in range(3)},
                enemies=enemies, crawl_why=_name(CRAWL_WHY, crawl_why),
                last_reject=_name(COMBAT38_REJECTS, last_reject), npc_mode=_name(modes, npc_mode),
                nearest_enemy_m=None if nearest > 1e8 else round(nearest, 2),
                assist_degrees=round(assist_deg, 1), ring_error_cm=round(ring_error * 100, 1),
                ring_error_peak_cm=round(ring_peak * 100, 1), scan_hz=round(scan_hz, 1),
                roof_m=round(roof, 2),
                # version 2: the SpidyVR fold-in
                lunges=lunges, lunges_landed=landed, lunges_missed=missed, yanks=yanks, lock_changes=lock_changes,
                sight_rays=sight_rays, sight_blocked=sight_blocked, sight_unknown=sight_unknown,
                # version 3: Update 38.1
                swing_rejects=swing_rejects, holo_zone_swallowed=zone_swallowed, holo_taps=holo_taps,
                holo_look_opens=holo_look_opens, ring_hidden_frames=ring_hidden, lunge_held_off=lunge_held_off,
                lock_held=lock_held,
                # version 4: Update 38.2
                holo_forearm_opens=forearm_opens, ring_held_frames=ring_held, targeted_held_off=targeted_held_off,
                melee_taps=melee_taps, melee_delayed=melee_delayed, melee_dropped=melee_dropped,
                melee_merged=melee_merged, targeted_held_off_far=held_off_far,
                gadgets_recognised=gadgets_recognised, grab_attached=grab_attached, grab_yanks=grab_yanks,
                grab_released=grab_released, smart_assists=smart_assists, blood_coats=blood_coats,
                mirror_suit_frames=mirror_suit_frames, ring_wrist_cm=round(ring_wrist_cm, 1),
                ring_wrist_peak_cm=round(ring_wrist_peak_cm, 1), mirror_joints=mirror_joints,
                # version 5: Update 38.3
                look_switches=look_switches, strike_steers=strike_steers, grab_assisted=grab_assisted,
                gadget_hints=gadget_hints, sonic_hints=sonic_hints, yank_gestures=yank_gestures,
                window_calls=window_calls, window_ready=window_ready, window_writes=window_writes,
                window_game_m=round(window_game_m, 1), window_m=round(window_m, 1),
                suit_samples=suit_samples, suit_snapshots=suit_snapshots, suit_learned=bool(suit_learned),
                suit_primary='#%06x' % suit_primary, suit_secondary='#%06x' % suit_secondary,
                suit_emblem='#%06x' % suit_emblem)


LUNGE_PHASES = ('idle', 'flying', 'hanging')
# mirror_view.hpp Phase / Why (the MIRROR VIEW TEST in the v003 record; 0.0.6 exports it).
MIRROR_TEST_PHASES = ('off', 'waiting', 'running', 'done', 'failed')
MIRROR_TEST_WHYS = ('', 'fewer than 4 view slots free', 'the eyes are not up', 'the game made no view',
                    'the view left the pool', 'too slow: under 20 fps for 5 s',
                    'the picture could not be shown')
LUNGE_MODES = ('GAME BUTTON', 'SPIDYVR MOVE')


def parse_lunge(raw):
    """Update 38: the swing bridge's SPIDYVR MOVE lunge (combat_spidy.hpp LungeData)."""
    if not _header(raw, 'lunge'):
        return None
    phase, = struct.unpack_from('<I', raw, 12)
    opened, landed, missed, target = struct.unpack_from('<4Q', raw, 16)
    hand, mode = struct.unpack_from('<2i', raw, 48)
    return dict(phase=_name(LUNGE_PHASES, phase), opened=opened, landed=landed, missed=missed,
                target=target, hand=hand, mode=_name(LUNGE_MODES, mode))


MIRROR_KINDS = ('game', 'eye_left', 'eye_right', 'offscreen', 'other')
MIRROR_STATUS = ('not started', 'watching', 'stopped: over its 60 us budget', 'stopped: a read faulted')
MIRROR_WHERE = ('instance', 'model', 'record')


def _pointer_like(offset, v):
    """The low half of a pointer, likely: 8-aligned, at an 8-aligned offset. Such a field moves when the
    game reallocates anything, so it is a weaker suit-ID candidate than a small number."""
    return offset % 8 == 0 and v >= 0x10000 and v % 8 == 0


WRIST_MUZZLE_HOOKED = {0: 'off', 1: 'muzzle', 2: 'parent muzzle', 3: 'both'}


def parse_wrist_muzzle(raw):
    """Update 38.5: BALL LOOK WRIST's web-shooter muzzle hook (wrist_muzzle.hpp Status; read-only)."""
    if not _header(raw, 'wrist_muzzle'):
        return None
    hooked, error, enabled = struct.unpack_from('<3I', raw, 12)
    calls, web, moved, no_shot, stale, too_far, not_finite = struct.unpack_from('<7Q', raw, 24)
    shots = struct.unpack_from('<2Q', raw, 80)
    index = struct.unpack_from('<4Q', raw, 96)
    shift, age = struct.unpack_from('<2f', raw, 128)
    return dict(hooked=WRIST_MUZZLE_HOOKED.get(hooked, hooked), start_error=error, enabled=bool(enabled),
                calls=calls, web_calls=web, moved=moved, no_shot=no_shot, stale=stale, too_far=too_far,
                not_finite=not_finite, shots_left=shots[0], shots_right=shots[1],
                index_seen=dict(zip(('0', '1', '2', 'other'), index)), last_shift_m=round(shift, 3),
                last_age_s=round(age, 3))


def parse_hero_switch(raw):
    """Update 38.5: the HERO SWITCH PROBE (hero_switch.hpp Data; read-only)."""
    if not _header(raw, 'hero_switch'):
        return None
    swaps, view_back, body_back, screen_after, pending = struct.unpack_from('<5I', raw, 12)
    hero, record, last_hero, last_record = struct.unpack_from('<4Q', raw, 40)
    gap, view, body, worst_view, worst_body = struct.unpack_from('<5f', raw, 72)
    gaps, = struct.unpack_from('<Q', raw, 96)
    return dict(swaps=swaps, view_back=view_back, body_back=body_back, screen_after=screen_after,
                pending=bool(pending), hero=hex(hero), record=hex(record), last_hero=hex(last_hero),
                last_record=hex(last_record), last_gap_ms=round(gap), last_view_ms=round(view),
                last_body_ms=round(body), worst_view_ms=round(worst_view), worst_body_ms=round(worst_body),
                gaps_without_hero=gaps)


def parse_air_watch(raw):
    """Update 38.5: Y IN THE AIR (air_watch.hpp Data; read-only)."""
    if not _header(raw, 'air_watch'):
        return None
    yielding, = struct.unpack_from('<I', raw, 12)
    (y_sent, y_in_air, yank_inputs, yanks_in_air, yields, outlived, taken_back, capped,
     _spare) = struct.unpack_from('<9Q', raw, 16)
    longest, last = struct.unpack_from('<2I', raw, 88)
    return dict(yielding=bool(yielding), y_sent=y_sent, y_in_air=y_in_air, yank_inputs=yank_inputs,
                yanks_in_air=yanks_in_air, yields=yields, past_3s=outlived, taken_back_by_trigger=taken_back,
                capped_60s=capped, longest_ms=longest, last_ms=last)


PREVIEW_HERO = ('another', 'the hero', 'on the hero', 'the hero model')


def preview_is_ours(previews, solves_in_preview):
    """Update 38.5: is the suits menu's collapsed preview the port's body? Only when the preview slot
    draws the hero's own instance (or one on his root) while the port's body was solving it."""
    if not previews:
        return 'unknown (no preview seen)'
    hero = [p for p in previews if p['is_hero'] in ('the hero', 'on the hero')]
    if hero and solves_in_preview:
        return 'likely ours (the preview draws the hero the body solves)'
    if hero:
        return 'the hero, but the body did not solve during it'
    return 'not ours (the preview draws its own copy)'


def parse_mirror_probe(raw):
    """Update 38.4: the MIRROR PROBE (mirror_probe.hpp Data; read-only)."""
    if not _header(raw, 'mirror_probe'):
        return None
    status, = struct.unpack_from('<I', raw, 12)
    frames, = struct.unpack_from('<Q', raw, 16)
    (used, used_max, ours, free_min, builtin, builtin_max, listed, listed_max) = struct.unpack_from('<8I', raw, 24)
    headroom, event_next = struct.unpack_from('<iI', raw, 56)
    created, removed, ours_created = struct.unpack_from('<3Q', raw, 64)
    events = []
    for i in range(min(event_next, 8)):
        e = struct.unpack_from('<8I', raw, 88 + 32 * ((event_next - 1 - i) % 8))
        events.append(dict(frame=e[0], slot=e[1], created=bool(e[2]), ours=bool(e[3]), flags=hex(e[4]),
                           size=f'{e[5]}x{e[6]}', kind=e[7]))
    flags = struct.unpack_from('<8I', raw, 344)
    width = struct.unpack_from('<8I', raw, 376)
    height = struct.unpack_from('<8I', raw, 408)
    kind = struct.unpack_from('<8I', raw, 440)
    slots = [dict(slot=i, flags=hex(flags[i]), size=f'{width[i]}x{height[i]}', kind=kind[i])
             for i in range(8) if flags[i] or width[i] or kind[i]]
    jobs = struct.unpack_from('<5Q', raw, 472)
    jobs_max, views_max = struct.unpack_from('<2I', raw, 512)
    sampled, sampled_game, hero_missing = struct.unpack_from('<3Q', raw, 520)
    hero_draws = struct.unpack_from('<5Q', raw, 544)
    piece_draws = struct.unpack_from('<5Q', raw, 584)
    pieces_n, overflow = struct.unpack_from('<2I', raw, 624)
    pieces = []
    for i in range(min(pieces_n, 8)):
        inst, model, radius, pflags = struct.unpack_from('<QQfI', raw, 632 + 48 * i)
        draws = struct.unpack_from('<5I', raw, 632 + 48 * i + 24)
        pieces.append(dict(instance=hex(inst), model=hex(model), radius_m=round(radius, 3), flags=hex(pflags),
                           drawn_off=bool(pflags & 0x20), draws=dict(zip(MIRROR_KINDS, draws))))
    (hero_frames, hidden, by_port, by_game, game_hides, port_hide, body_drawn) = struct.unpack_from('<7Q', raw, 1016)
    hero_flags, switch_now, switch_changes, capped = struct.unpack_from('<4I', raw, 1072)
    (inst, model, record, inst_changes, model_changes, snaps) = struct.unpack_from('<6Q', raw, 1088)
    suit_next, fields_n = struct.unpack_from('<2I', raw, 1136)
    suit = []
    for i in range(min(suit_next, 6)):
        fr, what, old, new, changed = struct.unpack_from('<IIQQI', raw, 1144 + 32 * ((suit_next - 1 - i) % 6))
        suit.append(dict(frame=fr, what=_name(('none', 'instance', 'model', 'instance and model'), what),
                         old_model=hex(old), new_model=hex(new), changed_words=changed))
    fields = []
    for i in range(min(fields_n, 16)):
        where, offset, with_suit, noise, before, after = struct.unpack_from('<4H2I', raw, 1336 + 16 * i)
        fields.append(dict(where=_name(MIRROR_WHERE, where), offset=hex(offset), with_suit=with_suit, noise=noise,
                           before=hex(before), after=hex(after),
                           pointer_like=_pointer_like(offset, before) and _pointer_like(offset, after)))
    total, = struct.unpack_from('<d', raw, 1592)
    cost_max, cost_mean = struct.unpack_from('<2f', raw, 1600)
    sampled_draws, = struct.unpack_from('<Q', raw, 1608)
    # Update 38.5 PROBE v2 (V2 at 1616)
    event_ms = struct.unpack_from('<8I', raw, 1616)
    suit_ms = struct.unpack_from('<6I', raw, 1648)
    for i, e in enumerate(events):
        e['ms'] = event_ms[(event_next - 1 - i) % 8]
    for i, e in enumerate(suit):
        e['ms'] = suit_ms[(suit_next - 1 - i) % 6]
    first_n, start_used = struct.unpack_from('<2I', raw, 2248)
    first = []
    for i in range(min(first_n, 16)):
        e = struct.unpack_from('<8I', raw, 1672 + 32 * i)
        ms, = struct.unpack_from('<I', raw, 2184 + 4 * i)
        first.append(dict(frame=e[0], ms=ms, slot=e[1], created=bool(e[2]), ours=bool(e[3]), flags=hex(e[4]),
                          size=f'{e[5]}x{e[6]}', kind=e[7]))
    hero_by_slot = struct.unpack_from('<8Q', raw, 2256)
    other_by_slot = struct.unpack_from('<8Q', raw, 2320)
    menu_frames, preview_frames, preview_starts = struct.unpack_from('<3Q', raw, 2384)
    (p_slot, p_w, p_h, p_flags, p_kind, p_first, p_last, p_n) = struct.unpack_from('<8I', raw, 2408)
    solved_in, hero_jobs_in, solved_out, body_drawn_in = struct.unpack_from('<4Q', raw, 2440)
    rig_in, model_in, inst_in, snaps_in = struct.unpack_from('<4I', raw, 2472)
    previews = []
    for i in range(min(p_n, 8)):
        inst_p, model_p, draws_p, slot_p, hero_p, flags_p = struct.unpack_from('<QQ4I', raw, 2488 + 32 * i)
        previews.append(dict(instance=hex(inst_p), model=hex(model_p), draws=draws_p, slot=slot_p,
                             is_hero=_name(PREVIEW_HERO, hero_p), flags=hex(flags_p)))
    preview = dict(frames=preview_frames, starts=preview_starts, menu_frames=menu_frames,
                   slot=None if p_slot >= 8 else p_slot, size=f'{p_w}x{p_h}', flags=hex(p_flags), kind=p_kind,
                   first_ms=p_first, last_ms=p_last, instances=previews,
                   hero_draws_by_slot=list(hero_by_slot), other_draws_by_slot=list(other_by_slot),
                   body_solves_in_preview=solved_in, body_hero_jobs_in_preview=hero_jobs_in,
                   body_solves_outside=solved_out, body_drawn_frames_in_preview=body_drawn_in,
                   rig_switches_in_preview=rig_in, model_changes_in_preview=model_in,
                   instance_changes_in_preview=inst_in, suit_snapshots_in_preview=snaps_in,
                   ours=preview_is_ours(previews, solved_in))
    return dict(status=_name(MIRROR_STATUS, status), frames=frames, preview=preview, first_events=first,
                start_slots_used=start_used,
                slots=dict(used=used, used_max=used_max, ours=ours, free_min=free_min, created=created,
                           removed=removed, ours_created=ours_created, now=slots, events=events),
                view_list=dict(builtin=builtin, builtin_max=builtin_max, listed=listed, listed_max=listed_max,
                               headroom_min=headroom, smallest_cap=8),
                jobs=dict(zip(MIRROR_KINDS, jobs)), jobs_frame_max=jobs_max, views_frame_max=views_max,
                sampled_frames=sampled, sampled_game_frames=sampled_game, hero_missing_in_game=hero_missing,
                hero_draws=dict(zip(MIRROR_KINDS, hero_draws)), piece_draws=dict(zip(MIRROR_KINDS, piece_draws)),
                pieces=pieces, pieces_overflow=overflow, sampled_draws=sampled_draws, capped_frames=capped,
                hero=dict(frames=hero_frames, hidden=hidden, hidden_by_port=by_port, hidden_by_game=by_game,
                          game_hides=game_hides, port_hide_frames=port_hide, body_drawn_frames=body_drawn,
                          flags_now=hex(hero_flags), instance=hex(inst), model=hex(model), record=hex(record),
                          instance_changes=inst_changes, model_changes=model_changes),
                camera_switch=dict(now=switch_now, changes=switch_changes),
                suit=dict(snapshots=snaps, events=suit, fields=fields),
                cost=dict(total_ms=round(total / 1000, 3), peak_us=round(cost_max, 2), mean_us=round(cost_mean, 3)))


def copy_gpu_split(copy_gpu_ms, sharpen_gpu_ms):
    """copy_gpu is from the copy's submission until the CPU sees it finished: the GPU work (the sharpen
    pass, when on, else two plain copies) plus the wait behind the game's own frame on the same queue."""
    if copy_gpu_ms is None:
        return None
    work = max(0.0, float(sharpen_gpu_ms or 0.0))
    return dict(copy_gpu_ms=round(copy_gpu_ms, 3), work_ms=round(min(work, copy_gpu_ms), 3),
                wait_ms=round(max(0.0, copy_gpu_ms - work), 3))


class TraceJournal:
    """Appends each WORLD LOG event once to reports/world-trace-<stamp>.jsonl (the native ring holds the
    last 64; polled every second it misses none unless more than 64 land in one). Never raises."""

    def __init__(self, path):
        self.path, self.seen, self.written = path, set(), 0

    def add(self, trace):
        if not trace or 'events' not in trace:
            return 0
        new = []
        for e in trace['events']:
            key = (e['seconds'], e['event'], e['name'], e['text'], e['count'])
            if key not in self.seen:
                self.seen.add(key)
                new.append(e)
        if not new:
            return 0
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            with self.path.open('a', encoding='utf-8') as f:
                for e in new:
                    f.write(json.dumps(e) + '\n')
        except OSError:
            return 0
        self.written += len(new)
        return len(new)

    def finish(self, trace):
        """The census and totals as the last line."""
        self.add(trace)
        if not trace or 'census' not in trace:
            return
        try:
            with self.path.open('a', encoding='utf-8') as f:
                f.write(json.dumps(dict(summary={k: v for k, v in trace.items() if k != 'events'})) + '\n')
        except OSError:
            pass


ROPE_RELEASES = ('none', 'fling', 'slam', 'clap slam', 'let go', 'lost', 'timeout', 'drag')
PERF_RUNGS = ('YOUR SETTINGS', 'FAR WINDOWS 1.5X', 'FAR WINDOWS GAME', 'SHARED LOD GAME', 'FAR SHADOWS OFF')
PERF_WHYS = ('', 'frames missed', 'headroom', 'zone memory', 'floor', 'off: restored', 'on')


def parse_v003(raw):
    """Version 0.0.3: the V003 record (v003.hpp Data): AUTO PERFORMANCE, ROPE GRAB, WALL STICK, the
    SUIT ID WATCH (hero + 0x8c, CANDIDATE) and the BOT STATE PROBE."""
    if not _header(raw, 'v003'):
        return None
    flags, = struct.unpack_from('<I', raw, 12)
    values = struct.unpack_from('<7I', raw, 16)
    on, rung, zone, events = struct.unpack_from('<4I', raw, 44)
    target, mean, missed, hz = struct.unpack_from('<4f', raw, 60)
    user = struct.unpack_from('<3I', raw, 76)
    now_far, now_lod, now_shadows, refused, user_changed = struct.unpack_from('<5I', raw, 88)
    drops, raises, zone_jumps, bans, restores = struct.unpack_from('<5Q', raw, 112)
    log = []
    for i in range(16):
        seconds, fps, frm, to, why, z = struct.unpack_from('<2f4I', raw, 152 + 24 * i)
        if frm == to == why == 0 and seconds == 0:
            continue
        log.append(dict(seconds=round(seconds, 2), fps=round(fps, 1), frm=_name(PERF_RUNGS, frm),
                        to=_name(PERF_RUNGS, to), why=_name(PERF_WHYS, why), zone=z))
    t_on = struct.unpack_from('<2I', raw, 536)
    t_why = struct.unpack_from('<2I', raw, 544)
    t_id = struct.unpack_from('<2Q', raw, 552)
    t_len = struct.unpack_from('<2f', raw, 568)
    t_speed = struct.unpack_from('<2f', raw, 576)
    attaches, yanks, claps, no_target, both = struct.unpack_from('<5Q', raw, 584)
    releases = struct.unpack_from('<8Q', raw, 624)
    impacts, shown, walls, floors, unstuck, arms = struct.unpack_from('<6Q', raw, 688)
    nets_now, = struct.unpack_from('<I', raw, 736)
    suit_id, suit_have = struct.unpack_from('<2I', raw, 744)
    suit_reads, suit_failed, suit_changes, forgot = struct.unpack_from('<4Q', raw, 752)
    t_phase, t_why2, t_slot, t_runs = struct.unpack_from('<4I', raw, 784)
    t_secs, t_base, t_ms = struct.unpack_from('<3f', raw, 800)
    t_frames, = struct.unpack_from('<Q', raw, 816)
    t_w, t_h, t_tex, t_free = struct.unpack_from('<4I', raw, 824)
    t_hero, t_other = struct.unpack_from('<2Q', raw, 840)
    bot_on, = struct.unpack_from('<I', raw, 856)
    b = 864
    scans, objects, bot_states, flung, wall_stuck, changes, faults, watched = struct.unpack_from('<8Q', raw, b)
    found_count, snap_count = struct.unpack_from('<2I', raw, b + 64)
    found = []
    for i in range(min(found_count, 16)):
        first, second, level, vt, seen = struct.unpack_from('<2H3I', raw, b + 72 + 56 * i)
        name = raw[b + 72 + 56 * i + 16:b + 72 + 56 * i + 56].split(b'\0')[0].decode('ascii', 'replace')
        found.append(dict(at=[first, second], level=level, vtable_rva=hex(vt), seen=seen, name=name))
    names = [k for k, _ in V003_OPTIONS]
    return dict(flags=flags, values={names[i]: _name(V003_OPTIONS[i][1], v) for i, v in enumerate(values)},
                auto_perf=dict(on=bool(on), rung=_name(PERF_RUNGS, rung), zone=zone, events=events,
                               target_fps=round(target, 1), mean_fps=round(mean, 1), missed_share=round(missed, 3),
                               headset_hz=round(hz, 1),
                               user=dict(far_windows_percent=user[0], shared_lod=user[1], far_shadows=user[2]),
                               now=dict(far_windows_percent=now_far, shared_lod=now_lod, far_shadows=now_shadows),
                               apply_refused=refused, user_changed_while_on=user_changed, drops=drops, raises=raises,
                               zone_jumps=zone_jumps, bans=bans, restores=restores, log=log),
                rope_grab=dict(tether_on=[bool(x) for x in t_on], last_release=[_name(ROPE_RELEASES, x) for x in t_why],
                               tether_id=list(t_id), length_m=[round(x, 2) for x in t_len],
                               speed_ms=[round(x, 2) for x in t_speed], attaches=attaches, yanks=yanks, claps=claps,
                               no_target=no_target, both_on_one=both,
                               releases={ROPE_RELEASES[i]: releases[i] for i in range(1, 8)}),
                wall_stick=dict(impacts=impacts, shown=shown, walls=walls, floors=floors, unstuck=unstuck, arms=arms,
                                nets_now=nets_now),
                suit_watch=dict(suit_id=suit_id, have=bool(suit_have), reads=suit_reads, failed=suit_failed,
                                changes=suit_changes, colours_forgot=forgot),
                mirror_view_test=dict(phase=_name(MIRROR_TEST_PHASES, t_phase), why=_name(MIRROR_TEST_WHYS, t_why2),
                                      slot=t_slot, runs=t_runs, seconds=round(t_secs, 1),
                                      base_ms=round(t_base, 2), test_ms=round(t_ms, 2), frames=t_frames,
                                      width=t_w, height=t_h, texture=t_tex, free_slots=t_free,
                                      hero_draws=t_hero, other_draws=t_other),
                bot_probe=dict(on=bool(bot_on), scans=scans, objects_read=objects, bot_states=bot_states,
                               flung_seen=flung, wall_stuck_seen=wall_stuck, changes=changes, faults=faults,
                               watched=hex(watched), found=found, snaps=snap_count))


DAMAGE_STATES = ('none', 'read', 'CONFIRMED')
DAMAGE_WRITE = ('OFF', 'READY', 'SENT', 'CONFIRMED', 'REFUSED')
DAMAGE_CAUSES = ('no X press or blow', 'X press', "the mod's blow")
DAMAGE_CODES = {0: '', 9501: 'DirectDamage bytes differ', 9502: 'the allocator bytes differ',
                9503: 'the DamageSystem vtable differs', 9504: 'a fault (closed)', 9505: 'no game base'}


SFX_STATES = ('not started', 'starting', 'playing', 'no XAudio2', 'COM failed', 'engine failed',
              'no audio device', 'voice failed', 'thread failed', 'bad sound')


def parse_v005(raw):
    """Version 0.0.5: the V005 record (v005.hpp Data): HERO HEIGHT, the DAMAGE PROBE (read-only)
    and DAMAGE TEST (the candidate write, OFF by default)."""
    if not _header(raw, 'v005'):
        return None
    flags, = struct.unpack_from('<I', raw, 12)
    values = struct.unpack_from('<2I', raw, 16)
    k, = struct.unpack_from('<f', raw, 24)
    health_state, write_state, enemies_now, with_health = struct.unpack_from('<4I', raw, 32)
    (components, samples, enemies_read, falls, falls_melee, falls_ours, falls_unexplained, knockouts,
     rises) = struct.unpack_from('<9Q', raw, 48)
    biggest, nearest_hp, nearest_most = struct.unpack_from('<3f', raw, 120)
    melee_presses, = struct.unpack_from('<Q', raw, 136)
    recent = []
    for i in range(8):
        eid, before, after, most, cause, delay, ko = struct.unpack_from('<Q3f3I', raw, 144 + 32 * i)
        if not eid:
            continue
        recent.append(dict(id=hex(eid), before=round(before, 1), after=round(after, 1), most=round(most, 1),
                           cause=_name(DAMAGE_CAUSES, cause), delay_ms=delay, knocked_out=bool(ko)))
    status, code = struct.unpack_from('<2I', raw, 400)
    queued, sent, not_slot, full, faults, refused_off, last_record = struct.unpack_from('<7Q', raw, 408)
    last_handle, = struct.unpack_from('<I', raw, 464)
    sfx_state, sfx_code = struct.unpack_from('<2I', raw, 472)
    asked, played, dropped, muted = struct.unpack_from('<4Q', raw, 480)
    gain, = struct.unpack_from('<f', raw, 512)
    values = values + struct.unpack_from('<4I', raw, 520)
    engages, fly_speed = struct.unpack_from('<If', raw, 536)
    names = [n for n, _ in V005_OPTIONS]
    return dict(flags=flags, values={names[i]: _name(V005_OPTIONS[i][1], v) for i, v in enumerate(values)},
                sfx=dict(state=_name(SFX_STATES, sfx_state), code=sfx_code, asked=asked, played=played,
                         dropped=dropped, muted=muted, gain=round(gain, 2)),
                supersonic_engages=engages, fly_speed=round(fly_speed, 1),
                hero_height_k=round(k, 2),
                probe=dict(health=_name(DAMAGE_STATES, health_state), write=_name(DAMAGE_STATES, write_state),
                           enemies_now=enemies_now, enemies_with_health=with_health, health_components=components,
                           samples=samples, enemies_read=enemies_read, falls=falls, falls_after_x=falls_melee,
                           falls_after_mod_blow=falls_ours, falls_unexplained=falls_unexplained,
                           knockouts=knockouts, rises=rises, biggest_fall=round(biggest, 1),
                           nearest_hp=round(nearest_hp, 1), nearest_most=round(nearest_most, 1),
                           x_presses=melee_presses, recent=recent),
                write=dict(status=_name(DAMAGE_WRITE, status), code=code, why=DAMAGE_CODES.get(code, str(code)),
                           queued=queued, sent=sent, not_actor_slot=not_slot, pool_full=full, faults=faults,
                           refused_off=refused_off, last_record=hex(last_record), last_handle=hex(last_handle)))


HEROES = ('SPIDER-MAN', 'IRON MAN', 'DAREDEVIL', 'HULK')
SONAR_STATES = ('off', 'on', 'could not start (the eye pass failed)')


def parse_v006(raw):
    """Version 0.0.6: the V006 record (v006.hpp Data): the hero now, DAREDEVIL's batons and punches,
    HULK's punches, IRON MAN's blows, DAREDEVIL SONAR and SUPERSONIC's reached speed."""
    if not _header(raw, 'v006'):
        return None
    flags, = struct.unpack_from('<I', raw, 12)
    values = struct.unpack_from('<3I', raw, 16)
    hero, = struct.unpack_from('<I', raw, 28)
    throws, hits, catches, refused, baton_blows = struct.unpack_from('<5Q', raw, 32)
    dd_blows, hulk_blows, misses, iron_blows = struct.unpack_from('<4Q', raw, 72)
    sonar, = struct.unpack_from('<I', raw, 104)
    sonic_last, sonic_best = struct.unpack_from('<2f', raw, 112)
    names = [n for n, _ in V006_OPTIONS]
    return dict(flags=flags, values={names[i]: _name(V006_OPTIONS[i][1], v) for i, v in enumerate(values)},
                hero=_name(HEROES, hero),
                batons=dict(throws=throws, hits=hits, catches=catches, refused=refused, blows_sent=baton_blows),
                daredevil_punch_blows=dd_blows, hulk_punch_blows=hulk_blows, punches_out_of_reach=misses,
                iron_man_blows=iron_blows, sonar=_name(SONAR_STATES, sonar),
                supersonic_reached=round(sonic_last, 1), supersonic_best=round(sonic_best, 1))


PARSERS = dict(air_watch=parse_air_watch, view_gates=parse_view_gates, combat=parse_combat, hand_crawl=parse_hand_crawl,
               health=parse_health, mirror=parse_mirror, world_trace=parse_world_trace, blood=parse_blood,
               perf=parse_perf, color=parse_color, fovea=parse_fovea, npc_fade=parse_npc_fade,
               combat38=parse_combat38, mirror_probe=parse_mirror_probe, wrist_muzzle=parse_wrist_muzzle,
               hero_switch=parse_hero_switch, v003=parse_v003, v005=parse_v005,
               v006=parse_v006)


def snapshot(game, xr, key):
    """One record as a dict; None when not exported or never steady."""
    export, _, _, size = RECORDS[key]
    if export not in xr:
        return None
    address = xr[export]
    for _ in range(8):
        raw = game.read(address, size)
        if len(raw) != size:
            return None
        data = PARSERS[key](raw)
        if data is not None and raw[size - 8:] == game.read(address + size - 8, 8):
            return data
    return None


def snapshots(game, xr):
    out = {}
    for key in RECORDS:
        try:
            out[key] = snapshot(game, xr, key)
        except (OSError, RuntimeError) as error:
            out[key] = dict(fault=str(error))
    return out


def console_lines(records):
    """Short lines for the session's end."""
    lines = []
    h = (records or {}).get('health')
    if h and 'status' in h:
        lines.append(f"Update 37 health: {h['status']} ({h['falls']} falls, {h['rises']} rises, "
                     f"{h['heal_presses']} heal presses, {h['heals_that_rose']} healed).")
    c = (records or {}).get('combat')
    if c and 'fired' in c:
        lines.append('Update 37 gestures: ' + ', '.join(f'{k} {v}' for k, v in c['fired'].items()) +
                     f"; punches {sum(c['punches'])}.")
    v = (records or {}).get('view_gates')
    if v and 'bloom_fx' in v:
        lines.append(f"Update 37 view gates: BLOOM FX {v['bloom_fx']} ({v['fx_writes']} writes), LOD FADE "
                     f"{v['lod_fade']} ({sum(v['fade_answered'])} answered; {v['fade_why']}), LOD REACH "
                     f"{v['lod_reach']}%, IMPOSTORS {'on' if v['impostors'] else 'off'}.")
    m = (records or {}).get('mirror')
    if m and m.get('used'):
        lines.append(f"Update 37 mirror: {m['frames']} frames, mean {m['mean_ms']} ms, peak {m['peak_ms']} ms.")
    w = (records or {}).get('world_trace')
    if w and 'status' in w:
        lines.append(f"Update 37 world log: {w['status']} (fault: {w['fault']}); {w['actors']} actors, "
                     f"{sum(w['counts'].values())} events, scan mean {w['mean_scan_ms']} ms every {w['interval_s']} s.")
    b = (records or {}).get('blood')
    if b and b.get('fired'):
        lines.append(f"Update 37 blood: {b['fired']} splashes ({b['plan_b']} plan B: punch near an enemy), "
                     f"mean {b['mean_ms']} ms.")
    p = (records or {}).get('perf')
    if p and 'sections' in p:
        o = p['overlay']
        lines.append(f"Update 37 perf: overlay {o['total_ms']} ms (world+holo {o['world_and_holo_ms']}, wheel/phone "
                     f"{o['wheel_phone_ms']}, draw {o['draw_ms']}); holo {p['holo_mean_ms']} ms (build "
                     f"{p['holo_build_ms']}, reuse {'ON' if p['holo_reuse'] else 'OFF'}: {p['holo_reused']} of "
                     f"{p['holo_built'] + p['holo_reused']} frames reused at {p['holo_replay_ms']} ms).")
        lines.append('Update 37 frame prepare: ' + ', '.join(f"{k} {v['mean_ms']}/{v['peak_ms']} ms"
                                                             for k, v in p['sections'].items()) + ' (mean/peak).')
        s = p['spike']
        if s['count']:
            lines.append(f"Update 37 slowest prepare: {s['ms']} ms at {s['at_seconds']} s, mostly {s['section']} "
                         f"({s['section_ms']} ms); {s['count']} frames over 100 ms.")
    col = (records or {}).get('color')
    if col and 'filter' in col:
        lines.append(f"Update 37.2 color: COLOR FX {'ON' if col['on'] else 'OFF'}, {col['filter']} {col['strength']}%, "
                     f"exposure {col['exposure_ev']:+.1f} EV, contrast {col['contrast']}%, saturation "
                     f"{col['saturation']}%, vibrance {col['vibrance']}%, warmth {col['warmth']}, tint {col['tint']}, "
                     f"vignette {col['vignette']}%; {col['graded_pairs']} eye pairs graded.")
        if col.get('filter') == '90s ANIMATED' or col.get('toon_fault', 'NONE') != 'NONE':
            lines.append(f"Update 38.5 90s ANIMATED: cartoon {col['cartoon']}%, outlines {'ON' if col['outlines'] else 'OFF'}, "
                         f"{col['bands']} bands; shader: {'fine' if col['toon_fault'] == 'NONE' else col['toon_fault']}.")
    f = (records or {}).get('fovea')
    if f and 'per_level' in f:
        lines.append(f"Update 37.2 FOVEATED (experimental): {f['level']} (inner {f['inner']}%, VRS tier {f['vrs_tier']}, "
                     f"fault: {f['fault']}); " + ', '.join(
                         f"{k} {v['avg_fps']} avg / {v['low_fps']} low / {v['high_fps']} high FPS, {v['gpu_ms']} ms GPU "
                         f"({v['seconds']} s)" for k, v in f['per_level'].items() if v['seconds']) + '.')
    n = (records or {}).get('npc_fade')
    if n and 'mode' in n:
        lines.append(f"Update 37.2 NPC NEAR FADE: {n['mode']} ({n['why']}); {n['near_episodes']} close passes "
                     f"({n['near_while_game']} with GAME, {n['near_while_fixed']} with OFF/SWITCH), nearest "
                     f"{n['nearest_m']} m.")
    c8 = (records or {}).get('combat38')
    if c8 and 'options' in c8:
        lines.append('Update 38 COMBAT: ' + ', '.join(f'{k} {v}' for k, v in c8['options'].items()) + '.')
        lost = {k: v for k, v in c8['rejects'].items() if k != 'sent' and v}
        lines.append(f"Update 38 punches: {c8['gesture_hits']} by gesture, {c8['contact_hits']} by contact, "
                     f"{c8['air_punches']} in the air, {c8['zips']} zips ({c8['zips_aimed']} aimed); "
                     f"not sent: " + (', '.join(f'{k} {v}' for k, v in lost.items()) or 'none') + '.')
        lines.append(f"Update 38 crawl cap {c8['crawl_caps']} times ({c8['crawl_held_off']} held off by a fight); "
                     f"web balls {c8['globs']}, assisted {c8['assists']}; ring error {c8['ring_error_cm']} cm "
                     f"(peak {c8['ring_error_peak_cm']}); holo opens waited {c8['holo_deferred']}, cancelled "
                     f"{c8['holo_cancelled']}; view levelled {c8['tilt_resets']}; enemies within 2 m: "
                     + ', '.join(f'{k} {v}' for k, v in c8['enemy_near'].items()) + '.')
        lines.append(f"Update 38 SpidyVR: {c8['lunges']} lunges ({c8['lunges_landed']} landed, {c8['lunges_missed']} "
                     f"missed), {c8['yanks']} yanks, lock changed {c8['lock_changes']} times; sight rays "
                     f"{c8['sight_rays']} ({c8['sight_blocked']} blocked, {c8['sight_unknown']} unknown so counted clear).")
        lines.append(f"Update 38.1: {c8['swing_rejects']} swings not sent (one each), {c8['lunge_held_off']} lunges "
                     f"held off (no visible locked target), lock kept {c8['lock_held']} times; holo zone swallowed "
                     f"{c8['holo_zone_swallowed']} presses, {c8['holo_taps']} taps, {c8['holo_look_opens']} palm-look "
                     f"opens; ring/holo hidden as stale {c8['ring_hidden_frames']} frames.")
        if 'holo_forearm_opens' in c8:
            lines.append(f"Update 38.2: holo forearm opens {c8['holo_forearm_opens']}; ring held {c8['ring_held_frames']} "
                         f"frames, {c8['ring_wrist_cm']} cm from the drawn wrist (peak {c8['ring_wrist_peak_cm']}); "
                         f"{c8['targeted_held_off']} attacks held off (no enemy in reach or locked); PUNCH ONLY taps "
                         f"{c8['melee_taps']} ({c8['melee_delayed']} spaced, {c8['melee_dropped']} dropped); gadgets "
                         f"recognised {c8['gadgets_recognised']}; grab webs {c8['grab_attached']} on an enemy "
                         f"({c8['grab_yanks']} yanked, {c8['grab_released']} let go); SMART assists "
                         f"{c8['smart_assists']}; BRUTAL glove coats {c8['blood_coats']}; mirror SUIT "
                         f"{c8['mirror_suit_frames']} frames ({c8['mirror_joints']} joints).")
        if 'melee_merged' in c8:
            asked = c8['melee_taps'] + c8['melee_dropped'] + c8['melee_merged']
            lines.append(f"Version 0.0.3 PUNCHES (the game's X from your fists; the old 'punch'/'strikes' blocks are "
                         f"Spider-Man 1's damage path, not ported, always 0): {asked} blows asked, {c8['melee_taps']} "
                         f"sent as X, {c8['melee_merged']} rode a waiting tap, {c8['melee_dropped']} dropped; "
                         f"{c8['targeted_held_off']} swings held off ({c8['targeted_held_off_far']} with an enemy "
                         f"past the 3 m reach).")
        if 'look_switches' in c8:
            lines.append(f"Update 38.3: look-lock switches {c8['look_switches']}; strike steers {c8['strike_steers']}; "
                         f"grab assists {c8['grab_assisted']}; yank gestures {c8['yank_gestures']}; gadget hints "
                         f"{c8['gadget_hints']} ({c8['sonic_hints']} sonic); far windows {c8['window_calls']} calls, "
                         f"{c8['window_writes']} writes ({c8['window_game_m']} m game, {c8['window_m']} m now); suit "
                         f"{c8['suit_samples']} samples, {c8['suit_snapshots']} copies, learned {c8['suit_learned']} "
                         f"({c8['suit_primary']} / {c8['suit_secondary']} / {c8['suit_emblem']}).")
    mp = (records or {}).get('mirror_probe')
    if mp and 'status' in mp:
        sl, vl, h = mp['slots'], mp['view_list'], mp['hero']
        lines.append(f"Update 38.4 MIRROR PROBE: {mp['status']}; slots used {sl['used']} (peak {sl['used_max']}, "
                     f"ours {sl['ours']}, fewest free {sl['free_min']}); view list peak {vl['listed_max']} of "
                     f"{vl['smallest_cap']} (headroom {vl['headroom_min']}); hero drawn in " +
                     ', '.join(f'{k} {v}' for k, v in mp['hero_draws'].items() if v) +
                     f" ({len(mp['pieces'])} pieces); hidden by the game {h['hidden_by_game']} frames "
                     f"({h['game_hides']} times); suit changes {h['model_changes']} "
                     f"({len(mp['suit']['fields'])} candidate fields); cost mean {mp['cost']['mean_us']} us.")
        pv = mp.get('preview')
        if pv:
            lines.append(f"Update 38.5 PROBE v2: slots at start {mp['start_slots_used']}, first events "
                         f"{len(mp['first_events'])}; preview frames {pv['frames']} ({pv['starts']} times, slot "
                         f"{pv['slot']}, {pv['size']}), menu frames {pv['menu_frames']}; body solves in the preview "
                         f"{pv['body_solves_in_preview']}; preview is {pv['ours']}.")
    wm = (records or {}).get('wrist_muzzle')
    if wm and 'hooked' in wm:
        shots = wm['shots_right'] + wm['shots_left']
        verdict = ('the candidate muzzle is not what the ball reads (balls stayed the game\'s)'
                   if shots and not wm['moved'] else 'balls moved to the wrist' if wm['moved'] else 'no balls fired')
        lines.append(f"Update 38.5 BALL WRIST: hook {wm['hooked']} (error {wm['start_error']}); balls "
                     f"{wm['shots_right']} right / {wm['shots_left']} left; web-shooter calls {wm['web_calls']}, "
                     f"moved {wm['moved']} (too far {wm['too_far']}, stale {wm['stale']}, outside a press "
                     f"{wm['no_shot']}); {verdict}.")
    hs = (records or {}).get('hero_switch')
    if hs and 'swaps' in hs:
        if not hs['swaps']:
            lines.append('Update 38.5 HERO SWITCH: no hero switch this session (switch in the game\'s phone to test it).')
        else:
            lines.append(f"Update 38.5 HERO SWITCH: {hs['swaps']} hero replacements (switch, respawn or load); VR view "
                         f"back after {hs['view_back']}, body back after {hs['body_back']}, game screen after "
                         f"{hs['screen_after']}; latest: no hero {hs['last_gap_ms']} ms, view {hs['last_view_ms']} ms, "
                         f"body {hs['last_body_ms']} ms (worst {hs['worst_view_ms']} / {hs['worst_body_ms']} ms).")
    aw = (records or {}).get('air_watch')
    if aw and 'y_sent' in aw:
        lines.append(f"Update 38.5 Y IN THE AIR: {aw['y_sent']} Y presses sent ({aw['y_in_air']} in the swing's flight); "
                     f"{aw['yank_inputs']} grab-web yanks on the WEB YANK input ({aw['yanks_in_air']} in the air); the "
                     f"game took the hero out of the swing's flight {aw['yields']} times, {aw['past_3s']} past 3 s (a "
                     f"glide's sign, CANDIDATE), longest {aw['longest_ms']} ms; the swing took him back from a move "
                     f"still going {aw['capped_60s']} times (60 s cap), by a trigger {aw['taken_back_by_trigger']}.")
    v3 = (records or {}).get('v003')
    if v3 and 'auto_perf' in v3:
        ap, rg, ws, sw, bp = v3['auto_perf'], v3['rope_grab'], v3['wall_stick'], v3['suit_watch'], v3['bot_probe']
        if v3['values']['auto_perf'] == 'ON' or ap['events']:
            lines.append(f"Version 0.0.3 AUTO PERFORMANCE: {ap['rung']} at {ap['mean_fps']} of {ap['target_fps']} FPS "
                         f"(headset {ap['headset_hz']} Hz); {ap['drops']} drops, {ap['raises']} raises, {ap['zone_jumps']} "
                         f"zone jumps, {ap['bans']} bans, {ap['restores']} restores, {ap['apply_refused']} refused; the "
                         f"knobs' FPS gains are CANDIDATE.")
            for e in ap['log']:
                lines.append(f"  AUTO PERF at {e['seconds']} s: {e['frm']} -> {e['to']} ({e['why']}, {e['fps']} FPS)")
        if v3['values']['rope_grab'] == 'ON' or rg['attaches']:
            lines.append(f"Version 0.0.3 ROPE GRAB: {rg['attaches']} tethers ({rg['both_on_one']} with both hands on one), "
                         f"{rg['no_target']} grips with no enemy in the cone; releases {rg['releases']}; {rg['yanks']} "
                         f"sent the game's yank (CANDIDATE); the enemy is not moved by the mod.")
        if v3['values']['wall_stick'] != 'OFF' or ws['impacts']:
            lines.append(f"Version 0.0.3 WALL STICK: armed {ws['arms']} times, {ws['impacts']} impacts ({ws['walls']} walls, "
                         f"{ws['floors']} floors), {ws['shown']} nets shown, {ws['unstuck']} got up (a mod net, not the "
                         f"game's takedown).")
        lines.append(f"Version 0.0.3 SUIT ID WATCH (hero + 0x8c less its flag bits, CANDIDATE): id "
                     f"{hex(sw['suit_id']) if sw['have'] else 'not read'}, "
                     f"{sw['changes']} changes, {sw['reads']} reads, {sw['failed']} failed; colours relearned "
                     f"{sw['colours_forgot']} times.")
        mt = v3.get('mirror_view_test')
        if mt and (mt['runs'] or mt['phase'] != 'off'):
            lines.append(f"VERSION 0.0.6 MIRROR VIEW TEST: phase {mt['phase']}"
                         f"{' (' + mt['why'] + ')' if mt['why'] else ''}, {mt['runs']} runs, {mt['seconds']} s, "
                         f"{mt['frames']} frames, {mt['base_ms']} -> {mt['test_ms']} ms; hero draws {mt['hero_draws']}, "
                         f"other draws {mt['other_draws']} (hero draws above 0 with phase done: a real mirror can be built).")
        if bp['on'] or bp['scans']:
            lines.append(f"Version 0.0.3 BOT STATE PROBE: {bp['scans']} scans, {bp['bot_states']} BotState objects, "
                         f"Flung seen {bp['flung_seen']}, WallStuck seen {bp['wall_stuck_seen']}, {len(bp['found'])} finds.")
    v5 = (records or {}).get('v005')
    if v5 and 'probe' in v5:
        pr, wr = v5['probe'], v5['write']
        lines.append(f"VERSION 0.0.5 HERO HEIGHT {v5['values']['hero_height']} (body x{v5['hero_height_k']}).")
        punch = ('CONFIRMED: enemy HP fell after X' if pr['health'] == 'CONFIRMED' else
                 'CANDIDATE: no HP fall after X yet' if pr['health'] == 'read' else 'CANDIDATE: no enemy HP read yet')
        lines.append(f"VERSION 0.0.5 DAMAGE PROBE (read-only): {punch}; {pr['enemies_with_health']} of "
                     f"{pr['enemies_now']} enemies near with HP ({pr['health_components']} health parts in the world); "
                     f"{pr['x_presses']} X presses, {pr['falls']} HP falls ({pr['falls_after_x']} after X, "
                     f"{pr['falls_after_mod_blow']} after the mod's blow, {pr['falls_unexplained']} other), "
                     f"{pr['knockouts']} knockouts, biggest fall {pr['biggest_fall']}.")
        for e in pr['recent']:
            lines.append(f"  HP {e['before']} -> {e['after']} of {e['most']}: {e['cause']} ({e['delay_ms']} ms)"
                         f"{', KNOCKED OUT' if e['knocked_out'] else ''}")
        state = 'CONFIRMED' if pr['write'] == 'CONFIRMED' else 'CANDIDATE'
        lines.append(f"VERSION 0.0.5 DAMAGE TEST {v5['values']['hero_damage']} ({state}): {wr['status']}"
                     f"{' - ' + wr['why'] if wr['why'] else ''}; {wr['queued']} queued, {wr['sent']} sent, "
                     f"{wr['not_actor_slot']} not an actor, {wr['pool_full']} pool full, {wr['faults']} faults.")
        sf = v5.get('sfx')
        if sf:
            lines.append(f"VERSION 0.0.5 SFX {sf['state'].upper()} (volume {v5['values']['sfx_volume']}): "
                         f"{sf['asked']} asked, {sf['played']} played, {sf['muted']} muted, {sf['dropped']} dropped"
                         f"{', code ' + hex(sf['code']) if sf['code'] else ''}.")
            lines.append(f"VERSION 0.0.5 PUNCH SENSITIVITY {v5['values']['punch_sense']}; FLY SUPERSONIC "
                         f"{v5['values']['supersonic']} {v5['values']['supersonic_speed']}: "
                         f"{v5['supersonic_engages']} engages, flying {v5['fly_speed']} m/s now.")
    return lines
