# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR (Ilya Mezerowsky).
"""Visual settings changed while the game runs, from a text file the player edits.

live.txt (in the project folder, next to tools) holds lines such as

    reach = 100
    monitor = 15

and is read again whenever it is saved. Only the settings the mod applies at once are here:

    reach    the far buildings' full-model distance, percent (50-400; -BuildingReach)
    monitor  the game window's left-eye refreshes a second (0-240, 0 every frame; -MonitorRate)
    aa       the mod's eye anti-aliasing: 1 FXAA, 0 off (-EyeAA)
    history  the eye history: this percent of each eye picture is the one before it, turned to the
             new view (0-95, 0 off; Update 21: was 0-80, and from 50 the old picture is held less
             tightly, so 80-95 is plainly heavy): fine detail that shimmers settles, at the price of a
             soft trail behind what moves fast (include/spidy/eye_history.hpp)
    web_width  the webs' thickness in cm, one width the whole way along (0.4-4, decimals allowed,
             default 1.3; include/spidy/web_visual.hpp)
    web_travel  the web shot's flight (Update 15): 1 the tip flies from the palm to the anchor
             (default), 0 the web is drawn whole at once. Only the picture: the swing attaches as before
    web_speed  that tip's speed in m/s (20-2000, default 180); a flight lasts 0.03-0.11 s either way
    hud      the wrist hologram (Update 14): 1 on, 0 off (default 1). Off hides it and frees the
             wrist-touch trigger; 1 also brings it back after a fault switched it off
    hud_hand the wrist the hologram stands on: left (default) or right. With left, bring the right hand
             within about 15 cm of the left wrist or forearm (anywhere from the hand to near the elbow) and
             pull the right trigger; right mirrors it. Update 20: F8 on the PC keyboard toggles it too
    hud_scale  the hologram's size, 0.5-2 (1 is about 18 x 22 cm), or a percent such as 120%
    sharpen  (Update 16) the headset images' contrast-adaptive sharpening, 0-200 (0 off, default: no
             pass at all; 1-100 CAS, Update 21's 101-200 a strong mode past CAS's top, with halos). Only the headset: the desktop window shows the unsharpened picture
    reticles (Update 16) the swing aim reticles: 1 shown (default), 0 hidden. Only the picture:
             aiming and swinging stay exactly the same
    hud_height  (Update 17) the hologram's lower edge over the wrist light, cm 0-25 (default 9)
    hud_text  (Update 17) the settings pages' text size, percent 75-175 (default 100); bigger text
             wraps onto more lines and the page scrolls
    hud_theme  (Update 17) the colours: spiderman (default, red/blue), contrast (high contrast),
             deutan (blue/orange, for red-green colour blindness, protan too), tritan (red/teal), mono or
             (Update 36) raimi ("2000s Raimi": the 2004 handheld game's orange/yellow pixel text on navy)
    hud_text_color  (Update 17) the text: theme (default, the theme's own), white, yellow or cyan
    web_style  (Update 18) how the webs look: messy (default since Update 20: a frayed silvery strand), modern
             (tight and clean), 90s (the old cartoon rope with its coils, the default until Update 20), symbiote
             (black tendrils) or energy (plasma fibres) (or 0-4). Since Update 24 all are twisted fibre strands
             with loose fibres near the eye.
             Only the look: swinging is the same. It shows on the mod's own overlay webs (launch with
             -OverlayWebs, or when the game's own web lines are not available); the game's own web lines
             keep their look
    web_color  (Update 18) the shade of the style chosen now, from that style's own list; each style
             keeps its own (web_color_90s, web_color_messy, web_color_modern, web_color_symbiote,
             web_color_energy):
               90s: cream (default), ivory, parchment, warm_grey
               messy: silver (default), pearl, smoke, frost
               modern: bright_white (default), platinum, ice, chrome
               symbiote: blue_sheen (default), purple_sheen, crimson_sheen, matte
               energy: blue (default), red, green, orange, purple, gold
             (spaces or hyphens instead of underscores, or the number in the list from 0, also work).
             A shade's name belongs to one style only, so web_color = gold sets ENERGY's
    music_volume  (Update 19) the hologram MUSIC page's loudness, percent 0-100 (default 70). Only the
             mod's music: the game's own sound is not changed
    music_visualizer  (Update 19) the picture that moves with the music: original (default, red/blue
             bars and a waveform), bars, fractal_bloom, tunnel or starburst (or 0-4)
    web_start  (Update 23) where the webs are drawn leaving the hand: wrist (default, the drawn suit's web
             shooter on the inside of the wrist) or palm (Update 20's spot, a little under the palm). Only
             the picture: where webs aim and stick is the same either way
    eye_debug  (Update 24) a test for far-away flicker: same (both eyes are shown the LEFT eye's picture,
             so the world looks flat but the two eyes can no longer disagree) or off (normal 3D, the
             default). Applied as soon as live.txt is saved; never kept for the next session
    address_finder  (Update 27) on starts a ~60 s read-only scan for missing shadow/AA data; off stops
             early. Candidates only (reports/address-finder-*.json|.txt). Never kept for the next session
    eye_lod  (Update 28) on gives the eyes the flat game's model detail and texture distances (the
             hologram's EYE LOD); off (the default) leaves the eye lens's own. Never kept for the next
             session; turns itself off for the session if the value it changes stops matching
    music_track  (Update 19) the song the MUSIC page starts from, 1 the first of the Music folder's songs
             in A-Z order (0 none, the default); it is only picked, never played by itself

The wrist hologram's pages (Update 15) set web_width, web_speed, aa, history, reach and monitor
(and, Update 16, sharpen and reticles; Update 17, hud_height, hud_scale, hud_text, hud_theme and
hud_text_color; Update 18, web_style and web_color_<style>; Update 19, music_volume, music_visualizer
and music_track) too: the launcher reads what they set back from the mod once a second (SpidyHudSettings) and writes
it into this file (only the lines naming those settings change; comments stay), so a value set on
the hologram is kept. Those (PERSISTED) are also applied when the next session starts, from the
live.txt there (Update 23: web_start too); the other settings in it still wait until it is saved again. The page's NEXT LAUNCH
eye supersample goes to next-launch.txt instead: it is never applied live, only read by the launcher
at the next start (when -Supersample is not given, i.e. is 100).

The others need a new session: the render scale sizes the eye buffers when the session starts,
and the eyes' reflections, shadows and temporal AA stay off in Spider-Man 2 either way.
"""
import pathlib

