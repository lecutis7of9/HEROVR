# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 TAA WATCH: SpidyTaaWatchData (include/spidy/taa_watch.hpp Data) for the report and the console. Every
10 seconds: the eye pairs a second (fps), the GPU time a pair (SPIDY TAA's own pass, and the game's two eye jobs from
a timestamp at each job's begin and end marker), and the still / moving split - the pairs the history classed as
moving, and the pairs with a still head (the eye's view moved under 2 degrees and 3 cm a second) it classed as
moving anyway (over 20%: the thresholds are wrong, not the blend). And the eye depth log: the fmt 19 depth leaving
each eye job's G-buffer pass for the first 200 pairs, CONFIRMED or CANDIDATE. No Windows imports: `game` is
anything with read(address, size) -> bytes."""
import struct

EXPORT = 'SpidyTaaWatchData'
MAGIC = 0x44575453  # STWD
HEAD = '<6I'
DEPTH = '<12I2Q'
WINDOW = '<8IfIf3I'
WINDOWS = 90
SIZE = struct.calcsize(HEAD) + struct.calcsize(DEPTH) + struct.calcsize(WINDOW) * (WINDOWS + 1)  # 5184
WRONG_SHARE = 20  # percent of still-head pairs classed moving past which the thresholds are wrong
BUILDS = {0: 'shared', 1: 'A-depth', 2: 'B-ss125', 3: 'C-still094'}
VERDICTS = ('not seen', 'CANDIDATE', 'CONFIRMED')
FORMATS = {19: 'R32G8X24_TYPELESS', 20: 'D32_FLOAT_S8X24_UINT'}


def _window(values):
    (ms, pairs, moving, eased, still, still_eased, still_moving, taa_timed, taa_ms, eye_timed, eye_ms,
     depth_pairs, depth_missed, restarted) = values
    return dict(ms=ms, pairs=pairs, moving=moving, eased=eased, still_head=still, still_eased=still_eased,
                still_moving=still_moving, taa_timed=taa_timed, taa_gpu_ms=taa_ms, eye_timed=eye_timed,
                eye_gpu_ms=eye_ms, depth_pairs=depth_pairs, depth_missed=depth_missed, restarted=restarted)


def _pct(part, whole):
    return round(100.0 * part / whole, 1) if whole else None


def _mean(total, count):
    return round(total / count, 3) if count else None


def window_summary(w, end_seconds=None):
    """One 10 s window as the report gives it."""
    seconds = w['ms'] / 1000.0
    out = dict(seconds=round(seconds, 1), pairs=w['pairs'],
               fps=round(w['pairs'] / seconds, 1) if seconds > 0 else None,
               taa_gpu_ms_per_pair=_mean(w['taa_gpu_ms'], w['taa_timed']),
               eye_gpu_ms_per_pair=_mean(w['eye_gpu_ms'], w['eye_timed']),
               still_pct=_pct(w['pairs'] - w['eased'] - w['restarted'], w['pairs']),
               moving_pct=_pct(w['moving'], w['pairs']), eased_pct=_pct(w['eased'], w['pairs']),
               restarted_pct=_pct(w['restarted'], w['pairs']),
               still_head_pairs=w['still_head'],
               still_head_classed_moving_pct=_pct(w['still_eased'], w['still_head']),
               still_head_past_half_pct=_pct(w['still_moving'], w['still_head']),
               depth_pct=_pct(w['depth_pairs'], w['pairs']), depth_missed=w['depth_missed'])
    if end_seconds is not None:
        out['ends_s'] = round(end_seconds, 1)
    return out


def taa_watch_parse(raw):
    """The record as a dict from its bytes, or None (wrong size, magic or version)."""
    if raw is None or len(raw) != SIZE:
        return None
    head = struct.unpack_from(HEAD, raw, 0)
    magic, version, size, window_ms, closed, build = head
    if magic != MAGIC or version != 1 or size != SIZE:
        return None
    at = struct.calcsize(HEAD)
    d = struct.unpack_from(DEPTH, raw, at)
    at += struct.calcsize(DEPTH)
    depth = dict(frames=d[0], both_eyes=d[1], format=d[2], format_name=FORMATS.get(d[2], str(d[2])),
                 width=d[3], height=d[4], distinct=[d[5], d[6]], shared=d[7], flat_seen=d[8], other_list=d[9],
                 verdict=VERDICTS[d[10]] if d[10] < len(VERDICTS) else f'unknown {d[10]}', exits=d[11],
                 resource=[hex(d[12]), hex(d[13])])
    step = struct.calcsize(WINDOW)
    current = _window(struct.unpack_from(WINDOW, raw, at))
    at += step
    ring = [_window(struct.unpack_from(WINDOW, raw, at + i * step)) for i in range(WINDOWS)]
    count = min(closed, WINDOWS)
    first = closed - count
    windows, ends = [], 0.0
    for n in range(first, closed):
        w = ring[n % WINDOWS]
        ends += w['ms'] / 1000.0
        windows.append(w)
    return dict(build=BUILDS.get(build, str(build)), window_ms=window_ms, closed=closed, dropped=first,
                depth=depth, current=current, windows=windows)


def taa_watch_snapshot(game, address):
    """The record at `address` (None: no export, unreadable or not the record)."""
    if not address:
        return None
    try:
        raw = game.read(address, SIZE)
    except OSError:
        return None
    return taa_watch_parse(raw)


def taa_watch_report(record):
    """The report's section: each 10 s window, the session's totals and the verdicts."""
    if not record:
        return None
    rows, ends = [], 0.0
    for w in record['windows']:
        ends += w['ms'] / 1000.0
        if w['pairs']:
            rows.append(window_summary(w, ends))
    keys = ('ms', 'pairs', 'moving', 'eased', 'still_head', 'still_eased', 'still_moving', 'taa_timed', 'taa_gpu_ms',
            'eye_timed', 'eye_gpu_ms', 'depth_pairs', 'depth_missed', 'restarted')
    counted = record['windows'] or [record['current']]  # the closed windows (the running one before any closed)
    total = {k: sum(w[k] for w in counted) for k in keys}
    totals = window_summary(total) if total['pairs'] else None
    wrong = [r for r in rows if (r['still_head_classed_moving_pct'] or 0) > WRONG_SHARE and r['still_head_pairs'] >= 20]
    still_share = totals['still_head_classed_moving_pct'] if totals else None
    return dict(build=record['build'], windows=rows, totals=totals, depth=record['depth'],
                thresholds_wrong=bool(still_share is not None and still_share > WRONG_SHARE),
                windows_over_20pct=len(wrong))


def _num(v, fmt):
    return format(v, fmt) if v is not None else '-'


def taa_watch_lines(section):
    """The console's words: the depth log, the session's totals and one line a 10 s window."""
    if not section:
        return []
    lines = []
    d = section['depth']
    if d['frames'] or d['exits']:
        lines.append(f"TAA WATCH eye depth: {d['verdict']} - {d['format_name']} {d['width']}x{d['height']} "
                     f"over {d['frames']} pairs, both eyes {d['both_eyes']}, resources per eye "
                     f"{d['distinct'][0]}/{d['distinct'][1]}, same in both {d['shared']}, flat view {d['flat_seen']}, "
                     f"other lists {d['other_list']} ({d['exits']} seen in all).")
    else:
        lines.append('TAA WATCH eye depth: not seen (no eye-sized fmt 19 depth left an eye job).')
    t = section['totals']
    if t:
        lines.append(f"TAA WATCH {section['build']} session: {_num(t['fps'], '.1f')} fps, eye jobs "
                     f"{_num(t['eye_gpu_ms_per_pair'], '.2f')} ms GPU a pair, SPIDY TAA "
                     f"{_num(t['taa_gpu_ms_per_pair'], '.2f')} ms; still {_num(t['still_pct'], '.0f')}% / eased "
                     f"{_num(t['eased_pct'], '.0f')}% (moving past half {_num(t['moving_pct'], '.0f')}%); still head "
                     f"{t['still_head_pairs']} pairs, {_num(t['still_head_classed_moving_pct'], '.0f')}% classed moving"
                     + (' - OVER 20%: the thresholds are wrong' if section['thresholds_wrong'] else '') + '.')
    return lines + taa_watch_window_lines(section)


def taa_watch_window_lines(section):
    """One console line a 10 s window."""
    lines = []
    for r in (section or {}).get('windows') or []:
        lines.append(f"TAA WATCH {r['ends_s']:6.0f}s: {_num(r['fps'], '5.1f')} fps, eye "
                     f"{_num(r['eye_gpu_ms_per_pair'], '.2f')} ms, taa {_num(r['taa_gpu_ms_per_pair'], '.2f')} ms, "
                     f"still {_num(r['still_pct'], '.0f')}% eased {_num(r['eased_pct'], '.0f')}% moving "
                     f"{_num(r['moving_pct'], '.0f')}%, still head {r['still_head_pairs']} "
                     f"({_num(r['still_head_classed_moving_pct'], '.0f')}% classed moving)"
                     + (f", depth {_num(r['depth_pct'], '.0f')}%" if r['depth_pct'] or r['depth_missed'] else ''))
    return lines
