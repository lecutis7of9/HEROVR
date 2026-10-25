# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 35: FAR SHADOWS' native record (stereo_probe.cpp SpidyFarShadowsData,
include/spidy/far_shadows.hpp Data) for the console and the game-vr report.

The cascade fit 0x2a8dc80 gives a view far cascades only when IsActiveView says yes
(0x2a8e145); the eyes never were the active view, so each eye's shadows ended at its
base box 1-2 blocks ahead. FAR SHADOWS answers "active" for an eye there (EYES), and
in EYES+CACHE (plan B) also for the left eye inside the far cache (0x2ad1238).
The record counts each eye's fits and those with far cascades (far > 0 means it
worked), and the game view's for comparison.
"""
import struct

MAGIC, VERSION, SIZE = 0x53524146, 1, 256
MODES = {0: 'OFF', 1: 'EYES', 2: 'EYES+CACHE'}
# far_shadows::reasonText, in Reason order (tests/far_shadows_tests.py checks they agree).
REASONS = {
    0: None,
    1: "this game build differs, so it was left alone",
    2: "the eyes still got no far shadows",
    3: "the game made no far shadows here either",
    4: "the frame rate halved",
    5: "the eyes' shadows are fitted elsewhere",
}
EYE_AT = 120


def _round(values):
    return [round(v, 3) for v in values]


def parse(raw):
    """Data bytes -> dict, None while it is being written. Raises on a protocol mismatch."""
    if len(raw) != SIZE:
        return None
    magic, version, size, packed = struct.unpack_from('<4I', raw, 0)
    if (magic, version, size) != (MAGIC, VERSION, SIZE):
        raise RuntimeError('Native FAR SHADOWS protocol mismatch')
    sequence, = struct.unpack_from('<Q', raw, 248)
    if sequence & 1:
        return None
    hooked, changes, reverts, last_revert = struct.unpack_from('<4I', raw, 16)
    answered_fit, answered_cache, game_fits, other_fits = struct.unpack_from('<4Q', raw, 32)
    game_base, game_far = struct.unpack_from('<2I', raw, 64)
    game_lod = struct.unpack_from('<4f', raw, 72)
    game_far_lod = struct.unpack_from('<4f', raw, 88)
    judged_fits, judged_far = struct.unpack_from('<2Q', raw, 104)
    eyes = []
    for i in range(2):
        o = EYE_AT + 64 * i
        fits, far_fits = struct.unpack_from('<2Q', raw, o)
        base, far, kind = struct.unpack_from('<3I', raw, o + 16)
        lod = struct.unpack_from('<4f', raw, o + 32)
        far_lod = struct.unpack_from('<4f', raw, o + 48)
        eyes.append(dict(eye='left' if i == 0 else 'right', fits=fits, fits_with_far=far_fits,
                         base_cascades=base, far_cascades=far, view_kind=kind,
                         lod_consts=_round(lod), far_lod_consts=_round(far_lod)))
    mode, reason = packed & 0xff, packed >> 8 & 0xff
    working = all(e['far_cascades'] > 0 for e in eyes)
    return dict(mode=MODES.get(mode, f'mode {mode}'), reverted=REASONS.get(reason, f'reason {reason}'),
                hooked=bool(hooked), mode_changes=changes, reverts=reverts,
                last_revert=REASONS.get(last_revert, f'reason {last_revert}'),
                answered=dict(fit=answered_fit, cache=answered_cache),
                game_view=dict(fits=game_fits, base_cascades=game_base, far_cascades=game_far,
                               lod_consts=_round(game_lod), far_lod_consts=_round(game_far_lod)),
                other_fits=other_fits, judged=dict(eye_fits=judged_fits, with_far=judged_far),
                eyes=eyes, eyes_have_far=working,
                status=('working (both eyes have far cascades)' if working and mode
                        else 'off' if not mode else 'judging or not working'))


def snapshot(game, address):
    for _ in range(8):
        raw = game.read(address, SIZE)
        if len(raw) != SIZE:
            return None
        data = parse(raw)
        if data is not None and raw[248:256] == game.read(address + 248, 8):
            return data
    return None


def said_key(data):
    if not data:
        return None
    return (data['mode'], data['reverted'], data['eyes_have_far'], data['game_view']['far_cascades'] > 0)


def console_line(data):
    """One line for the console when the mode or its outcome changes."""
    if not data:
        return None
    if not data['hooked']:
        return 'FAR SHADOWS: N/A (the cascade fit differs in this game build; shadows left as they were).'
    l, r = data['eyes']
    line = (f"FAR SHADOWS: {data['mode']}, eyes far cascades {l['far_cascades']}/{r['far_cascades']}"
            f" (base {l['base_cascades']}/{r['base_cascades']}), game view far {data['game_view']['far_cascades']}")
    if data['reverted']:
        line += f" - back on its own: {data['reverted']}"
    return line + '.'


def report_fields(data):
    if not data:
        return dict(far_shadows=dict(status='not read'))
    return dict(far_shadows=data)
