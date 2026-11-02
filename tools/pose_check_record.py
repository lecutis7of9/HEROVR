# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 POSE CHECK: SpidyPoseCheckData (include/spidy/pose_check.hpp Data) for the report and the console.
No Windows imports: `game` is anything with read(address, size) -> bytes."""
import struct

EXPORT = 'SpidyPoseCheckData'
MAGIC = 0x4b434850  # PHCK
SIZE = 256
OVER_DEGREES, OVER_MM = 0.05, 2.0  # pose_check::overDegrees / overMm


def pose_check_parse(raw):
    """The record from its 256 bytes, or None (wrong size, magic, version or length)."""
    if raw is None or len(raw) != SIZE:
        return None
    magic, version, size, flags = struct.unpack_from('<4I', raw)
    if magic != MAGIC or size != SIZE or version != 1:
        return None
    out = dict(version=version, flags=flags)
    for i, eye in enumerate(('left', 'right')):
        base = 16 + 64 * i
        checked, unread, over_degrees, over_mm = struct.unpack_from('<4Q', raw, base)
        last_deg, max_deg, last_mm, max_mm = struct.unpack_from('<4f', raw, base + 32)
        sum_deg, sum_mm = struct.unpack_from('<2d', raw, base + 48)
        out[eye] = dict(checked=checked, unread=unread, over_degrees=over_degrees, over_mm=over_mm,
                        last_degrees=round(last_deg, 4), max_degrees=round(max_deg, 4),
                        mean_degrees=round(sum_deg / checked, 4) if checked else None,
                        last_mm=round(last_mm, 3), max_mm=round(max_mm, 3),
                        mean_mm=round(sum_mm / checked, 3) if checked else None)
    submits, reused = struct.unpack_from('<2Q', raw, 144)
    turn_last, turn_max = struct.unpack_from('<2f', raw, 160)
    turn_sum, = struct.unpack_from('<d', raw, 168)
    turn_over, = struct.unpack_from('<Q', raw, 176)
    travel_max, age_last, age_max, _ = struct.unpack_from('<4f', raw, 184)
    age_sum, = struct.unpack_from('<d', raw, 200)
    sequence, = struct.unpack_from('<Q', raw, 208)
    command = struct.unpack_from('<4f', raw, 216)
    out.update(submits=submits, reused=reused, sequence=sequence,
               world_turn=dict(last_degrees=round(turn_last, 4), max_degrees=round(turn_max, 4),
                               mean_degrees=round(turn_sum / submits, 4) if submits else None, over=turn_over),
               image_age_ms=dict(last=round(age_last, 2), max=round(age_max, 2),
                                 mean=round(age_sum / submits, 2) if submits else None),
               travel_max_mm=round(travel_max, 1))
    for i, eye in enumerate(('left', 'right')):
        out[eye]['command_max_degrees'] = round(command[i], 4)
        out[eye]['command_max_mm'] = round(command[2 + i], 3)
    return out


def pose_check_snapshot(game, address):
    """The POSE CHECK record at `address` (None: no export, unreadable or not the record)."""
    if not address:
        return None
    try:
        raw = game.read(address, SIZE)
    except OSError:
        return None
    return pose_check_parse(raw)


def pose_check_verdict(record):
    """'match', 'differs' or None (nothing checked): whether any eye's rendered camera left the submitted one."""
    if not record or not (record['left']['checked'] or record['right']['checked']):
        return None
    over = any(record[e]['over_degrees'] or record[e]['over_mm'] for e in ('left', 'right'))
    return 'differs' if over else 'match'


def pose_check_line(record):
    """The console's word on render-vs-submit poses (pose_check_snapshot), or None before any immersive submit."""
    verdict = pose_check_verdict(record)
    if not verdict:
        return None
    eyes = '; '.join(
        f"{e} max {record[e]['max_degrees']:.3f} deg / {record[e]['max_mm']:.1f} mm, mean "
        f"{record[e]['mean_degrees'] or 0:.3f} deg / {record[e]['mean_mm'] or 0:.1f} mm "
        f"(over {record[e]['over_degrees']} / {record[e]['over_mm']} of {record[e]['checked']})"
        for e in ('left', 'right'))
    turn = record['world_turn']
    word = ('MATCH: each eye was shown with the pose it was rendered with' if verdict == 'match' else
            'DIFFERS: an eye was shown with a pose it was not rendered with')
    return (f"POSE CHECK render vs submit: {word}. {eyes}. World turned in the room before submit: max "
            f"{turn['max_degrees']:.3f} deg, mean {turn['mean_degrees'] or 0:.3f} deg, {turn['over']} of "
            f"{record['submits']} submits over {OVER_DEGREES} deg; image age mean "
            f"{record['image_age_ms']['mean'] or 0:.1f} ms, max {record['image_age_ms']['max']:.1f} ms.")
