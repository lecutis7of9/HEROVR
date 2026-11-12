# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""79.4 JITTER CHECK: SpidyJitterCheckData (include/spidy/jitter_check.hpp Data, 1024 bytes) for the report and the
console: EYE JITTER's level and its changes, each eye's lens jitter given / stored against what the game's TemporalAa
job read, the left-right difference and the other TemporalAa jobs. No Windows imports: `game` is anything with
read(address, size) -> bytes."""
import struct

EXPORT = 'SpidyJitterCheckData'
MAGIC = 0x4b434a37  # 7JCK
SIZE = 1024
LEVELS = ('off', 'low', 'med', 'full', 'e5d2746')
PIXELS = (0.0, 0.10, 0.22, 0.44, 0.88)
SOURCES = ('start', 'live.txt', 'hologram')
EYE = struct.Struct('<24Q9fI')  # 232 bytes
HEADER = struct.Struct('<8I2Q')  # 48 bytes


def _level(n):
    return LEVELS[n] if n < len(LEVELS) else str(n)


def jitter_check_parse(raw, seconds_now=None):
    """The record's fields as a dict, or None (wrong size, magic or version). seconds_now: the session's seconds when
    the record was read, so each level change gets the session second it took effect (from the DLL's tick clock)."""
    if raw is None or len(raw) != SIZE:
        return None
    magic, version, size, level, change_count, other_count, other_dropped, _, start, last = HEADER.unpack_from(raw)
    if magic != MAGIC or version != 1 or size != SIZE:
        return None
    out = dict(level=_level(level), level_number=level, max_stored_px=PIXELS[level] if level < len(PIXELS) else None,
               other_dropped=other_dropped, started=bool(start))
    eyes = []
    for i in range(2):
        v = EYE.unpack_from(raw, HEADER.size + EYE.size * i)
        runs = v[2]
        e = dict(lens=v[0], lens_rule_ok=v[1], runs=runs, job_zero=v[3], job_equals_lens_stored=v[4],
                 job_equals_twice_given=v[5], job_equals_given=v[6], job_matches_nothing=v[7],
                 match_lag_frames={k: n for k, n in enumerate(v[8:24]) if n},
                 given=[float(f'{x:.4g}') for x in v[24:26]], stored=[float(f'{x:.4g}') for x in v[26:28]],
                 job_read=[float(f'{x:.4g}') for x in v[28:30]], max_given=float(f'{v[30]:.4g}'),
                 max_job=float(f'{v[31]:.4g}'), left_right_job_diff_max=float(f'{v[32]:.4g}'))
        if runs:
            e['share'] = {k: round(100 * e[k] / runs, 1) for k in ('job_equals_lens_stored', 'job_equals_twice_given',
                                                                   'job_equals_given', 'job_matches_nothing')}
        eyes.append(e)
    out['left'], out['right'] = eyes
    out['left_right_job_diff_max'] = max(eyes[0]['left_right_job_diff_max'], eyes[1]['left_right_job_diff_max'])
    base = HEADER.size + EYE.size * 2
    out['other_jobs'] = []
    for i in range(min(other_count, 8)):
        job, runs, x, y, aa, _ = struct.unpack_from('<2Q2f2I', raw, base + 32 * i)
        out['other_jobs'].append(dict(job=hex(job), runs=runs, jitter=[float(f'{x:.4g}'), float(f'{y:.4g}')], aa=aa))
    base += 32 * 8
    out['changes'] = []
    for i in range(min(change_count, 16)):
        tick, lvl, source = struct.unpack_from('<Q2I', raw, base + 16 * i)
        change = dict(level=_level(lvl), max_stored_px=PIXELS[lvl] if lvl < len(PIXELS) else None,
                      source=SOURCES[source] if source < len(SOURCES) else str(source),
                      dll_seconds=round((tick - start) / 1000, 1) if start else None)
        if seconds_now is not None and last >= tick:
            change['session_seconds'] = round(seconds_now - (last - tick) / 1000, 1)
        out['changes'].append(change)
    return out


def jitter_check_snapshot(game, address, seconds_now=None):
    """The JITTER CHECK at `address` (None: no export, unreadable or not the record)."""
    if not address:
        return None
    try:
        raw = game.read(address, SIZE)
    except OSError:
        return None
    return jitter_check_parse(raw, seconds_now)


def jitter_check_line(record):
    """The console's word on the JITTER CHECK (jitter_check_snapshot), or None before any eye lens call."""
    if not record or not (record['left']['lens'] or record['right']['lens']):
        return None
    parts = [f"79.4 JITTER CHECK (report): level {record['level']} (up to {record['max_stored_px']} px)"]
    for name in ('left', 'right'):
        e = record[name]
        share = e.get('share')
        said = (f"{name} lens given {e['given']} stored {e['stored']}, job read {e['job_read']}"
                f" ({e['runs']} runs: = stored {share['job_equals_lens_stored']}%, 2x given "
                f"{share['job_equals_twice_given']}%, 1x {share['job_equals_given']}%, neither "
                f"{share['job_matches_nothing']}%)") if share else f'{name} lens given {e["given"]}, no TemporalAa run yet'
        parts.append(said)
    parts.append(f"left-right job difference max {record['left_right_job_diff_max']}; "
                 f"{len(record['other_jobs'])} other TemporalAa job(s)")
    if record['changes']:
        parts.append('levels: ' + ', '.join(
            f"{c['level']} at {c.get('session_seconds', c['dll_seconds'])} s ({c['source']})" for c in record['changes']))
    return '; '.join(parts) + '.'
