# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 A-depth CHECKED: SpidyTaaDepthCheckData (include/spidy/taa_depth_check.hpp Data) for the report and the
console. The depth's convention check (the round trip of the centre pixel through the jittered eye view-projection
the lens renders with, and the stereo test at depth edges that tells the conventions apart), and every 10 seconds,
with live.txt vr_taa_depth on: the share of pairs with each eye's depth, the round trip in pixels, the history
rejection split clip alone / depth test alone / both, per path (depth or head motion) and movement (still, moving,
fast swing: the head travelling 6 m/s or more, or turning 120 degrees a second or more), the share of pixels that fell
back to head motion (depth 0, 1, NaN or out of the eye's frustum), fps and the depth pass's own GPU time a pair
(the two depth copies plus what the pass costs more with the depth than without). No Windows imports: `game` is
anything with read(address, size) -> bytes."""
import math
import struct

EXPORT = 'SpidyTaaDepthCheckData'
MAGIC = 0x4B434454  # TDCK
HEAD = '<6I'
CHECK = '<8I5f6f2i2f5I'
COUNTS = '<8I'
WINDOW_HEAD = '<10IfIfIf2I2f'
PATHS = ('head', 'depth')
CATEGORIES = ('still', 'moving', 'fast')
WINDOWS = 90
WINDOW_SIZE = struct.calcsize(WINDOW_HEAD) + struct.calcsize(COUNTS) * len(PATHS) * len(CATEGORIES) + 4  # 272
SIZE = struct.calcsize(HEAD) + struct.calcsize(CHECK) + WINDOW_SIZE * (WINDOWS + 1)  # 24888
STATES = ('WAITING', 'PASSED', 'FAILED')
CONVENTIONS = ('reversed-Z, D3D 0..1, reciprocal, finite far', 'reversed-Z, D3D 0..1, reciprocal, infinite far',
               'standard Z, D3D 0..1, reciprocal', 'standard Z, GL -1..1, reciprocal', 'linear 0..1 of the far')
REASONS = ('fine', 'round trip over 0.05 px', 'no depth on the centre row', 'too few depth-edge points',
           'no convention clearly best', 'another convention fits better', 'no eye lens near/far yet',
           'centre point outside the eye frustum')
FAST_SWING = 'fast swing = head travel >= 6 m/s or turn >= 120 deg/s'
ROUND_TRIP_LIMIT = 0.05


def _name(table, n):
    return table[n] if n < len(table) else None


def _fin(v, digits=4):
    return round(v, digits) if v is not None and math.isfinite(v) else None


def _pct(part, whole):
    return round(100.0 * part / whole, 1) if whole else None


def _window(raw, at):
    h = struct.unpack_from(WINDOW_HEAD, raw, at)
    (ms, pairs, wanted, found0, found1, used, gated, unchecked, refused, copy_timed, copy_ms, depth_timed, depth_ms,
     plain_timed, plain_ms, checks, checks_passed, trip_max, trip_last) = h
    at += struct.calcsize(WINDOW_HEAD)
    counts = {}
    for p in PATHS:
        for c in CATEGORIES:
            v = struct.unpack_from(COUNTS, raw, at)
            at += struct.calcsize(COUNTS)
            counts[(p, c)] = dict(zip(('pairs', 'pixels', 'sampled', 'clip_only', 'depth_only', 'both', 'fallback',
                                       'depth_tested'), v))
    return dict(ms=ms, pairs=pairs, wanted=wanted, found=[found0, found1], used=used, gated=gated,
                unchecked=unchecked, refused=refused, copy_timed=copy_timed, copy_ms=copy_ms, depth_timed=depth_timed,
                depth_ms=depth_ms, plain_timed=plain_timed, plain_ms=plain_ms, checks=checks,
                checks_passed=checks_passed, round_trip_max=trip_max, round_trip_last=trip_last, counts=counts)


def taa_depth_check_parse(raw):
    """The record as a dict from its bytes, or None (wrong size, magic or version)."""
    if raw is None or len(raw) != SIZE:
        return None
    magic, version, size, window_ms, closed, _ = struct.unpack_from(HEAD, raw, 0)
    if magic != MAGIC or version != 1 or size != SIZE:
        return None
    at = struct.calcsize(HEAD)
    c = struct.unpack_from(CHECK, raw, at)
    at += struct.calcsize(CHECK)
    check = dict(probes=c[0], passed=c[1], failed=c[2], inconclusive=c[3], state=_name(STATES, c[4]) or str(c[4]),
                 best=_name(CONVENTIONS, c[5]), reason=_name(REASONS, c[6]) or str(c[6]), edge_points=c[7],
                 agree_pct={CONVENTIONS[k]: round(100 * c[8 + k], 1) for k in range(5)},
                 round_trip_px=[_fin(c[13]), _fin(c[14])], centre_depth=[_fin(c[15], 7), _fin(c[16], 7)],
                 centre_distance_m=[_fin(c[17], 3), _fin(c[18], 3)], centre_offset_px=[c[19], c[20]],
                 near_m=_fin(c[21], 5), far_m=_fin(c[22], 3), clears_reversed=c[23], clears_standard=c[24],
                 in_force=_name(CONVENTIONS, c[25]), serial=c[26])
    current = _window(raw, at)
    at += WINDOW_SIZE
    ring = [_window(raw, at + i * WINDOW_SIZE) for i in range(WINDOWS)]
    count = min(closed, WINDOWS)
    windows = [ring[n % WINDOWS] for n in range(closed - count, closed)]
    return dict(window_ms=window_ms, closed=closed, dropped=closed - count, check=check, current=current,
                windows=windows)


def taa_depth_check_snapshot(game, address):
    """The record at `address` (None: no export, unreadable or not the record)."""
    if not address:
        return None
    try:
        raw = game.read(address, SIZE)
    except OSError:
        return None
    return taa_depth_check_parse(raw)


def _split(c):
    """Clip alone / depth alone / both, each a share of the pixels with a history place; fallback of all counted."""
    return dict(pairs=c['pairs'], clip_pct=_pct(c['clip_only'], c['sampled']),
                depth_pct=_pct(c['depth_only'], c['sampled']), both_pct=_pct(c['both'], c['sampled']),
                rejected_pct=_pct(c['clip_only'] + c['depth_only'] + c['both'], c['sampled']),
                fallback_pct=_pct(c['fallback'], c['pixels']), depth_tested_pct=_pct(c['depth_tested'], c['pixels']))


def window_summary(w, plain_ms_per_pair=None, end_seconds=None):
    """One 10 s window as the report gives it. `plain_ms_per_pair`: the pass without the depth, carried from an
    earlier window when this one had no such pair."""
    seconds = w['ms'] / 1000.0
    copy = w['copy_ms'] / w['copy_timed'] if w['copy_timed'] else None
    depth_draw = w['depth_ms'] / w['depth_timed'] if w['depth_timed'] else None
    plain = w['plain_ms'] / w['plain_timed'] if w['plain_timed'] else plain_ms_per_pair
    extra = max(0.0, depth_draw - plain) if depth_draw is not None and plain is not None else None
    depth_pass = (copy or 0.0) + (extra or 0.0) if copy is not None or extra is not None else None
    out = dict(seconds=round(seconds, 1), pairs=w['pairs'],
               fps=round(w['pairs'] / seconds, 1) if seconds > 0 else None, wanted=w['wanted'],
               found_pct=[_pct(w['found'][0], w['wanted']), _pct(w['found'][1], w['wanted'])],
               used_pct=_pct(w['used'], w['wanted']), gated=w['gated'], unchecked=w['unchecked'],
               refused=w['refused'], checks=w['checks'], checks_passed=w['checks_passed'],
               round_trip_px=_fin(w['round_trip_last']) if w['checks'] else None,
               round_trip_max_px=_fin(w['round_trip_max']) if w['checks'] else None,
               copy_ms_per_pair=_fin(copy, 3), depth_draw_ms_per_pair=_fin(depth_draw, 3),
               plain_draw_ms_per_pair=_fin(plain, 3), depth_pass_ms_per_pair=_fin(depth_pass, 3),
               split={p: {c: _split(w['counts'][(p, c)]) for c in CATEGORIES} for p in PATHS})
    if end_seconds is not None:
        out['ends_s'] = round(end_seconds, 1)
    return out, (w['plain_ms'] / w['plain_timed'] if w['plain_timed'] else plain_ms_per_pair)


def taa_depth_check_report(record):
    """The report's section: the check, each 10 s window and the session's totals."""
    if not record:
        return None
    rows, ends, plain = [], 0.0, None
    for w in record['windows']:
        ends += w['ms'] / 1000.0
        row, plain = window_summary(w, plain, ends)
        if w['pairs']:
            rows.append(row)
    counted = record['windows'] or [record['current']]
    total = dict(counted[-1])
    for k in ('ms', 'pairs', 'wanted', 'used', 'gated', 'unchecked', 'refused', 'copy_timed', 'copy_ms',
              'depth_timed', 'depth_ms', 'plain_timed', 'plain_ms', 'checks', 'checks_passed'):
        total[k] = sum(w[k] for w in counted)
    total['found'] = [sum(w['found'][i] for w in counted) for i in range(2)]
    total['round_trip_max'] = max(w['round_trip_max'] for w in counted)
    total['counts'] = {key: {k: sum(w['counts'][key][k] for w in counted) for k in counted[0]['counts'][key]}
                       for key in counted[0]['counts']}
    totals = window_summary(total)[0] if total['pairs'] else None
    return dict(check=record['check'], fast_swing=FAST_SWING, windows=rows, totals=totals)


def _n(v, fmt):
    return format(v, fmt) if v is not None else '-'


def _s(v):
    return f'{v:.1f}%' if v is not None else '-'


def check_line(section):
    """The convention check, stated plainly."""
    if not section:
        return None
    c = section['check']
    if not c['probes']:
        return ('TAA DEPTH CHECK: no check yet (vr_taa_depth off, or no pair with both eyes\' depth) - the depth '
                'path stays off until it passes.')
    trip = '/'.join(_n(v, '.4f') for v in c['round_trip_px'])
    agree = ', '.join(f"{k.split(',')[0] if 'linear' in k else k} {v:.0f}%" for k, v in c['agree_pct'].items())
    said = (f"TAA DEPTH CHECK {c['state']}: the lens's convention is reversed-Z, D3D 0..1, reciprocal, finite far "
            f"(near {_n(c['near_m'], '.3f')} m, far {_n(c['far_m'], '.0f')} m); round trip of the centre pixel "
            f"through the jittered eye view-projection L/R {trip} px (limit {ROUND_TRIP_LIMIT}); stereo test at "
            f"{c['edge_points']} depth-edge points: {agree}; best {c['best'] or '-'}; last check {c['reason']}; "
            f"{c['probes']} checks ({c['passed']} passed, {c['failed']} failed, {c['inconclusive']} inconclusive); "
            f"in force {c['in_force'] or 'none (depth path off)'}; {section['fast_swing']}.")
    if c['state'] == 'FAILED':
        said += (f" Depth path NOT enabled: likely convention error - the depth reads as {c['best'] or 'unknown'}"
                 if c['reason'] == REASONS[5] else f" Depth path NOT enabled: {c['reason']}")
    return said


def _split_words(split):
    parts = []
    for c in CATEGORIES:
        s = split[c]
        if s['pairs']:
            parts.append(f"{c} clip {_s(s['clip_pct'])} depth {_s(s['depth_pct'])} both {_s(s['both_pct'])} "
                         f"fallback {_s(s['fallback_pct'])} ({s['pairs']} pairs)")
    return '; '.join(parts) or 'no counted pairs'


def window_line(r):
    head = (f"TAA DEPTH {r['ends_s']:6.0f}s: " if 'ends_s' in r else 'TAA DEPTH session: ')
    return (head + f"{_n(r['fps'], '5.1f')} fps, depth found L {_s(r['found_pct'][0])} R {_s(r['found_pct'][1])}, "
            f"used {_s(r['used_pct'])} (gated {r['gated']}, unchecked {r['unchecked']}, refused {r['refused']}), "
            f"round trip {_n(r['round_trip_px'], '.4f')} px (max {_n(r['round_trip_max_px'], '.4f')}), depth pass "
            f"{_n(r['depth_pass_ms_per_pair'], '.3f')} ms GPU a pair (copies {_n(r['copy_ms_per_pair'], '.3f')}, "
            f"draw {_n(r['depth_draw_ms_per_pair'], '.3f')} vs {_n(r['plain_draw_ms_per_pair'], '.3f')} without) | "
            f"DEPTH PATH {_split_words(r['split']['depth'])} | HEAD PATH {_split_words(r['split']['head'])}")


def taa_depth_check_window_lines(section):
    return [window_line(r) for r in (section or {}).get('windows') or []]


def taa_depth_check_lines(section):
    """The console's words: the check, the session's totals, then one line a 10 s window."""
    if not section:
        return []
    lines = [check_line(section)]
    if section.get('totals'):
        lines.append(window_line(section['totals']))
    return lines + taa_depth_check_window_lines(section)
