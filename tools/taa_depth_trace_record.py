# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR. Made by Agent Mulder.
"""0.0.76 A-depth TRACE: SpidyTaaDepthTraceData (include/spidy/taa_depth_trace.hpp Data) for the report and the
console. Read-only diagnostics; nothing here changes a picture. Every 10 seconds, whatever vr_aa / vr_taa_depth /
vr_ss are (each window is tagged with them, so the B tests can be told apart): fps and the pair-to-pair frame-time
spread, render hitches (pairs 50 ms or more apart) on their own lines with their QPC time (the clock
time.perf_counter reads, also the sound diagnostics'), the eye render size and the headset eye size; with vr_taa_depth
on: each eye's own depth (this pair's, the shown one's), the share of pairs that took the depth path (the rest:
head-motion fallback), what a per-eye choice would pick (both / left only / right only / neither, and how often the
eyes split), the convention check's round trip (mean and max px), the history age of 8 static-surface pixels per eye
(mean and max, still / moving / fast), and the grazing-angle bins (frontal |n.v| >= .5, mid .25-.5, grazing
1/16-.25, beyond 16x anisotropy < 1/16, depth edge, no depth) with their clip-only / depth-only / both rejection,
per-pixel fallback, history age, frame-to-frame luma change and depth residual. No Windows imports: `game` is
anything with read(address, size) -> bytes."""
import math
import struct
import time

EXPORT = 'SpidyTaaDepthTraceData'
MAGIC = 0x52544454  # TDTR
HEAD = '<8I'
WINDOW_HEAD = '<20II2f6I2f4I'
AGES = '<7I2f'
GRAZE = '<7I3f5If4If2I'
HITCH = '<QfI'
CATEGORIES = ('still', 'moving', 'fast')
BINS = ('frontal', 'mid', 'grazing', 'beyond_aniso', 'depth_edge', 'no_depth')
BIN_WORDS = {'frontal': '|n.v|>=.5', 'mid': '.25-.5', 'grazing': '1/16-.25', 'beyond_aniso': '<1/16 (past 16x AF)',
             'depth_edge': 'depth edge', 'no_depth': 'no depth'}
FRAME_BUCKETS = ('<12', '12-17', '17-25', '25-34', '34-50', '50+')
GRAZE_KEYS = ('pixels', 'sampled', 'clip_only', 'depth_only', 'both', 'fallback', 'age_sum', 'luma_sum',
              'residual_sum', 'residual_max', 'edge_pixels', 'edge_sampled', 'edge_clip_only', 'edge_depth_only',
              'edge_both', 'edge_luma_sum', 'spikes', 'spike_clipped', 'flips', 'edge_flips', 'edge_out_luma_sum',
              'edge_age_sum', 'edge_age_max')
FLIP_LUMA = 0.25
WINDOWS = 90
HITCHES = 64
WINDOW_SIZE = (struct.calcsize(WINDOW_HEAD) + 3 * struct.calcsize(AGES) +
               3 * len(BINS) * struct.calcsize(GRAZE))  # 968
SIZE = struct.calcsize(HEAD) + WINDOW_SIZE * (WINDOWS + 1) + struct.calcsize(HITCH) * HITCHES
AA_MODES = ('ours', 'smaa', 'legacy')
DEPTH_SETTINGS = ('off', 'on', 'auto')
MASKS = ('neither', 'left only', 'right only', 'both')
FAULTS = {0: None, 1: 'its root signature or shader', 2: 'its pipeline', 3: 'the eye size', 4: 'its buffers'}
ROUND_TRIP_LIMIT = 0.05


def _fin(v, digits=3):
    return round(v, digits) if v is not None and math.isfinite(v) else None


def _pct(part, whole):
    return round(100.0 * part / whole, 1) if whole else None


def _mean(total, n, digits=2):
    return round(total / n, digits) if n else None


