"""0.0.74 test 10 WINDOW PROBE (include/spidy/window_probe.hpp; read-only, logging only).

Reads SpidyWindowProbeData from the VR module and appends every new sample to
reports/window-probe-<stamp>.jsonl: per view (game / left / right) the time, the impostor query counts, the
view's lens (CONFIRMED offsets) and what the frustum maker was given (+2e0 / +280 matrices, CONFIRMED as its
inputs) and made (the cull object's first 128 bytes, CANDIDATE layout), with angles worked out here:
  lens_deg       the view's own lens edges (atan of +400 left, +404 right, +408 top, +40c bottom tangents)
  matrix_deg     the edges a projection matrix among +2e0 / +280 gives (CANDIDATE: whichever looks like one)
  plane_deg      half-angles of the cull object's planes read as 4-wide x/y/z/d groups against the view's
                 ahead axis (CANDIDATE layout)
  edge_gap_deg   matrix edges minus lens edges (negative: the cull is narrower than the eye sees -> things at
                 the edge of the eye can pop)
Impostor and full-model INSTANCE counts are not available (no confirmed offset); the calls / non-null result
counts are. Never raises: a missing export, a bad record or a write error skips quietly.
"""
import json
import math
import struct

EXPORT = 'SpidyWindowProbeData'
MAGIC, VERSION = 0x42525057, 1
VIEWS = ('game', 'left', 'right')
RING = 96
SAMPLE_BYTES = 376
HEADER_BYTES = 88
SIZE = HEADER_BYTES + RING * SAMPLE_BYTES + 8  # 36192, window_probe.hpp static_assert
SAMPLE = struct.Struct('<dQ2I4f2ffI16f16f16f32f')
assert SAMPLE.size == SAMPLE_BYTES


def parse(raw):
    """The record as a dict, or None (wrong size or magic, or written to while read)."""
    if not raw or len(raw) != SIZE:
        return None
    magic, version, size, count = struct.unpack_from('<4I', raw, 0)
    if (magic, version, size) != (MAGIC, VERSION, SIZE):
        return None
    if struct.unpack_from('<Q', raw, SIZE - 8)[0] & 1:
        return None
    calls = struct.unpack_from('<3Q', raw, 16)
    results = struct.unpack_from('<3Q', raw, 40)
    other, faults = struct.unpack_from('<2Q', raw, 64)
    started, = struct.unpack_from('<d', raw, 80)
    samples = []
    for i in range(max(0, count - RING), count):
        v = SAMPLE.unpack_from(raw, HEADER_BYTES + (i % RING) * SAMPLE_BYTES)
        samples.append(dict(index=i, seconds=v[0], frame=v[1], view=v[2], flags=v[3], lens=list(v[4:8]),
                            near_far=list(v[8:10]), reach=v[10], view_flags=v[11], pose=list(v[12:28]),
                            matrix_a=list(v[28:44]), matrix_b=list(v[44:60]), cull=list(v[60:92])))
    return dict(count=count, calls=dict(zip(VIEWS, calls)), results=dict(zip(VIEWS, results)), other=other,
                faults=faults, started=started, samples=samples)


def read(game, xr):
    """A steady snapshot from the game, or None (export absent, never steady, or a read error)."""
    try:
        address = (xr or {}).get(EXPORT)
        if not address:
            return None
        for _ in range(8):
            raw = game.read(address, SIZE)
            data = parse(raw)
            if data is not None and raw[SIZE - 8:] == game.read(address + SIZE - 8, 8):
                return data
    except Exception:  # noqa: BLE001 - telemetry must never stop the session
        return None
    return None


def _finite(values):
    return all(isinstance(x, (int, float)) and math.isfinite(x) for x in values)


def _deg(t):
    return round(math.degrees(math.atan(t)), 3)


def lens_angles(lens):
    """Signed edge angles in degrees (left, right, top, bottom) from the lens tangents."""
    if not _finite(lens):
        return None
    return dict(left=_deg(lens[0]), right=_deg(lens[1]), top=_deg(lens[2]), bottom=_deg(lens[3]),
                width=round(_deg(lens[1]) - _deg(lens[0]), 3), height=round(_deg(lens[3]) - _deg(lens[2]), 3))


