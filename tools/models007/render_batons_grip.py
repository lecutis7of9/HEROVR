"""Version 0.0.7 DAREDEVIL BATON GRIP previews, no headset: the held baton BEFORE (0.0.62 to
0.0.7 af399a9: along the hand's aim, AIM AXIS HAND = the grip's -Y, 0.12 m behind the grip point and
0.45 m ahead) and AFTER (baton_grip.hpp: from the controller's GRIP pose only, the shaft through the
closed fist, 20 degrees off the forearm toward the thumb, the hand a third of the way from the butt),
for the built-in baton (the CLUB / 0.0.62 colours) and the Yanez Designs model (Models/Baton), on a
stand-in hand round a Touch-style handle: a first-person view of each hand and a side view.

The grip frame is OpenXR's (/input/grip/pose): +X normal to the palm (right on both hands), -Z
through the curled fingers' tube toward the thumb, +Y by the right-hand rule (toward the wrist). A
hand round a Touch handle holds the grip's +Y 30 degrees off the forearm toward +Z (tracking.hpp,
Update 20); baton_grip::handleTiltDeg per controller.

  render_batons_grip.py <Models folder> <out folder>"""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from render_models import read_glb
from render_cap import nz, prism, ball, render, sheet

LENGTH = .57          # baton_grip::length
SHARE = 1. / 3.       # baton_grip::gripShare: the hand a third of the way from the butt
OFF_FOREARM = 20.     # baton_grip::offForearmDeg (15..25)
TILT = {'touch': 30., 'index': 15.}  # baton_grip::handleTiltDeg (Touch CONFIRMED in the project, Index CANDIDATE)
SKIN = np.array([.55, .36, .27])
SUIT = np.array([.20, .26, .40])  # the stand-in sleeve (render_cap's navy), so the red baton stands out
DARK = np.array([.05, .05, .06])
RED = np.array([.42, .03, .04])
STEEL = np.array([.5, .5, .55])


def local_dir(deg):
    """The grip's -Y turned deg toward its -Z (the thumb's side), grip-local."""
    a = np.radians(deg)
    return np.array([0., -np.cos(a), -np.sin(a)])


def grip_pose(hand, at, forearm_w, up_w, tilt):
    """R (grip local -> world) for a hand whose forearm points forearm_w with the thumb's side up_w."""
    f_l, u_l = local_dir(tilt), local_dir(tilt + 90.)
    fw = nz(forearm_w)
    uw = nz(np.asarray(up_w, float) - fw * np.dot(up_w, fw))
    xw = np.cross(fw, uw)  # +X to the right on both hands (OpenXR); X = F x U locally too, so one map for both
    L = np.stack([np.cross(f_l, u_l), f_l, u_l], 1)
    W = np.stack([xw, fw, uw], 1)
    R = W @ L.T
    assert np.allclose(R @ R.T, np.eye(3), atol=1e-9) and np.linalg.det(R) > 0
    return np.asarray(at, float), R


def before(at, R):
    a = R @ np.array([0., -1, 0])                   # AIM AXIS HAND: webShooter's -Z = the grip's -Y
    return at - a * .12, at + a * .45


def after(at, R, tilt):
    d = R @ local_dir(tilt + OFF_FOREARM)
    return at - d * LENGTH * SHARE, at + d * LENGTH * (1 - SHARE)


def hand_parts(at, R, tilt):
    """A stand-in fist round a Touch-style handle, the forearm, a sleeve."""
    P = lambda v: at + R @ np.asarray(v, float)
    fw = R @ local_dir(tilt)
    wrist = at - fw * .075 + R @ np.array([0, 0, .0])
    elbow = wrist - fw * .27
    parts = [prism(P([0, 0, .055]), P([0, 0, -.05]), .019, .017, 10, DARK),          # the handle (the fist's tube, -Z)
             ball(P([0, -.035, -.06]), .03, DARK * 1.6, 10),                          # the tracking ring's end, over the thumb
             prism(P([.0, .0, .048]), P([.0, .0, -.04]), .041, .043, 10, SKIN),      # the curled fingers round it
             prism(wrist, at - fw * .01, .03, .036, 10, SKIN),                         # the back of the hand / palm
             ball(P([0, -.01, -.052]), .016, SKIN * .9, 8),                            # the thumb on top
             prism(elbow, wrist, .043, .032, 10, SUIT)]
    return parts


def club(butt, tip):
    d = tip - butt
    n = nz(d)
    return [prism(butt - n * .012, butt + n * .008, .02, .02, 12, STEEL),
            prism(butt, butt + d * .32, .017, .017, 12, DARK),
            prism(butt + d * .32, butt + d * .66, .016, .016, 12, RED),
            prism(butt + d * .66, tip - n * .012, .016, .0185, 12, RED),
            prism(tip - n * .015, tip, .019, .019, 12, STEEL)]