# name: (range, the export that applies it, what the console says)
SETTINGS = {
    'reach': ((50, 400), 'SpidyImpostorReach', 'building reach {}%'),
    'monitor': ((0, 240), 'SpidyMonitorRate', 'monitor rate {} a second'),
    'aa': ((0, 1), 'SpidyEyeAA', 'eye anti-aliasing {} (0 off, 1 FXAA)'),
    'history': ((0, 95), 'SpidyEyeHistory', 'eye history {}% (0 off; settles shimmer, 80-95 heavy: trails when moving)'),
    'web_width': ((0.4, 4), 'SpidyWebWidth', 'web width {} cm'),
    'web_travel': ((0, 1), 'SpidyWebTravel', 'web shot flight {} (0 drawn at once, 1 flies)'),
    'web_speed': ((20, 2000), 'SpidyWebSpeed', 'web shot speed {} m/s'),
    # Update 23 (include/spidy/tracking.hpp webStart)
    'web_start': ((0, 1), 'SpidyWebStart', 'webs drawn from the {} (0 wrist, 1 palm)'),
    # Update 24 (include/spidy/eye_debug.hpp): a diagnostic, never persisted
    'eye_debug': ((0, 1), 'SpidyEyeDebug',
                  "eye debug {} (off: normal 3D; same: both eyes show the left eye's picture, flat - a flicker test)"),
    # Update 27: ADDRESS FINDER, never persisted
    'address_finder': ((0, 1), 'SpidyAddressFinder',
                       'ADDRESS FINDER {} (off idle/stop; on ~60 s read-only scan, candidates only)'),
    # Update 28: EYE LOD (stereo_probe.cpp eyeLod), never persisted
    'eye_lod': ((0, 1), 'SpidyEyeLod',
                'EYE LOD {} (off: the eye lens default; on: the flat game\'s detail distances)'),
    # Update 32: shadow_camera (Update 31's SHADOW CAMERA test) retired: no visible effect.
    'hud': ((0, 1), 'SpidyWristHud', 'wrist hologram {} (0 off, 1 on)'),
    'hud_hand': ((0, 1), 'SpidyWristHudHand', 'wrist hologram on the {} wrist'),
    'hud_scale': ((0.5, 2), 'SpidyWristHudScale', 'wrist hologram size {}'),
    'sharpen': ((0, 200), 'SpidyEyeSharpen', 'headset sharpening {}% (0 off, 1-100 CAS, 101-200 strong)'),
    'reticles': ((0, 1), 'SpidyReticles', 'aim reticles {} (0 hidden, 1 shown)'),
    'hud_height': ((0, 25), 'SpidyWristHudHeight', 'wrist hologram {} cm over the wrist'),
    'hud_text': ((75, 175), 'SpidyWristHudText', 'wrist hologram text {}%'),
    'hud_theme': ((0, 5), 'SpidyWristHudTheme', 'wrist hologram theme {}'),
    'hud_text_color': ((0, 3), 'SpidyWristHudTextColor', 'wrist hologram text colour {}'),
    # Update 18 (include/spidy/web_visual.hpp web_style)
    'web_style': ((0, 4), 'SpidyWebStyle', 'web style {}'),
    'web_color': ((0, 5), 'SpidyWebColor', 'web colour {}'),
    # Update 19 (include/spidy/music_player.hpp, music_audio.hpp): the MUSIC page
    'music_volume': ((0, 100), 'SpidyMusicVolume', 'music volume {}%'),
    'music_visualizer': ((0, 4), 'SpidyMusicVisualizer', 'music visualizer {}'),
    'music_track': ((0, 65535), 'SpidyMusicTrack', 'music song {} (0 none)'),
    # Update 34.2: EYE AA's history kept at swing speed (eye_history::movedWeight), live
    'trail': ((0, 100), 'SpidyEyeTrail', 'EYE AA trail {}% at swing speed (0 none: sharpest motion, 100 full blend)'),
    # Update 34.2: ENGINE TAA's jitter (engine_taa::JitterMode) and its scale, live, never persisted
    'taa_jitter': ((0, 3), 'SpidyEngineTaaJitter', 'ENGINE TAA jitter {} (0 ours, 1 game camera, 2 pixels, 3 none)'),
    'taa_jitter_scale': ((0, 200), 'SpidyEngineTaaJitterScale', 'ENGINE TAA jitter scale {}% (100 as made)'),
    # Update 35: FAR SHADOWS (include/spidy/far_shadows.hpp), live, never persisted
    'far_shadows': ((0, 2), 'SpidyFarShadows',
                    'FAR SHADOWS {} (0 off, 1 eyes: far cascades for both eyes, 2 eyes+cache: plan B)'),
    # Update 36: EYE BLOOM (include/spidy/eye_bloom.hpp), live, never persisted
    'eye_bloom': ((0, 2), 'SpidyEyeBloom', "EYE BLOOM {} (0 off, 1 copy: the game view's bloom, 2 gate: plan B)"),
    # Update 36: SHARED LOD (include/spidy/shared_lod.hpp), live, never persisted
    'shared_lod': ((0, 2), 'SpidySharedLod',
                   "SHARED LOD {} (0 off, 1 game view's detail scale for the eyes, 2 higher: plan B)"),
    # ---- Update 37 (every one live unless it says RESTART; one sentence each) ----
    # The eyes' view gates (include/spidy/view_gates.hpp): SpidyViewGate(selector << 16 | value).
    'bloom_fx': ((0, 2), 'SpidyViewGate', 'BLOOM FX {} (0 off, 1 lens: removes the head-locked lens effects, 2 lens+: plan B)'),
    'lod_fade': ((0, 2), 'SpidyViewGate', 'LOD FADE {} (0 off: trees pop, 1 left eye fades them in, 2 both: plan B)'),
    'lod_reach': ((100, 300), 'SpidyViewGate', "LOD REACH {}% (the eyes keep full detail this much farther)"),
    'impostors': ((0, 1), 'SpidyViewGate', 'IMPOSTORS {} (1 far buildings as full models for the eyes, 0 the game\'s stand-ins)'),
    # RESTART: the eyes' occlusion is made when VR starts (no export: kept for the next start).
    'eye_occlusion': ((0, 1), None, 'EYE OCCLUSION {} (1 the eyes cull hidden objects, 0 off) - RESTART NOTICE: '
                      'takes effect the next time VR starts'),
    'history_cut': ((25, 400), 'SpidyEyeHistoryCut', 'HISTORY CUT {}% (lower drops old history sooner when you move: fewer trails; higher keeps it longer: smoother, more trail)'),
    'aa_quality': ((0, 1), 'SpidyEyeAAQuality', 'AA QUALITY {} (0 FXAA, 1 strong FXAA: smoother edges, a little softer)'),
    # The ring on the wrist (health_ring::Place).
    'ring_face': ((0, 1), 'SpidyRingFace', 'RING FACE {} (1 flat on the web shooter, fading as it turns away; 0 faces your eyes)'),
    'ring_offset': ((0, 150), 'SpidyRingOffset', 'RING OFFSET {} mm (how far the ring sits out from the wrist)'),
    'ring_tilt': ((-60, 60), 'SpidyRingTilt', 'RING TILT {} degrees (tips the flat ring toward or away from you)'),
    # Gadgets by gesture, fired through the game's own gadget input (combat_input.hpp).
    'combat': ((0, 1), 'SpidyCombat', 'GADGET GESTURES {} (0 off: the triggers only swing and shoot webs)'),
    'tap_ms': ((100, 800), 'SpidyCombatTapMs', 'TAP TIME {} ms (a trigger pull shorter than this is a tap: the web ball)'),
    'gesture_ms': ((300, 3000), 'SpidyCombatGestureMs', 'GESTURE TIME {} ms (how long a held trigger waits for a gadget move)'),
    'grabber_slot': ((0, 4), 'SpidyGrabberSlot', 'WEB GRABBER button {} (0 off, 1 RB+A, 2 RB+B, 3 RB+X, 4 RB+Y; candidate)'),
    'sonic_slot': ((0, 4), 'SpidySonicSlot', 'SONIC BURST button {} (0 off, 1 RB+A, 2 RB+B, 3 RB+X, 4 RB+Y; candidate)'),
    'upshot_slot': ((0, 4), 'SpidyUpshotSlot', 'UPSHOT button {} (0 off, 1 RB+A, 2 RB+B, 3 RB+X, 4 RB+Y; candidate)'),
    'ricochet_slot': ((0, 4), 'SpidyRicochetSlot', 'RICOCHET WEB button {} (0 off, 1 RB+A, 2 RB+B, 3 RB+X, 4 RB+Y; candidate)'),
    'yank_speed': ((50, 500), 'SpidyYankSpeed', 'YANK SPEED {} cm/s (how fast the hand pulls back to fire the Web Grabber)'),
    'upshot_speed': ((50, 500), 'SpidyUpshotSpeed', 'UPSHOT SPEED {} cm/s (how fast the upward swing must be)'),
    'upshot_low': ((0, 60), 'SpidyUpshotLow', 'UPSHOT LOW {} degrees (how far below level the wrist must aim first)'),
    'flick_speed': ((50, 500), 'SpidyFlickSpeed', 'FLICK SPEED {} cm/s (the forward wrist flick for the Ricochet Web)'),
    'sonic_gap': ((5, 60), 'SpidySonicGap', 'SONIC GAP {} cm (how close the hands must be for the Sonic Burst)'),
    'sonic_reach': ((5, 70), 'SpidySonicReach', 'SONIC REACH {} cm (how far forward the hands must be for the Sonic Burst)'),
    'punch': ((0, 1), 'SpidyPunch', 'HAND HITS {} (1 a fast hand is a punch: the game\'s attack, with a buzz)'),
    'punch_speed': ((100, 800), 'SpidyPunchSpeed', 'PUNCH SPEED {} cm/s (how fast a hand must move to punch)'),
    'aim_steer': ((0, 1), 'SpidyAimSteer', 'LOCK-ON {} (1 the wrist aim steers the game\'s lock-on; candidate)'),
    'aim_cone': ((0, 60), 'SpidyAimCone', 'AIM CONE {} degrees (how far off the wrist may point and still lock)'),
    'lock_stick': ((0, 45), 'SpidyLockStick', 'LOCK STICK {} degrees (how far the aim must move to switch target)'),
    'looseness': ((50, 200), 'SpidyLooseness', 'LOOSENESS {}% (every gesture threshold at once: higher is more forgiving)'),
    # Web strands (web_visual.hpp).
    'web_physics': ((0, 1), 'SpidyWebPhysics', 'WEB PHYSICS {} (1 strands fly, wiggle and bend; 0 straight lines)'),
    'web_wiggle': ((0, 300), 'SpidyWebWiggle', 'WEB WIGGLE {}% (how much a strand ripples as it flies)'),
    'web_bend': ((0, 300), 'SpidyWebBend', 'WEB BEND {}% (how much a strand sags and bows when you move)'),
    # Heal and walls.
    'heal': ((0, 1), 'SpidyHeal', "HEAL {} (1 right stick down heals; also holds the stick's up-zip back)"),
    'heal_hold_ms': ((50, 1000), 'SpidyHealHold', 'HEAL HOLD {} ms (how long the stick stays down before healing)'),
    'wall_crawl': ((0, 1), 'SpidyWallCrawl', "WALL CRAWL {} (1 the game's own crawl lays the body on walls, 0 the old stand-on-walls; candidate)"),
    'hand_climb': ((0, 1), 'SpidyHandClimb', 'HAND CLIMB {} (1 pull yourself along walls hand over hand; 0 the stick: plan B)'),
    'climb_pull': ((25, 300), 'SpidyClimbPull', 'CLIMB PULL {}% (how far a hand pull moves you)'),
    'climb_grip': ((0, 2), 'SpidyClimbGrip', 'CLIMB GRIP {} (0 grip, 1 trigger, 2 either holds the wall)'),
    # The MIRROR (vr_mirror.hpp; also on the hologram's VR SETTINGS page).
    'mirror': ((0, 1), 'SpidyMirror', 'MIRROR {} (1 your tracked head and hands reflected in front of you; 0 off costs nothing)'),
    'mirror_distance': ((50, 300), 'SpidyMirrorDistance', 'MIRROR DISTANCE {} cm (how far in front the mirror stands)'),
    'mirror_size': ((25, 200), 'SpidyMirrorSize', 'MIRROR SIZE {}% (100 life size)'),
    'mirror_spot': ((0, 2), 'SpidyMirrorSpot', 'MIRROR SPOT {} (0 left, 1 centre, 2 right of where it was placed)'),
    'mirror_anchor': ((0, 1), 'SpidyMirrorAnchor', 'MIRROR ANCHOR {} (0 playspace: travels with you, 1 world: pinned in the city)'),
    'mirror_style': ((0, 1), 'SpidyMirrorStyle', 'MIRROR STYLE {} (0 see-through figure, 1 panel with a dark backing: plan B)'),
    'mirror_recentre': ((0, 1), 'SpidyMirrorRecentre', 'MIRROR RECENTRE {} (change it, 0 or 1, to place the mirror in front of you again now)'),
    # Update 37: the WORLD LOG tracers (world_trace.hpp: read-only, silent, on by default) and BLOOD SPLATTER.
    'world_log': ((0, 1), 'SpidyWorldLog', 'WORLD LOG {} (1 quietly notes NPC talk, spawns and missions into the reports; 0 stops it)'),
    'npc_log': ((0, 1), 'SpidyWorldLogKind', 'NPC LOG {} (1 notes who spoke near you and whether you looked at them)'),
    'spawn_log': ((0, 1), 'SpidyWorldLogKind', 'SPAWN LOG {} (1 notes what appeared and vanished around you)'),
    'mission_log': ((0, 1), 'SpidyWorldLogKind', 'MISSION LOG {} (1 notes missions and objectives starting and stopping)'),
    'blood_splatter': ((0, 200), 'SpidyBloodSplatter', 'BLOOD SPLATTER {}% (0 off costs nothing; 100 a short splash where your punch meets a near enemy; 150+ BRUTAL; 200 EXTREME)'),
    'blood_visor': ((0, 1), 'SpidyBloodVisor', 'VISOR DROPS {} (EXTREME only: 1 a few drops land on your visor after a close hit and run off; 0 none, the default)'),
    # Update 38.5 EXTRAS (extras385.hpp), each 0 by default; the cheats start OFF every session.
    'invincible': ((0, 1), 'SpidyInvincible', 'INVINCIBLE {} (1 keeps your health topped up; one-shot and scripted deaths still happen)'),
    'fly_mode': ((0, 3), 'SpidyFlyMode', 'FLY MODE {} (0 off, 1 slow, 2 normal, 3 fast: free flight in the air, jump to take off)'),
    'fly_vignette': ((0, 1), 'SpidyFlyVignette', 'FLY VIGNETTE {} (1 darkens the edges of your view while you fly fast, for comfort)'),
    'fly_roll': ((0, 2), 'SpidyFlyRoll', 'FLY ROLL {} (0 off, 1 comfort: tilts up to 45 degrees with a dark edge and a horizon line, 2 full roll)'),
    'fly_roll_speed': ((30, 180), 'SpidyFlyRollSpeed', 'FLY ROLL SPEED {} (degrees a second the right stick rolls you while flying)'),
    'fly_auto_level': ((0, 1), 'SpidyFlyAutoLevel', 'FLY AUTO LEVEL {} (1 levels the roll back when you let go of the stick)'),
    'symbiote_look': ((0, 1), 'SpidySymbioteLook', 'SYMBIOTE LOOK {} (1 black goo hands, symbiote webs and a black mirror; not the game suit)'),
    'prompt_handling': ((0, 1), 'SpidyPromptHandling', 'PROMPT HANDLING {} (0 manual PAD MODE; 1 auto, which needs the prompts hooked: not yet in SM2)'),
    'gesture_moves': ((0, 2), 'SpidyGestureMoves', 'GESTURE MOVES {} (0 off; 1 TRAINING draws the picked move path in the air (TRAIN MOVE) and names each move, sends nothing; 2 ON sends abilities, parry and wings)'),
    # Update 37 performance pass (perf37.hpp).
    'holo_reuse': ((0, 1), 'SpidyHoloReuse', 'HOLO REUSE {} (1 redraws the hologram every other frame while it sits still, to save CPU; 0 every frame)'),
    # ---- Update 37.2 (live; one sentence each) ----
    'eye_temporal': ((0, 1), 'SpidyEyeAaStart', 'EYE AA AT START {} (1 EYE AA goes on when the game starts, 0 it starts off)'),
    'wall_body': ((0, 1), 'SpidyWallBody', 'WALL BODY {} (1 your drawn body lies against a wall instead of standing out from it)'),
    'zip_strike': ((0, 1), 'SpidyZipStrike', 'ZIP STRIKE {} (1 a punch at an enemy 5-10 m away zips you to them)'),
    'web_yank': ((0, 1), 'SpidyWebYank', 'WEB YANK {} (1, the 38.3 default: with an enemy webbed by GRAB WEB, a pull back yanks him; 0 off)'),
    'yank_slot': ((0, 7), 'SpidyYankSlot', 'WEB YANK button {} (0 off: a grab-web pull sends the Grabber input as in 38.4, 1 RB+A, 2 RB+B, 3 RB+X, 4 RB+Y, 5 RB hold, 6 Y alone, 7 Y hold: the default, the game\'s hold Triangle)'),
    'npc_fade': ((0, 2), 'SpidyNpcFade', "NPC NEAR FADE {} (0 game: people near you fade as on the flat screen, 1 off: they stay, 2 switch only; candidate)"),
    # COLOR / ATMOSPHERE (eye_color.hpp; the hologram's COLOR page), both eyes alike, default OFF.
    'color_fx': ((0, 1), 'SpidyColorFx', 'COLOR FX {} (1 the colour grade below is drawn on both eyes; 0 the game\'s own colours)'),
    'color_filter': ((0, 6), 'SpidyColorFilter', 'FILTER {} (a look: none, vivid, golden, neon, raimi, noir or animated, the 90s cartoon)'),
    # Update 38.5: 90s ANIMATED's own (eye_color.hpp toon), live; nothing under another filter.
    'color_cartoon': ((0, 100), 'SpidyColorCartoon', 'CARTOON STRENGTH {}% (90s ANIMATED: how flat and painted it looks, and how bold the ink lines are)'),
    'color_outlines': ((0, 1), 'SpidyColorOutlines', 'OUTLINES {} (90s ANIMATED: 1 dark ink lines around shapes, the same in both eyes; 0 none)'),
    'color_bands': ((3, 5), 'SpidyColorBands', 'BANDS {} (90s ANIMATED: how many flat shades each colour gets; 3 the most cartoony)'),
    'color_strength': ((0, 100), 'SpidyColorStrength', 'STRENGTH {}% (how much of the filter is mixed in)'),
    'exposure': ((-2, 2), 'SpidyExposure', 'EXPOSURE {} EV (brighter or darker before contrast and saturation; 0 the game\'s)'),
    'contrast': ((50, 150), 'SpidyContrast', 'CONTRAST {}% (100 the game\'s; higher deeper shadows and brighter highlights)'),
    'saturation': ((0, 200), 'SpidySaturation', 'SATURATION {}% (100 the game\'s; 0 black and white)'),
    'vibrance': ((0, 100), 'SpidyVibrance', 'VIBRANCE {}% (lifts dull colours more than strong ones, sparing skin tones)'),
    'warmth': ((-100, 100), 'SpidyWarmth', 'WARMTH {} (above 0 warmer orange, below 0 cooler blue)'),
    'tint': ((-100, 100), 'SpidyTint', 'TINT {} (above 0 toward magenta, below 0 toward green)'),
    'vignette': ((0, 100), 'SpidyVignette', 'VIGNETTE {}% (darkens the lens edges; 0 off, fixed to the lens)'),
    # FOVEATED (EXPERIMENTAL, foveation.hpp): D3D12 VRS tier 2 on the eye passes, default OFF.
    'foveated': ((0, 3), 'SpidyFoveated', 'FOVEATED {} (EXPERIMENTAL: off, light, balanced or aggressive lower-rate lens edges for FPS)'),
    'fovea_inner': ((50, 150), 'SpidyFoveaInner', 'INNER SIZE {}% (the full-detail middle of each lens; bigger is safer, smaller faster)'),
    'fovea_debug': ((0, 1), 'SpidyFoveaDebug', 'FOVEATED DEBUG {} (1 tints the zones green, yellow and red to check them)'),
    # ---- Update 38: the COMBAT settings (combat38.hpp; the hologram's VR SETTINGS page), all live,
    # each 0 the recommended fix and the others its plan B. ----
    'crawl_cap': ((0, 2), 'SpidyCrawlCap', 'CRAWL CAP {} (0 auto: crouch only under a ceiling below 1.6 m, never in a fight; 1 also on the crawl flag; 2 off)'),
    'zip_mode': ((0, 3), 'SpidyZipMode', 'ZIP MODE {} (0 SpidyVR: a fist thrown at the enemy you look at lunges, 1 both, 2 distance: an enemy 3-20 m away, 3 aimed: your wrist on an enemy)'),
    'aim_assist': ((0, 3), 'SpidyAimAssist', "BALL AIM {} (0 smart: the enemy nearest your aim by angle weighted by distance, at his chest, kept 0.35 s; 1 38.1's SpidyVR line; 2 within 20 degrees; 3 off)"),
    'ball_look': ((0, 3), 'SpidyBallLook', 'BALL LOOK {} (0 ours: one mod web ball per press from the pulled wrist, bent to the enemy by AIM STRENGTH, no game ball, Version 0.0.2; 1 wrist: the game\'s own ball from the wrist, 38.5\'s; 2 glob: that plus our white glob; 3 game only: the game\'s ball where the game puts it)'),
    'punch_mode': ((0, 3), 'SpidyPunchMode', 'PUNCH MODE {} (0 targeted: a real outward punch with an enemy in reach or locked only; 1 both as 38.1; 2 fast punches only; 3 a hand touching an enemy only)'),
    'ring_anchor': ((0, 2), 'SpidyRingAnchor', "RING ANCHOR {} (0 wrist: the health ring pinned to the drawn hand's web shooter; 1 hand: 38.1's tracked hand; 2 world: as 37.2 drew it)"),
    'holo_open': ((0, 3), 'SpidyHoloOpen', "HOLO OPEN {} (0 forearm: point at your other forearm and pull the trigger; 1 palm look: looking opens it; 2 click: Update 20's trigger click; 3 F8 only; the hand at the forearm never reaches the game)"),
    'wall_tilt': ((0, 1), 'SpidyWallTilt', 'WALL TILT {} (0 safe: the view levels on death or respawn and off a wall past 60 degrees; 1 off)'),
    'web_rope': ((0, 1), 'SpidyWebRope', 'WEB ROPE {} (0 the swing web a thick bending rope, 1 the 37.2 frayed fibre)'),
    # Update 38, the SpidyVR fold-in
    'lunge': ((0, 1), 'SpidyLunge', "LUNGE {} (0 game button: the game's own web strike, 1 SpidyVR move: the swing bridge flies you to him, EXPERIMENTAL)"),
    'ball_fire': ((0, 1), 'SpidyBallFire', 'BALL FIRE {} (0 press: the web ball the moment the trigger goes down, 1 release: on letting go, as 37.2)'),
    # Update 38.1 (run38/DIAGNOSIS.md): each fix 0, its plan B the others; all live.
    'aim_axis': ((0, 2), 'SpidyAimAxis', "AIM AXIS {} (0 hand -Y: balls, punches and blood aim along the hand like the swing webs; 1 the controller's aim pose; 2 38's grip -Z)"),
    'ring_layer': ((0, 2), 'SpidyRingLayer', "RING LAYER {} (0 hold: the ring and hologram never hidden, a bad pose keeps the last good one; 1 newest: 38.1, hidden in a stale picture (it flickered); 2 image: as 38)"),
    'lunge_from': ((0, 2), 'SpidyLungeFrom', 'LUNGE FROM {} (0 2.5 m: nearer swings punch; 1 1.3 m as 38; 2 4 m)'),
    'lunge_speed': ((0, 2), 'SpidyLungeSpeed', 'LUNGE SPEED {} (0 2.2 m/s; 1 2.6 as 38; 2 1.9)'),
    'lunge_aim': ((0, 1), 'SpidyLungeAim', 'LUNGE AIM {} (0 steady: only at a visible enemy you look at, held 0.5 s; 1 as 38)'),
    'punch_angle': ((0, 2), 'SpidyPunchAngle', 'PUNCH ANGLE {} (0 wide: cos 0.2; 1 cos 0.35 as 38; 2 any direction)'),
    'ball_lead': ((0, 1), 'SpidyBallLead', "BALL LEAD {} (0 on target: no target, the RB at once; 1 always: every shot's RB waits 30 ms)"),
    'blood_look': ((0, 2), 'SpidyBloodLook', 'BLOOD LOOK {} (0 brutal: 2.2 s, 3-8 cm dark drops, an 8 s floor pool and blood on your glove; 1 big as 38.1; 2 37 small; blood_splatter still sets how much, default off)'),
    # Update 38.2 (run381/DIAGNOSIS.md): each fix 0, its plan B the others; all live.
    'melee_anim': ((0, 2), 'SpidyMeleeAnim', "MELEE ANIM {} (0 every hit: each blow its own short X tap at once, stick neutral, 0.13 s apart at most (Version 0.0.4); 1 game: as 38.1, chained X may kick; 2 spaced: 0.0.3's punch only, at least 0.65 s apart, fast blows ride one tap)"),
    'gadget_keys': ((0, 2), 'SpidyGadgetKeys', 'GADGET KEYS {} (0 38.2: Grabber RB+B (CONFIRMED), Sonic RB+X, Upshot RB+Y, Ricochet RB+A (CANDIDATE); 1 game default: Grabber RB+X, Sonic RB+B; 2 custom: grabber_slot ... ricochet_slot)'),
    'grab_web': ((0, 2), 'SpidyGrabWeb', 'GRAB WEB {} (0 assist: a grip web within 12 degrees of an enemy sticks to him, pull back to yank; 1 exact: 38.2\'s ray hit; 2 off)'),
    'mirror_look': ((0, 1), 'SpidyMirrorLook', "MIRROR LOOK {} (0 suit: a Spider-Man on your solved body pose in colours read from the eye images; 1 figure: 37's stand-in)"),
    'gadget_feel': ((0, 1), 'SpidyGadgetFeel', 'GADGET FEEL {} (0 easy: simpler, looser gadget moves, any order, a buzz when one is recognised; 1 38.1 strict)'),
    # ---- Update 38.3 (combat38.hpp's lookLock ... gadgetHint, codes 5325-5328), each 0 the fix ----
    'look_lock': ((0, 1), 'SpidyLookLock', 'LOOK LOCK {} (0 quick: the lock moves to the enemy you look straight at for 0.15 s; 1 steady: 38.2\'s)'),
    'strike_steer': ((0, 1), 'SpidyStrikeSteer', 'STRIKE STEER {} (0 to lock: the zip strike and jump go to your locked enemy; 1 off: 38.2\'s)'),
    'far_windows': ((0, 3), 'SpidyFarWindowsOption', 'FAR WINDOWS {} (0 2x, 1 3x, 2 1.5x the eyes\' building detail reach so night windows light sooner; 3 game; costs GPU)'),
    'gadget_hint': ((0, 1), 'SpidyGadgetHint', 'GADGET HINT {} (0 on: the gadget\'s name at your wrist as it fires and a Sonic aim tip after a miss; 1 off)'),
    # ---- Version 0.0.2 (combat38.hpp's aimStrength, ballHit, codes 5329-5330), for BALL LOOK OURS ----
    'aim_strength': ((0, 4), 'SpidyAimStrength', 'AIM STRENGTH {} (0 strong: a 45 degree cone (70 near), the lock to 90, homing and lead (Version 0.0.4; was 28); 1 max: 65 degrees, homes hardest; 2 off: straight; 3 low; 4 normal)'),
    'ball_hit': ((0, 1), 'SpidyBallHit', 'BALL HIT {} (0 ours only: a hit buzzes and splashes; 1 +game web: also asks the game for its web ball at the enemy, EXPERIMENTAL)'),
    # Version 0.0.3
    'rope_grab': ((0, 1), 'SpidyRopeGrab', 'ROPE GRAB {} (0 off; 1 on: grip to rope an enemy 3-9 m away; he is not moved yet, EXPERIMENTAL)'),
    'rope_strength': ((0, 2), 'SpidyRopeStrength', 'ROPE STRENGTH {} (0 normal; 1 low; 2 strong)'),
    'wall_stick': ((0, 2), 'SpidyWallStick', 'WALL STICK {} (0 off; 1 low health only; 2 always: a web net where a thrown or punched enemy hits, EXPERIMENTAL)'),
    'auto_perf': ((0, 1), 'SpidyAutoPerf', 'AUTO PERFORMANCE {} (0 off: your values back exactly; 1 on: lowers live graphics when frames drop, EXPERIMENTAL)'),
    'perf_target': ((0, 4), 'SpidyPerfTarget', 'TARGET FPS {} (0 auto: the headset rate; 1 72; 2 80; 3 90; 4 120)'),
    'perf_floor': ((0, 2), 'SpidyPerfFloor', 'QUALITY FLOOR {} (0 medium; 1 high; 2 low)'),
    'perf_status': ((0, 1), 'SpidyPerfStatus', 'SHOW STATUS {} (0 on; 1 off)'),
    # Version 0.0.4
    'iron_man': ((0, 1), 'SpidyShooterType', 'IRON MAN ABILITIES {} (0 off: Spider-Man; 1 on: palm repulsor blasts and, both triggers held, a chest beam every 30 s; no web of any kind (no swing, zip, web shooter, gadgets, grab or rope); blasts and beam do real damage; one hero at a time, EXPERIMENTAL)'),
    'mirror_view': ((0, 1), 'SpidyMirrorView', 'MIRROR VIEW {} (0 off; 1 test: a small picture of you in your suit, low in your view, made when VR starts: RESTART to run it; needs BODY ON; it turns itself OFF on any problem, EXPERIMENTAL)'),
    # Version 0.0.5
    'hero_height': ((0, 4), 'SpidyHeroHeight', 'HERO HEIGHT {} (0 1.00x: your size; 1 1.15x; 2 1.30x; 3 1.45x; 4 1.60x: the VR body grows for a big skin like Hulk, live)'),
    'hero_damage': ((0, 1), 'SpidyHeroDamage', 'WEB BALL DAMAGE {} (0 off; 1 on: Spider-Man\'s web ball hits ask the game for damage directly; the heroes\' hits always do, EXPERIMENTAL)'),
    'punch_sense': ((0, 3), 'SpidyPunchSense', 'PUNCH SENSITIVITY {} (0 forgiving: slow, sloppy jabs count; 1 max; 2 0.0.4; 3 strict, live)'),
    'sfx_volume': ((0, 4), 'SpidySfxVolume', 'SFX VOLUME {} (0 100%; 1 75%; 2 50%; 3 25%; 4 off: the mod\'s own sounds, live)'),
    'supersonic': ((0, 1), 'SpidySupersonic', 'SUPERSONIC {} (0 on; 1 off: in FLY MODE, both hands out and the left stick forward is a sonic boom dash, live)'),
    'supersonic_speed': ((0, 3), 'SpidySupersonicSpeed', 'SUPERSONIC SPEED {} (0 2x; 1 1.5x; 2 2.5x; 3 3x the FAST fly speed, live)'),
    # Version 0.0.6 (Update 39.0): one hero toggle at a time (turning one on turns the others off)
    'daredevil': ((0, 1), 'SpidyDaredevil', 'DAREDEVIL ABILITIES {} (0 off; 1 on: a baton in each hand, the trigger throws it (strong aim assist, bounces to 2 more enemies within 5 m, always comes back), punches hit DAREDEVIL PUNCH times harder, sonar colours; no web of any kind; one hero at a time, EXPERIMENTAL, live)'),
    'hulk': ((0, 1), 'SpidyHulk', 'HULK ABILITIES {} (0 off; 1 on: the charged jump (hold A) goes 4x as high and far, a punch knocks a normal enemy out, brutes take 2-3; no web of any kind; one hero at a time, EXPERIMENTAL, live)'),
    'daredevil_punch': ((0, 4), 'SpidyDaredevilPunch', 'DAREDEVIL PUNCH {} (0 3x; 1 1x; 2 2x; 3 4x; 4 5x a normal punch, live)'),
    # Version 0.0.62
    'hit_indicator': ((0, 1), 'SpidyHitIndicator', 'HIT INDICATOR {} (0 off; 1 on: a brief white X where your punches, batons, blasts and webs land, live)'),
    'hand_flips': ((0, 1), 'SpidyHandFlips', 'HAND FLIPS {} (0 on: hand swings add to REALISTIC FLIPS as in 0.0.6; 1 off: moving your hands never starts a flip, live)'),
    'lens_flares': ((0, 1), 'SpidyLensFlares', 'LENS FLARES {} (0 game: the game\'s own setting; 1 off in vr: the game\'s lens flares off while VR runs, put back after; RESTART)'),
    'punch_hits': ((0, 2), 'SpidyPunchHits', 'PUNCH HITS {} (0 game + instant: a small blow lands the moment your fist touches, as SpidyVR, and the game\'s move still plays; 1 game move: the X only, as 0.0.6; 2 instant only: no game move, live)'),
    'beacons_vision': ((0, 1), 'SpidyBeaconsVision', 'BEACONS IN VISION {} (0 on: DAREDEVIL\'s sonar keeps bright coloured beacons and markers in colour, outlined and pulsing; 1 off: 0.0.6\'s sonar, live)'),
    'foe_track': ((0, 1), 'SpidyFoeTrack', 'ENEMY TRACKING {} (0 live: enemies by their world positions, 30 a second; 1 0.0.6: by their offset from you, 10 a second, live)'),
    # Version 0.0.7 HERO PROFILES (one hero toggle at a time with the 0.0.6 ones; each off by default)
    'batman': ((0, 1), 'SpidyBatman', 'BATMAN ABILITIES {} (0 off; 1 on: hold a trigger and a batarang appears in that hand, let go to throw it on a slight curve (strong aim assist, BATARANG CHAIN rolls to bounce on, 3 enemies at most, always comes back), the trigger aimed above level with no enemy in your sights fires the grapple gun up to a rooftop or ledge, both arms out in the air glide on the cape (dip an arm to turn, tuck to dive, raise them to pull up, drop them to let go), grip at the front of the belt for a smoke pellet and let go to throw it (it bursts into smoke, a look only); no web of any kind and no flight; one hero at a time, EXPERIMENTAL, live)'),
    'wolverine': ((0, 1), 'SpidyWolverine', 'WOLVERINE ABILITIES {} (0 off; 1 on: hold the trigger for that hand\'s claws (their sweep hits), health climbs back 3 s after a hit (never a revive, never past the game\'s cap); no web of any kind; one hero at a time, EXPERIMENTAL, live)'),
    'superman': ((0, 1), 'SpidySuperman', 'SUPERMAN ABILITIES {} (0 off; 1 on: both triggers burn HEAT VISION along your gaze, with a hand at your mouth COLD BREATH (damage and a frosty look only: nobody freezes), punches hit harder, FLY MODE\'s SUPERSONIC is his speed; no web of any kind; one hero at a time, EXPERIMENTAL, live)'),
    'punisher': ((0, 1), 'SpidyPunisher', 'PUNISHER ABILITIES {} (0 off; 1 on: a pistol on each hip and a sawed-off on your back, grip at the holster to draw, let go there to holster; each trigger fires its own gun; no web of any kind; one hero at a time, EXPERIMENTAL, live)'),
    'batarang_chain': ((0, 4), 'SpidyBatarangChain', 'BATARANG CHAIN {} (0 25%; 1 50%; 2 75%; 3 100%; 4 off: each batarang hit\'s chance to bounce on, 3 enemies at most, live)'),
    'easy_reload': ((0, 1), 'SpidyEasyReload', 'EASY RELOAD {} (0 auto: an empty gun refills by itself; 1 realistic: flick the gun down to drop the magazine (or open the sawed-off), touch your hip to reload, live)'),
    'holster_hands': ((0, 1), 'SpidyHolsterHands', 'HOLSTER HANDS {} (0 same side: each hand draws only from its own hip; 1 either hand: cross draws too; the back is either hand\'s, live)'),
    'baton_holsters': ((0, 1), 'SpidyBatonHolsters', 'BATON HOLSTERS {} (0.0.72: always on whatever the value - DAREDEVIL\'s batons worn on the back, grip over a shoulder to draw, let go there to holster; no trigger spawn, live)'),
    'baton_look': ((0, 2), 'SpidyBatonLook', 'BATON LOOK {} (0 classic: the old red bar, as 0.0.63; 1 club: the 0.0.7 billy club; 2 model: the two converted batons in Models\\Baton, CLUB when missing, live)'),
    'batarang_style': ((0, 3), 'SpidyBatarangStyle', 'BATARANG STYLE {} (0 default: the built-in batarang; 1 eira, 2 clooney, 3 joker: the model dropped in Models\\Batarang\\<STYLE>, DEFAULT when missing or broken, live)'),
    'gun_models': ((0, 1), 'SpidyGunModels', 'GUN MODELS {} (0 built-in: the drawn pistols and sawed-off; 1 models: the converted models in Models\\Pistol and Models\\SawedOff, built-in when missing, live)'),
    # Version 0.0.7 CAPTAIN AMERICA (a hero toggle; his shield's settings, each its first value by default)
    'captain_america': ((0, 1), 'SpidyCaptainAmerica', 'CAPTAIN AMERICA ABILITIES {} (0 off; 1 on: his shield on your back, grip over the SHIELD ARM\'s shoulder to draw it onto that forearm, the trigger throws it backhand (flat spin, ricochets, comes back to the hand), a swing of its rim bashes; no web of any kind; one hero at a time, EXPERIMENTAL, live)'),
    'shield_arm': ((0, 1), 'SpidyShieldArm', 'SHIELD ARM {} (0 right: worn on the right forearm, drawn by the right hand; 1 left, live)'),
    'shield_damage': ((0, 3), 'SpidyShieldDamage', 'SHIELD DAMAGE {} (0 35 hp; 1 20 hp; 2 50 hp; 3 70 hp: each thrown shield hit, live)'),
    'shield_chain': ((0, 1), 'SpidyShieldChain', 'SHIELD CHAIN {} (0 on: a thrown shield ricochets on to two more enemies like the batons, then comes back; 1 off: one enemy, then back, live)'),
    'shield_block': ((0, 1), 'SpidyShieldBlock', 'SHIELD BLOCK {} (0 off; 1 experimental: CANDIDATE, a hit taken while the shield on your arm faces the nearest enemy is given back, never a revive, never past the cap, live)'),
    'shield_look': ((0, 1), 'SpidyShieldLook', 'SHIELD LOOK {} (0 built-in: drawn rings and star; 1 model: the converted shield in Models\\Shield, built-in when missing, live)'),
    # Version 0.0.72 (TEST): THOR, GREEN LANTERN and the 0.0.72 list
    'thor': ((0, 1), 'SpidyThor', 'THOR ABILITIES {} (0 off; 1 on: the hammer on your back, grip over a shoulder to draw it, trigger throws it (it chains and flies back, each hit electrocutes), hold it straight up overhead 1 s for a lightning strike (a gesture, no trigger); no webs; one hero at a time, EXPERIMENTAL, live)'),
    'thor_lightning': ((0, 1), 'SpidyThorLightning', 'THOR LIGHTNING {} (0 on: lightning on his arms and hammer in a fight; 1 off, live)'),
    'thor_storm': ((0, 1), 'SpidyThorStorm', 'THOR STORM {} (0 on: the hammer held straight up overhead for 1 s calls a radial lightning strike, no trigger; 1 off, live)'),
    'thor_chain': ((0, 1), 'SpidyThorChain', 'HAMMER CHAIN {} (0 on: a thrown hammer jumps to two more enemies; 1 off: one, then home, live)'),
    'green_lantern': ((0, 1), 'SpidyGreenLantern', 'GREEN LANTERN ABILITIES {} (0 off; 1 on: ring flight, construct fist, ring beam, shield bubble, chain construct, ring charge; no webs; one hero at a time, EXPERIMENTAL, live)'),
    'gl_flight': ((0, 1), 'SpidyGlFlight', "RING FLIGHT {} (0 on: GREEN LANTERN takes off with both arms straight up overhead, then the hands steer (FLY MODE's poses), never the triggers, even with FLY MODE off; 1 off, live)"),
    'gl_fist': ((0, 1), 'SpidyGlFist', 'CONSTRUCT FIST {} (0 on: the right grip held and a fast punch throw a giant green fist; 1 off, live)'),
    'gl_beam': ((0, 1), 'SpidyGlBeam', "RING BEAM {} (0 on: the right trigger fires the ring's beam (a projectile); 1 off, live)"),
    'gl_bubble': ((0, 1), 'SpidyGlBubble', 'SHIELD BUBBLE {} (0 on: both grips held and the hands pulled apart raise a bubble that blocks hits, CANDIDATE; 1 off, live)'),
    'gl_chain': ((0, 1), 'SpidyGlChain', 'CHAIN CONSTRUCT {} (0 on: the left trigger throws a green chain (a projectile) that yanks the enemy it hits; 1 off, live)'),
    'gl_charge': ((0, 1), 'SpidyGlCharge', "RING CHARGE {} (0 on: the left wrist's ring charge drains with the powers (not flight) and refills at rest; 1 off: unlimited, live)"),
    'spider_climb_only': ((0, 1), 'SpidySpiderClimbOnly', 'SPIDER CLIMB ONLY {} (0 on: only SPIDER-MAN crawls walls; 1 off: every hero, as 0.0.7, live)'),
    'uppercut': ((0, 1), 'SpidyUppercut', 'UPPERCUT LAUNCH {} (0 on: a fast upward punch that lands launches the enemy; 1 off, live)'),
    'smoke_blind': ((0, 1), 'SpidySmokeBlind', "SMOKE BLIND {} (0 on: enemies in BATMAN's smoke stagger blind; 1 off, live)"),
    'dd_grapple': ((0, 1), 'SpidyDdGrapple', "BATON GRAPPLE {} (0 on: DAREDEVIL's grip draws a baton from the back, the trigger grapples along it, a throw with the grip let go throws it, no glide; 1 off: the trigger throws, live)"),
    'dd_parry': ((0, 1), 'SpidyDdParry', 'BATON PARRY {} (0 on: batons crossed in front of the face block hits, CANDIDATE; 1 off, live)'),
    'iron_hud': ((0, 1), 'SpidyIronHud', 'IRON MAN HUD {} (0 on: the helmet frame, a lock box, HOSTILES DETECTED, TARGET LOCKED, ALL HOSTILES NEUTRALIZED, hostiles left, speed and altitude flying; 1 off, live)'),
    'punisher_rage': ((0, 1), 'SpidyPunisherRage', 'PUNISHER RAGE {} (0 on: damage taken or downs in a fight turn the view red and punches harder; 1 off, live)'),
    'reputation': ((0, 1), 'SpidyReputation', "HERO REPUTATION {} (0 on: each hero's own rank on the wrist, up for downs, cleared fights and fires out, down for fights walked away from and defeats; 1 off, live)"),
    'survival': ((0, 1), 'SpidySurvival', 'SURVIVAL MODE {} (0 off; 1 on: hits can hurt a limb (SPIDER-MAN, BATMAN, PUNISHER, DAREDEVIL, CAPTAIN AMERICA), healing over time, live)'),
    'punisher_suit': ((0, 1), 'SpidyPunisherSuit', 'PUNISHER SUIT {} (0 vest: the VEST kit by floh; 1 warzone: the WARZONE kit by spybg; which PunisherSuit kit you installed with Overstrike, live)'),
    'natural_invincible': ((0, 1), 'SpidyNaturalInvincible', 'NATURAL INVINCIBLE {} (0 on: THOR and SUPERMAN take no damage (fall and slam too) and no SURVIVAL injuries, never a revive; 1 off, live)'),
    'hulk_rage': ((0, 1), 'SpidyHulkRage', 'HULK RAGE {} (0 on: health HULK loses in a fight builds his punch damage up to 2x, a green tint shows it, it fades after the fight; 1 off, live)'),
    'rage_blood': ((0, 2), 'SpidyRageBlood', "RAGE BLOOD {} (0 extreme: lots of mod-drawn blood from enemies shot in PUNISHER RAGE, capped, off by itself on a frame drop; 1 normal; 2 off, live)"),
    'rage_tint': ((0, 3), 'SpidyRageTint', "RAGE TINT {} (0 normal, 1 strong, 2 subtle, 3 off: PUNISHER RAGE's red view pulsing like a heartbeat at the edges, live)"),
    'fire_douse': ((0, 1), 'SpidyFireDouse', "FIRE DOUSE {} (0 on: each hero's own way to put fires out; the game's fire hook is CANDIDATE, so a look only for now; 1 off, live)"),
    'shield_slam': ((0, 1), 'SpidyShieldSlam', "SHIELD SLAM {} (0 on: CAP's shield driven down fast knocks back and hurts enemies near and douses fire; 1 off, live)"),
    'punisher_hook': ((0, 1), 'SpidyPunisherHook', "GRAPPLE HOOK {} (0 on: the PUNISHER's tactical hook launcher, drawn by the grip at the chest, fired by the trigger, slower than BATMAN's; 1 off, live)"),
    'claw_climb': ((0, 1), 'SpidyClawClimb', "CLAW CLIMB {} (0 off, the default: only SPIDER-MAN climbs walls; 1 on: WOLVERINE with claws out climbs walls by pulling with his hands, grip only; live)"),
    'npc_shove': ((0, 1), 'SpidyNpcShove', "SHOVE {} (0 on: both open hands pushed fast at a close enemy knock him back; grabbing people is CANDIDATE, not in; 1 off, live)"),
    'faction': ((0, 4), 'SpidyFaction', "ENEMY FACTION {} (0 auto: the enemies match the hero, Batman the joker gang; 1 vanilla, 2 the hand, 3 hydra, 4 joker gang (no model yet: vanilla); the game-side swap is CANDIDATE, so shown only; live)"),
    'hero_suit': ((0, 1), 'SpidyHeroSuit', "HERO SUIT {} (0 the hero's default suit, 1 his next unlocked one; suits unlock with that hero's reputation, live)"),
    'test_unlocks': ((0, 3), 'SpidyTestUnlocks', "TEST UNLOCKS {} (0 off: suits, ultimates and perks come with each hero's reputation, kept for good; 1 suits, 2 ultimates and suits, 3 all and perks, for testing; live)"),
    'ultimates': ((0, 1), 'SpidyUltimates', "ULTIMATES {} (0 on: at max reputation both fists crossed on the chest with both grips fire the hero's ultimate, long cooldown; 1 off, live)"),
    'perk_branch': ((0, 3), 'SpidyPerkBranch', "HERO PERKS {} (0 auto: the tree in order; 1 chain first; 2 hold points; 3 perks off; live)"),
    'own_suit': ((0, 1), 'SpidyOwnSuit', "USE MY OWN OR GAME SUIT {} (0 off: the picked HERO SUIT; 1 on: the fallback for a model glitch or crash, the game's or your own mod's suit; live)"),
    'perch_takedown': ((0, 1), 'SpidyPerchTakedown', "PERCH TAKEDOWN {} (0 on: BATMAN quick-presses the game's takedown prompt button from a perch or a glide to hang the enemy by a rope; 1 off; live)"),
    'takedown_light': ((0, 1), 'SpidyTakedownLight', "TAKEDOWN LIGHT {} (0 on: GREEN over the target when no one sees it, RED when others would be alerted; 1 off; live)"),
    'punisher_van': ((0, 1), 'SpidyPunisherVan', "PUNISHER VAN (EXPERIMENTAL) {} (0 on, the default; 1 off. On: the PUNISHER taps X to summon his van near him on the street, grips the driver door handle to get in or out, grips the steering wheel's rim to steer; only a hand on the wheel works the pedals - its trigger drives, its trigger with its own stick back reverses, both hands on: RT drives, LT reverses, both brake; a van that cannot drive stays parked; live)"),
    'game_body': ((0, 1), 'SpidyGameBody', "GAME BODY (EXPERIMENTAL) {} (0 off: the body as before; 1 on: the game's own body only while every safety check passes, else hidden with gloves; live)"),
    'thor_hammer_model': ((0, 1), 'SpidyThorHammerModel', "THOR HAMMER MODEL {} (0 on: Thor draws the THOR HAMMER model; 1 off: no hammer model drawn (the hammer still works), a crash bisect key, live)"),
    'suit_body': ((0, 2), 'SpidySuitBody', "SUIT BODY {} (0 arms: your picked suit model's arms in first person; 1 full_body: everything below the neck; 2 off: none in first person - the mirror always shows the whole suit; live)"),
    'cap_sprint': ((0, 1), 'SpidyCapSprint', "CAP SPRINT {} (0 on: Captain America pumps his arms and pushes the left stick to super-sprint on the ground; 1 off: normal running; live)"),  # Version 0.0.73
    'cull_margin': ((0, 3), 'SpidyCullMargin', "CULL MARGIN {} (0 10deg: the game's view reaches 10 degrees past your eyes so far windows and street lights stay lit; 1 6deg as build 12; 2 15deg; 3 20deg; live)"),  # Version 0.0.73 test 2
    'shadow_match': ((0, 1), 'SpidyShadowMatch', "SHADOW MATCH {} (0 on, the default since 0.0.73 test 3: SpidyVR's and the Miles VR mod's shadow fix, both eyes' sun shadows fitted to one lens so they agree, CANDIDATE; 1 off as 0.0.73 test 2; live)"),  # Version 0.0.73 test 2
    'calib_look': ((0, 1), 'SpidyCalibLook', "calibration panel: holo checklist or classic"),  # Version 0.0.73 test 2
    'fly_pose': ((0, 1), 'SpidyFlyPose', "flying body: lean with speed or stay upright"),  # Version 0.0.73 test 2
    'weapon_collision': ((0, 1), 'SpidyWeaponCollision', "WEAPON COLLISION {} (0 on: thrown shields, batons, batarangs and the hammer bounce or stop off walls, cars and the ground, held ones are drawn short of them; 1 off: 0.0.73 test 2's flight; live)"),  # Version 0.0.73 test 3
    'light_hold': ((0, 2), 'SpidyLightHold', "LIGHT HOLD {} (0 on: steadier street lights and small bright lights - EYE AA's sub-pixel step no larger than the history can average, bright history held a little; 1 strong: no step, full hold; 2 off: 0.0.73 test 2; live)"),  # Version 0.0.73 test 3
    'van_screen': ((0, 1), 'SpidyVanScreen', "the Punisher van's dash screen: on or off"),  # Version 0.0.73 test 3
    'palm_blast': ((0, 1), 'SpidyPalmBlast', "PALM BLAST {} (0 palm: IRON MAN's blast leaves the open palm straight out of it; 1 aim: along the hand's aim, 0.0.73 fix 15; live)"),  # Version 0.0.73 test 3
    'thor_flight': ((0, 1), 'SpidyThorFlight', "THOR FLIGHT {} (0 on: Thor takes off with both arms up and flies; 1 off: no flight for Thor, a crash bisect key, live)"),
    'thor_hammer': ((0, 3), 'SpidyThorHammer', "THOR HAMMER {} (0 boooop: Boooooop's Mjolnir; 1 devilseye: TheDevilsEye's; 2 love_thunder: Jesus Osco's Love and Thunder; 3 fccooper: fccooper's; live)"),
}
# Update 37: the view gates share one export (SpidyViewGate): the selector in the high half.
VIEW_GATES = {'bloom_fx': 0, 'lod_fade': 1, 'lod_reach': 2, 'impostors': 3}
# Update 37: the three world tracers share SpidyWorldLogKind: the kind in the high half.
WORLD_LOG_KINDS = {'npc_log': 0, 'spawn_log': 1, 'mission_log': 2}
# Update 37: settings that cannot change while VR runs: kept for the next start, with this notice.
RESTART = {'eye_occlusion': 'RESTART NOTICE: eye_occlusion takes effect the next time VR starts.',
           # Version 0.0.62: the game reads its Lens Flares setting when it starts
           'lens_flares': 'RESTART NOTICE: lens_flares takes effect the next time VR starts.'}