def projection_tangents(m):
    """CANDIDATE: a 4x4 (16 floats, row-major as stored) read as a perspective projection in either vector
    convention; the edge tangents x/ahead at clip x = -w, +w and y/ahead at y = -w, +w, or None when it is
    not one (no row or column of (0, 0, +-1, 0))."""
    if not _finite(m):
        return None
    rows = [m[0:4], m[4:8], m[8:12], m[12:16]]
    cols = [[rows[r][c] for r in range(4)] for c in range(4)]

    def w_like(v):
        return abs(v[0]) < 1e-4 and abs(v[1]) < 1e-4 and abs(abs(v[2]) - 1) < 1e-3 and abs(v[3]) < 1e-4

    for layout, x, y, w in (('columns', cols[0], cols[1], cols[3]), ('rows', rows[0], rows[1], rows[3])):
        if not w_like(w) or abs(x[0]) < 1e-6 or abs(y[1]) < 1e-6:
            continue
        s = 1.0 if w[2] > 0 else -1.0
        return dict(layout=layout, x=((-1 - x[2] * s) / x[0], (1 - x[2] * s) / x[0]),
                    y=((-1 - y[2] * s) / y[1], (1 - y[2] * s) / y[1]))
    return None


def plane_angles(cull, pose):
    """CANDIDATE: the cull object's floats as two 4-wide groups of x, y, z, d planes; for each plane the
    half-angle between it and the view's ahead axis (pose row 2), asin(n . ahead) with n normalised."""
    if not _finite(cull) or not _finite(pose):
        return None
    ahead = pose[8:11]
    out = []
    for group in (0, 16):
        for i in range(4):
            n = (cull[group + i], cull[group + 4 + i], cull[group + 8 + i])
            length = math.sqrt(sum(a * a for a in n))
            if not 1e-6 < length < 1e6:
                out.append(None)
                continue
            d = sum(a * b for a, b in zip(n, ahead)) / length
            out.append(round(math.degrees(math.asin(max(-1.0, min(1.0, d)))), 3))
    return out


def analyse(sample):
    """One jsonl line's worth for a sample."""
    flags = sample['flags']
    line = dict(seconds=round(sample['seconds'], 4), frame=sample['frame'],
                view=VIEWS[sample['view']] if sample['view'] < len(VIEWS) else sample['view'],
                result=bool(flags & 1), game_active=bool(flags & 2), lens_path=bool(flags & 4),
                read_ok=bool(flags & 8), view_flags=sample['view_flags'], near_far=sample['near_far'],
                reach=round(sample['reach'], 3) if math.isfinite(sample['reach']) else None)
    if not flags & 8:
        return line
    lens = lens_angles(sample['lens'])
    line['lens'] = sample['lens']
    line['lens_deg'] = lens
    for name in ('matrix_a', 'matrix_b'):
        t = projection_tangents(sample[name])
        if t:
            edges = dict(left=_deg(t['x'][0]), right=_deg(t['x'][1]), top=_deg(t['y'][0]),
                         bottom=_deg(t['y'][1]))
            line['matrix_deg'] = dict(source=('+2e0' if name == 'matrix_a' else '+280'), layout=t['layout'],
                                      **edges)
            if lens:
                line['edge_gap_deg'] = dict(left=round(lens['left'] - edges['left'], 3),
                                            right=round(edges['right'] - lens['right'], 3))
            break
    line['plane_deg'] = plane_angles(sample['cull'], sample['pose'])
    line['matrix_a'], line['matrix_b'], line['cull'] = sample['matrix_a'], sample['matrix_b'], sample['cull']
    line['pose'] = sample['pose']
    return line


class Journal:
    """Appends each new sample once to the jsonl (the ring holds 96, about 3 s; polled every second)."""

    def __init__(self, path):
        self.path, self.next, self.written = path, 0, 0

    def add(self, data):
        try:
            if not data:
                return 0
            new = [s for s in data['samples'] if s['index'] >= self.next]
            if not new:
                return 0
            missed = new[0]['index'] - self.next if self.next else 0
            self.next = new[-1]['index'] + 1
            self.path.parent.mkdir(parents=True, exist_ok=True)
            with self.path.open('a', encoding='utf-8') as f:
                if missed > 0:
                    f.write(json.dumps(dict(missed_samples=missed)) + '\n')
                for s in new:
                    f.write(json.dumps(analyse(s)) + '\n')
                f.write(json.dumps(dict(seconds=round(new[-1]['seconds'], 4), counts=dict(calls=data['calls'], results=data['results'],
                                                    other_views=data['other'], faults=data['faults'],
                                                    instance_counts='not available (no confirmed offset)'))) +
                        '\n')
            self.written += len(new)
            return len(new)
        except Exception:  # noqa: BLE001 - never raises
            return 0
