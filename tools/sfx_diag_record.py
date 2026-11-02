# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR. Made by Agent Mulder.
"""0.0.76 SFX DIAG: SpidySfxDiagData (include/spidy/sfx_diag.hpp Data), the mod's own XAudio2 sound effects layer,
for the report and the console. Read-only; nothing is written to disk from the audio callbacks (the DLL only counts
into this record, the launcher reads it with the rest every few seconds). The format line once (our voices, the
mastering voice, the default device's shared-mode mix format and name), then every 10 s: buffers submitted /
completed, the deepest queue, XAudio2 glitches, voice and critical errors, the audio passes and their largest gap, the
player thread's late wake-ups, the layer's state (live.txt vr_sfx_layer) and the active voices; glitches, critical
errors, late passes / wake-ups (50 ms or more) and layer switches each on their own line with their QPC time (the clock
time.perf_counter reads; the render hitches use it too). No Windows imports: `game` is anything with
read(address, size) -> bytes."""
import struct

from taa_depth_trace_record import hitch_wall

EXPORT = 'SpidySfxDiagData'
MAGIC = 0x47445853  # SXDG
HEAD = '<8I'
FORMAT = '<16I64s'
WINDOW = '<16I'
MOMENT = '<QII'
WINDOWS = 90
MOMENTS = 64
SIZE = (struct.calcsize(HEAD) + struct.calcsize(FORMAT) + struct.calcsize(WINDOW) * (WINDOWS + 1) +
        struct.calcsize(MOMENT) * MOMENTS)  # 7008
WINDOW_KEYS = ('ms', 'submitted', 'completed', 'queued_last', 'queued_max', 'glitches', 'voice_errors',
               'critical_errors', 'passes', 'pass_gap_us_sum', 'pass_gap_us_max', 'wakes', 'wake_late_us_sum',
               'wake_late_us_max', 'layer', 'active_voices')
FORMAT_KEYS = ('made', 'master_rate', 'master_channels', 'master_mask', 'mix_hr', 'mix_rate', 'mix_channels',
               'mix_bits', 'source_rate_min', 'source_rate_max', 'source_channels_min', 'source_channels_max',
               'source_bits', 'source_voices', 'resampled', 'xaudio')
KINDS = {1: 'XAudio2 glitch (total so far)', 2: 'XAudio2 critical error (HRESULT)',
         3: 'late audio pass (us since the last)', 4: 'late player wake-up (us late)', 5: 'layer OFF (vr_sfx_layer 1)',
         6: 'layer ON (vr_sfx_layer 0)'}


def sfx_diag_parse(raw):
    """The record as a dict from its bytes, or None (wrong size, magic or version)."""
    if raw is None or len(raw) != SIZE:
        return None
    magic, version, size, window_ms, closed, noted, layer, _ = struct.unpack_from(HEAD, raw, 0)
    if magic != MAGIC or version != 1 or size != SIZE:
        return None
    at = struct.calcsize(HEAD)
    f = struct.unpack_from(FORMAT, raw, at)
    fmt = dict(zip(FORMAT_KEYS, f[:16]))
    fmt['device'] = f[16].split(b'\0', 1)[0].decode('utf-8', 'replace')
    at += struct.calcsize(FORMAT)
    windows = [dict(zip(WINDOW_KEYS, struct.unpack_from(WINDOW, raw, at + i * struct.calcsize(WINDOW))))
               for i in range(WINDOWS + 1)]
    current, ring = windows[0], windows[1:]
    at += struct.calcsize(WINDOW) * (WINDOWS + 1)
    moment_ring = [struct.unpack_from(MOMENT, raw, at + i * struct.calcsize(MOMENT)) for i in range(MOMENTS)]
    count = min(closed, WINDOWS)
    held = min(noted, MOMENTS)
    moments = [dict(qpc_us=q, kind=KINDS.get(k, str(k)), value=v)
               for q, k, v in (moment_ring[n % MOMENTS] for n in range(noted - held, noted))]
    return dict(window_ms=window_ms, closed=closed, dropped=closed - count, moments_noted=noted, layer_on=bool(layer),
                format=fmt if fmt['made'] else None, current=current,
                windows=[ring[n % WINDOWS] for n in range(closed - count, closed)], moments=moments)


