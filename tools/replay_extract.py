#!/usr/bin/env python3
"""Update 38.2: compact replay fixtures from a session's game-vr-*.json (its `samples`).

    python3 tools/replay_extract.py RUN_JSON NAME   ->  tests/replay/NAME.csv

One row per recorded sample (~19 Hz, the last 12000 of the session):
  t                          session seconds
  hx hy hz hqx hqy hqz hqw   the head, game world (metres, y up), as a Pose
  lx .. lqw / rx .. rqw      each hand: the GRIP position with the AIM orientation (the
                             sample's `hands`, game_tracking.cpp out.hands); untracked: nan
  pad                        the pad buttons sent to the game that frame (XInput bits)
  presses toggles dist       wrist_hud: the pointing hand's pulls, hologram toggles, its
                             distance from the hologram forearm (-1: none)
  play                       1 when the controllers play VR in gameplay (wrist_hud state)
  el er                      body hand_error_m (drawn IK wrist to the controller wrist, -1 none)
  hp                         health_hp (-1 none)
  sub uniq                   presented frames and the game's unique images so far (the
                             image's age between them: RING LAYER NEWEST's stale hide)
The sample matrices are relativePose's rows (x, -y, -z of the pose), position last.
"""
import json, math, sys, os


def quat(m):  # columns X, Y, Z of a rotation matrix -> x y z w
    (a, b, c), (d, e, f), (g, h, i) = m  # rows of the matrix whose columns are X,Y,Z
    tr = a + e + i
    if tr > 0:
        s = math.sqrt(tr + 1) * 2
        w, x, y, z = .25 * s, (h - f) / s, (c - g) / s, (d - b) / s
    elif a > e and a > i:
        s = math.sqrt(1 + a - e - i) * 2
        w, x, y, z = (h - f) / s, .25 * s, (b + d) / s, (c + g) / s
    elif e > i:
        s = math.sqrt(1 + e - a - i) * 2
        w, x, y, z = (c - g) / s, (b + d) / s, .25 * s, (f + h) / s
    else:
        s = math.sqrt(1 + i - a - e) * 2
        w, x, y, z = (d - b) / s, (c + g) / s, (f + h) / s, .25 * s
    n = math.sqrt(w * w + x * x + y * y + z * z) or 1
    return [x / n, y / n, z / n, w / n]


def pose(m):
    if not m or len(m) != 16 or any(v is None or not math.isfinite(v) for v in m):
        return [float('nan')] * 7
    X, Y, Z = m[0:3], [-v for v in m[4:7]], [-v for v in m[8:11]]
    rows = [[X[0], Y[0], Z[0]], [X[1], Y[1], Z[1]], [X[2], Y[2], Z[2]]]
    return [m[12], m[13], m[14]] + quat(rows)


def rows(samples):
    for s in samples:
        hands = s.get('hands') or []
        wh = s.get('wrist_hud') or {}
        body = s.get('body') or {}
        he = body.get('hand_error_m') or [-1, -1]
        state = wh.get('state') or []
        play = int('controllers_play_vr' in state and 'gameplay' in state)
        hp = s.get('health_hp')
        hp = hp if isinstance(hp, (int, float)) else -1
        pad = s.get('pad_buttons') or '0x0'
        pad = int(pad, 16) if isinstance(pad, str) else int(pad)
        L = pose(hands[0]) if len(hands) > 0 else [float('nan')] * 7
        R = pose(hands[1]) if len(hands) > 1 else [float('nan')] * 7
        yield [s.get('seconds', 0)] + pose(s.get('head')) + L + R + [
            pad, wh.get('presses', 0), wh.get('toggles', 0), wh.get('distance_m', -1) if wh.get('distance_m') is not None else -1,
            play, he[0] if he[0] is not None else -1, he[1] if he[1] is not None else -1, hp,
            int(s.get('submitted') or 0), int(s.get('unique_submitted') or 0)]


def main():
    src, name = sys.argv[1], sys.argv[2]
    data = json.load(open(src))
    samples = data['samples'] if 'samples' in data else data
    out = os.path.join(os.path.dirname(__file__), '..', 'tests', 'replay', name + '.csv')
    n = 0
    with open(out, 'w') as f:
        f.write('# ' + os.path.basename(src) + ' (tools/replay_extract.py)\n')
        for r in rows(samples):
            f.write(','.join('nan' if isinstance(v, float) and not math.isfinite(v) else
                             (str(v) if isinstance(v, int) else '%.5g' % v if abs(v) < 1 else '%.4f' % v) for v in r) + '\n')
            n += 1
    print(out, n, 'rows')


if __name__ == '__main__':
    main()
