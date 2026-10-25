# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 28.1: EYE LOD's native state (stereo_probe.cpp SpidyEyeLodData,
include/spidy/native_view.hpp EyeLodData) for the report and the console.

EYE LOD's reason used to go only to OutputDebugStringA, which nothing in a
session shows; the launcher now prints it once and writes eye_lod_status and
eye_lod_reason into the game-vr report.
"""
import struct

MAGIC, VERSION, SIZE = 0x444f4c45, 1, 88
STATES = {0: 'off', 1: 'on', 2: 'failed'}
# native_view::eyeLodReasonText, in EyeLodReason order (tests/eye_lod_tests.py checks they agree).
REASONS = {
    0: None,
    1: "the eye view could not be read",
    2: "the write to the eye view failed",
    3: "the eye view's value was out of range",
    4: "the eye view's value never held still",
}


def parse(raw):
    """EyeLodData bytes -> dict, None while it is being written. Raises on a protocol mismatch."""
    if len(raw) != SIZE:
        return None
    magic, version, size, offset = struct.unpack_from('<4I', raw, 0)
    if (magic, version, size) != (MAGIC, VERSION, SIZE):
        raise RuntimeError('Native EYE LOD protocol mismatch')
    if struct.unpack_from('<q', raw, 16)[0] & 1:
        return None
    state, reason = struct.unpack_from('<2I', raw, 24)
    frames, writes, held, settling = struct.unpack_from('<4Q', raw, 32)
    last, base, target, flat, eye_scale = struct.unpack_from('<5f', raw, 64)
    eye = struct.unpack_from('<I', raw, 84)[0]
    status = STATES.get(state, 'unknown')
    why = REASONS.get(reason, f'reason {reason}') if status == 'failed' else None
    if status == 'on' and not writes and not held:
        why = 'waiting for a steady value at the eye view' if frames else 'no eye view reached EYE LOD yet'
    return dict(status=status, reason=why, offset=f'+0x{offset:x}', frames=frames, writes=writes, held=held,
                settling=settling, last_read=round(last, 5), engine_value=round(base, 5),
                written_value=round(target, 5), flat_game_scale=round(flat, 5), eye_lens_scale=round(eye_scale, 5),
                failed_eye=eye if status == 'failed' else None)


def snapshot(game, address):
    for _ in range(8):
        raw = game.read(address, SIZE)
        if len(raw) != SIZE:
            return None
        data = parse(raw)
        if data is not None and raw[16:24] == game.read(address+16, 8):
            return data
    return None


def console_line(data):
    """One line per change of EYE LOD's state (None when there is nothing new to say)."""
    if not data:
        return None
    if data['status'] == 'failed':
        return (f"EYE LOD: off for this session - {data['reason']} (view {data['offset']} read "
                f"{data['last_read']:g} on eye {data['failed_eye']}); what it changed was given back.")
    if data['status'] == 'off' and (data['writes'] or data['held']):
        # Update 29: say when it is turned off again (28.1 printed only "on", once).
        return "EYE LOD: off - the eye views keep the game's own value."
    if data['status'] == 'on' and data['writes']:
        return (f"EYE LOD: on - view {data['offset']} {data['engine_value']:g} raised to "
                f"{data['written_value']:g} (eye lens {data['eye_lens_scale']:g}, flat game "
                f"{data['flat_game_scale']:g}).")
    return None


def said_key(data):
    """What a printed line is about: one line per state and reason, never per frame's values."""
    return (data['status'], data['reason']) if data else None


def report_fields(data):
    """The report's top-level eye_lod_status / eye_lod_reason and the whole eye_lod record."""
    if not data:
        return dict(eye_lod_status='unknown', eye_lod_reason='the EYE LOD data could not be read', eye_lod=None)
    return dict(eye_lod_status=data['status'], eye_lod_reason=data['reason'], eye_lod=data)


def timeline_step(timeline, seconds, data):
    """Update 29: (seconds, status) each time EYE LOD's status changes, for the report."""
    status = data['status'] if data else None
    if status and (not timeline or timeline[-1][1] != status):
        timeline.append((round(seconds, 1), status))
    return timeline
