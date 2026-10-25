# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR (Ilya Mezerowsky).
"""Update 32: ORTHO CAPTURE dump — the orthographic (shadow-cascade-shaped) matrices the
ADDRESS FINDER caught in uploaded constants, made readable.

Candidates only. For each capture group (a burst of a few frames every 2.5 s) it prints
each matrix with its GPU page + offset, the box's width / height / depth in metres, the
light direction (the box's depth axis), its centre against the head and the flat camera,
the upload call chain, and an ascending-float scan of the record around it (a cascade
split array would show up there). Then which boxes share one sun direction, how the
boxes move with head pitch, and which call chains wrote them.

    python ortho_dump.py C:\\sm2-port\\reports\\address-finder-latest.json  [out.txt]
"""
import json
import math
import sys
from collections import Counter, defaultdict
from pathlib import Path

LATEST_NAME = 'address-finder-latest-ortho.txt'


def captures(lines):
    return [o for o in lines if o.get('type') == 'ortho_capture']


def _dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def _norm(v):
    n = math.sqrt(_dot(v, v))
    return [x / n for x in v] if n > 0 else [0.0, 0.0, 0.0]


def axes(m, row_form):
    """The three scaled world axes of the box (row form: columns; column form: rows)."""
    if row_form:
        return [[m[a], m[4 + a], m[8 + a]] for a in range(3)]
    return [[m[a * 4], m[a * 4 + 1], m[a * 4 + 2]] for a in range(3)]


def translation(m, row_form):
    return [m[12], m[13], m[14]] if row_form else [m[3], m[7], m[11]]


def _solve3(rows, rhs):
    """Solve rows . x = rhs (rows: 3 equations), None if singular."""
    (a, b, c), (d, e, f), (g, h, i) = rows
    det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)
    if abs(det) < 1e-30:
        return None
    inv = [[(e * i - f * h) / det, (c * h - b * i) / det, (b * f - c * e) / det],
           [(f * g - d * i) / det, (a * i - c * g) / det, (c * d - a * f) / det],
           [(d * h - e * g) / det, (b * g - a * h) / det, (a * e - b * d) / det]]
    return [sum(inv[r][k] * rhs[k] for k in range(3)) for r in range(3)]


def centre(m, row_form):
    """World point the box maps to clip (0,0,0): solve axis_a . p + t_a = 0 for each axis."""
    t = translation(m, row_form)
    return _solve3(axes(m, row_form), [-x for x in t])


def pitch_yaw(fwd):
    if not fwd or not any(fwd):
        return None, None
    f = _norm(fwd)
    return (math.degrees(math.asin(max(-1.0, min(1.0, f[1])))),
            math.degrees(math.atan2(f[0], f[2])))


def analyse(c):
    m = c.get('matrix') or [0.0] * 16
    rf = bool(c.get('row_form'))
    ax = axes(m, rf)
    half = c.get('half_m') or [1 / math.sqrt(_dot(a, a)) if _dot(a, a) else 0 for a in ax]
    light = _norm(ax[2])
    cen = centre(m, rf)
    out = dict(capture=c, size=[2 * h for h in half], light=light, centre=cen)
    for who in ('head', 'flat'):
        pose = c.get(who)
        if pose and cen:
            pos, fwd = pose[:3], pose[3:6]
            d = [cen[k] - pos[k] for k in range(3)]
            flat_fwd = _norm([fwd[0], 0.0, fwd[2]])
            out[who + '_offset'] = d
            out[who + '_ahead'] = d[0] * flat_fwd[0] + d[2] * flat_fwd[2]
            out[who + '_horizontal'] = math.hypot(d[0], d[2])
            out[who + '_pitch'], out[who + '_yaw'] = pitch_yaw(fwd)
    return out


