# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 33/34: SHADOW DISTANCE's native state (src/native_shadow_distance.cpp
SpidyShadowDistanceData, include/spidy/shadow_distance.hpp Data) for the console and
the game-vr report.

The slider is locked until a read-only search of this session's memory finds a value
that equals (or is in the same proportions as) the live sun cascade sizes and holds
still. Every address here is a candidate: only one whose change made the sun boxes
follow (outcome 'worked') is a confirmed finding, and then only for this game build.
Update 34 (layout version 2): EnvLightingDef objects found in the heap, and the sun boxes
before and after each trial.
"""
import struct

MAGIC, VERSION, SIZE = 0x54534453, 2, 2360
CANDIDATES_AT, EVENTS_AT, TRIALS_AT, ENV_AT = 176, 688, 1464, 2104
MAX_CANDIDATES, MAX_EVENTS, MAX_SEEN, MAX_TRIALS, MAX_ENV = 16, 32, 8, 8, 8
STATES = ('NOT FOUND', 'SEARCHING', 'READY', 'TESTING', 'APPLIED', 'NO EFFECT', 'PUT BACK', 'NOT FOUND')
STATE_KEYS = ('idle', 'searching', 'ready', 'testing', 'applied', 'no effect', 'reverted', 'not found')
# shadow_distance::reasonText, in Reason order (tests/shadow_distance_tests.py checks they agree).
REASONS = (None, 'the memory write faulted', 'sun shadows stopped coming after the change',
           'the shadow boxes went out of range', 'the game set its own value again', 'the value changed under it',
           'no sun shadows seen; play outdoors by day', 'no value in memory matched the shadow size',
           'the values found did not hold still', 'the memory search faulted', "ADDRESS FINDER's upload hook is off",
           'no sun shadows in view; try outdoors', 'VR ended', 'the shadows stopped following the value',
           'the game already reaches that far')
ORIGINS = ('code', 'module', 'heap', 'EnvLightingDef')
FORMS = ('float', 'double', 'float triple', 'double triple', 'RegLodRange+TotalShadowRange')
VERDICTS = ('followed', 'no change', 'wild')
TARGETS = ('half size', 'width', 'half of half', 'depth', 'full depth', 'half depth')
OUTCOMES = ('untried', 'worked', 'no move', 'moved', 'faulted', 'dropped')
EVENT_KINDS = {1: 'state', 2: 'trial', 3: 'apply', 4: 'put back', 5: 'search step',
               6: 'asked'}  # Update 34.2: each slider value asked (cars; a metres, b the game reach)
CAR_METRES = 4.5


def _name(table, i):
    return table[i] if 0 <= i < len(table) else f'#{i}'


def reason_text(r):
    return REASONS[r] if 0 <= r < len(REASONS) else 'unknown'


def parse(raw):
    """Data bytes -> dict, None while it is being written or not started. Raises on a protocol mismatch."""
    if len(raw) != SIZE:
        return None
    magic, version, size, packed = struct.unpack_from('<4I', raw, 0)
    if magic == 0 and version == 0:
        return None  # never published (ADDRESS FINDER not pressed)
    if (magic, version, size) != (MAGIC, VERSION, SIZE):
        raise RuntimeError('Native SHADOW DISTANCE protocol mismatch')
    if struct.unpack_from('<q', raw, 16)[0] & 1:
        return None
    game, live, want, scale = struct.unpack_from('<4f', raw, 24)
    sizes, candidates, stable, trials = struct.unpack_from('<4I', raw, 40)
    scanned, regions, raw_hits, boxes = struct.unpack_from('<4Q', raw, 56)
    half = struct.unpack_from(f'<{MAX_SEEN}f', raw, 88)
    depth = struct.unpack_from(f'<{MAX_SEEN}f', raw, 120)
    sites = struct.unpack_from('<4I', raw, 152)
    active, events = struct.unpack_from('<2I', raw, 168)
    state, reason = packed & 0xff, packed >> 8 & 0xff
    found = []
    for i in range(min(candidates, MAX_CANDIDATES)):
        o = CANDIDATES_AT + 32 * i
        addr, rva = struct.unpack_from('<QI', raw, o)
        origin, form, target, outcome = struct.unpack_from('<4B', raw, o + 12)
        stride, confirms = struct.unpack_from('<2H', raw, o + 16)
        value = struct.unpack_from('<3f', raw, o + 20)
        row = dict(address=f'0x{addr:x}', rva=None if rva == 0xffffffff else f'0x{rva:x}',
                   origin=_name(ORIGINS, origin), form=_name(FORMS, form),
                   matched='proportions' if target == 0xff else 'all boxes' if target == 0xfe else _name(TARGETS, target),
                   outcome=_name(OUTCOMES, outcome), confirms=confirms,
                   value=[round(v, 4) for v in (value[:2] if form == 4 else value if form >= 2 else value[:1])],
                   label='confirmed this session' if outcome == 1 else 'candidate', in_use=i == active)
        if form >= 2:
            row['stride'] = stride
        found.append(row)
    log = []
    for i in range(max(0, events - MAX_EVENTS), events):
        ms, kind, code, a, b = struct.unpack_from('<Q2I2f', raw, EVENTS_AT + (i % MAX_EVENTS) * 24)
        log.append(dict(index=i, ms=ms, kind=EVENT_KINDS.get(kind, f'kind {kind}'), code=code,
                        a=round(a, 3), b=round(b, 3)))
    trials_logged, env_found = struct.unpack_from('<2I', raw, TRIALS_AT - 8)
    trial_log = []
    for i in range(max(0, trials_logged - MAX_TRIALS), trials_logged):
        o = TRIALS_AT + (i % MAX_TRIALS) * 80
        cand, tscale, matched = struct.unpack_from('<I2f', raw, o)
        verdict, nb, na, is_depth = struct.unpack_from('<4B', raw, o + 12)
        before = struct.unpack_from(f'<{MAX_SEEN}f', raw, o + 16)
        after = struct.unpack_from(f'<{MAX_SEEN}f', raw, o + 48)
        trial_log.append(dict(index=i, candidate=None if cand == 0xffffffff else cand, scale=round(tscale, 3),
                              against=f'{"depth" if is_depth else "box"} {round(matched, 2)} m' if matched else 'all boxes',
                              verdict=_name(VERDICTS, verdict),
                              before_m=[round(v, 2) for v in before[:min(nb, MAX_SEEN)]],
                              after_m=[round(v, 2) for v in after[:min(na, MAX_SEEN)]]))
    env = []
    for i in range(min(env_found, MAX_ENV)):
        o = ENV_AT + 32 * i
        addr, reg_range, total, lo, hi = struct.unpack_from('<Q4f', raw, o)
        reg_n, far_n = struct.unpack_from('<2B', raw, o + 24)
        env.append(dict(address=f'0x{addr:x}', reg_lod_range=round(reg_range, 2), reg_lod_count=reg_n,
                        far_lod_count=far_n, total_shadow_range=round(total, 2),
                        caster_range=[round(lo, 2), round(hi, 2)], label='candidate'))
    return dict(state=_name(STATE_KEYS, state), shown=_name(STATES, state), reason=reason_text(reason) if reason else None,
                game_reach_m=round(game, 2), live_reach_m=round(live, 2), want_cars=round(want),
                scale=round(scale, 3), sun_boxes=boxes,
                sizes=[dict(half_m=round(half[i], 2), depth_m=round(depth[i], 2)) for i in range(min(sizes, MAX_SEEN))],
                upload_sites=[f'0x{s:x}' for s in sites if s], scanned_mb=round(scanned / 2**20, 1),
                regions=regions, raw_hits=raw_hits, stable=stable, trials=trials, candidates=found,
                events=events, log=log, trial_log=trial_log, env_lighting_found=env_found, env_lighting=env)


def snapshot(game, address):
    for _ in range(8):
        raw = game.read(address, SIZE)
        if len(raw) != SIZE:
            return None
        data = parse(raw)
        if data is not None and raw[16:24] == game.read(address + 16, 8):
            return data
    return None


def said_key(data):
    return (data['state'], data['reason'], data['want_cars'], data['trials']) if data else None


def console_line(data):
    """One line for the console when the state changes."""
    if not data:
        return None
    line = f"SHADOW DISTANCE: {data['shown']}"
    if data['game_reach_m']:
        line += f", game reach {data['game_reach_m']} m"
    if data.get('want_cars'):  # Update 34.2: what the slider asks, so "never moved" shows
        line += f", asked {data['want_cars']} cars ({round(data['want_cars'] * CAR_METRES)} m)"
    if data['state'] == 'applied':
        line += f", now {data['live_reach_m']} m ({round(data['live_reach_m'] / CAR_METRES)} cars)"
    if data['state'] in ('searching', 'ready', 'not found', 'no effect'):
        line += f", {data['stable']} steady candidate(s), {data['trials']} trial(s)"
        if data.get('env_lighting_found'):
            line += f", {data['env_lighting_found']} EnvLightingDef"
    if data['reason']:
        line += f" - {data['reason']}"
    return line + '.'


def report_fields(data):
    if not data:
        return dict(shadow_distance=dict(state='not started',
                                         note='press ADDRESS FINDER, then play outdoors in daylight'))
    return dict(shadow_distance=data)
