# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR (Ilya Mezerowsky).
"""ADDRESS FINDER (Update 27 / 27.3): SpidyAddressFinderData / SpidyAddressFinderReport.

Candidates only — never applied as live SM2 addresses. The player turns it on from the wrist
hologram GRAPHICS page (or live.txt address_finder = on), plays about a minute, then sends the
written report files with the normal session report.

Reports always land in <port>/reports/ as address-finder-<stamp>.{json,txt} plus
address-finder-latest.{json,txt}. Update 27.2 also writes partials while running.
Update 27.3: Data.candidates = confirmed count; Data.rawHits = capped store size; HUD DONE n
is confirmed only. Helpers below mirror trivial reject / K-frame confirm / raw-hit cap.
Update 28: candidate lines carry "almost" / "near_miss"; the .txt lists CONFIRMED, then ALMOST
(unconfirmed but seen on 2+ frames with a value, or within 4x tolerance), then the rest; the
console line says how many were almost (the HUD shows DONE n +m ALMOST).
"""
import json
import math
import struct
import time
from pathlib import Path

import ortho_dump

# Update 30.1: the finder report says which update wrote it (PORT-VERSION beside tools/ too).
UPDATE = 33


def public_version():
    try:
        return (Path(__file__).resolve().parent.parent / 'VERSION').read_text(encoding='utf-8').strip() or None
    except OSError:
        return None


def port_version():
    try:
        return (Path(__file__).resolve().parent.parent / 'PORT-VERSION').read_text(encoding='utf-8').strip() or None
    except OSError:
        return None

MAGIC = 0x53414644  # SAFD
VERSION = 1
DATA_BYTES = 80
REPORT_BYTES = 1024 * 1024  # Update 32: room for ORTHO CAPTURE (was 256 KiB)
STATUS = {0: 'idle', 1: 'running', 2: 'done', 3: 'failed'}


def parse_data(raw):
    """Snapshot from SpidyAddressFinderData, or None."""
    if not raw or len(raw) < DATA_BYTES:
        return None
    magic, version, size, status = struct.unpack_from('<4I', raw, 0)
    if magic != MAGIC or version != VERSION or size != DATA_BYTES:
        return None
    sequence, = struct.unpack_from('<q', raw, 16)
    seconds_left, candidates, generation, fault = struct.unpack_from('<4I', raw, 24)
    frames, scans, export_hits, string_hits = struct.unpack_from('<4Q', raw, 40)
    lod_offset, raw_hits = struct.unpack_from('<2I', raw, 72)
    return dict(
        version=version,
        status=status,
        status_name=STATUS.get(status, str(status)),
        sequence=sequence,
        seconds_left=seconds_left,
        candidates=candidates,  # confirmed (HUD DONE n)
        generation=generation,
        fault=fault,
        frames=frames,
        scans=scans,
        export_hits=export_hits,
        string_hits=string_hits,
        lod_offset=None if lod_offset == 0xFFFFFFFF else lod_offset,
        raw_hits=raw_hits,
        label='candidates_only',
    )


def snapshot(game, address):
    """Consistent read of SpidyAddressFinderData, or None."""
    try:
        for _ in range(8):
            raw = game.read(address, DATA_BYTES)
            if len(raw) < DATA_BYTES:
                return None
            if struct.unpack_from('<q', raw, 16)[0] % 2 != 0:
                continue
            if game.read(address + 16, 8) != raw[16:24]:
                continue
            return parse_data(raw)
    except Exception:  # noqa: BLE001
        return None
    return None


def read_report(game, address):
    """NUL-terminated JSON Lines from SpidyAddressFinderReport, or ''."""
    try:
        raw = game.read(address, REPORT_BYTES)
        if not raw:
            return ''
        end = raw.find(b'\0')
        if end < 0:
            end = len(raw)
        return raw[:end].decode('utf-8', errors='replace')
    except Exception:  # noqa: BLE001
        return ''


def parse_report_lines(text):
    """List of JSON objects from JSON Lines text."""
    out = []
    for line in (text or '').splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            out.append(json.loads(line))
        except json.JSONDecodeError:
            out.append({'type': 'raw', 'line': line})
    return out



