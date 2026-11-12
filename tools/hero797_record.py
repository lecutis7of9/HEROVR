# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""79.7 records for the report: SpidyWeblineMarkerData (include/spidy/webline_marker.hpp, 104 bytes: the BOLD WEB LINE MARKER's
set / drawn / first-frame counters) and SpidyJitterLevelData (include/spidy/jitter_slow.hpp, 624 bytes: per EYE JITTER level
the TemporalAa job's lag and the left-right difference of the eyes). No Windows imports: `game` is anything with
read(address, size) -> bytes."""
import struct

WEBLINE_EXPORT = 'SpidyWeblineMarkerData'
WEBLINE_MAGIC = 0x4b4d4c57  # WLMK
WEBLINE_SIZE = 104
JITTER_EXPORT = 'SpidyJitterLevelData'
JITTER_MAGIC = 0x4c4a4a37  # 7JJL
JITTER_SIZE = 624
LEVELS = ('off', 'low', 'med', 'full', 'e5d2746')
LEVEL = struct.Struct('<2Q8Q2Q2Q2f')  # runs[2], lag[2][4], unmatched[2], compared+differs, lrDiffMax, lrDiffSum = 120
JITTER_HEADER = struct.Struct('<4IQ')  # 24
WEBLINE = struct.Struct('<4I11Q')  # 16 + 88 = 104


def webline_marker_parse(raw):
    """The marker counters as a dict, or None (wrong size, magic or version)."""
    if raw is None or len(raw) != WEBLINE_SIZE:
        return None
    magic, version, size, _, gestures, s0, s1, d0, d1, first, first_valid, valid, invalid, no_target, preview = \
        WEBLINE.unpack(raw)
    if magic != WEBLINE_MAGIC or version != 1 or size != WEBLINE_SIZE:
        return None
    set_frames = {'left': s0, 'right': s1}
    drawn = {'left': d0, 'right': d1}
    out = dict(gestures=gestures, marker_set_frames=set_frames, marker_drawn_frames=drawn, first_frames=first,
               first_frame_valid=first_valid, valid_frames=valid, invalid_frames=invalid, no_target_frames=no_target,
               preview_line_drawn_frames=preview)
    total_set = s0 + s1
    out['drawn_of_set_percent'] = round(100 * (d0 + d1) / total_set, 1) if total_set else None
    return out


def webline_marker_snapshot(game, address):
    if not address:
        return None
    try:
        raw = game.read(address, WEBLINE_SIZE)
    except OSError:
        return None
    return webline_marker_parse(raw)


def webline_marker_line(record):
    """The console's word on the marker, or None before any gesture."""
    if not record or not record['gestures']:
        return None
    s, d = record['marker_set_frames'], record['marker_drawn_frames']
    return (f"79.7 WEBLINE MARKER (report): {record['gestures']} first anchors; marker set {s['left']}/{s['right']} frames "
            f"(left/right), drawn {d['left']}/{d['right']} ({record['drawn_of_set_percent']}% of set); first frame had a valid "
            f"second target {record['first_frame_valid']} of {record['first_frames']}; frames valid {record['valid_frames']}, "
            f"not valid {record['invalid_frames']}, no target {record['no_target_frames']}; preview line drawn "
            f"{record['preview_line_drawn_frames']}.")


def jitter_levels_parse(raw):
    """Per level counters as a dict, or None. lag_frames: runs whose job read the lens value 0 / 1 / 2 / 3+ frames back."""
    if raw is None or len(raw) != JITTER_SIZE:
        return None
    magic, version, size, level, frames = JITTER_HEADER.unpack_from(raw)
    if magic != JITTER_MAGIC or version != 1 or size != JITTER_SIZE:
        return None
    out = dict(level=LEVELS[level] if level < len(LEVELS) else str(level), lens_calls=frames, levels={})
    for i, name in enumerate(LEVELS):
        v = LEVEL.unpack_from(raw, JITTER_HEADER.size + LEVEL.size * i)
        runs = (v[0], v[1])
        if not (runs[0] or runs[1]):
            continue
        lag = {'left': list(v[2:6]), 'right': list(v[6:10])}
        unmatched = {'left': v[10], 'right': v[11]}
        compared, differs, diff_max, diff_sum = v[12], v[13], v[14], v[15]
        entry = dict(runs={'left': runs[0], 'right': runs[1]}, lag_frames=lag, matched_nothing=unmatched,
                     lr_compared=compared, lr_differs=differs, lr_diff_max=float(f'{diff_max:.4g}'))
        if compared:
            entry['lr_differs_percent'] = round(100 * differs / compared, 1)
            entry['lr_diff_mean'] = float(f'{diff_sum / compared:.4g}')
        for side in ('left', 'right'):
            if runs[0 if side == 'left' else 1]:
                n = runs[0 if side == 'left' else 1]
                entry.setdefault('lag_percent', {})[side] = [round(100 * x / n, 1) for x in lag[side]]
        out['levels'][name] = entry
    return out


def jitter_levels_snapshot(game, address):
    if not address:
        return None
    try:
        raw = game.read(address, JITTER_SIZE)
    except OSError:
        return None
    return jitter_levels_parse(raw)


def jitter_levels_line(record):
    """The console's word on the per-level jitter counters, or None before a run."""
    if not record or not record['levels']:
        return None
    parts = []
    for name, e in record['levels'].items():
        lp = e.get('lag_percent', {})
        parts.append(f"{name}: lag 0/1/2/3+ frames left {lp.get('left')} right {lp.get('right')} %, "
                     f"eyes differ {e.get('lr_differs_percent')}% (max {e['lr_diff_max']})")
    return '79.7 JITTER LEVELS (report): ' + '; '.join(parts) + '.'