# Update 37: the run36 closing settings, the defaults now (reach and monitor are the launcher's own
# -BuildingReach 150 and -MonitorRate 30 again in Update 37.2); applied at the start for any setting live.txt does not keep.
UPDATE37_DEFAULTS = {'history': 80, 'sharpen': 140, 'trail': 0, 'taa_jitter': 3, 'hud_theme': 'raimi',
                     'web_width': 1.8, 'web_speed': 70, 'reticles': 0, 'aa': 1,
                     # Update 37.2: web_width 1.8 cm (SM1's thick rope); EYE AA, FAR SHADOWS EYES+CACHE, EYE BLOOM
                     # COPY and SHARED LOD HIGHER started on every start (run371 started them OFF or low); the
                     # Update 36 values back (bloom_fx off, aim_steer off, the ring facing the eyes, wall_crawl
                     # the swing's wall); the medium web bend; the new 37.2 settings' defaults.
                     'eye_temporal': 1, 'far_shadows': 2, 'eye_bloom': 1, 'shared_lod': 2, 'bloom_fx': 0,
                     'aim_steer': 0, 'ring_face': 0, 'wall_crawl': 0, 'web_bend': 100, 'wall_body': 0,
                     'zip_strike': 1, 'web_yank': 1, 'npc_fade': 'off', 'color_fx': 0, 'foveated': 'off',
                     'lod_fade': 0}  # Build 12 WINDOW LIGHTS: LOD FADE OFF
