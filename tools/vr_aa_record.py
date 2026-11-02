# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 SPIDY TAA: SpidyVrAaData (include/spidy/vr_aa.hpp Data) for the report and the console: which eye
anti-aliasing live.txt vr_aa asked for, what ran on the last eye pair, how many pairs each pass smoothed and why a
fallback happened. No Windows imports: `game` is anything with read(address, size) -> bytes."""
import struct

EXPORT = 'SpidyVrAaData'
MAGIC = 0x41415653  # SVAA
FORMAT = '<8I3QdQ'
SIZE = struct.calcsize(FORMAT)  # 72
MODES = ('ours', 'smaa', 'legacy')
RUNNING = ('none', 'spidy_taa', 'smaa', 'fxaa')
SMAA_FAULTS = {1: 'root signature', 2: 'shader compile (d3dcompiler_47)', 3: 'pipeline', 4: 'descriptor heaps',
               5: 'textures'}
HISTORY_FAULTS = {1: 'root signature', 2: 'shader compile (d3dcompiler_47)', 3: 'pipeline', 4: 'descriptor heaps',
                  5: 'resources'}


def vr_aa_parse(raw):
    """The record as a dict from its bytes, or None (wrong size, magic or version)."""
    if raw is None or len(raw) != SIZE:
        return None
    (magic, version, size, asked, running, smaa_fault, history_fault, fell_back,
     taa_pairs, smaa_pairs, fxaa_pairs, smaa_ms, smaa_timed) = struct.unpack(FORMAT, raw)
    if magic != MAGIC or version != 1 or size != SIZE:
        return None
    return dict(asked=MODES[asked] if asked < len(MODES) else f'unknown {asked}',
                running=RUNNING[running] if running < len(RUNNING) else f'unknown {running}',
                smaa_fault=smaa_fault, history_fault=history_fault, fell_back=bool(fell_back),
                taa_pairs=taa_pairs, smaa_pairs=smaa_pairs, fxaa_pairs=fxaa_pairs,
                smaa_gpu_ms_mean=round(smaa_ms / smaa_timed, 3) if smaa_timed else None)


def vr_aa_snapshot(game, address):
    """The VR AA record at `address` (None: no export, unreadable or not the record)."""
    if not address:
        return None
    try:
        raw = game.read(address, SIZE)
    except OSError:
        return None
    return vr_aa_parse(raw)


def vr_aa_line(record):
    """The console's word on the eyes' anti-aliasing this session, or None (no record, legacy with nothing run)."""
    if not record:
        return None
    if record['asked'] == 'legacy':
        return None  # sm1style_line / eye_aa_line say it
    parts = []
    if record['taa_pairs']:
        parts.append(f"SPIDY TAA on {record['taa_pairs']} eye pairs")
    if record['smaa_pairs']:
        cost = (f", {record['smaa_gpu_ms_mean']:.2f} ms GPU per pair" if record['smaa_gpu_ms_mean'] is not None
                else '')
        parts.append(f"SMAA on {record['smaa_pairs']} eye pairs{cost}")
    if record['fxaa_pairs']:
        parts.append(f"FXAA on {record['fxaa_pairs']} eye pairs (SMAA could not run)")
    why = []
    if record['history_fault']:
        why.append('SPIDY TAA could not start (' +
                   HISTORY_FAULTS.get(record['history_fault'], str(record['history_fault'])) + ')')
    if record['smaa_fault']:
        why.append('SMAA could not start (' + SMAA_FAULTS.get(record['smaa_fault'], str(record['smaa_fault'])) + ')')
    asked = 'SPIDY TAA' if record['asked'] == 'ours' else 'SMAA'
    text = f'VR AA ({asked} asked): ' + ('; '.join(parts) if parts else 'no eye pair smoothed')
    if record['fell_back']:
        text += '; fell back'
    if why:
        text += ' - ' + '; '.join(why)
    return text + '.'