# Mirror of include/spidy/address_finder.hpp quality helpers (Update 27.3).
CONFIRM_FRAMES_K = 3
MAX_RAW_HITS = 256
CONFIRM_MIN_DELTA = 1e-4
STRONG_SCORE = 3.0
TRIVIAL_EPS = 1e-6
DENORMAL_FLOOR = 1e-38


def is_denormal(v):
    return v != 0.0 and math.isfinite(v) and abs(v) < DENORMAL_FLOOR



def is_trivial_scalar(v):
    if not math.isfinite(v) or (v != 0.0 and abs(v) < DENORMAL_FLOOR):
        return True
    return abs(v) <= TRIVIAL_EPS or abs(v - 1.0) <= TRIVIAL_EPS or abs(v + 1.0) <= TRIVIAL_EPS


def is_trivial_mat(m16):
    if any(not math.isfinite(v) for v in m16):
        return True
    if all(abs(v) <= TRIVIAL_EPS for v in m16):
        return True
    for i, v in enumerate(m16):
        want = 1.0 if (i % 5 == 0) else 0.0
        if abs(v - want) > 1e-4:
            return False
    return True


def note_confirm_sample(state, frame, sample, need=CONFIRM_FRAMES_K, min_delta=CONFIRM_MIN_DELTA):
    """state: dict with distinct_frames, last_frame, last, has_last. Returns confirmed bool."""
    if sample is None:
        return state.get('distinct_frames', 0) >= need
    if state.get('last_frame') == frame:
        return state.get('distinct_frames', 0) >= need
    changed = not state.get('has_last')
    if state.get('has_last'):
        d = max(abs(sample[i] - state['last'][i]) for i in range(4))
        changed = d >= min_delta
    if not changed and state.get('distinct_frames', 0) > 0:
        return state.get('distinct_frames', 0) >= need
    state['last_frame'] = frame
    state['last'] = list(sample[:4])
    state['has_last'] = True
    state['distinct_frames'] = state.get('distinct_frames', 0) + 1
    return state['distinct_frames'] >= need


def score_confirmed(distinct_frames, raw_hits, error, category):
    score = float(distinct_frames) * 2.0 + min(float(raw_hits), 16.0) * 0.25
    score += max(0.0, 1.0 - error * 100.0)
    if category in ('lighting', 'taa', 'lod'):
        score += 1.0
    return score


def raw_hits_at_cap(stored, cap=MAX_RAW_HITS):
    return stored >= cap

# Mirror of include/spidy/address_finder.hpp frame-budget helpers (Update 27.2).
FRAME_BUDGET_NS = 500_000  # 0.5 ms
FRAME_BUDGET_MAX_OPS = 8
PARTIAL_WRITE_S = 10


def frame_budget_allows(ops_done, max_ops, elapsed_ns, budget_ns):
    """True while the render thread may do another live sample this frame."""
    return ops_done < max_ops and elapsed_ns < budget_ns


def frame_budget_next_skip(previous_skip, over_budget):
    """Backoff: double skip frames after an overrun, else decay by one."""
    if over_budget:
        return 2 if previous_skip == 0 else (previous_skip * 2 if previous_skip < 32 else 32)
    return previous_skip - 1 if previous_skip > 0 else 0


def candidate_bucket(obj):
    """Update 28: 'confirmed', 'almost' or 'raw' for a candidate line (None for others)."""
    if obj.get('type') != 'candidate':
        return None
    if obj.get('confirmed'):
        return 'confirmed'
    return 'almost' if obj.get('almost') else 'raw'


def almost_count(lines):
    return sum(1 for o in lines if candidate_bucket(o) == 'almost')


def is_almost(confirmed, value_bearing, distinct_frames, near_miss):
    """Mirror of address_finder.hpp isAlmost (Update 28)."""
    if confirmed:
        return False
    return bool(near_miss) or (bool(value_bearing) and distinct_frames >= 2)