def _window(raw, at):
    h = struct.unpack_from(WINDOW_HEAD, raw, at)
    (ms, pairs, wanted, used, aa, depth, eye_w, eye_h, out_w, out_h, ss, _, now0, now1, before0, before1,
     m0, m1, m2, m3, trip_n, trip_sum, trip_max, f0, f1, f2, f3, f4, f5, frame_sum, frame_max, hitches,
     probe_reads, probe_skipped, graze_reads) = h
    at += struct.calcsize(WINDOW_HEAD)
    ages = []
    for _c in CATEGORIES:
        a = struct.unpack_from(AGES, raw, at)
        at += struct.calcsize(AGES)
        ages.append(dict(zip(('samples', 'match_sum', 'match_max', 'kept_sum', 'kept_max', 'match_resets',
                              'residual_n', 'residual_sum', 'residual_max'), a)))
    graze = []
    for _c in CATEGORIES:
        row = []
        for _b in BINS:
            g = struct.unpack_from(GRAZE, raw, at)
            at += struct.calcsize(GRAZE)
            row.append(dict(zip(GRAZE_KEYS, g)))
        graze.append(row)
    return dict(ms=ms, pairs=pairs, wanted=wanted, used=used, aa=aa, depth=depth, eye=[eye_w, eye_h],
                out=[out_w, out_h], ss=ss, now_valid=[now0, now1], before_valid=[before0, before1],
                masks=[m0, m1, m2, m3], trip_n=trip_n, trip_sum=trip_sum, trip_max=trip_max,
                frames=[f0, f1, f2, f3, f4, f5], frame_ms_sum=frame_sum, frame_ms_max=frame_max, hitches=hitches,
                probe_reads=probe_reads, probe_skipped=probe_skipped, graze_reads=graze_reads, ages=ages,
                graze=graze)


def taa_depth_trace_parse(raw):
    """The record as a dict from its bytes, or None (wrong size, magic or version)."""
    if raw is None or len(raw) != SIZE:
        return None
    magic, version, size, window_ms, closed, hitches_noted, age_fault, _ = struct.unpack_from(HEAD, raw, 0)
    if magic != MAGIC or version != 1 or size != SIZE:
        return None
    at = struct.calcsize(HEAD)
    current = _window(raw, at)
    at += WINDOW_SIZE
    ring = [_window(raw, at + i * WINDOW_SIZE) for i in range(WINDOWS)]
    at += WINDOW_SIZE * WINDOWS
    hitch_ring = [struct.unpack_from(HITCH, raw, at + i * struct.calcsize(HITCH)) for i in range(HITCHES)]
    count = min(closed, WINDOWS)
    windows = [ring[n % WINDOWS] for n in range(closed - count, closed)]
    held = min(hitches_noted, HITCHES)
    hitches = [dict(zip(('qpc_us', 'ms', 'serial'), hitch_ring[n % HITCHES]))
               for n in range(hitches_noted - held, hitches_noted)]
    for h in hitches:
        h['ms'] = _fin(h['ms'], 1)
    return dict(window_ms=window_ms, closed=closed, dropped=closed - count, hitches_noted=hitches_noted,
                trace_fault=FAULTS.get(age_fault, str(age_fault)), current=current, windows=windows,
                hitches=hitches)


def taa_depth_trace_snapshot(game, address):
    """The record at `address` (None: no export, unreadable or not the record)."""
    if not address:
        return None
    try:
        raw = game.read(address, SIZE)
    except OSError:
        return None
    return taa_depth_trace_parse(raw)


def _name(table, n):
    return table[n] if 0 <= n < len(table) else str(n)