def model(tris, cols, butt, tip):
    """hero007_visual appendBatonModel: forward butt->tip, up the world's up off it, the model's +z
    end (its butt) at `butt`; model x right, y up, -z forward."""
    f = nz(tip - butt)
    u = np.array([0, 1., 0]) - f * f[1]
    u = nz(u) if np.linalg.norm(u) > 1e-3 else np.array([0, 0, 1.])
    r = np.cross(f, u)
    # 0.0.73 BATON FLIP: the model's -z end at the butt, +z toward the tip (turned half about up).
    lo = max(0., -tris[..., 2].min())
    at = butt + f * lo
    t = at - tris[..., :1] * r + tris[..., 1:2] * u + tris[..., 2:3] * f
    return [(t, cols)]


def angle(a, b):
    return np.degrees(np.arccos(np.clip(np.dot(nz(a), nz(b)), -1, 1)))


def scene(look, which, mesh, tilt=TILT['touch']):
    eye = np.array([0, 1.62, 0.])
    hands = []
    for h in (0, 1):
        s = 1 if h == 1 else -1
        at = np.array([.19 * s, 1.27, -.38])
        fore = np.array([-.10 * s, .30, -1.])        # forearm forward, raised a little, a touch inward
        up = np.array([.15 * s, 1., .25])            # the thumb's side up, the palm inward
        hands.append(grip_pose(h, at, fore, up, tilt))
    parts, info = [], []
    for h, (at, R) in enumerate(hands):
        butt, tip = (before(at, R) if which == 'before' else after(at, R, tilt))
        parts += hand_parts(at, R, tilt)
        parts += club(butt, tip) if look == 'built-in' else model(*mesh[h], butt, tip)
        fw = R @ local_dir(tilt)
        share = np.dot(at - butt, nz(tip - butt)) / np.linalg.norm(tip - butt)
        side = np.dot(nz(tip - butt), R @ np.array([0, 0, -1.]))
        info.append((h, angle(tip - butt, fw), share, butt, tip, side))
    return eye, hands, parts, info


def views(look, which, mesh, out):
    eye, hands, parts, info = scene(look, which, mesh)
    label = 'BEFORE (af399a9: along the aim, .12 behind / .45 ahead)' if which == 'before' else \
            'AFTER (grip pose: through the fist, %g deg off the forearm, hand at 1/3)' % OFF_FOREARM
    ims = []
    for h, name in ((0, 'LEFT hand'), (1, 'RIGHT hand')):
        _, ang, share, butt, tip, thumb = info[h]
        at = hands[h][0]
        im, d, proj = render(parts, eye + np.array([.03 if h else -.03, 0, 0]), at + np.array([0, .02, -.08]),
                             'DAREDEVIL baton %s - first person, %s' % (look, name), label, fov=62,
                             notes=('shaft %.0f deg off the forearm, hand at %.2f of the length from the butt' % (ang, share),
                                    'tip toward the thumb: %s' % ('yes' if thumb > .2 else 'no (out past the knuckles)')))
        ims.append(im)
    at = hands[1][0]
    side_eye = at + np.array([.9, .05, -.05])
    im, d, proj = render(parts, side_eye, at + np.array([0, 0, -.05]), 'DAREDEVIL baton %s - side view (right hand)' % look, label, fov=48,
                         notes=('right: shaft %.0f deg off the forearm, hand at %.2f from the butt' % (info[1][1], info[1][2]),
                                'left/right mirror: %s' % ('yes' if np.allclose(info[0][3] * [-1, 1, 1], info[1][3], atol=1e-6) else 'no')))
    ims.append(im)
    for k, im in enumerate(ims):
        im.save(os.path.join(out, '%s-%s-%s.png' % (look.replace(' ', '-'), which, ('first-person-left', 'first-person-right', 'side')[k])))
    return ims, info


def main(models, out):
    os.makedirs(out, exist_ok=True)
    mesh = []
    for s in ('Left', 'Right'):
        t, c, _ = read_glb(os.path.join(models, 'Baton', s, 'scene.glb'))
        mesh.append((np.asarray(t, float), np.asarray(c, float)))
    report = []
    for look in ('built-in', 'yanez-model'):
        rows = []
        for which in ('before', 'after'):
            ims, info = views(look, which, mesh, out)
            rows += ims
            for h, ang, share, butt, tip, thumb in info:
                report.append('%s %s %s: %.1f deg off the forearm, hand at %.3f, toward the thumb %.2f' %
                              (look, which, 'LR'[h], ang, share, thumb))
        sheet(rows, cols=3).save(os.path.join(out, '%s-before-after.png' % look))
    open(os.path.join(out, 'measurements.txt'), 'w').write('\n'.join(report) + '\n')
    print('\n'.join(report))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