def candidate_text(rows):
    out = []
    for obj in rows:
        conf = obj.get('confirmed')
        conf_s = '' if conf is None else f" confirmed={conf}"
        score = obj.get('score')
        score_s = '' if score is None else f" score={score}"
        df = obj.get('distinct_frames')
        df_s = '' if df is None else f" frames={df}"
        near_s = ' NEAR' if obj.get('near_miss') else ''
        out.append(
            f"  [{obj.get('kind')}] rva=0x{int(obj.get('rva') or 0):x} "
            f"fn=0x{int(obj.get('function_rva') or 0):x} hits={obj.get('hits')} "
            f"cb+{obj.get('cb_offset')} slot={obj.get('root_slot')}{conf_s}{df_s}{score_s}{near_s}"
        )
        if obj.get('signature'):
            out.append(f"    sig: {obj['signature']}")
        if obj.get('detail'):
            out.append(f"    {obj['detail']}")
    return out


def write_files(reports_dir, data, report_text, when=None, partial=False):
    """Write reports/address-finder-<time>.{json,txt} and address-finder-latest.*.

    Returns (json_path, txt_path). Always creates the reports folder. Latest aliases
    overwrite so the player can look for a fixed name under C:\\sm2-port\\reports.
    """
    reports_dir = Path(reports_dir)
    reports_dir.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime('%Y%m%d-%H%M%S', time.localtime(when or time.time()))
    tag = 'partial-' + stamp if partial else stamp
    base = reports_dir / f'address-finder-{tag}'
    lines = parse_report_lines(report_text)
    payload = {
        'label': 'candidates_only',
        'update': UPDATE,
        'version': public_version(),
        'port_version': port_version(),
        'partial': bool(partial),
        'data': data,
        'lines': lines,
    }
    json_path = base.with_suffix('.json')
    txt_path = base.with_suffix('.txt')
    json_path.write_text(json.dumps(payload, indent=2) + '\n', encoding='utf-8')
    status = (data or {}).get('status_name')
    kind = 'PARTIAL' if partial else 'report'
    txt_lines = [
        f'ADDRESS FINDER {kind} (Update {UPDATE}; Version {public_version() or "unknown"}, port {port_version() or "unknown"}) — CANDIDATES ONLY, not live addresses.',
        f"status: {status}  confirmed: {(data or {}).get('candidates')}  almost: {almost_count(lines)}  "
        f"raw_hits: {(data or {}).get('raw_hits')}  shadow_pass_candidates: {shadow_count(lines)}  "
        f"fault: {(data or {}).get('fault')}  frames: {(data or {}).get('frames')}",
        f"lod_offset: {(data or {}).get('lod_offset')}",
        f'folder: {reports_dir.resolve()}',
        '',
    ]
    txt_lines.extend(shadow_pass_summary(lines))
    txt_lines.extend(ortho_dump.summary_lines(lines))  # Update 32
    txt_lines.extend(camera_summary(lines))
    others = [o for o in lines if o.get('type') != 'candidate']
    for title, rows in (('--- CONFIRMED ---', [o for o in lines if candidate_bucket(o) == 'confirmed']),
                        ('--- ALMOST (not confirmed; worth a look) ---',
                         [o for o in lines if candidate_bucket(o) == 'almost']),
                        ('--- other raw hits ---', [o for o in lines if candidate_bucket(o) == 'raw'])):
        txt_lines.append(title)
        if not rows:
            txt_lines.append('  (none)')
        txt_lines.extend(candidate_text(rows))
    if others:
        txt_lines.append('--- header / tracks / footer ---')
    for obj in others:
        if obj.get('type') in ('jitter_track', 'lod', 'header', 'footer', 'bind_summary', 'table_summary',
                                'ortho_capture_summary'):
            txt_lines.append(f"  {obj}")
    if not lines:
        txt_lines.append('  (report buffer empty — native side had nothing to export yet)')
    txt_path.write_text('\n'.join(txt_lines) + '\n', encoding='utf-8')
    latest_j = reports_dir / 'address-finder-latest.json'
    latest_t = reports_dir / 'address-finder-latest.txt'
    latest_j.write_text(json_path.read_text(encoding='utf-8'), encoding='utf-8')
    latest_t.write_text(txt_path.read_text(encoding='utf-8'), encoding='utf-8')
    # Update 32: the readable ORTHO CAPTURE dump beside them (a failure loses only the dump).
    try:
        (reports_dir / ortho_dump.LATEST_NAME).write_text(ortho_dump.dump_text(lines), encoding='utf-8')
    except Exception:
        pass
    return json_path, txt_path