# Update 38: the COMBAT defaults, each the recommended fix (combat38.hpp index 0), sent at the start for
# whatever live.txt does not keep. All live: none needs a restart.
UPDATE38_DEFAULTS = {'crawl_cap': 'auto', 'zip_mode': 'spidyvr', 'aim_assist': 'spidyvr', 'ball_look': 'ours',
                     'punch_mode': 'both', 'ring_anchor': 'hand', 'holo_open': 'tap', 'wall_tilt': 'safe',
                     'web_rope': 'rope', 'lunge': 'game_button', 'ball_fire': 'press',
                     # Update 38.1
                     'aim_axis': 'hand_-y', 'ring_layer': 'newest', 'lunge_from': '2.5_m', 'lunge_speed': '2.2',
                     'lunge_aim': 'steady', 'punch_angle': 'wide_.2', 'ball_lead': 'on_target', 'blood_look': 'brutal',
                     # Update 38.2 (the first value of each, the fix): aim_assist smart, punch_mode targeted,
                     # ring_anchor wrist, holo_open forearm, ring_layer hold, blood_look brutal
                     'melee_anim': 'every_hit', 'gadget_keys': '38.2', 'grab_web': 'assist', 'mirror_look': 'suit',
                     'gadget_feel': 'easy',
                     # Update 38.3
                     'look_lock': 'quick', 'strike_steer': 'to_lock', 'far_windows': '2x', 'gadget_hint': 'on',
                     # Version 0.0.2
                     'aim_strength': 'strong', 'ball_hit': 'ours_only',
                     # Version 0.0.3 (each experimental one off)
                     'rope_grab': 'off', 'rope_strength': 'normal', 'wall_stick': 'off', 'auto_perf': 'off',
                     'perf_target': 'auto', 'perf_floor': 'medium', 'perf_status': 'on',
                     'iron_man': 'off', 'mirror_view': 'off',
                     # Version 0.0.5
                     'hero_height': '1.00x', 'hero_damage': 'off',
                     'punch_sense': 'forgiving', 'sfx_volume': '100%', 'supersonic': 'on', 'supersonic_speed': '2x',
                     # Version 0.0.6 (the heroes experimental: off)
                     'daredevil': 'off', 'hulk': 'off', 'daredevil_punch': '3x',
                     # Version 0.0.62 (HIT INDICATOR off: the user's call; HAND FLIPS on and LENS FLARES
                     # game: 0.0.6's behaviour and the graphics defaults kept)
                     'hit_indicator': 'off', 'hand_flips': 'on', 'lens_flares': 'game',
                     # Version 0.0.62 (the SpidyVR comparison; 0.0.6's: game_move, 0.0.6)
                     'punch_hits': 'game_instant', 'foe_track': 'live',
                     'beacons_vision': 'on',  # Version 0.0.62 (the user's call)
                     # Version 0.0.7 (the heroes experimental: off; the rest as 0.0.62)
                     'batman': 'off', 'wolverine': 'off', 'superman': 'off', 'punisher': 'off',
                     'batarang_chain': '25%', 'easy_reload': 'auto', 'holster_hands': 'same_side',
                     'baton_holsters': 'on', 'baton_look': 'classic', 'batarang_style': 'default',
                     'gun_models': 'built-in',
                     # Version 0.0.7 CAPTAIN AMERICA (off; RIGHT arm (LEFT selectable), 35 HP, chain on, block off, built-in)
                     'captain_america': 'off', 'shield_arm': 'right', 'shield_damage': '35hp', 'shield_chain': 'on',
                     'shield_block': 'off', 'shield_look': 'built-in',
                     # Version 0.0.72 (TEST)
                     'thor': 'off', 'thor_lightning': 'on', 'thor_storm': 'on', 'thor_chain': 'on', 'green_lantern': 'off', 'gl_flight': 'on', 'gl_fist': 'on', 'gl_beam': 'on', 'gl_bubble': 'on', 'gl_chain': 'on', 'gl_charge': 'on', 'spider_climb_only': 'on', 'uppercut': 'on', 'smoke_blind': 'on', 'dd_grapple': 'on', 'dd_parry': 'on', 'iron_hud': 'on', 'punisher_rage': 'on', 'reputation': 'on', 'survival': 'off', 'punisher_suit': 'vest', 'natural_invincible': 'on', 'hulk_rage': 'on', 'rage_blood': 'extreme', 'rage_tint': 'normal', 'fire_douse': 'on', 'shield_slam': 'on', 'punisher_hook': 'on', 'claw_climb': 'off', 'npc_shove': 'on', 'faction': 'auto', 'hero_suit': 'default', 'test_unlocks': 'off', 'ultimates': 'on', 'perk_branch': 'auto', 'own_suit': 'off', 'perch_takedown': 'on', 'takedown_light': 'on', 'punisher_van': 'on', 'thor_hammer': 'boooop', 'game_body': 'off', 'thor_hammer_model': 'on', 'thor_flight': 'on', 'suit_body': 'arms', 'cap_sprint': 'on', 'cull_margin': '10deg', 'shadow_match': 'on', 'calib_look': 'holo', 'fly_pose': 'lean', 'weapon_collision': 'on', 'light_hold': 'on', 'van_screen': 'on', 'palm_blast': 'palm'}
UPDATE38_DEFAULTS.update({'aim_assist': 'smart', 'punch_mode': 'targeted', 'ring_anchor': 'wrist',
                          'holo_open': 'forearm', 'ring_layer': 'hold'})
# Version 0.0.72 MODEL LOOKS (the user, 27 Oct, after 0.0.71: "the guns look awful and NOT like the
# picked Sketchfab models"): 0.0.7-0.0.71 started every model look on its built-in stand-in (index 0)
# and nothing ever turned the shipped Models\ on. Now the models are the start values; BUILT-IN /
# CLASSIC / DEFAULT stay selectable. migrate_072 sets them once over an existing live.txt.
MODEL_LOOKS_072 = {'gun_models': 'models', 'shield_look': 'model', 'baton_look': 'model', 'batarang_style': 'eira'}
UPDATE38_DEFAULTS.update(MODEL_LOOKS_072)
# Update 37.2: the startup checks (tests): these must start this way on a fresh install and over an
# existing live.txt (migrate_372, once).
STARTUP_ON = {'eye_temporal': 1, 'far_shadows': 2, 'sharpen': 140}
# Update 37.2: what migrate_372 sets once in an existing live.txt (the settings above whose old value
# the player saw in run371, and the Update 36 values back).
MIGRATE_372 = {'sharpen': 140, 'eye_temporal': 1, 'far_shadows': 2, 'eye_bloom': 1, 'shared_lod': 2,
               'bloom_fx': 0, 'aim_steer': 0, 'ring_face': 0, 'wall_crawl': 0, 'web_bend': 100}
def startup_line(current):
    """Update 37.2: one console line on STARTUP_ON as the session runs them (current: LiveFile.current)."""
    def norm(name, value):
        try:
            return export_value(name, value)
        except Exception:
            return value
    names = {'eye_temporal': 'EYE AA', 'far_shadows': 'FAR SHADOWS EYES+CACHE', 'sharpen': 'sharpen 140'}
    parts, off = [], []
    for name, want in STARTUP_ON.items():
        ok = name in current and norm(name, current[name]) == norm(name, want)
        parts.append(f"{names.get(name, name)} {'ON' if ok else 'NOT ON (' + str(current.get(name)) + ')'}")
        if not ok:
            off.append(name)
    return 'Update 37.2 start-up: ' + ', '.join(parts) + ('.' if not off else ' - check live.txt.')
MIGRATED_372 = ('# Update 37.2: EYE AA, FAR SHADOWS EYES+CACHE and sharpen 140 set once (migrated; '
                'a value chosen after this line stays)')
# Update 37: biased exports (the export takes value + bias).
BIAS = {'ring_tilt': 60, 'exposure': 20, 'warmth': 100, 'tint': 100}
# Update 19: music::visualizerKey's order, the first the default.
VISUALIZERS = ('original', 'bars', 'fractal_bloom', 'tunnel', 'starburst')
# Update 18: the styles and each one's shades, as web_style::key and web_style::shadeKey; each list's first
# shade the default. Update 20: the default style is messy (DEFAULT_WEB_STYLE), no longer the first.
WEB_STYLES = ('90s', 'messy', 'modern', 'symbiote', 'energy')
DEFAULT_WEB_STYLE = 'messy'
WEB_SHADES = {'90s': ('cream', 'ivory', 'parchment', 'warm_grey'),
              'messy': ('silver', 'pearl', 'smoke', 'frost'),
              'modern': ('bright_white', 'platinum', 'ice', 'chrome'),
              'symbiote': ('blue_sheen', 'purple_sheen', 'crimson_sheen', 'matte'),
              'energy': ('blue', 'red', 'green', 'orange', 'purple', 'gold')}