def _graze_row(g):
    return dict(pixels=g['pixels'], history_pct=_pct(g['sampled'], g['pixels']),
                clip_only_pct=_pct(g['clip_only'], g['sampled']), depth_only_pct=_pct(g['depth_only'], g['sampled']),
                both_pct=_pct(g['both'], g['sampled']),
                neither_pct=_pct(g['sampled'] - g['clip_only'] - g['depth_only'] - g['both'], g['sampled']),
                fallback_pct=_pct(g['fallback'], g['pixels']), age_mean=_mean(g['age_sum'], g['sampled'], 1),
                luma_change_mean=_mean(g['luma_sum'], g['sampled'], 4),
                residual_mean=_mean(g['residual_sum'], g['sampled'] - g['fallback'] if g['sampled'] > g['fallback']
                                    else g['sampled'], 4),
                residual_max=_fin(g['residual_max'], 4),
                edge_share_pct=_pct(g['edge_pixels'], g['pixels']),
                edge_clip_only_pct=_pct(g['edge_clip_only'], g['edge_sampled']),
                edge_depth_only_pct=_pct(g['edge_depth_only'], g['edge_sampled']),
                edge_both_pct=_pct(g['edge_both'], g['edge_sampled']),
                edge_luma_change_mean=_mean(g['edge_luma_sum'], g['edge_sampled'], 4),
                edge_shown_change_mean=_mean(g['edge_out_luma_sum'], g['edge_sampled'], 4),
                edge_age_mean=_mean(g['edge_age_sum'], g['edge_sampled'], 1), edge_age_max=g['edge_age_max'],
                edge_shown_vs_input=_mean(g['edge_out_luma_sum'], g['edge_luma_sum'], 3),
                spike_pct=_pct(g['spikes'], g['sampled']), spike_clipped_pct=_pct(g['spike_clipped'], g['spikes']),
                flip_pct=_pct(g['flips'], g['sampled']),
                edge_flip_pct=_pct(g['edge_flips'], g['edge_sampled']),
                flat_flip_pct=_pct(g['flips'] - g['edge_flips'], g['sampled'] - g['edge_sampled']))


def _sum_graze(rows):
    keys = rows[0].keys()
    out = {k: sum(r[k] for r in rows) for k in keys if k not in ('residual_max', 'edge_age_max')}
    out['residual_max'] = max(r['residual_max'] for r in rows)
    out['edge_age_max'] = max(r['edge_age_max'] for r in rows)
    return out


def window_summary(w, end_seconds=None):
    """One 10 s window as the report gives it."""
    seconds = w['ms'] / 1000.0
    frames = sum(w['frames'])
    pixels = [sum(w['graze'][c][b]['pixels'] for c in range(3)) for b in range(len(BINS))]
    counted = sum(pixels)
    out = dict(seconds=round(seconds, 1), pairs=w['pairs'],
               fps=round(w['pairs'] / seconds, 1) if seconds > 0 else None,
               vr_aa=_name(AA_MODES, w['aa']), vr_taa_depth=_name(DEPTH_SETTINGS, w['depth']),
               vr_ss_percent=w['ss'], eye_render=w['eye'], eye_out=w['out'],
               frame_ms_mean=_mean(w['frame_ms_sum'], frames, 2), frame_ms_max=_fin(w['frame_ms_max'], 1),
               frame_ms_spread={b: _pct(n, frames) for b, n in zip(FRAME_BUCKETS, w['frames'])},
               hitches=w['hitches'], depth_wanted=w['wanted'], depth_used_pct=_pct(w['used'], w['wanted']),
               head_fallback_pct=_pct(w['wanted'] - w['used'], w['wanted']),
               depth_now_pct=[_pct(w['now_valid'][i], w['wanted']) for i in range(2)],
               depth_before_pct=[_pct(w['before_valid'][i], w['wanted']) for i in range(2)],
               per_eye_pick_pct={m: _pct(n, w['wanted']) for m, n in zip(MASKS, w['masks'])},
               split_pct=_pct(w['masks'][1] + w['masks'][2], w['wanted']),
               round_trip_mean_px=_mean(w['trip_sum'], w['trip_n'], 4), round_trip_max_px=(
                   _fin(w['trip_max'], 4) if w['trip_n'] else None),
               round_trip_over_limit=bool(w['trip_n'] and w['trip_max'] > ROUND_TRIP_LIMIT),
               history_age={}, graze={}, graze_share_pct={b: _pct(p, counted) for b, p in zip(BINS, pixels)},
               probe_reads=w['probe_reads'], probe_skipped=w['probe_skipped'], graze_reads=w['graze_reads'])
    for c, a in zip(CATEGORIES, w['ages']):
        if a['samples']:
            out['history_age'][c] = dict(samples=a['samples'], match_mean=_mean(a['match_sum'], a['samples'], 1),
                                         match_max=a['match_max'], kept_mean=_mean(a['kept_sum'], a['samples'], 1),
                                         kept_max=a['kept_max'], match_reset_pct=_pct(a['match_resets'],
                                                                                      a['samples']),
                                         residual_mean=_mean(a['residual_sum'], a['residual_n'], 4),
                                         residual_max=_fin(a['residual_max'], 4) if a['residual_n'] else None)
    for c, row in zip(CATEGORIES, w['graze']):
        if any(g['pixels'] for g in row):
            out['graze'][c] = {b: _graze_row(g) for b, g in zip(BINS, row) if g['pixels']}
    every = [_sum_graze([w['graze'][c][b] for c in range(3)]) for b in range(len(BINS))]
    out['graze']['all'] = {b: _graze_row(g) for b, g in zip(BINS, every) if g['pixels']}
    if not out['graze']['all']:
        del out['graze']['all']
    if end_seconds is not None:
        out['ends_s'] = round(end_seconds, 1)
    return out


