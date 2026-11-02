# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 TAA FRAME LOG: SpidyTaaFrameLogData (include/spidy/taa_frame_log.hpp Data) for the report and the console.
SPIDY TAA's history pass, eye 0, the last 300 pairs: the view rendered with and the view the history was fetched from
(camera-to-world matrix and lens tangents, our sub-pixel step already in the lens), the step on each, the history UV
of the centre pixel and its shift in eye pixels, that shift with the step's change taken out (the drift), its change
from the pair before, and a still-head flag. Plus the one static line worked out from the code: which matrices the
reprojection uses and how often, with which sign, the step is applied. No Windows imports: `game` is anything with
read(address, size) -> bytes."""
import math
import struct

EXPORT = 'SpidyTaaFrameLogData'
MAGIC = 0x474c4654  # TFLG
CAPACITY, ENTRY, HEADER = 300, 232, 48
SIZE = HEADER + ENTRY * CAPACITY
ENTRY_FORMAT = '<2I2f14f20f20f'
STILL, HISTORY, OURS = 1, 2, 4
JITTERS = ('on', 'off', 'fixed', 'half')  # taa_jitter::Mode (live.txt vr_taa_jitter)
TIMES = ('none', 'once', 'twice')


def _r(values, digits=4):
    return [round(v, digits) for v in values]


def taa_frame_log_parse(raw):
    """{'written', 'width', 'height', 'matrices', 'applied', 'sign', 'frames': [...] oldest first}, or None."""
    if raw is None or len(raw) != SIZE:
        return None
    (magic, version, size, entry, cap, written, matrices, applied, sign, width, height,
     _reserved) = struct.unpack_from('<8Ii3I', raw)
    if magic != MAGIC or version != 1 or size != SIZE or entry != ENTRY or cap != CAPACITY:
        return None
    count = min(written, CAPACITY)
    frames = []
    for n in range(count):
        index = (written - count + n) % CAPACITY
        v = struct.unpack_from(ENTRY_FORMAT, raw, HEADER + ENTRY * index)
        serial, flags, turn, travel = v[0], v[1], v[2], v[3]
        f = v[4:18]
        mode = (flags >> 8) & 0xff
        frames.append(dict(serial=serial, still=bool(flags & STILL), history=bool(flags & HISTORY),
                           spidy_taa=bool(flags & OURS), jitter_mode=JITTERS[mode] if mode < 4 else mode,
                           turn_deg=round(math.degrees(turn), 4), travel_mm=round(travel * 1000, 3),
                           jitter_now_px=_r(f[0:2]), jitter_before_px=_r(f[2:4]), history_uv=_r(f[4:6], 6),
                           shift_px=_r(f[6:8]), expected_px=_r(f[8:10]), drift_px=_r(f[10:12]),
                           delta_px=_r(f[12:14]), view=_r(v[18:34], 6), lens=_r(v[34:38], 6),
                           history_view=_r(v[38:54], 6), history_lens=_r(v[54:58], 6)))
    return dict(written=written, width=width, height=height, matrices='jittered' if matrices else 'unjittered',
                applied=TIMES[applied] if applied < 3 else str(applied), sign='+' if sign >= 0 else '-',
                frames=frames)


def taa_frame_log_snapshot(game, address):
    """The TAA FRAME LOG at `address` (None: no export, unreadable or not the record)."""
    if not address:
        return None
    try:
        return taa_frame_log_parse(game.read(address, SIZE))
    except (OSError, ValueError, struct.error):
        return None


def static_line(record):
    """D: the plain line worked out from the code."""
    if not record:
        return None
    return f"matrices: {record['matrices']}, jitter applied: {record['applied']}, sign: {record['sign']}"


def _stats(values):
    if not values:
        return None, None
    return round(sum(values) / len(values), 4), round(max(values), 4)


def taa_frame_log_summary(record):
    """Max and mean drift (the step's change taken out), still head and all, the history UV step, and the gain:
    the share of the step's change the history shift carries (+1 once, +2 twice, 0 none, -1 the wrong sign)."""
    if not record:
        return None
    frames = [f for f in record['frames'] if f['history']]
    still = [f for f in frames if f['still']]
    drift_still = [math.hypot(*f['drift_px']) for f in still]
    drift_all = [math.hypot(*f['drift_px']) for f in frames]
    delta_still = [math.hypot(*f['delta_px']) for f in still]
    num = sum(f['shift_px'][0] * f['expected_px'][0] + f['shift_px'][1] * f['expected_px'][1] for f in still)
    den = sum(f['expected_px'][0] ** 2 + f['expected_px'][1] ** 2 for f in still)
    gain = round(num / den, 3) if den > 1e-6 else None
    mean_still, max_still = _stats(drift_still)
    mean_all, max_all = _stats(drift_all)
    mean_delta, max_delta = _stats(delta_still)
    modes = sorted({str(f['jitter_mode']) for f in record['frames']})
    return dict(frames=len(record['frames']), history_frames=len(frames), still_frames=len(still),
                drift_still_mean_px=mean_still, drift_still_max_px=max_still,
                drift_all_mean_px=mean_all, drift_all_max_px=max_all,
                uv_step_still_mean_px=mean_delta, uv_step_still_max_px=max_delta, measured_gain=gain,
                jitter_modes=modes, static=static_line(record))


def taa_frame_log_report(record):
    """The report's section: the static line, the summary and every frame."""
    if not record:
        return None
    return dict(static=static_line(record), width=record['width'], height=record['height'],
                written=record['written'], summary=taa_frame_log_summary(record), frames=record['frames'])


def _n(v, fmt='{:.3f}'):
    return '-' if v is None else fmt.format(v)


def taa_frame_log_line(section):
    """One console line from the report's section (or a record's summary)."""
    s = section and (section.get('summary') if 'summary' in section else section)
    if not s:
        return None
    gain = s['measured_gain']
    counted = '-' if gain is None else ('once' if abs(gain - 1) < .25 else 'twice' if abs(gain - 2) < .25
                                        else 'none' if abs(gain) < .25 else 'WRONG SIGN' if gain < 0 else 'other')
    return (f"TAA FRAME LOG (eye 0, last {s['frames']} pairs, jitter {'/'.join(s['jitter_modes']) or '-'}): "
            f"{s['still_frames']} head still - drift with the step taken out mean {_n(s['drift_still_mean_px'])} / "
            f"max {_n(s['drift_still_max_px'])} px still (all {_n(s['drift_all_mean_px'])} / "
            f"{_n(s['drift_all_max_px'])} px), history UV step still mean {_n(s['uv_step_still_mean_px'])} / max "
            f"{_n(s['uv_step_still_max_px'])} px, measured step gain {_n(gain, '{:+.2f}')} ({counted})")


def better_block(best, record):
    """The block of frames to keep for the report: the one with the most still-head frames seen."""
    if not record:
        return best
    still = sum(1 for f in record['frames'] if f['history'] and f['still'])
    if best is None or still >= sum(1 for f in best['frames'] if f['history'] and f['still']):
        return record
    return best