for _style in WEB_STYLES:
    SETTINGS[f'web_color_{_style}'] = ((0, len(WEB_SHADES[_style]) - 1), 'SpidyWebColor',
                                       f'{_style} web colour {{}}')
# Which style a shade's word belongs to (each word is one style's only).
SHADE_STYLE = {word: style for style in WEB_STYLES for word in WEB_SHADES[style]}
# Settings that take decimals, and what the export is given per unit (web_width: tenths of a millimetre,
# hud_scale: percent).
SCALE = {'web_width': 100, 'hud_scale': 100, 'exposure': 10}  # Update 37.2: exposure in tenths of EV
# Settings that take a word: the words, and the number the export is given for each.
# Update 17: the themes and text colours in wrist_hud::themeKey/textColorKey's order.
CHOICE_NAMES = {'hud_hand': ('left', 'right'),
                'hud_theme': ('spiderman', 'contrast', 'deutan', 'tritan', 'mono', 'raimi'),
                'hud_text_color': ('theme', 'white', 'yellow', 'cyan'),
                'web_style': WEB_STYLES,
                'web_start': ('wrist', 'palm'),
                'eye_debug': ('off', 'same'),
                'address_finder': ('off', 'on'),
                'eye_lod': ('off', 'on'),
                'music_visualizer': VISUALIZERS,
                'color_filter': ('none', 'vivid', 'golden', 'neon', 'raimi', 'noir', 'animated'),
                'foveated': ('off', 'light', 'balanced', 'aggressive'),
                'npc_fade': ('game', 'off', 'switch'),
                # Update 38 (combat38::optionInfo's values, in order)
                'crawl_cap': ('auto', 'with_flag', 'off'),
                'zip_mode': ('spidyvr', 'both', 'distance', 'aimed'),
                'aim_assist': ('smart', 'spidyvr', 'cone_20', 'off'),
                'ball_look': ('ours', 'wrist', 'glob', 'game_only'),
                'punch_mode': ('targeted', 'both', 'gesture', 'contact'),
                'ring_anchor': ('wrist', 'hand', 'world'),
                'holo_open': ('forearm', 'palm_look', 'click', 'f8_only'),
                'wall_tilt': ('safe', 'off'),
                'web_rope': ('rope', 'fibre'),
                'lunge': ('game_button', 'spidyvr_move'),
                'ball_fire': ('press', 'release'),
                # Update 38.1
                'aim_axis': ('hand_-y', 'aim_pose', 'old_-z'),
                'ring_layer': ('hold', 'newest', 'image'),
                'lunge_from': ('2.5_m', '1.3_m', '4_m'),
                'lunge_speed': ('2.2', '2.6', '1.9'),
                'lunge_aim': ('steady', '38'),
                'punch_angle': ('wide_.2', '38_.35', 'any_0'),
                'ball_lead': ('on_target', 'always'),
                'blood_look': ('brutal', 'big', '37_small'),
                # Update 38.2
                'melee_anim': ('every_hit', 'game', 'spaced'),
                'gadget_keys': ('38.2', 'game_default', 'custom'),
                'grab_web': ('assist', 'exact', 'off'),
                'mirror_look': ('suit', 'figure'),
                'gadget_feel': ('easy', '38.1'),
                # Update 38.3
                'look_lock': ('quick', 'steady'),
                'strike_steer': ('to_lock', 'off'),
                'far_windows': ('2x', '3x', '1.5x', 'game'),
                'gadget_hint': ('on', 'off'),
                # Version 0.0.2
                'aim_strength': ('strong', 'max', 'off', 'low', 'normal'),
                'ball_hit': ('ours_only', '+game_web'),
                # Version 0.0.3
                'rope_grab': ('off', 'on'), 'rope_strength': ('normal', 'low', 'strong'),
                'wall_stick': ('off', 'low_health_only', 'always'), 'auto_perf': ('off', 'on'),
                'perf_target': ('auto', '72', '80', '90', '120'), 'perf_floor': ('medium', 'high', 'low'),
                'perf_status': ('on', 'off'),
                'iron_man': ('off', 'on'), 'mirror_view': ('off', 'test'),
                'hero_height': ('1.00x', '1.15x', '1.30x', '1.45x', '1.60x'), 'hero_damage': ('off', 'on'),
                'punch_sense': ('forgiving', 'max', '0.0.4', 'strict'),
                'sfx_volume': ('100%', '75%', '50%', '25%', 'off'), 'supersonic': ('on', 'off'),
                'supersonic_speed': ('2x', '1.5x', '2.5x', '3x'),
                'daredevil': ('off', 'on'), 'hulk': ('off', 'on'), 'daredevil_punch': ('3x', '1x', '2x', '4x', '5x'),
                'hit_indicator': ('off', 'on'), 'hand_flips': ('on', 'off'), 'lens_flares': ('game', 'off_in_vr'),
                'punch_hits': ('game_instant', 'game_move', 'instant_only'), 'foe_track': ('live', '0.0.6'),
                'beacons_vision': ('on', 'off'),
                'batman': ('off', 'on'), 'wolverine': ('off', 'on'), 'superman': ('off', 'on'), 'punisher': ('off', 'on'),
                'batarang_chain': ('25%', '50%', '75%', '100%', 'off'), 'easy_reload': ('auto', 'realistic'),
                'holster_hands': ('same_side', 'either_hand'), 'baton_holsters': ('on', 'always_on'),
                'baton_look': ('classic', 'club', 'model'), 'batarang_style': ('default', 'eira', 'clooney', 'joker'),
                'gun_models': ('built-in', 'models'),
                'captain_america': ('off', 'on'), 'shield_arm': ('right', 'left'),
                'shield_damage': ('35hp', '20hp', '50hp', '70hp'), 'shield_chain': ('on', 'off'),
                'shield_block': ('off', 'experimental'), 'shield_look': ('built-in', 'model'),
                'thor': ('off', 'on'), 'thor_lightning': ('on', 'off'), 'thor_storm': ('on', 'off'), 'thor_chain': ('on', 'off'), 'green_lantern': ('off', 'on'), 'gl_flight': ('on', 'off'), 'gl_fist': ('on', 'off'), 'gl_beam': ('on', 'off'), 'gl_bubble': ('on', 'off'), 'gl_chain': ('on', 'off'), 'gl_charge': ('on', 'off'), 'spider_climb_only': ('on', 'off'), 'uppercut': ('on', 'off'), 'smoke_blind': ('on', 'off'), 'dd_grapple': ('on', 'off'), 'dd_parry': ('on', 'off'), 'iron_hud': ('on', 'off'), 'punisher_rage': ('on', 'off'), 'reputation': ('on', 'off'), 'survival': ('off', 'on'), 'punisher_suit': ('vest', 'warzone'), 'natural_invincible': ('on', 'off'), 'hulk_rage': ('on', 'off'), 'rage_blood': ('extreme', 'normal', 'off'), 'rage_tint': ('normal', 'strong', 'subtle', 'off'), 'fire_douse': ('on', 'off'), 'shield_slam': ('on', 'off'), 'punisher_hook': ('on', 'off'), 'claw_climb': ('off', 'on'), 'npc_shove': ('on', 'off'), 'faction': ('auto', 'vanilla', 'the hand', 'hydra', 'joker gang'), 'hero_suit': ('default', 'next'), 'test_unlocks': ('off', 'suits', 'ultimates', 'all'), 'ultimates': ('on', 'off'), 'perk_branch': ('auto', 'chain_first', 'hold_points', 'perks_off'), 'own_suit': ('off', 'on'), 'perch_takedown': ('on', 'off'), 'takedown_light': ('on', 'off'), 'punisher_van': ('on', 'off'), 'thor_hammer': ('boooop', 'devilseye', 'love_thunder', 'fccooper'), 'game_body': ('off', 'on'), 'thor_hammer_model': ('on', 'off'), 'thor_flight': ('on', 'off'), 'suit_body': ('arms', 'full_body', 'off'), 'cap_sprint': ('on', 'off'), 'cull_margin': ('10deg', '6deg', '15deg', '20deg'), 'shadow_match': ('on', 'off'), 'calib_look': ('holo', 'classic'), 'fly_pose': ('lean', 'upright'), 'weapon_collision': ('on', 'off'), 'light_hold': ('on', 'strong', 'off'), 'van_screen': ('on', 'off'), 'palm_blast': ('palm', 'aim'),
                'web_color': tuple(SHADE_STYLE),
                **{f'web_color_{style}': WEB_SHADES[style] for style in WEB_STYLES}}
CHOICES = {'hud_hand': {'left': 0, 'right': 1, 'l': 0, 'r': 1, '0': 0, '1': 1},
           'hud_theme': {**{w: i for i, w in enumerate(CHOICE_NAMES['hud_theme'])}, **{str(i): i for i in range(6)},
                         'spider-man': 0, 'spider_man': 0, 'default': 0, 'high_contrast': 1, 'protan': 2,
                         'deuteranopia': 2, 'protanopia': 2, 'tritanopia': 3, 'monochrome': 4, 'grey': 4,
                         'gray': 4, '2000s_raimi': 5, '2000s': 5},
           'hud_text_color': {**{w: i for i, w in enumerate(CHOICE_NAMES['hud_text_color'])},
                              **{str(i): i for i in range(4)}},
           'web_start': {'wrist': 0, 'palm': 1, 'shooter': 0, 'heel': 1, '0': 0, '1': 1},
           'eye_debug': {'off': 0, 'same': 1, 'on': 1, 'normal': 0, 'stereo': 0, 'mono': 1, 'left': 1, 'flat': 1,
                         '0': 0, '1': 1},
           'address_finder': {'off': 0, 'on': 1, 'start': 1, 'stop': 0, '0': 0, '1': 1},
           'eye_lod': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'color_filter': {'none': 0, 'vivid': 1, 'golden': 2, 'golden_hour': 2, 'neon': 3, 'night_neon': 3,
                            'raimi': 4, '2000s_raimi': 4, '2000s': 4, 'noir': 5, 'animated': 6, '90s_animated': 6,
                            '90s': 6, 'cartoon': 6, **{str(i): i for i in range(7)}},
           'foveated': {'off': 0, 'light': 1, 'balanced': 2, 'aggressive': 3, **{str(i): i for i in range(4)}},
           'npc_fade': {'game': 0, 'off': 1, 'switch': 2, 'switch_only': 2, **{str(i): i for i in range(3)}},
           # Update 38: each COMBAT setting's words (and its numbers, and a few spellings)
           **{name: {**{w: i for i, w in enumerate(words)}, **{str(i): i for i in range(len(words))}}
              for name, words in (('crawl_cap', ('auto', 'with_flag', 'off')),
                                  ('zip_mode', ('spidyvr', 'both', 'distance', 'aimed')),
                                  ('ball_fire', ('press', 'release')),
                                  ('punch_mode', ('targeted', 'both', 'gesture', 'contact')),
                                  ('ring_anchor', ('wrist', 'hand', 'world')),
                                  ('wall_tilt', ('safe', 'off')),
                                  # Update 38.2
                                  ('gadget_keys', ('38.2', 'game_default', 'custom')),
                                  ('mirror_look', ('suit', 'figure')),
                                  ('gadget_feel', ('easy', '38.1')),
                                  # Update 38.3
                                  ('look_lock', ('quick', 'steady')),
                                  ('strike_steer', ('to_lock', 'off')),
                                  ('gadget_hint', ('on', 'off')))},
           # Version 0.0.4: EVERY HIT first (0, the number PUNCH ONLY had: a saved punch_only or 0 is the new
           # default); SPACED is 0.0.3's punch only, now 2
           'melee_anim': {'every_hit': 0, 'every': 0, 'punch_only': 0, 'game': 1, 'spaced': 2, '0': 0, '1': 1, '2': 2},
           # Update 38.3: ASSIST first; 38.2's ENEMY was the exact ray hit
           'grab_web': {'assist': 0, 'on': 0, 'exact': 1, 'enemy': 1, 'ray': 1, 'off': 2, '0': 0, '1': 1, '2': 2},
           'far_windows': {'2x': 0, '2': 0, '200': 0, '3x': 1, '300': 1, '1.5x': 2, '150': 2, 'game': 3, 'off': 3,
                           '100': 3, '0': 0, '1': 1, '3': 3},
           # Update 38.2: SMART first; 38.1's words keep their meaning (wide_35 -> cone_20, the nearest left)
           'aim_assist': {'smart': 0, 'on': 0, 'spidyvr': 1, 'spidy': 1, '38.1': 1, 'cone_20': 2, '20': 2, 'cone': 2,
                          'wide_35': 2, 'wide': 2, '35': 2, 'off': 3, '0': 0, '1': 1, '2': 2, '3': 3},
           'lunge': {'game_button': 0, 'game': 0, 'button': 0, 'spidyvr_move': 1, 'spidyvr': 1, 'move': 1,
                     '0': 0, '1': 1},
           'ball_look': {'ours': 0, 'mod': 0, 'wrist': 1, 'glob': 2, 'glow': 2, 'game_only': 3, 'game': 3,
                         '0': 0, '1': 1, '2': 2, '3': 3},  # Version 0.0.2 order
           'aim_strength': {'strong': 0, 'max': 1, 'maximum': 1, 'off': 2, 'low': 3, 'normal': 4,
                            '0': 0, '1': 1, '2': 2, '3': 3, '4': 4},
           'ball_hit': {'ours_only': 0, 'ours': 0, '+game_web': 1, 'game_web': 1, 'game': 1, '0': 0, '1': 1},
           # Version 0.0.3
           'rope_grab': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'rope_strength': {'normal': 0, 'low': 1, 'strong': 2, '0': 0, '1': 1, '2': 2},
           'wall_stick': {'off': 0, 'low_health_only': 1, 'low_health': 1, 'low': 1, 'always': 2, '0': 0, '1': 1, '2': 2},
           'auto_perf': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'perf_target': {'auto': 0, '72': 1, '80': 2, '90': 3, '120': 4},
           'perf_floor': {'medium': 0, 'high': 1, 'low': 2, '0': 0, '1': 1, '2': 2},
           'perf_status': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'iron_man': {'off': 0, 'on': 1, 'web': 0, 'repulsor': 1, '0': 0, '1': 1},
           'mirror_view': {'off': 0, 'test': 1, 'on': 1, '0': 0, '1': 1},
           'hero_height': {'1.00x': 0, '1.15x': 1, '1.30x': 2, '1.45x': 3, '1.60x': 4, '1.00': 0, '1.15': 1,
                           '1.30': 2, '1.45': 3, '1.60': 4, '1.6': 4, 'off': 0, 'hulk': 4, '0': 0, '1': 1,
                           '2': 2, '3': 3, '4': 4},
           'hero_damage': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'punch_sense': {'forgiving': 0, 'max': 1, '0.0.4': 2, 'old': 2, 'strict': 3, '0': 0, '1': 1, '2': 2, '3': 3},
           'sfx_volume': {'100%': 0, '75%': 1, '50%': 2, '25%': 3, 'off': 4, '100': 0, '75': 1, '50': 2, '25': 3,
                          '0': 0, '1': 1, '2': 2, '3': 3, '4': 4},
           'supersonic': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'supersonic_speed': {'2x': 0, '1.5x': 1, '2.5x': 2, '3x': 3, '2': 0, '1.5': 1, '2.5': 2, '3': 3, '0': 0, '1': 1},
           'daredevil': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'hulk': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'daredevil_punch': {'3x': 0, '1x': 1, '2x': 2, '4x': 3, '5x': 4, '3': 0, '1': 1, '2': 2, '4': 3, '5': 4},
           'hit_indicator': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'hand_flips': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'lens_flares': {'game': 0, 'off_in_vr': 1, 'off': 1, '0': 0, '1': 1},
           'punch_hits': {'game_instant': 0, 'game_move': 1, 'instant_only': 2, 'instant': 2, '0': 0, '1': 1, '2': 2},
           'foe_track': {'live': 0, '0.0.6': 1, 'old': 1, '0': 0, '1': 1},
           'beacons_vision': {'on': 0, 'off': 1, '0': 0, '1': 1},
           # Version 0.0.7
           'batman': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'wolverine': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'superman': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'punisher': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'batarang_chain': {'25%': 0, '50%': 1, '75%': 2, '100%': 3, 'off': 4, '25': 0, '50': 1, '75': 2, '100': 3,
                              '0': 0, '1': 1, '2': 2, '3': 3, '4': 4},
           'easy_reload': {'auto': 0, 'realistic': 1, 'easy': 0, '0': 0, '1': 1},
           'holster_hands': {'same_side': 0, 'either_hand': 1, 'same': 0, 'either': 1, '0': 0, '1': 1},
           'baton_holsters': {'off': 0, 'on': 0, 'always_on': 1, '0': 0, '1': 1},
           'baton_look': {'classic': 0, '0.0.62': 0, 'club': 1, 'model': 2, 'old': 0, '0': 0, '1': 1, '2': 2},
           'gun_models': {'built-in': 0, 'builtin': 0, 'models': 1, 'model': 1, '0': 0, '1': 1},
           'batarang_style': {'default': 0, 'eira': 1, 'clooney': 2, 'joker': 3, '0': 0, '1': 1, '2': 2, '3': 3},
           # Version 0.0.7 CAPTAIN AMERICA
           'captain_america': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'shield_arm': {'right': 0, 'left': 1, 'r': 0, 'l': 1, '0': 0, '1': 1},
           'shield_damage': {'35hp': 0, '20hp': 1, '50hp': 2, '70hp': 3, '35_hp': 0, '20_hp': 1, '50_hp': 2, '70_hp': 3,
                             '35': 0, '20': 1, '50': 2, '70': 3,
                             '0': 0, '1': 1, '2': 2, '3': 3},
           'shield_chain': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'shield_block': {'off': 0, 'experimental': 1, 'on': 1, '0': 0, '1': 1},
           'shield_look': {'built-in': 0, 'builtin': 0, 'model': 1, 'models': 1, '0': 0, '1': 1},
           # Version 0.0.72 (TEST)
           'thor': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'thor_lightning': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'thor_storm': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'thor_chain': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'green_lantern': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'gl_flight': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'gl_fist': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'gl_beam': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'gl_bubble': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'gl_chain': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'gl_charge': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'spider_climb_only': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'uppercut': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'smoke_blind': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'dd_grapple': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'dd_parry': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'iron_hud': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'punisher_rage': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'reputation': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'survival': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'punisher_suit': {'vest': 0, 'warzone': 1, '0': 0, '1': 1},
           'natural_invincible': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'hulk_rage': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'rage_blood': {'extreme': 0, 'normal': 1, 'off': 2, '0': 0, '1': 1, '2': 2},
           'rage_tint': {'normal': 0, 'strong': 1, 'subtle': 2, 'off': 3, '0': 0, '1': 1, '2': 2, '3': 3},
           'fire_douse': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'shield_slam': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'punisher_hook': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'claw_climb': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'npc_shove': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'faction': {'auto': 0, 'vanilla': 1, 'the hand': 2, 'hydra': 3, 'joker gang': 4, 'joker_gang': 4, 'joker': 4, '0': 0, '1': 1, '2': 2, '3': 3, '4': 4},
           'hero_suit': {'default': 0, 'next': 1, '0': 0, '1': 1},
           'test_unlocks': {'off': 0, 'suits': 1, 'ultimates': 2, 'all': 3, 'on': 3, '0': 0, '1': 1, '2': 2, '3': 3},
           'ultimates': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'perk_branch': {'auto': 0, 'chain_first': 1, 'chain first': 1, 'hold_points': 2, 'hold points': 2, 'hold': 2, 'perks_off': 3, 'perks off': 3, 'off': 3, 'branch1': 1, 'branch2': 2, 'branch3': 3, 'branch_1': 1, 'branch_2': 2, 'branch_3': 3, 'branch 1': 1, 'branch 2': 2, 'branch 3': 3, '0': 0, '1': 1, '2': 2, '3': 3},
           'own_suit': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'perch_takedown': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'takedown_light': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'punisher_van': {'on': 0, 'off': 1, '0': 0, '1': 1},  # 0.0.73 test 3: ON first
           'game_body': {'off': 0, 'on': 1, '0': 0, '1': 1},
           'thor_hammer_model': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'thor_flight': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'suit_body': {'arms': 0, 'full_body': 1, 'full body': 1, 'body': 1, 'off': 2, '0': 0, '1': 1, '2': 2},
           'cap_sprint': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'cull_margin': {'10deg': 0, '6deg': 1, '15deg': 2, '20deg': 3, '10_deg': 0, '6_deg': 1, '15_deg': 2, '20_deg': 3, '10': 0, '6': 1, '15': 2, '20': 3, '0': 0, '1': 1, '2': 2, '3': 3},
           'shadow_match': {'on': 0, 'off': 1, 'experimental': 0, '0': 0, '1': 1},
           'calib_look': {'holo': 0, 'classic': 1, '0': 0, '1': 1},
           'fly_pose': {'lean': 0, 'upright': 1, '0': 0, '1': 1},
           'weapon_collision': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'light_hold': {'on': 0, 'strong': 1, 'off': 2, '0': 0, '1': 1, '2': 2},
           'van_screen': {'on': 0, 'off': 1, '0': 0, '1': 1},
           'palm_blast': {'palm': 0, 'aim': 1, 'on': 0, 'off': 1, '0': 0, '1': 1},
           'thor_hammer': {'boooop': 0, 'devilseye': 1, 'love_thunder': 2, 'love+thunder': 2, 'fccooper': 3, '0': 0, '1': 1, '2': 2, '3': 3},
           # Update 38.1: 38's words kept (safe -> tap, long_press -> click, f8_only -> 3)
           # Update 38.2: FOREARM first (the tap on the forearm the player asked for; 38.1's 'tap' too)
           'holo_open': {'forearm': 0, 'tap': 0, 'palm_look': 1, 'palm': 1, 'look': 1, 'click': 2, 'f8_only': 3,
                         'f8': 3, 'safe': 0, 'long_press': 2, 'long': 2, '0': 0, '1': 1, '2': 2, '3': 3},
           'aim_axis': {'hand_-y': 0, 'hand': 0, '-y': 0, 'hand_y': 0, 'aim_pose': 1, 'aim': 1, 'old_-z': 2, 'old': 2,
                        '-z': 2, '38': 2, '0': 0, '1': 1, '2': 2},
           'ring_layer': {'hold': 0, 'newest': 1, 'headset_rate': 1, 'headset': 1, '38.1': 1, 'image': 2, '38': 2,
                          '0': 0, '1': 1, '2': 2},
           'lunge_from': {'2.5_m': 0, '2.5': 0, '1.3_m': 1, '1.3': 1, '38': 1, '4_m': 2, '4': 2, '0': 0, '1': 1, '2': 2},
           'lunge_speed': {'2.2': 0, '2.6': 1, '38': 1, '1.9': 2, '0': 0, '1': 1, '2': 2},
           'lunge_aim': {'steady': 0, '38': 1, 'loose': 1, '0': 0, '1': 1},
           'punch_angle': {'wide_.2': 0, 'wide': 0, '.2': 0, '0.2': 0, '38_.35': 1, '38': 1, '.35': 1, '0.35': 1,
                           'any_0': 2, 'any': 2, '0': 0, '1': 1, '2': 2},
           'ball_lead': {'on_target': 0, 'target': 0, 'always': 1, '38': 1, '0': 0, '1': 1},
           'blood_look': {'brutal': 0, 'big': 1, '38.1': 1, '37_small': 2, 'small': 2, '37': 2, '0': 0, '1': 1, '2': 2},
           'web_rope': {'rope': 0, 'fibre': 1, 'fiber': 1, '0': 0, '1': 1},
           'web_style': {**{w: i for i, w in enumerate(WEB_STYLES)}, **{str(i): i for i in range(5)},
                         'nineties': 0, '90': 0},
           'music_visualizer': {**{w: i for i, w in enumerate(VISUALIZERS)}, **{str(i): i for i in range(5)},
                                'red_blue': 0, 'classic': 0, 'default': 0, 'fractal': 2, 'bloom': 2,
                                'star_burst': 4, 'stars': 4},
           # web_color: a shade's word (its own style's), or a number (the style chosen now's list)
           'web_color': {**{w: i for i, w in enumerate(SHADE_STYLE)}, **{str(i): i for i in range(6)}},
           **{f'web_color_{style}': {**{w: i for i, w in enumerate(WEB_SHADES[style])},
                                     **{str(i): i for i in range(len(WEB_SHADES[style]))}}
              for style in WEB_STYLES}}