def hitch_wall(qpc_us, perf_now=None, wall_now=None):
    """A QPC time (microseconds) as box-local wall time text (the launcher's perf_counter is the same clock)."""
    perf_now = time.perf_counter() if perf_now is None else perf_now
    wall_now = time.time() if wall_now is None else wall_now
    t = wall_now - (perf_now - qpc_us / 1e6)
    return time.strftime('%H:%M:%S', time.localtime(t)) + f'.{int((t % 1) * 1000):03d} {time.strftime("%Z")}'


def taa_depth_trace_report(record):
    """The report's section: each 10 s window, the hitches and the fault (if any)."""
    if not record:
        return None
    rows, ends = [], 0.0
    for w in record['windows']:
        ends += w['ms'] / 1000.0
        if w['pairs']:
            rows.append(window_summary(w, ends))
    return dict(windows=rows, hitches=record['hitches'], hitches_noted=record['hitches_noted'],
                trace_fault=record['trace_fault'], dropped=record['dropped'],
                bins={b: BIN_WORDS[b] for b in BINS})


def _n(v, fmt):
    return format(v, fmt) if v is not None else '-'


def _s(v):
    return f'{v:.1f}%' if v is not None else '-'


def mode_tag(r):
    return (f"[vr_aa {r['vr_aa']}, vr_taa_depth {r['vr_taa_depth']}, vr_ss {r['vr_ss_percent']}%, eye "
            f"{r['eye_render'][0]}x{r['eye_render'][1]} -> {r['eye_out'][0]}x{r['eye_out'][1]}]")