def split_scan(window, matrix_index, sizes=()):
    """Runs of 3+ ascending positive floats (0.05 .. 20000, last >= 4x first) outside the matrix.
    matrix_index: the matrix's first float in the window. sizes: box sizes to compare against."""
    found = []
    skip = set(range(matrix_index, matrix_index + 16)) if matrix_index is not None else set()
    run = []

    def close():
        if len(run) >= 3 and window[run[-1]] >= 4 * window[run[0]]:
            vals = [window[k] for k in run]
            match = [v for v in vals if any(abs(v - s) <= .02 * s or abs(v - s / 2) <= .02 * s / 2 for s in sizes)]
            found.append(dict(index=run[0], values=vals, matches_box=match))

    for k, v in enumerate(window):
        ok = k not in skip and isinstance(v, (int, float)) and math.isfinite(v) and .05 <= v <= 20000
        if ok and run and v > window[run[-1]] * 1.0001 and k == run[-1] + 1:
            run.append(k)
            continue
        close()
        run = [k] if ok else []
    close()
    return found


def _f(v, w=9):
    return f'{v:{w}.4g}'


def _corr(xs, ys):
    if len(xs) < 3:
        return None
    mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
    sx = math.sqrt(sum((x - mx) ** 2 for x in xs))
    sy = math.sqrt(sum((y - my) ** 2 for y in ys))
    if sx < 1e-9 or sy < 1e-9:
        return None
    return sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / (sx * sy)


def size_class(size):
    return f'{size[0]:.3g}x{size[1]:.3g}x{size[2]:.3g}'