def web_color_code(name, value):
    """SpidyWebColor's argument for web_color or web_color_<style> = `value`: the shade (bits 0-3) and
    its style + 1 (bits 4-7; 0 the style chosen now, for web_color = a number)."""
    word = str(value)
    if name == 'web_color':
        if word in SHADE_STYLE:
            style = SHADE_STYLE[word]
            return (WEB_STYLES.index(style) + 1) << 4 | WEB_SHADES[style].index(word)
        return int(word) & 15
    style = name.removeprefix('web_color_')
    shade = WEB_SHADES[style].index(word) if word in WEB_SHADES[style] else int(word)
    return (WEB_STYLES.index(style) + 1) << 4 | shade


def export_value(name, value):
    """The whole number the setting's export takes for `value`."""
    if name.startswith('web_color'):
        return web_color_code(name, value)
    if name in CHOICES:
        return CHOICES[name][str(value)]
    number = int(round(value * SCALE.get(name, 1))) + BIAS.get(name, 0)
    if name in VIEW_GATES:
        return VIEW_GATES[name] << 16 | number
    if name in WORLD_LOG_KINDS:
        return WORLD_LOG_KINDS[name] << 16 | number
    return number
# What a line may name instead (the launcher's own words).
ALIASES = {'blood': 'blood_splatter', 'worldlog': 'world_log', 'world_trace': 'world_log', 'eye_trail': 'trail', 'eyetrail': 'trail', 'motion_blur': 'trail', 'eye_aa_trail': 'trail',
           'taajitter': 'taa_jitter', 'jitter_mode': 'taa_jitter', 'taa_jitter_size': 'taa_jitter_scale',
           'jitter_scale': 'taa_jitter_scale', 'eyedebug': 'eye_debug', 'eye_mono': 'eye_debug', 'addressfinder': 'address_finder', 'addr_finder': 'address_finder', 'eyelod': 'eye_lod', 'eye_detail': 'eye_lod', 'webstart': 'web_start', 'web_origin': 'web_start', 'buildingreach': 'reach', 'building_reach': 'reach', 'monitorrate': 'monitor',
           'monitor_rate': 'monitor', 'eyeaa': 'aa', 'eye_aa': 'aa', 'eyehistory': 'history',
           'eye_history': 'history', 'webwidth': 'web_width', 'web': 'web_width',
           'webtravel': 'web_travel', 'web_shot': 'web_travel', 'webspeed': 'web_speed',
           'shot_speed': 'web_speed', 'wristhud': 'hud', 'wrist_hud': 'hud', 'hologram': 'hud', 'hudhand': 'hud_hand',
           'hud_side': 'hud_hand', 'hudscale': 'hud_scale', 'hud_size': 'hud_scale',
           'sharpness': 'sharpen', 'cas': 'sharpen', 'reticle': 'reticles', 'aim_reticles': 'reticles',
           'hudheight': 'hud_height', 'hud_offset': 'hud_height', 'hudtext': 'hud_text', 'hud_text_size': 'hud_text',
           'text_size': 'hud_text', 'hudtheme': 'hud_theme', 'theme': 'hud_theme', 'hud_colors': 'hud_theme',
           'hudtextcolor': 'hud_text_color', 'hud_text_colour': 'hud_text_color', 'text_color': 'hud_text_color',
           'webstyle': 'web_style', 'webcolor': 'web_color', 'web_colour': 'web_color', 'webcolour': 'web_color',
           **{f'web_colour_{style}': f'web_color_{style}' for style in WEB_STYLES},
           'musicvolume': 'music_volume', 'music_vol': 'music_volume', 'musicvisualizer': 'music_visualizer',
           'music_visualiser': 'music_visualizer', 'visualizer': 'music_visualizer', 'visualiser': 'music_visualizer',
           'musictrack': 'music_track', 'music_song': 'music_track', 'song': 'music_track'}
NOT_LIVE = {'renderscale': 'the render scale sizes the eye buffers when VR starts: relaunch with -RenderScale',
            'render_scale': 'the render scale sizes the eye buffers when VR starts: relaunch with -RenderScale',
            'reflections': "the eyes' reflections stay off in Spider-Man 2",
            'noeyereflections': "the eyes' reflections stay off in Spider-Man 2"}


def parse(text):
    """The settings in `text` as {name: value}, and a problem per line it could not take."""
    values, problems = {}, []
    for number, line in enumerate(text.splitlines(), 1):
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        name, sep, value = line.partition('=')
        if not sep:
            name, _, value = line.partition(' ')
        name = name.strip().lower().lstrip('-')
        name = ALIASES.get(name, name)
        if name in NOT_LIVE:
            problems.append(f'line {number}: {NOT_LIVE[name]}')
            continue
        if name not in SETTINGS:
            problems.append(f'line {number}: unknown setting "{name}" (live: {", ".join(SETTINGS)})')
            continue
        if name in CHOICES:
            word = value.strip().lower()
            if word not in CHOICES[name]:  # Update 18: "warm grey", "warm-grey" are warm_grey
                word = '_'.join(word.replace('-', ' ').split())
            if word not in CHOICES[name]:
                problems.append(f'line {number}: {name} must be ' + ' or '.join(CHOICE_NAMES[name]))
                continue
            # web_color = a number: the style chosen now's list, so it stays a number
            values[name] = int(word) if name == 'web_color' and word.isdigit() else \
                CHOICE_NAMES[name][CHOICES[name][word]]
            continue
        try:
            raw = value.strip()
            text_value = raw.rstrip('%').strip()
            if name == 'hud_scale' and raw.endswith('%'):
                number_value = round(float(text_value) / 100, 2)
                if number_value != number_value:
                    raise ValueError
            elif name in SCALE:
                number_value = round(float(text_value.lower().removesuffix('cm').removesuffix('ev').strip()), 2)
                if number_value != number_value:
                    raise ValueError
            elif name == 'hud_height':
                number_value = int(text_value.lower().removesuffix('cm').strip())
            else:
                number_value = int(text_value)
        except ValueError:
            problems.append(f'line {number}: {name} needs a ' + ('number' if name in SCALE else 'whole number'))
            continue
        low, high = SETTINGS[name][0]
        if not low <= number_value <= high:
            problems.append(f'line {number}: {name} must be {low}-{high}')
            continue
        values[name] = number_value
    one_hero(values, problems)
    return values, problems


def one_hero(values, problems=None):
    """Version 0.0.6: only one hero toggle (HERO_KEYS) on; with two or more on, the first is kept."""
    on = [k for k in HERO_KEYS if values.get(k) == 'on']
    for k in on[1:]:
        values[k] = 'off'
        if problems is not None:
            problems.append(f'{k}: only one hero at a time, {on[0]} kept on')
    return values


class LiveFile:
    """Watches one file; poll() gives the settings that changed since the last applied ones."""

    def __init__(self, path, current):
        self.path = pathlib.Path(path)
        self.current = dict(current)  # what the session runs with now
        self.stamp = self._stamp()   # a file already there at the start is read only once it is saved

    def _stamp(self):
        try:
            s = self.path.stat()
            return (s.st_mtime_ns, s.st_size)
        except OSError:
            return None

    def poll(self):
        """({name: value} to apply, [problems]); empty while the file is unchanged."""
        stamp = self._stamp()
        if stamp == self.stamp:
            return {}, []
        self.stamp = stamp
        if stamp is None:
            return {}, []
        try:
            text = self.path.read_text(encoding='utf-8-sig', errors='replace')
        except OSError as error:
            return {}, [f'{self.path.name}: {error}']
        values, problems = parse(text)
        return {k: v for k, v in values.items() if self.current.get(k) != v}, problems

    def applied(self, name, value):
        self.current[name] = value

    def adopt(self, updates):
        """Writes `updates` ({name: value}) into the file, as the wrist hologram set them, and takes
        them as applied, so poll() does not apply them again. Does nothing (returns False) while the
        file was saved since the last poll() (the player's save is read first) or on an I/O error;
        call again later. Never raises."""
        if not updates:
            return True
        if self._stamp() != self.stamp:
            return False
        try:
            text = self.path.read_text(encoding='utf-8-sig', errors='replace') if self.path.exists() else HEADER
            write_atomically(self.path, write_back(text, updates))
        except OSError:
            return False
        self.stamp = self._stamp()
        self.current.update(updates)
        return True


# Update 15: what the wrist hologram's pages set.
HEADER = '# Live settings (tools/live_settings.py). Save to apply; the wrist hologram writes its own here.\n'
# Version 0.0.73 test 3: HEADER gains the MIGRATED_073T3 marker below, so a live.txt made from now on starts
# migrated (a SHADOW MATCH OFF the player picks in it is never reset).
PERSISTED = ('web_width', 'web_speed', 'aa', 'history', 'reach', 'monitor', 'sharpen', 'reticles',
             'hud_height', 'hud_scale', 'hud_text', 'hud_theme', 'hud_text_color',
             'web_style', *(f'web_color_{style}' for style in WEB_STYLES),
             'music_volume', 'music_visualizer', 'music_track',
             'lens_flares',  # Version 0.0.62: the hologram's LENS FLARES (RESTART: kept for the next start)
             'mirror_view')  # 0.0.7 MIRROR PERSIST FIX: the hologram's MIRROR VIEW (RESTART), the same