CAMERA_TAGS = ('FLAT-ONLY', 'HEAD-ONLY', 'EYE-ONLY')


def camera_summary(lines):
    """Update 29: how many constant-buffer hits each camera explains (cams= / FLAT-ONLY in the detail).

    FLAT-ONLY hits were built from the game's own camera while the eyes were elsewhere: the
    suspects for shadows and lighting that slide with the head. Empty before Update 29 reports."""
    rows = [o for o in lines if o.get('type') == 'candidate' and o.get('kind') in ('cb_match', 'cb_near')
            and 'cams=' in (o.get('detail') or '')]
    if not rows:
        return []
    out = ['--- WHICH CAMERA (Update 29; candidates only) ---']
    for tag in CAMERA_TAGS:
        hit = [o for o in rows if ' ' + tag + ' ' in (o.get('detail') or '') + ' ']
        exact = sum(1 for o in hit if o.get('kind') == 'cb_match')
        out.append(f'  {tag}: {len(hit)} ({exact} exact)')
        sites = sorted({(o.get('function_rva') or o.get('rva') or 0) for o in hit if o.get('kind') == 'cb_match'})
        if sites:
            out.append('    functions: ' + ', '.join(f'0x{x:x}' for x in sites[:8]))
    mixed = sum(1 for o in rows if not any(' ' + t + ' ' in (o.get('detail') or '') + ' ' for t in CAMERA_TAGS))
    out.append(f'  mixed or not told apart: {mixed}')
    out.append('')
    return out

def done_line(data, json_path, almost=0, shadow=0):
    """Console line when a run finishes (or fails). HUD n = confirmed (Update 28: +m almost;
    Update 30: k shadow pass candidates)."""
    n = (data or {}).get('candidates', 0)
    raw = (data or {}).get('raw_hits', 0)
    st = (data or {}).get('status_name', '?')
    almost_s = f', {almost} almost' if almost else ''
    shadow_s = f', {shadow} shadow pass candidates' if shadow else ''
    return f'ADDRESS FINDER {st}: {n} confirmed{almost_s}{shadow_s} ({raw} raw hits), report saved to {json_path}'


def is_shadow_candidate(obj):
    """Mirror of address_finder.hpp shadowPassCandidate (Update 30): a depth-only pass
    that bound this session's camera data (seed29 rows never count)."""
    return (obj.get('type') == 'bind' and obj.get('pass') == 'depth_only' and not obj.get('seed29')
            and (obj.get('cams') or 'none') != 'none')


def shadow_count(lines):
    return sum(1 for o in lines if is_shadow_candidate(o))


def bind_text(obj):
    vp = obj.get('viewport') or [0, 0]
    depth = obj.get('depth') or {}
    depth_s = ''
    if depth.get('format'):
        depth_s = f" depth fmt={depth.get('format')} {depth.get('width')}x{depth.get('height')}"
    seed_s = ' SEED-29 (page from the Update 29 run; camera unknown now)' if obj.get('seed29') else ''
    tag = obj.get('tag') or ''
    table_s = f" table[{int(obj['table']) - 1}]" if obj.get('table') else ''  # Update 31: heap index
    return (f"  [{obj.get('api')}]{table_s} rva=0x{int(obj.get('rva') or 0):x} fn=0x{int(obj.get('function_rva') or 0):x} "
            f"slot={obj.get('root_slot')} cb+{obj.get('cb_offset')} pass={obj.get('pass')} "
            f"rt={obj.get('rt_count')} dsv={obj.get('dsv')} viewport={vp[0]}x{vp[1]}{depth_s} "
            f"cams={obj.get('cams')}{' ' + tag if tag else ''} {obj.get('match')} "
            f"hits={obj.get('hits')} frames={obj.get('frames')}{seed_s}")


