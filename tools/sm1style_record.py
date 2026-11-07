# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 EYE HISTORY CHECK: SpidySm1StyleData (include/spidy/sm1style.hpp Data) for the report and the console.
No Windows imports: `game` is anything with read(address, size) -> bytes."""
import struct


def sm1style_snapshot(game, address):
    """0.0.76 EYE HISTORY CHECK: SM1STYLE's per-eye TemporalAa record (include/spidy/sm1style.hpp Data, 256
    bytes), or None. Per eye: TemporalAa runs for its job (calls), runs that wrote this frame (ran), runs whose
    history gate passed (history), and from version 2 the runs that read the eye's own history set (own_history)
    or a shared one (shared_history: the game's main set, the other eye's, or not the current set) and the runs
    whose job was not the eye's job of the run before (job_changed)."""
    raw = game.read(address, 256)
    if len(raw) != 256:
        return None
    magic, version, size, flags = struct.unpack_from('<4I', raw)
    if magic != 0x53314d53 or size != 256 or version not in (1, 2):
        return None
    reason, percent, width, height = struct.unpack_from('<4I', raw, 16)
    calls, ran, history, aa_writes, queued = (struct.unpack_from('<2Q', raw, 32 + 16 * i) for i in range(5))
    out = dict(version=version, flags=flags, reason=reason, percent=percent, render_width=width,
               render_height=height, sequence=struct.unpack_from('<Q', raw, 192)[0],
               left_jobs=struct.unpack_from('<Q', raw, 184)[0])
    extra = struct.unpack_from('<6Q', raw, 200) if version >= 2 else None
    # Build 2k: previously reserved trailing Q counts TemporalAa hook calls
    # that could not be matched to either sm1Jobs pointer.
    out['unmatched_taa_calls'] = struct.unpack_from('<Q', raw, 248)[0] if version >= 2 else None
    for i, eye in enumerate(('left', 'right')):
        out[eye] = dict(calls=calls[i], ran=ran[i], history=history[i],
                        aa_writes=aa_writes[i], queued=queued[i])
        if extra:
            out[eye].update(own_history=extra[i], shared_history=extra[2 + i], job_changed=extra[4 + i])
    return out


def sm1style_line(record):
    """The console's word on each eye's TAA history (sm1style_snapshot), or None before any eye TAA ran."""
    if not record:
        return None
    l, r = record['left'], record['right']
    if not (l['calls'] or r['calls']):
        return None
    line = (f"SM1STYLE eyes: the game's TAA ran {l['ran']} (left) / {r['ran']} (right) times, with last frame's "
            f"history {l['history']} / {r['history']}")
    if 'own_history' not in l:
        return line + '.'
    shared = l['shared_history'] + r['shared_history']
    unmatched = record.get('unmatched_taa_calls')
    missing = f'; unmatched native TAA hook calls {unmatched}' if unmatched is not None else ''
    return (line + f"; each eye's own history set in {l['own_history']} / {r['own_history']}, shared "
            f"{shared} ({'none: the eyes never read each other' if not shared else 'SHARED: the eyes mix history'}); "
            f"eye job changed {l['job_changed']} / {r['job_changed']}{missing}.")


# Build 2k: separate 16-QWORD atomic native-TAA audit (does not change
# SpidySm1StyleData's 256-byte ABI).
TAA_AUDIT_EXPORT = 'SpidyTaaAuditData'
TAA_AUDIT_FIELDS = (
    'all_hook_calls', 'unmatched_calls',
    'matched_left', 'matched_right',
    'frame_changes_left', 'frame_changes_right',
    'same_frame_left', 'same_frame_right',
    'aa_off_left', 'aa_off_right',
    'no_output_left', 'no_output_right',
    'history_gate_miss_left', 'history_gate_miss_right',
    'camera_cut_left', 'camera_cut_right',
)


def taa_audit_snapshot(game, address):
    if not address:
        return None
    raw = game.read(address, 128)
    if len(raw) != 128:
        return None
    return dict(zip(TAA_AUDIT_FIELDS, struct.unpack('<16Q', raw)))
