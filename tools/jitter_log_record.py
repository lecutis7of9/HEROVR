# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 JITTER FIX: SpidyJitterLogData (include/spidy/jitter_log.hpp Data) for the report and the console.
Each view's lens jitter for its first 200 frames: the source camera's stored +0x428/+0x42c, the pair passed to the
lens and the pair the new view stored. No Windows imports: `game` is anything with read(address, size) -> bytes."""
import struct

EXPORT = 'SpidyJitterLogData'
MAGIC = 0x474f4c4a  # JLOG
CAPACITY, ENTRY, HEADER = 2048, 32, 48
SIZE = HEADER + ENTRY * CAPACITY
VIEWS = ('head_view', 'aligned_head', 'eye_left', 'eye_right')


def jitter_log_parse(raw):
    """{'count', 'dropped', 'entries': [...]} from the record's bytes, or None (wrong size, magic or version)."""
    if raw is None or len(raw) != SIZE:
        return None
    magic, version, size, entry_size, count, dropped = struct.unpack_from('<6I', raw)
    if magic != MAGIC or version != 1 or size != SIZE or entry_size != ENTRY:
        return None
    entries, passes = [], {}
    for i in range(min(count, CAPACITY)):
        frame, view, *values = struct.unpack_from('<2I6f', raw, HEADER + ENTRY * i)
        base = view & 0xff
        if base >= len(VIEWS):
            continue
        name = VIEWS[base] + ('_stereo_screen' if view & 0x100 else '')
        key = (name, frame)
        passes[key] = passes.get(key, 0) + 1
        entries.append(dict(frame=frame, view=name, pass_=passes[key],
                            stored=[round(v, 7) for v in values[0:2]], passed=[round(v, 7) for v in values[2:4]],
                            made=[round(v, 7) for v in values[4:6]]))
    for e in entries:
        e['pass'] = e.pop('pass_')
    return dict(count=count, dropped=dropped, entries=entries)


def jitter_log_snapshot(game, address):
    """The JITTER LOG at `address` (None: no export, unreadable or not the record)."""
    if not address:
        return None
    try:
        raw = game.read(address, SIZE)
    except OSError:
        return None
    return jitter_log_parse(raw)


def jitter_log_summary(record):
    """Per view: entries, frames, the largest |stored|, |passed| and |made| per axis, and whether made is
    2*passed x and -2*passed y (the lens's rule) - the check that nothing is fed back doubled."""
    if not record:
        return {}
    out = {}
    for e in record['entries']:
        v = out.setdefault(e['view'], dict(entries=0, frames=set(), max_stored=[0, 0], max_passed=[0, 0],
                                           max_made=[0, 0], doubled_stored=0, lens_rule=0))
        v['entries'] += 1
        v['frames'].add(e['frame'])
        for k in ('stored', 'passed', 'made'):
            v[f'max_{k}'] = [max(v[f'max_{k}'][i], abs(e[k][i])) for i in range(2)]
        # The pair passed is the source's stored pair (and not zero): c71a59c's feedback, doubled again.
        v['doubled_stored'] += any(e['stored']) and e['passed'] == e['stored']
        v['lens_rule'] += (abs(e['made'][0] - 2 * e['passed'][0]) < 1e-6 and
                           abs(e['made'][1] + 2 * e['passed'][1]) < 1e-6)
    for v in out.values():
        v['frames'] = len(v['frames'])
    return out


def jitter_log_line(record):
    """The console's word on each view's jitter (jitter_log_snapshot), or None before any entry."""
    summary = jitter_log_summary(record)
    if not summary:
        return None
    parts = [f"{name}: {v['entries']} passes over {v['frames']} frames, passed max {v['max_passed'][0]:.6f}/"
             f"{v['max_passed'][1]:.6f}, stored by its lens max {v['max_made'][0]:.6f}/{v['max_made'][1]:.6f}, "
             f"fed the source's stored pair back {v['doubled_stored']}x"
             for name, v in sorted(summary.items())]
    return 'JITTER LOG (each view, first 200 frames): ' + '; '.join(parts) + '.'
