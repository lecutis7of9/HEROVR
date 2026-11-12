# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""79.7 WALL WATCH: SpidyWallAnimsData (include/spidy/wall_anims.hpp Data, 512 bytes) for the report and the console:
GAME WALL ANIMS ON / OFF, the wall contacts by kind, hand-overs, drops, out-of-world falls, view flips, recoveries, the
seconds on each kind of surface and the last 24 events with their session seconds. The game's own hero state
(HeroStateWallCrawl / WallRun / Perch) is NOT read by the mod: the surface kinds come from the swing bridge. No Windows
imports: `game` is anything with read(address, size) -> bytes."""
import struct

EXPORT = 'SpidyWallAnimsData'
MAGIC = 0x4c415737  # 7WAL
SIZE = 512
CLASSES = ('none', 'air', 'ground', 'own wall', 'game wall', 'floor crawl')
KINDS = ('contact', 'hand-over', 'leave', 'drop', 'out of world?', 'view flip', 'toggle', 'deferred', 'recover', 'auto-off')
HEADER = struct.Struct('<4I6I8I6f2f4I')  # 16 + 24 + 32 + 24 + 8 + 16 = 120 bytes
EVENT = struct.Struct('<I4B2f')  # 16 bytes


def wall_anims_parse(raw):
    """The record as a dict, or None (wrong size, magic or version)."""
    if raw is None or len(raw) != SIZE:
        return None
    h = HEADER.unpack_from(raw)
    magic, version, size, flags = h[:4]
    if magic != MAGIC or version != 1 or size != SIZE:
        return None
    contacts = h[4:10]
    (hand_overs, drops, drops_game, far_falls, flips, recoveries, toggles, deferrals) = h[10:18]
    seconds = h[18:24]
    max_tilt, last_speed = h[24:26]
    event_total, event_count, session_ms, _ = h[26:30]
    events = []
    for i in range(min(event_count, 24)):
        ms, kind, a_cls, b_cls, n, a, b = EVENT.unpack_from(raw, HEADER.size + EVENT.size * i)
        events.append(dict(session_seconds=round(ms / 1000, 1), kind=KINDS[kind] if kind < len(KINDS) else str(kind),
                           from_surface=CLASSES[a_cls] if a_cls < len(CLASSES) else str(a_cls),
                           to_surface=CLASSES[b_cls] if b_cls < len(CLASSES) else str(b_cls), n=n,
                           a=float(f'{a:.4g}'), b=float(f'{b:.4g}')))
    return dict(game_wall_anims='on' if flags & 1 else 'off', wanted='on' if flags & 2 else 'off',
                waiting_for_wall_to_end=bool(flags & 4), recovering=bool(flags & 8), auto_off=bool(flags & 16),
                contacts={CLASSES[i]: contacts[i] for i in range(3, 6)}, hand_overs=hand_overs, drops=drops,
                drops_off_game_wall=drops_game, out_of_world=far_falls, view_flips=flips, recoveries=recoveries,
                toggles=toggles, deferrals=deferrals,
                seconds={CLASSES[i]: round(seconds[i], 1) for i in range(1, 6)}, max_yaw_jump_degrees=round(max_tilt, 1),
                last_contact_speed=round(last_speed, 1), events_total=event_total, session_seconds=round(session_ms / 1000, 1),
                events=events, hero_state_read=False,
                note='the mod does not read the game hero state (HeroStateWallCrawl / WallRun / Perch); surfaces come from the swing bridge')


def wall_anims_snapshot(game, address):
    """The WALL WATCH at `address` (None: no export, unreadable or not the record)."""
    if not address:
        return None
    try:
        raw = game.read(address, SIZE)
    except OSError:
        return None
    return wall_anims_parse(raw)


def wall_anims_line(record):
    """The console's '79.7 WALL' line (wall_anims_snapshot), or None before anything happened."""
    if not record or not (record['events_total'] or record['toggles']):
        return None
    c, s = record['contacts'], record['seconds']
    return (f"79.7 WALL (report): GAME WALL ANIMS {record['game_wall_anims'].upper()}"
            f"{' (wanted ' + record['wanted'].upper() + ')' if record['wanted'] != record['game_wall_anims'] else ''}; "
            f"contacts own {c['own wall']} / game {c['game wall']} / floor {c['floor crawl']}; hand-overs {record['hand_overs']}; "
            f"drops {record['drops']} (off the game's wall {record['drops_off_game_wall']}); out-of-world? {record['out_of_world']}; "
            f"view yaw jumps put back {record['view_flips']}; recoveries {record['recoveries']}; "
            f"seconds own wall {s['own wall']}, game wall {s['game wall']}, floor crawl {s['floor crawl']}, air {s['air']}; "
            f"largest yaw jump {record['max_yaw_jump_degrees']} deg; the game's hero state is not read.")