def dump_text(lines):
    caps = captures(lines)
    summary = next((o for o in lines if o.get('type') == 'ortho_capture_summary'), None)
    out = ['ORTHO CAPTURE DUMP (Update 32; candidates only — nothing here is applied)']
    if summary:
        out.append(f"captures {summary.get('captures')}/{summary.get('cap')} in {summary.get('groups')} groups "
                   f"(one every {summary.get('every_ms')} ms)")
    if not caps:
        out.append('No ortho matrices were captured (ADDRESS FINDER did not run, or no cascade-shaped box '
                   'was uploaded through CopyBufferRegion while it did).')
        return '\n'.join(out) + '\n'
    out += ['Units: metres. size = width x height x depth of the box. light = the box depth axis '
            '(sign unknown: toward or away from the sun). centre = where the box maps to clip (0,0,0); '
            'ahead = metres along the head (or flat camera) facing, on the ground plane. '
            'pitch: + up, - down (Y up).', '']
    rows = [analyse(c) for c in caps]
    groups = defaultdict(list)
    for r in rows:
        groups[r['capture'].get('group')].append(r)
    for g in sorted(groups):
        first = groups[g][0]
        hp = first.get('head_pitch')
        fp = first.get('flat_pitch')
        c0 = first['capture']
        out.append(f"=== group {g}  t={c0.get('ms', 0) / 1000:.1f}s  frame {c0.get('frame')}  "
                   f"head pitch {'%.1f' % hp if hp is not None else '?'} yaw "
                   f"{'%.1f' % first['head_yaw'] if first.get('head_yaw') is not None else '?'}  "
                   f"flat pitch {'%.1f' % fp if fp is not None else '?'} yaw "
                   f"{'%.1f' % first['flat_yaw'] if first.get('flat_yaw') is not None else '?'} ===")
        for r in groups[g]:
            c = r['capture']
            va = int(c.get('gpu_va') or 0)
            m = c.get('matrix') or []
            chain = ' <- '.join(f'0x{x:x}' for x in c.get('chain') or []) or '?'
            out.append(f"  page 0x{va & ~0xfff:x} +0x{va & 0xfff:x}, copy+{c.get('matrix_offset')} "
                       f"(record+{(c.get('matrix_offset') or 0) & 255}) of {c.get('copy_bytes')} bytes, "
                       f"{'row' if c.get('row_form') else 'column'} form")
            out.append(f"    size {r['size'][0]:.2f} x {r['size'][1]:.2f} x {r['size'][2]:.2f} m   "
                       f"light ({r['light'][0]:+.4f}, {r['light'][1]:+.4f}, {r['light'][2]:+.4f})")
            for k in range(4):
                out.append('    | ' + ' '.join(_f(v, 11) for v in m[k * 4:k * 4 + 4]) + ' |')
            if r['centre']:
                cen = r['centre']
                s = f"    centre ({cen[0]:.1f}, {cen[1]:.1f}, {cen[2]:.1f})"
                if 'head_ahead' in r:
                    s += f"; from head: {r['head_horizontal']:.1f} m across, {r['head_ahead']:+.1f} m ahead, " \
                         f"{r['head_offset'][1]:+.1f} m up"
                if 'flat_ahead' in r:
                    s += f"; from flat camera {r['flat_horizontal']:.1f} m across, {r['flat_ahead']:+.1f} m ahead"
                out.append(s)
            out.append(f'    upload chain: {chain}')
            sizes = [x for rr in groups[g] for x in rr['size']]
            win = c.get('window') or []
            mi = ((c.get('matrix_offset') or 0) - (c.get('window_offset') or 0)) // 4
            for sp in split_scan(win, mi, sizes):
                vals = ', '.join(f'{v:.4g}' for v in sp['values'])
                tag = f" (matches box sizes: {', '.join(f'{v:.4g}' for v in sp['matches_box'])})" \
                    if sp['matches_box'] else ''
                out.append(f"    ascending floats at window+{sp['index'] * 4} "
                           f"(copy+{(c.get('window_offset') or 0) + sp['index'] * 4}): {vals}{tag}")
        # Boxes sharing one sun direction (cascades of one light share it).
        suns = []
        for r in groups[g]:
            for s in suns:
                if abs(_dot(s[0], r['light'])) >= .9995:
                    s[1].append(r)
                    break
            else:
                suns.append([r['light'], [r]])
        for s in suns:
            if len(s[1]) > 1:
                out.append(f"  shared sun direction ({s[0][0]:+.3f}, {s[0][1]:+.3f}, {s[0][2]:+.3f}): "
                           + ', '.join(size_class(r['size']) for r in s[1]))
        out.append('')
    # Head pitch: per box size, how its centre moves.
    out.append('--- PITCH (per box size: head pitch vs the box centre ahead of the head) ---')
    by = defaultdict(list)
    for r in rows:
        if r.get('head_pitch') is not None and 'head_ahead' in r:
            by[size_class(r['size'])].append(r)
    if not by:
        out.append('  (no head pose with these captures)')
    for k, rs in sorted(by.items(), key=lambda kv: -len(kv[1])):
        hp = [r['head_pitch'] for r in rs]
        ah = [r['head_ahead'] for r in rs]
        cr = _corr(hp, ah)
        fc = _corr([r['flat_pitch'] for r in rs if r.get('flat_pitch') is not None and 'flat_ahead' in r],
                   [r['flat_ahead'] for r in rs if r.get('flat_pitch') is not None and 'flat_ahead' in r])
        out.append(f"  {k}: {len(rs)} captures, head pitch {min(hp):.0f}..{max(hp):.0f} deg, ahead "
                   f"{min(ah):+.1f}..{max(ah):+.1f} m, correlation head {('%.2f' % cr) if cr is not None else 'n/a'}, "
                   f"flat {('%.2f' % fc) if fc is not None else 'n/a'}")
    out.append('')
    out.append('--- UPLOAD CHAINS (innermost first; the CPU writer is earlier in the frame) ---')
    for chain, n in Counter(tuple(r['capture'].get('chain') or []) for r in rows).most_common(8):
        out.append(f"  {n:4d}x " + (' <- '.join(f'0x{x:x}' for x in chain) or '?'))
    return '\n'.join(out) + '\n'


def summary_lines(lines):
    """Short block for address-finder-latest.txt."""
    caps = captures(lines)
    if not caps:
        return []
    groups = len({c.get('group') for c in caps})
    sizes = Counter(size_class(analyse(c)['size']) for c in caps)
    out = [f'--- ORTHO CAPTURE (Update 32; candidates only): {len(caps)} matrices in {groups} groups; '
           f'full dump in {LATEST_NAME} ---']
    out += [f'  {k}: {n}' for k, n in sizes.most_common(8)]
    out.append('')
    return out


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    payload = json.loads(Path(argv[1]).read_text(encoding='utf-8'))
    text = dump_text(payload.get('lines') or [])
    if len(argv) > 2:
        Path(argv[2]).write_text(text, encoding='utf-8')
    else:
        sys.stdout.write(text)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