# Update 23: live.txt-only settings (no hologram control) also applied when a session starts, so they keep.
START_ONLY = ('web_start', 'trail')  # Update 34.2: trail kept from live.txt
# Update 37: every new live.txt setting applies at the start too (a file written before the launch
# counts), all but the one-shot mirror_recentre.
UPDATE37_KEYS = ('bloom_fx', 'lod_fade', 'lod_reach', 'impostors', 'eye_occlusion', 'history_cut', 'aa_quality',
                 'ring_face', 'ring_offset', 'ring_tilt', 'combat', 'tap_ms', 'gesture_ms', 'grabber_slot',
                 'sonic_slot', 'upshot_slot', 'ricochet_slot', 'yank_speed', 'upshot_speed', 'upshot_low',
                 'flick_speed', 'sonic_gap', 'sonic_reach', 'punch', 'punch_speed', 'aim_steer', 'aim_cone',
                 'lock_stick', 'looseness', 'web_physics', 'web_wiggle', 'web_bend', 'heal', 'heal_hold_ms',
                 'wall_crawl', 'hand_climb', 'climb_pull', 'climb_grip', 'mirror', 'mirror_distance',
                 'mirror_size', 'mirror_spot', 'mirror_anchor', 'mirror_style', 'taa_jitter_scale',
                 'far_shadows', 'eye_bloom', 'shared_lod', 'world_log', 'npc_log', 'spawn_log', 'mission_log',
                 'blood_splatter', 'holo_reuse',
                 # Update 37.2
                 'eye_temporal', 'wall_body', 'zip_strike', 'web_yank', 'yank_slot', 'npc_fade', 'color_fx',
                 'color_filter', 'color_strength', 'exposure', 'contrast', 'saturation', 'vibrance', 'warmth',
                 'tint', 'vignette', 'foveated', 'fovea_inner', 'fovea_debug',
                 # Update 38.5
                 'color_cartoon', 'color_outlines', 'color_bands', 'blood_visor', 'fly_vignette', 'prompt_handling', 'gesture_moves',
                 'fly_roll', 'fly_roll_speed', 'fly_auto_level')
START_ONLY = START_ONLY + UPDATE37_KEYS
# Update 38: the COMBAT settings, kept from live.txt at the start too; the hologram's presses last the
# session (live.txt keeps a value).
UPDATE38_KEYS = ('crawl_cap', 'zip_mode', 'aim_assist', 'ball_look', 'punch_mode', 'ring_anchor', 'holo_open',
                 'wall_tilt', 'web_rope', 'lunge', 'ball_fire',
                 # Update 38.1
                 'aim_axis', 'ring_layer', 'lunge_from', 'lunge_speed', 'lunge_aim', 'punch_angle', 'ball_lead',
                 'blood_look',
                 # Update 38.2
                 'melee_anim', 'gadget_keys', 'grab_web', 'mirror_look', 'gadget_feel',
                 # Update 38.3
                 'look_lock', 'strike_steer', 'far_windows', 'gadget_hint',
                 # Version 0.0.2
                 'aim_strength', 'ball_hit',
                 # Version 0.0.3
                 'rope_grab', 'rope_strength', 'wall_stick', 'auto_perf', 'perf_target', 'perf_floor', 'perf_status',
                 # Version 0.0.4
                 'iron_man', 'mirror_view',
                 # Version 0.0.5
                 'hero_height', 'hero_damage', 'punch_sense', 'sfx_volume', 'supersonic', 'supersonic_speed',
                 # Version 0.0.6
                 'daredevil', 'hulk', 'daredevil_punch',
                 # Version 0.0.62
                 'hit_indicator', 'hand_flips', 'lens_flares', 'punch_hits', 'foe_track', 'beacons_vision',
                 # Version 0.0.7
                 'batman', 'wolverine', 'superman', 'punisher', 'batarang_chain', 'easy_reload', 'holster_hands',
                 'baton_holsters', 'baton_look', 'batarang_style', 'gun_models',
                 # Version 0.0.7 CAPTAIN AMERICA
                 'captain_america', 'shield_arm', 'shield_damage', 'shield_chain', 'shield_block', 'shield_look',
                 # Version 0.0.72 (TEST)
                 'thor', 'thor_lightning', 'thor_storm', 'thor_chain', 'green_lantern', 'gl_flight', 'gl_fist', 'gl_beam', 'gl_bubble', 'gl_chain', 'gl_charge', 'spider_climb_only', 'uppercut', 'smoke_blind', 'dd_grapple', 'dd_parry', 'iron_hud', 'punisher_rage', 'reputation', 'survival', 'punisher_suit', 'natural_invincible', 'hulk_rage', 'rage_blood', 'rage_tint', 'fire_douse', 'shield_slam', 'punisher_hook', 'claw_climb', 'npc_shove', 'faction', 'hero_suit', 'test_unlocks', 'ultimates', 'perk_branch', 'own_suit', 'perch_takedown', 'takedown_light', 'punisher_van', 'thor_hammer', 'game_body', 'thor_hammer_model', 'thor_flight', 'suit_body', 'cap_sprint', 'cull_margin', 'shadow_match', 'calib_look', 'fly_pose', 'weapon_collision', 'light_hold', 'van_screen', 'palm_blast')
# Version 0.0.6: the hero toggles; only one is on at a time (set_value turns the others off).
# Version 0.0.7: seven; CAPTAIN AMERICA eight.
# Version 0.0.72: THOR and GREEN LANTERN ten.
HERO_KEYS = ('iron_man', 'daredevil', 'hulk', 'batman', 'wolverine', 'superman', 'punisher', 'captain_america',
             'thor', 'green_lantern')
START_ONLY = START_ONLY + UPDATE38_KEYS
# The mod's wrist_hud::Control numbers (include/spidy/wrist_hud.hpp) of the settings, and the
# SpidyHudSettings selector that reads each back.
CONTROL_KEYS = {4: 'web_width', 5: 'web_speed', 6: 'aa', 7: 'history', 8: 'reach', 9: 'monitor',
                10: 'supersample', 11: 'sharpen', 12: 'reticles', 13: 'hud_height', 14: 'hud_scale',
                15: 'hud_text', 16: 'hud_theme', 17: 'hud_text_color', 18: 'web_style', 19: 'web_color',
                20: 'music_track', 27: 'music_volume', 28: 'music_visualizer',
                29: 'lens_flares',  # Version 0.0.62: the hologram's LENS FLARES (RESTART), kept in live.txt
                21: 'mirror_view'}  # 0.0.7: MIRROR VIEW (RESTART), kept in live.txt (was lost at every restart)
# Update 18: WEB COLOR sets the chosen style's shade; the launcher keeps every style's.
WEB_COLOR_KEYS = tuple(f'web_color_{style}' for style in WEB_STYLES)
SELECTOR = {'web_width': 0, 'web_speed': 0, 'aa': 1, 'history': 1, 'reach': 1, 'monitor': 1, 'supersample': 2,
            'sharpen': 4, 'reticles': 4, 'hud_height': 5, 'hud_scale': 5, 'hud_text': 5, 'hud_theme': 5,
            'hud_text_color': 5, 'web_style': 6, **{key: 6 for key in WEB_COLOR_KEYS},
            'music_volume': 7, 'music_visualizer': 7, 'music_track': 7, 'lens_flares': 10,
            'mirror_view': 10}
CHANGED_SELECTOR = 3
MARKER = 1 << 30
SUPERSAMPLES = (100, 200)
# Update 23: the marker every next-launch.txt written from now on carries, so the one-time reset below
# (migrate_next_launch) leaves a supersample chosen after it alone.
SUPERSAMPLE_MIGRATED = '# Update 23: supersample default is OFF (100); a higher value here was chosen after that'
NEXT_LAUNCH_HEADER = ('# Next launch (wrist hologram, GRAPHICS page): RESTART REQUIRED. Read when the game is\n'
                      '# started again without -Supersample; delete this file to go back to the default.\n'
                      + SUPERSAMPLE_MIGRATED + '\n')


def decode_hud_settings(selector, code):
    """SpidyHudSettings(selector)'s exit code as {name: value}; None if it is not one (no marker)."""
    if code is None or code < 0 or not code & MARKER:
        return None
    code &= MARKER - 1
    if selector == 0:
        return {'web_width': round((code & 1023) / 100, 2), 'web_speed': code >> 10 & 2047}
    if selector == 1:
        return {'aa': code & 1, 'history': code >> 1 & 127, 'reach': code >> 8 & 511, 'monitor': code >> 17 & 255}
    if selector == 2:
        return {'supersample': code & 1023}
    if selector == 10:  # Version 0.0.62: LENS FLARES (0 game, 1 off in vr); 0.0.7: MIRROR VIEW in bit 1
        return {'lens_flares': ('game', 'off_in_vr')[min(code & 1, 1)],
                'mirror_view': ('off', 'test')[code >> 1 & 1]}
    if selector == CHANGED_SELECTOR:
        changed = {CONTROL_KEYS[bit]: True for bit in CONTROL_KEYS if code >> bit & 1}
        if changed.pop('web_color', None):
            changed.update({key: True for key in WEB_COLOR_KEYS})
        return changed
    if selector == 4:  # Update 21: sharpening 0-200 in bits 0-7, the reticles in bit 8 (Update 16: 0-6, 7)
        return {'sharpen': min(code & 255, 200), 'reticles': code >> 8 & 1}
    if selector == 5:  # Update 17: height cm 0-4, size % 5-12, text % 13-20, theme 21-23, text colour 24-25
        themes, colors = CHOICE_NAMES['hud_theme'], CHOICE_NAMES['hud_text_color']
        return {'hud_height': min(code & 31, 25),
                'hud_scale': round(min(max(code >> 5 & 255, 50), 200) / 100, 2),
                'hud_text': min(max(code >> 13 & 255, 75), 175),
                'hud_theme': themes[min(code >> 21 & 7, len(themes) - 1)],
                'hud_text_color': colors[code >> 24 & 3]}
    if selector == 6:  # Update 18: web style 0-2, each style's shade 3 bits from 3
        values = {'web_style': WEB_STYLES[min(code & 7, len(WEB_STYLES) - 1)]}
        for i, style in enumerate(WEB_STYLES):
            shades = WEB_SHADES[style]
            values[f'web_color_{style}'] = shades[min(code >> (3 + 3 * i) & 7, len(shades) - 1)]
        return values
    if selector == 7:  # Update 19: music volume 0-6, visualizer 7-9, the song + 1 10-25 (0 none)
        return {'music_volume': min(code & 127, 100),
                'music_visualizer': VISUALIZERS[min(code >> 7 & 7, len(VISUALIZERS) - 1)],
                'music_track': code >> 10 & 0xffff}
    if selector == SESSION_FPS_SELECTORS[0]:  # Update 24: AVG tenths 0-10 (0 none), seconds tracked 11-28
        return {'fps_avg': (code & 2047) / 10 or None, 'fps_seconds': code >> 11 & 0x3ffff}
    if selector == SESSION_FPS_SELECTORS[1]:  # Update 24: HIGH tenths 0-10, LOW 11-21 (0 none)
        return {'fps_high': (code & 2047) / 10 or None, 'fps_low': (code >> 11 & 2047) / 10 or None}
    return None


# Update 24: the session HIGH / LOW / AVG (the VR SETTINGS page's boxes, FpsSession): SpidyHudSettings(8, 9).
SESSION_FPS_SELECTORS = (8, 9)


def session_fps(avg_code, high_low_code):
    """{'high', 'low', 'average', 'tracked_seconds'} from SpidyHudSettings(8) and (9); None before a whole
    second was tracked or from an older mod (3970, no marker)."""
    first = decode_hud_settings(SESSION_FPS_SELECTORS[0], avg_code)
    second = decode_hud_settings(SESSION_FPS_SELECTORS[1], high_low_code)
    if not first or not second or not first['fps_avg']:
        return None
    return dict(high=second['fps_high'], low=second['fps_low'], average=first['fps_avg'],
                tracked_seconds=first['fps_seconds'])


def session_fps_line(stats):
    """The console's line at exit for session_fps()'s numbers (None: nothing tracked)."""
    if not stats:
        return ('Session FPS (new eye pairs a second): nothing tracked (the first 10 s after loading are left out, '
                'or the mod is older than Update 24).')
    show = lambda v: f'{v:.1f}' if v else '--'
    return (f"Session FPS (new eye pairs a second, whole seconds from 10 s after loading or the last RESET, "
            f"loads and pauses left out): HIGH {show(stats['high'])}, LOW {show(stats['low'])}, "
            f"AVG {show(stats['average'])} over {stats['tracked_seconds']} s.")


# Update 24: why distant things shimmer, said when the session starts (shimmer_notes).
STRONG_SHARPEN_NOTE = ('Headset sharpening is {}% (above 100 is the strong mode): it also sharpens the flicker of '
                       'far roofs, windows and shadow edges. For a calmer distance try "sharpen = 50" or "sharpen = 0" '
                       'in live.txt (or SHARPNESS on the hologram).')
EYE_DEBUG_NOTE = ('live.txt has "eye_debug = {}": the flicker test (both eyes shown the left eye\'s picture, flat) '
                  'starts the next time live.txt is saved; set it back to "eye_debug = off" when done.')


def shimmer_notes(kept, live_values):
    """Console lines for the session start: strong sharpening kept from last time (`kept`, persisted()) and
    an eye_debug test left in live.txt (`live_values`, its parsed settings, which wait for a save)."""
    lines = []
    sharpen = kept.get('sharpen')
    if isinstance(sharpen, (int, float)) and sharpen > 100:
        lines.append(STRONG_SHARPEN_NOTE.format(int(sharpen)))
    debug = live_values.get('eye_debug')
    if debug is not None and CHOICES['eye_debug'].get(str(debug), 0):
        lines.append(EYE_DEBUG_NOTE.format(debug))
    return lines


def start_notes(path):
    """shimmer_notes for the live.txt at `path` (none when it cannot be read)."""
    try:
        text = pathlib.Path(path).read_text(encoding='utf-8-sig', errors='replace')
    except OSError:
        return []
    kept, _ = persisted(path)
    return shimmer_notes(kept, parse(text)[0])


def format_value(name, value):
    if name in SCALE:
        return f'{round(float(value), 2):g}'
    return str(value)


def write_back(text, updates):
    """`text` with every line that names a setting in `updates` (or an alias of it) set to the new value,
    its comment kept; a setting no line names is added at the end."""
    updates = dict(updates)
    for key in HERO_KEYS:  # Version 0.0.6: a hero turned on turns the others off in the file too
        if updates.get(key) == 'on':
            updates.update({k: 'off' for k in HERO_KEYS if k != key})
            break
    lines = text.splitlines()
    done = set()
    for i, line in enumerate(lines):
        body, hash_, comment = line.partition('#')
        if not body.strip():
            continue
        name, sep, _ = body.partition('=')
        if not sep:
            name = body.strip().partition(' ')[0]
        key = name.strip().lower().lstrip('-')
        key = ALIASES.get(key, key)
        if key in updates:
            lines[i] = f'{key} = {format_value(key, updates[key])}' + (f'  #{comment}' if hash_ else '')
            done.add(key)
    for key, value in updates.items():
        if key not in done:
            lines.append(f'{key} = {format_value(key, value)}')
    return '\n'.join(lines) + '\n'


# 0.0.7 MIRROR PERSIST FIX: the settings only read when VR starts (the MIRROR VIEW TEST's view is made with
# the eyes), so live.txt's value is sent before SpidyXrStart rather than with the other kept ones after it.
VR_START_KEYS = ('mirror_view',)


def vr_start_settings(path):
    """{name: value} of VR_START_KEYS the live.txt at `path` keeps (none when it cannot be read)."""
    kept, _ = persisted(path)
    return {k: kept[k] for k in VR_START_KEYS if k in kept}


def write_atomically(path, text):
    """Replaces `path` with `text` in one step (a reader sees the old file or the new one, whole)."""
    path = pathlib.Path(path)
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_text(text, encoding='utf-8')
    temporary.replace(path)


def persisted(path):
    """The PERSISTED settings in the live.txt at `path` (applied at the session's start), and problems."""
    try:
        text = pathlib.Path(path).read_text(encoding='utf-8-sig', errors='replace')
    except OSError:
        return {}, []
    values, _ = parse(text)
    kept = {k: v for k, v in values.items() if k in PERSISTED or k in START_ONLY}
    # Update 18: "web_color = gold" (or a number, of web_style's list) is kept as its style's shade.
    if 'web_color' in values:
        word = values['web_color']
        style = SHADE_STYLE.get(word) if isinstance(word, str) else kept.get('web_style', DEFAULT_WEB_STYLE)
        if isinstance(word, int):
            word = WEB_SHADES[style][word] if word < len(WEB_SHADES[style]) else None
        if word is not None:
            kept[f'web_color_{style}'] = word
    return kept, []


# Update 20: the marker migrate_web_style leaves, so it changes a live.txt once only.
WEB_STYLE_MIGRATED = '# Update 20: web_style default is now messy (migrated once; set web_style = 90s to go back)'