def sfx_diag_snapshot(game, address):
    """The record at `address` (None: no export, unreadable or not the record)."""
    if not address:
        return None
    try:
        raw = game.read(address, SIZE)
    except OSError:
        return None
    return sfx_diag_parse(raw)


def window_summary(w, end_seconds=None):
    out = dict(seconds=round(w['ms'] / 1000.0, 1), layer_on=bool(w['layer']), submitted=w['submitted'],
               completed=w['completed'], queued_max=w['queued_max'], glitches=w['glitches'],
               voice_errors=w['voice_errors'], critical_errors=w['critical_errors'], passes=w['passes'],
               pass_gap_ms_mean=round(w['pass_gap_us_sum'] / w['passes'] / 1000.0, 2) if w['passes'] else None,
               pass_gap_ms_max=round(w['pass_gap_us_max'] / 1000.0, 1), wakes=w['wakes'],
               wake_late_ms_mean=round(w['wake_late_us_sum'] / w['wakes'] / 1000.0, 2) if w['wakes'] else None,
               wake_late_ms_max=round(w['wake_late_us_max'] / 1000.0, 1), active_voices=w['active_voices'])
    if end_seconds is not None:
        out['ends_s'] = round(end_seconds, 1)
    return out


def sfx_diag_report(record):
    if not record:
        return None
    rows, ends = [], 0.0
    for w in record['windows']:
        ends += w['ms'] / 1000.0
        rows.append(window_summary(w, ends))
    return dict(format=record['format'], layer_on=record['layer_on'], windows=rows, moments=record['moments'],
                moments_noted=record['moments_noted'], dropped=record['dropped'])


def format_line(record):
    f = (record or {}).get('format')
    if not f:
        return None
    mix = (f"{f['mix_rate']} Hz {f['mix_channels']} ch {f['mix_bits']}-bit" if not f['mix_hr']
           else f"unread (0x{f['mix_hr']:08X})")
    return (f"SFX DIAG format: XAudio2 2.{f['xaudio']}, mastering voice {f['master_rate']} Hz {f['master_channels']} ch "
            f"(mask 0x{f['master_mask']:X}); default device '{f['device'] or '?'}' mix {mix}; our "
            f"{f['source_voices']} source voices {f['source_rate_min']}-{f['source_rate_max']} Hz "
            f"{f['source_channels_min']}-{f['source_channels_max']} ch {f['source_bits']}-bit, {f['resampled']} "
            f"resampled to the mastering rate")


def window_line(r):
    head = f"SFX DIAG {r['ends_s']:6.0f}s: " if 'ends_s' in r else 'SFX DIAG: '
    return head + (f"layer {'ON' if r['layer_on'] else 'OFF'}, buffers {r['submitted']} submitted / "
                   f"{r['completed']} done (queue max {r['queued_max']}), {r['active_voices']} voices active, "
                   f"{r['glitches']} glitches, {r['voice_errors']} voice / {r['critical_errors']} critical errors, "
                   f"{r['passes']} passes (gap {r['pass_gap_ms_mean'] if r['pass_gap_ms_mean'] is not None else '-'} "
                   f"ms mean / {r['pass_gap_ms_max']} max), player wake-ups late "
                   f"{r['wake_late_ms_mean'] if r['wake_late_ms_mean'] is not None else '-'} ms mean / "
                   f"{r['wake_late_ms_max']} max")


def moment_line(m, perf_now=None, wall_now=None):
    return (f"SFX MOMENT at qpc {m['qpc_us'] / 1e6:.3f} s ({hitch_wall(m['qpc_us'], perf_now, wall_now)}): "
            f"{m['kind']} {m['value']}")