def shadow_pass_summary(lines):
    """Update 30: SHADOW PASS CANDIDATES — where camera data was bound, and into what pass.

    Depth-only (no colour target, a depth target) passes come first: the likely shadow maps.
    Then compute / colour passes, where a lighting or shadow resolve may use a camera that
    is not the eye's (HEAD-ONLY / FLAT-ONLY there are the suspects for sliding shadows)."""
    binds = [o for o in lines if o.get('type') == 'bind']
    summary = next((o for o in lines if o.get('type') == 'bind_summary'), None)
    if not binds and not summary:
        return []
    out = ['--- SHADOW PASS CANDIDATES (Update 30; candidates only) ---']
    shadows = [o for o in binds if is_shadow_candidate(o)]
    out.append(f'  depth-only passes that bound camera data: {len(shadows)}')
    out.extend(bind_text(o) for o in sorted(shadows, key=lambda o: -(o.get('frames') or 0)))
    rank = {'FLAT-ONLY': 0, 'HEAD-ONLY': 1, 'EYE-ONLY': 2}
    rest = [o for o in binds if not is_shadow_candidate(o)]
    rest.sort(key=lambda o: (bool(o.get('seed29')), rank.get(o.get('tag') or '', 3), -(o.get('frames') or 0)))
    out.append(f'  other binds of camera data (lighting / resolve / main passes): {len(rest)}')
    out.extend(bind_text(o) for o in rest)
    if summary:
        out.append(f"  store: {summary.get('rows')}/{summary.get('cap')} rows, {summary.get('dropped')} dropped, "
                   f"{summary.get('watched')} watched values, pass hooks {summary.get('pass_hooks')}, "
                   f"descriptor-table hooks {summary.get('descriptor_tables')}, "
                   f"watch cap {summary.get('watched_cap', '?')}")
    out.extend(table_summary_text(lines))
    out.append('')
    return out


def table_summary_text(lines):
    """Update 31: descriptor-table binds per pass, the bindless verdict, depth targets
    (shadow map sizes) and orthographic boxes in uploads (cascade size candidates)."""
    out = []
    t = next((o for o in lines if o.get('type') == 'table_summary'), None)
    if t:
        heap = t.get('heap') or {}
        out.append('--- TABLE BINDS (Update 31; candidates only) ---')
        out.append(f"  hooks {t.get('hooks')}; table binds {t.get('table_binds')}, at heap index 0 "
                   f"{t.get('heap_start_binds')} -> bindless: {t.get('bindless')}")
        out.append(f"  heap: {heap.get('descriptors')} descriptors x {heap.get('increment')} bytes "
                   f"(known={heap.get('known')}); descriptors mapped {t.get('descriptors_mapped')}, created "
                   f"{t.get('descriptors_created')}, copied {t.get('descriptors_copied')}; tables covering "
                   f"camera data {t.get('tables_covering_camera')}")
        for api, kinds in (t.get('binds_by_pass') or {}).items():
            if any(kinds.values()):
                out.append(f"  {api}: " + ' '.join(f'{k}={v}' for k, v in kinds.items()))
    depth = [o for o in lines if o.get('type') == 'depth_target']
    if depth:
        out.append('--- DEPTH TARGETS (Update 31; square/large with depth-only binds = likely shadow maps) ---')
        for d in sorted(depth, key=lambda d: -(d.get('depth_only_binds') or 0)):
            out.append(f"  fmt={d.get('format')} {d.get('width')}x{d.get('height')} transitions="
                       f"{d.get('transitions')} depth_only_binds={d.get('depth_only_binds')}")
    boxes = [o for o in lines if o.get('type') == 'ortho_box']
    if boxes:
        out.append('--- ORTHO BOXES (Update 31; cascade size candidates, half sizes in metres) ---')
        for b in sorted(boxes, key=lambda b: -(b.get('hits') or 0)):
            h = b.get('half_m') or [0, 0, 0]
            out.append(f"  rva=0x{int(b.get('rva') or 0):x} record+{b.get('record_offset')} "
                       f"half={h[0]:g}x{h[1]:g}x{h[2]:g} hits={b.get('hits')}")
    return out


def partial_line(data, json_path):
    n = (data or {}).get('candidates', 0)
    raw = (data or {}).get('raw_hits', 0)
    return f'ADDRESS FINDER partial: {n} confirmed ({raw} raw) so far → {json_path}'


def missing_line(reports_dir, reason):
    """Console line when a report could not be written."""
    return f'ADDRESS FINDER: no report file under {Path(reports_dir)} ({reason})'