def migrate_web_style(path):
    """Update 20, once: a live.txt at `path` whose web_style is 90s (the default until Update 20, which the
    hologram wrote back) is set to messy, the new default. Every live.txt (made now if there is none) gets the
    marker, so a 90s chosen later stays. The line to show, or None (nothing changed, already done, or the
    file could not be read or written). Never raises."""
    path = pathlib.Path(path)
    try:
        text = path.read_text(encoding='utf-8-sig', errors='replace') if path.exists() else ''
    except OSError:
        return None
    if WEB_STYLE_MIGRATED in text:
        return None
    values, _ = parse(text)
    old = values.get('web_style') == '90s'
    if old:
        text = write_back(text, {'web_style': DEFAULT_WEB_STYLE})
    elif text and not text.endswith('\n'):
        text += '\n'
    try:
        write_atomically(path, text + WEB_STYLE_MIGRATED + '\n')
    except OSError:
        return None
    if not old:
        return None
    return (f'live.txt: web_style 90s changed once to {DEFAULT_WEB_STYLE}, the new default (Update 20); '
            'set "web_style = 90s" to go back, it stays from now on.')


MIGRATED_072 = '# Version 0.0.72: the model looks (GUN MODELS, SHIELD LOOK, BATON LOOK, BATARANG STYLE) set once (migrated).'
# The 0.0.7-0.0.71 start values migrate_072 replaces (a live.txt that chose another keeps it).
OLD_LOOKS_072 = {'gun_models': 'built-in', 'shield_look': 'built-in', 'baton_look': 'classic', 'batarang_style': 'default'}


def migrate_072(path):
    """Version 0.0.72, once: an existing live.txt whose model looks are the old built-in start values
    gets MODEL_LOOKS_072, and the MIGRATED_072 marker (a value chosen after it stays). The line to
    show, or None. Never raises."""
    path = pathlib.Path(path)
    try:
        if not path.exists():
            return None
        text = path.read_text(encoding='utf-8-sig', errors='replace')
    except OSError:
        return None
    if MIGRATED_072 in text:
        return None
    try:
        values, _ = parse(text)
    except Exception:
        return None
    changed = {k: v for k, v in MODEL_LOOKS_072.items()
               if k in values and str(values[k]).strip().lower() == OLD_LOOKS_072[k]}
    if changed:
        text = write_back(text, changed)
    if text and not text.endswith('\n'):
        text += '\n'
    try:
        write_atomically(path, text + MIGRATED_072 + '\n')
    except OSError:
        return None
    if not changed:
        return None
    return ('live.txt: set once to the 0.0.72 model looks: ' +
            ', '.join(f'{k} = {v}' for k, v in changed.items()) + ' (change them again and they stay).')


def migrate_372(path):
    """Update 37.2, once: an existing live.txt at `path` gets MIGRATE_372's values (EYE AA, FAR SHADOWS
    EYES+CACHE and sharpen 140 among them: run371's live.txt kept sharpen 100 and started them off), and
    the MIGRATED_372 marker, so a value chosen after it stays. No live.txt: nothing to do (the defaults
    apply). The line to show, or None (nothing changed, already done, or the file could not be read or
    written). Never raises."""
    path = pathlib.Path(path)
    try:
        if not path.exists():
            return None
        text = path.read_text(encoding='utf-8-sig', errors='replace')
    except OSError:
        return None
    if MIGRATED_372 in text:
        return None
    values, _ = parse(text)
    changed = {k: v for k, v in MIGRATE_372.items() if k in values and values[k] != v}
    if changed:
        text = write_back(text, changed)
    if text and not text.endswith('\n'):
        text += '\n'
    try:
        write_atomically(path, text + MIGRATED_372 + '\n')
    except OSError:
        return None
    if not changed:
        return None
    return ('live.txt: set once to the Update 37.2 start values: ' +
            ', '.join(f'{k} = {v}' for k, v in changed.items()) + ' (change them again and they stay).')


# Update 38.2: the COMBAT settings whose values moved (the 38.2 fix put first): a NUMBER a 38.1 live.txt
# holds is written back once as the word it meant then (words keep their meaning: CHOICES).
RENUMBERED_382 = {'aim_assist': ('spidyvr', 'cone_20', 'cone_20', 'off'), 'punch_mode': ('both', 'gesture', 'contact'),
                  'ring_anchor': ('hand', 'world'), 'ring_layer': ('newest', 'image'),
                  'blood_look': ('big', '37_small')}
MIGRATED_382 = '# Update 38.2: COMBAT numbers from 38.1 written as their words (migrated once)'



def migrate_382(path):
    """Update 38.2, once: in an existing live.txt, a RENUMBERED_382 setting given as a number is set to the
    word that number meant in 38.1 (so "ring_layer = 1", IMAGE then, stays IMAGE, not NEWEST). The line to
    show, or None. Never raises."""
    path = pathlib.Path(path)
    try:
        if not path.exists():
            return None
        text = path.read_text(encoding='utf-8-sig', errors='replace')
    except OSError:
        return None
    if MIGRATED_382 in text:
        return None
    changed = {}
    for line in text.splitlines():
        name, sep, value = line.split('#', 1)[0].partition('=')
        name, value = name.strip().lower(), value.strip()
        if sep and name in RENUMBERED_382 and value.isdigit():
            words = RENUMBERED_382[name]
            changed[name] = words[int(value)] if int(value) < len(words) else words[0]
    if changed:
        text = write_back(text, changed)
    if text and not text.endswith('\n'):
        text += '\n'
    try:
        write_atomically(path, text + MIGRATED_382 + '\n')
    except OSError:
        return None
    if not changed:
        return None
    return ('live.txt: Update 38.2 renumbered COMBAT settings, kept as you set them: ' +
            ', '.join(f'{k} = {v}' for k, v in changed.items()) + '.')


# Update 38.3, once: the run382 complaints' settings set to their 38.3 fix in an existing live.txt (the
# double ball, web grab / yank not working); a value chosen after this line stays. A 38.2 number keeps its
# meaning where the order moved (ball_look 0 was GLOB, grab_web 1 was OFF).
MIGRATE_383 = {'ball_look': 'game_only', 'grab_web': 'assist', 'web_yank': 1}
MIGRATED_383 = ('# Update 38.3: BALL LOOK GAME ONLY, GRAB WEB ASSIST and WEB YANK ON set once (migrated; '
                'a value chosen after this line stays)')


def migrate_383(path):
    """Update 38.3, once: MIGRATE_383 written into an existing live.txt. The line to show, or None.
    Never raises."""
    path = pathlib.Path(path)
    try:
        if not path.exists():
            return None
        text = path.read_text(encoding='utf-8-sig', errors='replace')
    except OSError:
        return None
    if MIGRATED_383 in text:
        return None
    changed = {}
    for line in text.splitlines():
        name, sep, value = line.split('#', 1)[0].partition('=')
        name, value = name.strip().lower(), value.strip().lower()
        if sep and name in MIGRATE_383 and value != str(MIGRATE_383[name]):
            changed[name] = MIGRATE_383[name]
    if changed:
        text = write_back(text, changed)
    if text and not text.endswith('\n'):
        text += '\n'
    try:
        write_atomically(path, text + MIGRATED_383 + '\n')
    except OSError:
        return None
    if not changed:
        return None
    return ('live.txt: set once to the Update 38.3 fixes: ' +
            ', '.join(f'{k} = {v}' for k, v in changed.items()) + ' (change them again and they stay).')


# Update 38.5, once: BALL LOOK GAME ONLY (38.3's default, the ball where the game puts it: the user saw it
# leave the top corners of the view) moved to WRIST, the game's own ball from the pulled hand's wrist.
# And YANK SLOT 5 (RB HOLD, the old default: run384's yank gesture fired 0 times on it) moved to 7 (Y HOLD,
# the game's published hold Triangle). RB HOLD stays available as plan B (yank_slot = 5 after this line).
MIGRATE_385 = {'ball_look': ('game_only', 'game', '2'), 'yank_slot': ('5',)}
MIGRATE_385_TO = {'ball_look': 'wrist', 'yank_slot': '7'}
MIGRATED_385 = ('# Update 38.5: BALL LOOK GAME ONLY moved to WRIST and YANK SLOT RB HOLD moved to Y HOLD once '
                '(migrated; a value chosen after this line stays)')


def migrate_385(path):
    """Update 38.5, once: a live.txt's BALL LOOK GAME ONLY becomes WRIST (GLOB stays GLOB) and YANK SLOT 5
    (RB HOLD) becomes 7 (Y HOLD). The line to
    show, or None. Never raises."""
    path = pathlib.Path(path)
    try:
        if not path.exists():
            return None
        text = path.read_text(encoding='utf-8-sig', errors='replace')
    except OSError:
        return None
    if MIGRATED_385 in text:
        return None
    changed = {}
    for line in text.splitlines():
        name, sep, value = line.split('#', 1)[0].partition('=')
        name, value = name.strip().lower(), value.strip().lower()
        if sep and name in MIGRATE_385 and value in MIGRATE_385[name]:
            changed[name] = MIGRATE_385_TO[name]
    if changed:
        text = write_back(text, changed)
    if text and not text.endswith('\n'):
        text += '\n'
    try:
        write_atomically(path, text + MIGRATED_385 + '\n')
    except OSError:
        return None
    if not changed:
        return None
    return ('live.txt: set once to the Update 38.5 fix: ' +
            ', '.join(f'{k} = {v}' for k, v in changed.items()) + ' (change it again and it stays).')


# Version 0.0.2, once: BALL LOOK becomes OURS (one mod web ball per press from the pulled wrist, no game
# ball: run001 saw the game's ball fire several at a press) whatever it was; WRIST, GLOB and GAME ONLY stay
# available (ball_look = wrist after this line). The numbers' meaning moved (0 is OURS now).
MIGRATE_386_TO = {'ball_look': 'ours'}
MIGRATED_386 = ('# Version 0.0.2: BALL LOOK moved to OURS once (migrated; a value chosen after this line stays)')


def migrate_386(path):
    """Version 0.0.2, once: a live.txt's BALL LOOK (any value) becomes OURS. The line to show, or None.
    Never raises."""
    path = pathlib.Path(path)
    try:
        if not path.exists():
            return None
        text = path.read_text(encoding='utf-8-sig', errors='replace')
    except OSError:
        return None
    if MIGRATED_386 in text:
        return None
    changed = {}
    for line in text.splitlines():
        name, sep, value = line.split('#', 1)[0].partition('=')
        name, value = name.strip().lower(), value.strip().lower()
        if sep and name in MIGRATE_386_TO and value != MIGRATE_386_TO[name]:
            changed[name] = MIGRATE_386_TO[name]
    try:
        if changed:
            text = write_back(text, changed)
        if text and not text.endswith('\n'):
            text += '\n'
        write_atomically(path, text + MIGRATED_386 + '\n')
    except (OSError, ValueError):
        return None
    if not changed:
        return None
    return ('live.txt: set once to the Version 0.0.2 fix: ' +
            ', '.join(f'{k} = {v}' for k, v in changed.items()) + ' (change it again and it stays).')


# Version 0.0.73 test 3, once: a fix the player asked for ships ON, even over a live.txt an earlier build
# wrote. A key at its OLD default (written before this marker: a test 2 live.txt can hold shadow_match = off
# saved as the old default, never picked) moves to the new default; any other value keeps what it meant then.
# A number is read with the OLD build's order (test 2's shadow_match 0 was OFF, 1 ON), so hand-typed numbers
# keep their meaning; after the marker, numbers follow the new order. Every key a fix makes default is listed
# here: {key: (old choice names in their old order, old default word, new default word)}.
MIGRATE_073T3 = {'shadow_match': (('off', 'on'), 'off', 'on'),
                 'punisher_van': (('off', 'on'), 'off', 'on')}  # 0.0.73 test 3: PUNISHER VAN ON first now
MIGRATE_073T3_ALIASES = {'shadow_match': {'experimental': 'on'}}
MIGRATED_073T3 = ('# Version 0.0.73 test 3: settings at an older build\'s default moved to the new default once '
                  '(SHADOW MATCH ON, PUNISHER VAN ON; migrated; a value chosen after this line stays, numbers follow the new order)')


def migrate_old_defaults(text, moves, aliases=None):
    """`moves` ({key: (old names in old order, old default, new default)}) applied to live.txt `text`
    written before the marker: {key: word} to write back. A value at the old default (word or old number)
    becomes the new default; another value is written as the word it meant then (an old number turned into
    its old word); an unknown value is left alone. Pure, never raises."""
    aliases = aliases or {}
    changed = {}
    for line in text.splitlines():
        name, sep, value = line.split('#', 1)[0].partition('=')
        name, value = name.strip().lower().lstrip('-'), value.strip().lower()
        name = ALIASES.get(name, name)
        if not sep or name not in moves:
            continue
        old_names, old_default, new_default = moves[name]
        word = aliases.get(name, {}).get(value, value)
        if word.isdigit():
            word = old_names[int(word)] if int(word) < len(old_names) else None
        if word is None or word not in old_names:
            continue
        target = new_default if word == old_default else word
        if target != value:
            changed[name] = target
    return changed


def migrate_073t3(path):
    """Version 0.0.73 test 3, once: MIGRATE_073T3 applied to an existing live.txt (migrate_old_defaults),
    then the MIGRATED_073T3 marker. No live.txt: nothing (the new defaults apply, HEADER carries the
    marker). The line to show, or None. Never raises."""
    path = pathlib.Path(path)
    try:
        if not path.exists():
            return None
        text = path.read_text(encoding='utf-8-sig', errors='replace')
    except OSError:
        return None
    if MIGRATED_073T3 in text:
        return None
    try:
        changed = migrate_old_defaults(text, MIGRATE_073T3, MIGRATE_073T3_ALIASES)
        if changed:
            text = write_back(text, changed)
        if text and not text.endswith('\n'):
            text += '\n'
        write_atomically(path, text + MIGRATED_073T3 + '\n')
    except (OSError, ValueError):
        return None
    if not changed:
        return None
    return ('live.txt: set once to the 0.0.73 test 3 defaults: ' +
            ', '.join(f'{k} = {v}' for k, v in changed.items()) + ' (change it again and it stays).')


HEADER += MIGRATED_073T3 + '\n'


def next_launch_supersample(path):
    """The eye supersample next-launch.txt asks for (100-200), or None."""
    try:
        text = pathlib.Path(path).read_text(encoding='utf-8-sig', errors='replace')
    except OSError:
        return None
    for line in text.splitlines():
        name, sep, value = line.split('#', 1)[0].partition('=')
        if sep and name.strip().lower() == 'supersample':
            try:
                number = int(value.strip().rstrip('%'))
            except ValueError:
                return None
            return number if SUPERSAMPLES[0] <= number <= SUPERSAMPLES[1] else None
    return None


def migrate_next_launch(path):
    """Update 23, once: a next-launch.txt at `path` written before Update 23 that asks for more than 100%
    eye supersample (a heavy frame-rate cost the October 23 player did not want kept) is set back to 100
    (OFF). A file that carries SUPERSAMPLE_MIGRATED (written by Update 23 or later) is left alone, so a value
    chosen from now on stays. The line to show, or None (nothing changed, no file, or it could not be read
    or written). Never raises."""
    path = pathlib.Path(path)
    try:
        if not path.exists():
            return None
        text = path.read_text(encoding='utf-8-sig', errors='replace')
    except OSError:
        return None
    if SUPERSAMPLE_MIGRATED in text:
        return None
    old = next_launch_supersample(path)
    try:
        write_atomically(path, NEXT_LAUNCH_HEADER + 'supersample = 100\n')
    except OSError:
        return None
    if not old or old <= 100:
        return None
    return (f'next-launch.txt: eye supersample {old}% set back once to OFF (100%), the new default (Update 23): '
            'it costs a lot of frame rate. Choose it again on the hologram\'s GRAPHICS page (needs a game '
            'restart) and it stays.')


def write_next_launch(path, supersample):
    """Keeps the hologram's NEXT LAUNCH eye supersample for the next start. Never raises; False on error."""
    if not SUPERSAMPLES[0] <= int(supersample) <= SUPERSAMPLES[1]:
        return False
    try:
        write_atomically(path, NEXT_LAUNCH_HEADER + f'supersample = {int(supersample)}\n')
    except OSError:
        return False
    return True


# Update 19: the folder the wrist hologram's MUSIC page plays from (the mod makes it too).
MUSIC_README = """Spider-Man 2 VR: the wrist hologram's MUSIC page (page 5) plays the songs in this folder.

Put MP3, WAV or FLAC files here (M4A/AAC and WMA work too, with Windows' own Media Foundation).
New songs show up within a few seconds, even while the game runs; they play in A-Z order and the
next one starts by itself. A file that will not play is skipped. Sub-folders are not read.
The page's VOLUME is only the music's: the game's own sound is not changed. To keep the game's
background music quiet meanwhile, set Music Volume to 0 in the game's Settings, Audio.
"""


def ensure_music_folder(root):
    """Makes the Music folder (and its README) under `root`; the line to show. Never raises."""
    folder = pathlib.Path(root)/'Music'
    try:
        folder.mkdir(exist_ok=True)
        readme = folder/'README.txt'
        if not readme.exists():
            readme.write_text(MUSIC_README, encoding='utf-8')
    except OSError as error:
        return f'Music: could not make {folder} ({error}); the MUSIC page will list no songs.'
    return f'Music: put MP3, WAV or FLAC songs in {folder} (wrist hologram, MUSIC page).'
