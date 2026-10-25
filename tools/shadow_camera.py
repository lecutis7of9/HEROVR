# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 31: SHADOW CAMERA's native state (stereo_probe.cpp SpidyShadowCameraData,
include/spidy/shadow_camera.hpp Data) for the console and the game-vr report.

SHADOW CAMERA is a live GRAPHICS-page test: each press steps OFF -> STOCK NEAR ->
FLAT CAMERA -> OFF. Every change is logged natively (GetTickCount64 time, the mode,
and the frames the previous mode was applied), so one session can compare all
three. The game view's own key-light cascades are sampled every frame; their
offsets come from the Remastered port and are unconfirmed in SM2 (counts of 0 say
so), so the numbers are candidates.

Update 32: SHADOW CAMERA left the hologram and live.txt (no visible effect in the
Oct 23 run; 0x2dd8d20 never ran for the game view or an eye). The record now also
carries FitViews: what the views 0x2dd8d20 does run for are (the flat camera, the
head, or other), who calls it (RVAs, candidates), and a pose sample every 2 s.
"""
import math
import struct

MAGIC, VERSION, SIZE = 0x4d414353, 2, 4272
MODES = {0: 'OFF', 1: 'STOCK NEAR', 2: 'FLAT CAMERA'}
# shadow_camera::reasonText, in Reason order (tests/shadow_camera_tests.py checks they agree).
REASONS = {
    0: None,
    1: "the engine rejected the head view",
    2: "the flat near plane was out of range",
}
MAX_EVENTS = 32
EVENTS_AT, GAME_AT, FIT_AT = 88, 856, 992
FIT_KINDS = ('unreadable', 'flat', 'head', 'other')
FIT_SLOTS, FIT_SAMPLES, FIT_SAMPLE_BYTES = 8, 32, 96


def parse(raw):
    """Data bytes -> dict, None while it is being written. Raises on a protocol mismatch."""
    if len(raw) != SIZE:
        return None
    magic, version, size, mode = struct.unpack_from('<4I', raw, 0)
    if (magic, version, size) != (MAGIC, VERSION, SIZE):
        raise RuntimeError('Native SHADOW CAMERA protocol mismatch')
    if struct.unpack_from('<q', raw, 16)[0] & 1:
        return None
    reason, toggles = struct.unpack_from('<2I', raw, 24)
    applied = struct.unpack_from('<3Q', raw, 32)
    fit_game, fit_eye, fit_other = struct.unpack_from('<3Q', raw, 56)
    events, fit_hook = struct.unpack_from('<2I', raw, 80)
    log = []
    first = max(0, events - MAX_EVENTS)
    for i in range(first, events):
        ms, m, r, frames = struct.unpack_from('<Q2IQ', raw, EVENTS_AT + (i % MAX_EVENTS) * 24)
        log.append(dict(index=i, ms=ms, mode=MODES.get(m, f'mode {m}'), reason=REASONS.get(r, f'reason {r}'),
                        previous_frames=frames))
    samples = struct.unpack_from('<Q', raw, GAME_AT)[0]
    counts = struct.unpack_from('<2I', raw, GAME_AT + 8)
    readable = struct.unpack_from('<I', raw, GAME_AT + 16)[0]
    head = struct.unpack_from('<3f', raw, GAME_AT + 24)
    near = struct.unpack_from('<f', raw, GAME_AT + 36)[0]
    extent = [struct.unpack_from('<3f', raw, GAME_AT + 40 + 12 * i) for i in range(4)]
    centre = [struct.unpack_from('<3f', raw, GAME_AT + 88 + 12 * i) for i in range(4)]
    count = min(sum(counts), 4)
    cascades = []
    for i in range(count):
        d = math.dist(head, centre[i]) if all(map(math.isfinite, head + centre[i])) else None
        cascades.append(dict(half_m=[round(v, 2) for v in extent[i]], centre_from_head_m=round(d, 2) if d else d))
    fit = parse_fit(raw)
    return dict(mode=MODES.get(mode, f'mode {mode}'), reverted=REASONS.get(reason, f'reason {reason}'),
                fit_views=fit,
                toggles=toggles, applied_frames={MODES[k]: applied[k] for k in range(3)},
                fit_hook='on' if fit_hook else 'not hooked',
                fit_calls=dict(game_view=fit_game, eyes=fit_eye, other=fit_other), events=events, log=log,
                game_cascades=dict(samples=samples, readable=bool(readable), counts=list(counts),
                                   near_m=round(near, 4), cascades=cascades,
                                   status='candidates' if count else 'SM2TODO (no cascades at the Remastered offsets)'))


def _pitch_yaw(fwd):
    n = math.sqrt(sum(v * v for v in fwd))
    if not n or not all(map(math.isfinite, fwd)):
        return None
    return [round(math.degrees(math.asin(max(-1.0, min(1.0, fwd[1] / n)))), 1),
            round(math.degrees(math.atan2(fwd[0], fwd[2])), 1)]


def parse_fit(raw):
    """Update 32: FitViews (shadow_camera.hpp) -> dict."""
    at = FIT_AT
    by_kind = struct.unpack_from('<4Q', raw, at)
    callers = struct.unpack_from('<8I', raw, at + 32)
    caller_calls = struct.unpack_from('<8I', raw, at + 64)
    views = struct.unpack_from('<8Q', raw, at + 96)
    view_calls = struct.unpack_from('<8I', raw, at + 160)
    samples, overflow = struct.unpack_from('<2I', raw, at + 192)
    ring = []
    for i in range(max(0, samples - FIT_SAMPLES), samples):
        o = at + 208 + (i % FIT_SAMPLES) * FIT_SAMPLE_BYTES
        ms, = struct.unpack_from('<Q', raw, o)
        view, flat, head = (struct.unpack_from('<6f', raw, o + 8 + 24 * k) for k in range(3))
        near, far = struct.unpack_from('<2f', raw, o + 80)
        kind, caller = struct.unpack_from('<2I', raw, o + 88)
        row = dict(ms=ms, kind=FIT_KINDS[kind] if kind < 4 else f'kind {kind}', caller=f'0x{caller:x}',
                   near_m=round(near, 4), far_m=round(far, 1))
        for name, pose in (('view', view), ('flat', flat), ('head', head)):
            if any(pose):
                row[name] = dict(pos=[round(v, 2) for v in pose[:3]], pitch_yaw=_pitch_yaw(pose[3:]))
        for name in ('flat', 'head'):
            if 'view' in row and name in row:
                row[f'view_to_{name}_m'] = round(math.dist(view[:3], (flat if name == 'flat' else head)[:3]), 2)
        ring.append(row)
    return dict(by_kind=dict(zip(FIT_KINDS, by_kind)),
                callers=[dict(rva=f'0x{r:x}', calls=n) for r, n in zip(callers, caller_calls) if n],
                views=sum(1 for n in view_calls if n), view_calls=[n for n in view_calls if n],
                overflow=overflow, samples=samples, ring=ring, label='candidates')


def snapshot(game, address):
    for _ in range(8):
        raw = game.read(address, SIZE)
        if len(raw) != SIZE:
            return None
        data = parse(raw)
        if data is not None and raw[16:24] == game.read(address+16, 8):
            return data
    return None


def new_lines(data, said):
    """Console lines for the log events not yet printed; said is the count printed so far."""
    if not data:
        return [], said
    lines = []
    start = data['log'][0]['ms'] if data['log'] else 0
    for e in data['log']:
        if e['index'] < said:
            continue
        line = f"SHADOW CAMERA: {e['mode']} (change {e['index'] + 1}, +{(e['ms'] - start) / 1000:.1f} s"
        line += f", the last mode ran {e['previous_frames']} frames)"
        if e['reason']:
            line += f" - back to OFF on its own: {e['reason']}"
        lines.append(line + '.')
    return lines, max(said, data['events'])


def segments(data):
    """How long each mode ran: [{mode, seconds, frames}] from the change log."""
    if not data or not data['log']:
        return []
    out, log = [], data['log']
    for a, b in zip(log, log[1:]):
        out.append(dict(mode=a['mode'], seconds=round((b['ms'] - a['ms']) / 1000, 1), frames=b['previous_frames']))
    out.append(dict(mode=log[-1]['mode'], seconds=None, frames=None))  # still running at the snapshot
    return out


def mode_step(per_mode, data):
    """The latest game-view cascade sample under each mode (the report compares them)."""
    if data and data['game_cascades']['samples']:
        per_mode[data['mode']] = data['game_cascades']
    return per_mode


def report_fields(data, per_mode=None):
    if not data:
        return dict(shadow_camera_status='unknown', shadow_camera=None)
    return dict(shadow_camera_status=data['mode'],
                shadow_camera=dict(data, segments=segments(data), cascades_by_mode=dict(per_mode or {})))