def window_lines(r):
    """One 10 s window: the main line, then (with the depth) the history age and the graze bins."""
    head = f"TAA TRACE {r['ends_s']:6.0f}s {mode_tag(r)}: " if 'ends_s' in r else f'TAA TRACE {mode_tag(r)}: '
    spread = ' '.join(f"{b} {_s(v)}" for b, v in r['frame_ms_spread'].items())
    lines = [head + f"{_n(r['fps'], '5.1f')} fps, frame {_n(r['frame_ms_mean'], '.1f')} ms mean / "
             f"{_n(r['frame_ms_max'], '.1f')} max ({spread} ms), {r['hitches']} hitches"]
    if r['depth_wanted']:
        pick = ', '.join(f"{m} {_s(v)}" for m, v in r['per_eye_pick_pct'].items())
        lines[0] += (f"; depth now L {_s(r['depth_now_pct'][0])} R {_s(r['depth_now_pct'][1])}, before L "
                     f"{_s(r['depth_before_pct'][0])} R {_s(r['depth_before_pct'][1])}, depth path "
                     f"{_s(r['depth_used_pct'])} / head fallback {_s(r['head_fallback_pct'])}; per-eye pick (logged "
                     f"only, rendering stays both-or-neither): {pick}, split {_s(r['split_pct'])}; round trip "
                     f"{_n(r['round_trip_mean_px'], '.4f')} mean / {_n(r['round_trip_max_px'], '.4f')} max px "
                     f"(limit {ROUND_TRIP_LIMIT}{', OVER' if r['round_trip_over_limit'] else ''})")
        ages = '; '.join(f"{c} match {_n(a['match_mean'], '.1f')} mean / {a['match_max']} max, kept "
                         f"{_n(a['kept_mean'], '.1f')} / {a['kept_max']}, resets {_s(a['match_reset_pct'])}, "
                         f"residual {_n(a['residual_mean'], '.4f')} / {_n(a['residual_max'], '.4f')} "
                         f"({a['samples']} readings)" for c, a in r['history_age'].items())
        lines.append(f"TAA TRACE AGE (static-surface probes, pairs in a row): {ages or 'no static-surface reading'}")
        g = r['graze'].get('all') or {}
        bins = '; '.join(f"{b} {_s(r['graze_share_pct'][b])} of pixels: clip {_s(x['clip_only_pct'])} depth "
                         f"{_s(x['depth_only_pct'])} both {_s(x['both_pct'])} neither {_s(x['neither_pct'])}, "
                         f"fallback {_s(x['fallback_pct'])}, age {_n(x['age_mean'], '.1f')}, luma change "
                         f"{_n(x['luma_change_mean'], '.4f')}, residual {_n(x['residual_mean'], '.4f')} / "
                         f"{_n(x['residual_max'], '.4f')}" for b, x in g.items())
        lines.append(f"TAA TRACE GRAZE (all movement): {bins or 'no counted pixels (trace off or depth not used)'}")
        edges = '; '.join(f"{b} edges {_s(x['edge_share_pct'])}: clip {_s(x['edge_clip_only_pct'])} depth "
                          f"{_s(x['edge_depth_only_pct'])} both {_s(x['edge_both_pct'])}, luma change "
                          f"{_n(x['edge_luma_change_mean'], '.4f')} in / {_n(x['edge_shown_change_mean'], '.4f')} shown (x"
                          f"{_n(x['edge_shown_vs_input'], '.3f')}), age {_n(x['edge_age_mean'], '.1f')} mean / "
                          f"{x['edge_age_max']} max, flips {_s(x['edge_flip_pct'])} (off edges "
                          f"{_s(x['flat_flip_pct'])}); spikes {_s(x['spike_pct'])} of pixels, clip moved the history "
                          f"at {_s(x['spike_clipped_pct'])} of them" for b, x in g.items())
        if edges:
            lines.append(f"TAA TRACE EDGES (depth step, no-depth neighbour or crease; flips: luma change over "
                         f"{FLIP_LUMA}; spikes: a pixel above its own 3x3 box): {edges}")
    return lines


def hitch_line(h, perf_now=None, wall_now=None):
    return (f"RENDER HITCH at qpc {h['qpc_us'] / 1e6:.3f} s ({hitch_wall(h['qpc_us'], perf_now, wall_now)}): "
            f"{_n(h['ms'], '.1f')} ms between eye pairs (pair {h['serial']})")


def fault_line(record):
    if record and record.get('trace_fault'):
        return (f"TAA TRACE: the age / graze trace is off this session ({record['trace_fault']} could not be made); "
                'the depth path runs counted as before.')
    return None
