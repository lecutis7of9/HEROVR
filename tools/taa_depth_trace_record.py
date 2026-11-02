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
per-pixel fallback, history age, frame-to-frame luma change and depth residual. 0.0.76 SEAM / EDGE TRACE (data
version 2; with live.txt vr_taa_seam on, vr_taa_depth off as well): the pre-TAA change (this pair's eye image
against the history read) and the shown change (the TAA output against the same read) by angle (frontal / mid /
grazing / silhouette) x distance (near < 8 m, mid, far >= 30 m) x flat / seam (depth step or crease), per movement;
the HOT EDGES (seam pixels whose own 3x3 luma spans over 0.25: a lit frame face against a dark recess) with their
history age, the history weight applied, clip-only / depth-only / both / neither rejection, 3x3 YCoCg luma span and
sigma, 3x3 depth spread, bright / dark state and the in / shown change; 8 raw hot-edge samples of the fastest pass;
and a verdict (F1 clip / F2 depth / F3 coverage / settles). No Windows imports: `game` is anything with
read(address, size) -> bytes."""
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
SEAM_PAIR = '<2I'
SEAM = '<I2f2I'
HOT = '<9I6f'
SAMPLES = '<2I64I'
ANGLES = ('frontal', 'mid', 'grazing', 'silhouette')
DISTANCES = ('near', 'mid', 'far')
SEAM_KINDS = ('flat', 'seam')
SEAM_CELLS = len(ANGLES) * len(DISTANCES) * len(SEAM_KINDS)  # 24
HOT_KEYS = ('pixels', 'age_sum', 'age_max', 'clip_only', 'depth_only', 'both', 'neither', 'bright', 'out_flicker',
            'weight_sum', 'span_sum', 'sigma_sum', 'depth_spread_sum', 'in_sum', 'out_sum')
SEAM_FLICKER = 1 / 16
HOT_SPAN = 0.25
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
               3 * len(BINS) * struct.calcsize(GRAZE) + struct.calcsize(SEAM_PAIR) +
               3 * SEAM_CELLS * struct.calcsize(SEAM) + 3 * len(ANGLES) * struct.calcsize(HOT) +
               struct.calcsize(SAMPLES))  # 4336
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


def _sample(words):
    xy, age, flags, weight, luma_span, depth_bits, depth_spread, in_out = words
    return dict(x=xy & 0xFFFF, y=xy >> 16, age=age, clipped=bool(flags & 1), refused=bool(flags & 2),
                bright=bool(flags & 4), bin=_name(BINS, (flags >> 8) & 0xFF), weight=weight / 1000.0,
                luma=(luma_span & 0xFFFF) / 1000.0, span=(luma_span >> 16) / 1000.0,
                depth=_fin(struct.unpack('<f', struct.pack('<I', depth_bits))[0], 6),
                depth_spread=depth_spread / 1000.0, in_change=(in_out & 0xFFFF) / 1000.0,
                shown_change=(in_out >> 16) / 1000.0)


def _window(raw, at):
    h = struct.unpack_from(WINDOW_HEAD, raw, at)
    (ms, pairs, wanted, used, aa, depth, eye_w, eye_h, out_w, out_h, ss, spare, now0, now1, before0, before1,
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
    seam_reads, seam_pairs = struct.unpack_from(SEAM_PAIR, raw, at)
    at += struct.calcsize(SEAM_PAIR)
    seam = []
    for _c in CATEGORIES:
        row = []
        for _k in range(SEAM_CELLS):
            row.append(dict(zip(('pixels', 'in_sum', 'out_sum', 'in_flicker', 'out_flicker'),
                                struct.unpack_from(SEAM, raw, at))))
            at += struct.calcsize(SEAM)
        seam.append(row)
    hot = []
    for _c in CATEGORIES:
        row = []
        for _a in ANGLES:
            row.append(dict(zip(HOT_KEYS, struct.unpack_from(HOT, raw, at))))
            at += struct.calcsize(HOT)
        hot.append(row)
    sm = struct.unpack_from(SAMPLES, raw, at)
    samples = [_sample(sm[2 + 8 * k:10 + 8 * k]) for k in range(min(sm[0], 8))]
    return dict(seam_setting=spare, seam_reads=seam_reads, seam_pairs=seam_pairs, seam=seam, hot=hot,
                samples=samples, sample_category=sm[1], ms=ms, pairs=pairs, wanted=wanted, used=used, aa=aa, depth=depth, eye=[eye_w, eye_h],
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
    if magic != MAGIC or version != 2 or size != SIZE:
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
    out['vr_taa_seam'] = 'on' if w.get('seam_setting') else 'off'
    out['seam_pairs'] = w.get('seam_pairs', 0)
    out['seam_reads'] = w.get('seam_reads', 0)
    if w.get('seam_reads'):
        out['seam'] = seam_summary(w['seam'])
        out['hot_edges'] = hot_summary(w['hot'])
        out['hot_samples'] = dict(category=_name(CATEGORIES, w['sample_category']), samples=w['samples'])
        out['verdict'] = seam_verdict(out['hot_edges'], out['seam'])
    if end_seconds is not None:
        out['ends_s'] = round(end_seconds, 1)
    return out


def _cell(angle, distance, kind):
    return (angle * len(DISTANCES) + distance) * len(SEAM_KINDS) + kind


def _seam_row(cells):
    n = sum(c['pixels'] for c in cells)
    i = sum(c['in_sum'] for c in cells)
    o = sum(c['out_sum'] for c in cells)
    return dict(pixels=n, in_change_mean=_mean(i, n, 4), shown_change_mean=_mean(o, n, 4),
                shown_vs_in=_mean(o, i, 3), in_flicker_pct=_pct(sum(c['in_flicker'] for c in cells), n),
                shown_flicker_pct=_pct(sum(c['out_flicker'] for c in cells), n))


def seam_summary(seam):
    """Per movement (and all): angle x distance x flat / seam, the pre-TAA and shown change."""
    out = {}
    for name, cats in list(zip(CATEGORIES, ([0], [1], [2]))) + [('all', [0, 1, 2])]:
        rows = {}
        for a, an in enumerate(ANGLES):
            for d, dn in enumerate(DISTANCES):
                for k, kn in enumerate(SEAM_KINDS):
                    r = _seam_row([seam[c][_cell(a, d, k)] for c in cats])
                    if r['pixels']:
                        rows[f'{an}/{dn}/{kn}'] = r
        for k, kn in enumerate(SEAM_KINDS):
            r = _seam_row([seam[c][_cell(a, d, k)] for c in cats for a in range(len(ANGLES))
                           for d in range(len(DISTANCES))])
            if r['pixels']:
                rows[f'any/{kn}'] = r
        if rows:
            out[name] = rows
    return out


def _hot_row(h):
    n = h['pixels']
    return dict(pixels=n, age_mean=_mean(h['age_sum'], n, 1), age_max=h['age_max'],
                weight_mean=_mean(h['weight_sum'], n, 3), clip_only_pct=_pct(h['clip_only'], n),
                depth_only_pct=_pct(h['depth_only'], n), both_pct=_pct(h['both'], n),
                neither_pct=_pct(h['neither'], n), span_mean=_mean(h['span_sum'], n, 3),
                sigma_mean=_mean(h['sigma_sum'], n, 3), depth_spread_mean=_mean(h['depth_spread_sum'], n, 3),
                bright_pct=_pct(h['bright'], n), in_change_mean=_mean(h['in_sum'], n, 4),
                shown_change_mean=_mean(h['out_sum'], n, 4), shown_vs_in=_mean(h['out_sum'], h['in_sum'], 3),
                shown_flicker_pct=_pct(h['out_flicker'], n))


def _sum_hot(rows):
    out = {k: sum(r[k] for r in rows) for k in HOT_KEYS if k != 'age_max'}
    out['age_max'] = max(r['age_max'] for r in rows)
    return out


def hot_summary(hot):
    """Per movement (and all): the hot edges by angle and over every angle."""
    out = {}
    for name, cats in list(zip(CATEGORIES, ([0], [1], [2]))) + [('all', [0, 1, 2])]:
        rows = {}
        for a, an in enumerate(ANGLES):
            h = _sum_hot([hot[c][a] for c in cats])
            if h['pixels']:
                rows[an] = _hot_row(h)
        h = _sum_hot([hot[c][a] for c in cats for a in range(len(ANGLES))])
        if h['pixels']:
            rows['any'] = _hot_row(h)
            out[name] = rows
    return out


HOT_ENOUGH = 20
SETTLED_RATIO = 0.3
YOUNG_AGE = 4.0


def seam_verdict(hot, seam):
    """Which explanation the hot edges match: settles, F1 (clip rejects their history), F2 (the depth refuses it)
    or F3 (coverage: the history lives, the shown edge still changes). The fastest movement with enough readings,
    grazing and silhouette first. CANDIDATE, from this window's readings only."""
    for cat in ('fast', 'moving', 'still'):
        rows = hot.get(cat) or {}
        pick = [rows[a] for a in ('grazing', 'silhouette') if a in rows]
        n = sum(r['pixels'] for r in pick)
        where = 'grazing/silhouette'
        if n < HOT_ENOUGH:
            pick, n, where = ([rows['any']] if 'any' in rows else []), (rows.get('any') or {}).get('pixels', 0), 'any angle'
        if n < HOT_ENOUGH:
            continue
        def w(key):
            return sum((r[key] or 0) * r['pixels'] for r in pick) / n
        age, ratio, clip, depth, both = w('age_mean'), w('shown_vs_in'), w('clip_only_pct'), w('depth_only_pct'), w('both_pct')
        flick = w('shown_flicker_pct')
        base = (f'{cat}, {where}, {n} hot-edge readings: age {age:.1f} pairs, shown/in change x{ratio:.2f}, '
                f'shown flicker {flick:.1f}%, clip-only {clip:.1f}% depth-only {depth:.1f}% both {both:.1f}%')
        if ratio <= SETTLED_RATIO and flick < 5:
            return dict(match='settles', text=f'SETTLES (the TAA damps the hot edges) - {base}')
        if age < YOUNG_AGE and clip + both >= depth + both and clip + both > 0:
            return dict(match='F1', text=f'F1 CANDIDATE: history rejected, the variance clip dominates - {base}')
        if age < YOUNG_AGE and depth + both > 0:
            return dict(match='F2', text=f'F2 CANDIDATE: history refused, the depth test dominates - {base}')
        return dict(match='F3', text=f'F3 CANDIDATE: coverage - the history lives but the shown edge still changes - {base}')
    return dict(match=None, text='no verdict: fewer than %d hot-edge readings in any movement' % HOT_ENOUGH)


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
    seam = f", vr_taa_seam {r['vr_taa_seam']}" if r.get('vr_taa_seam') == 'on' else ''
    return (f"[vr_aa {r['vr_aa']}, vr_taa_depth {r['vr_taa_depth']}{seam}, vr_ss {r['vr_ss_percent']}%, eye "
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
    lines += seam_lines(r)
    return lines


def seam_lines(r):
    """0.0.76 SEAM / EDGE TRACE: pre-TAA vs shown per movement, the hot edges, the samples and the verdict."""
    if not r.get('seam'):
        return []
    lines = []
    path = 'depth path' if r['vr_taa_depth'] == 'on' else 'head-motion path, depth read for the trace only'
    for cat in CATEGORIES + ('all',):
        rows = r['seam'].get(cat)
        if not rows:
            continue
        cells = '; '.join(f"{k} {x['pixels']}: in {_n(x['in_change_mean'], '.4f')} shown "
                          f"{_n(x['shown_change_mean'], '.4f')} (x{_n(x['shown_vs_in'], '.2f')}), flicker in "
                          f"{_s(x['in_flicker_pct'])} shown {_s(x['shown_flicker_pct'])}" for k, x in rows.items())
        lines.append(f"TAA SEAM {cat} ({path}; pre-TAA eye image vs TAA output against the history read; flicker: "
                     f"over {SEAM_FLICKER:.4f}): {cells}")
    for cat in CATEGORIES + ('all',):
        rows = (r.get('hot_edges') or {}).get(cat)
        if not rows:
            continue
        cells = '; '.join(f"{a} {x['pixels']}: age {_n(x['age_mean'], '.1f')} mean / {x['age_max']} max, weight "
                          f"{_n(x['weight_mean'], '.3f')}, clip-only {_s(x['clip_only_pct'])} depth-only "
                          f"{_s(x['depth_only_pct'])} both {_s(x['both_pct'])} neither {_s(x['neither_pct'])}, "
                          f"YCoCg luma span {_n(x['span_mean'], '.3f')} sigma {_n(x['sigma_mean'], '.3f')}, depth "
                          f"spread {_n(x['depth_spread_mean'], '.3f')}, bright {_s(x['bright_pct'])}, change in "
                          f"{_n(x['in_change_mean'], '.4f')} shown {_n(x['shown_change_mean'], '.4f')} (x"
                          f"{_n(x['shown_vs_in'], '.2f')}), shown flicker {_s(x['shown_flicker_pct'])}"
                          for a, x in rows.items())
        lines.append(f"TAA HOT EDGES {cat} (seam pixels with a 3x3 luma span over {HOT_SPAN}: a lit frame face "
                     f"against the dark recess): {cells}")
    hs = r.get('hot_samples') or {}
    if hs.get('samples'):
        one = '; '.join(f"({x['x']},{x['y']}) {x['bin']} age {x['age']} weight {x['weight']:.3f} "
                        f"{'clip' if x['clipped'] else ''}{'+' if x['clipped'] and x['refused'] else ''}"
                        f"{'depth' if x['refused'] else ''}{'neither' if not (x['clipped'] or x['refused']) else ''} "
                        f"luma {x['luma']:.3f} {'bright' if x['bright'] else 'dark'} span {x['span']:.3f} depth "
                        f"{_n(x['depth'], '.6f')} spread {x['depth_spread']:.3f} in {x['in_change']:.3f} shown "
                        f"{x['shown_change']:.3f}" for x in hs['samples'])
        lines.append(f"TAA HOT EDGE SAMPLES ({hs['category']} pass, left eye, raw): {one}")
    if r.get('verdict'):
        lines.append(f"TAA SEAM VERDICT: {r['verdict']['text']}")
    return lines


def hitch_line(h, perf_now=None, wall_now=None):
    return (f"RENDER HITCH at qpc {h['qpc_us'] / 1e6:.3f} s ({hitch_wall(h['qpc_us'], perf_now, wall_now)}): "
            f"{_n(h['ms'], '.1f')} ms between eye pairs (pair {h['serial']})")


def fault_line(record):
    if record and record.get('trace_fault'):
        return (f"TAA TRACE: the age / graze trace is off this session ({record['trace_fault']} could not be made); "
                'the depth path runs counted as before.')
    return None
