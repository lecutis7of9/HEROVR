"""Version 0.0.7 PUNISHER GUN GRIP previews, no headset: a held pistol and the sawed-off
BEFORE (along the hand's aim, AIM AXIS HAND = the grip's -Y, kept upright to the world, the look's
origin - the front of its grip - on the grip point) and AFTER (gun_grip.hpp: from the controller's
GRIP pose, the barrel along the forearm, the top toward the thumb, the look's grip middle on the
palm), for the BUILT-IN looks and the converted models (Models/Pistol, TORI106; Models/SawedOff,
LiliumLetifer), on render_batons_grip's stand-in hand: a first-person view of each hand and a side
view. The sawed-off also broken open (AFTER) to show the hinge still turns about its pivot.

  render_guns_grip.py <Models folder> <out folder>"""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from render_models import read_glb
from render_cap import nz, prism, ball, render, sheet
from render_batons_grip import grip_pose, local_dir, angle, TILT, SKIN, SUIT

POLY = np.array([.07, .07, .08])
STEEL = np.array([.30, .30, .33])
BLUED = np.array([.12, .13, .17])
WOOD = np.array([.36, .20, .09])
# gun_grip::palmPoint (forward, up): BUILT-IN and MODEL
PALM = {('pistol', False): (-.028, -.012), ('pistol', True): (-.026, -.034),
        ('sawed', False): (-.018, -.015), ('sawed', True): (-.038, -.016)}
MUZZLE = {'pistol': (.190, .074), 'sawed': (.388, .072)}   # gun_grip::muzzleFromPalm


def hand_parts(at, R, tilt):
    """A stand-in hand round a gun's grip (no controller drawn): the curled fingers' tube along the
    grip's -Z, slimmer than render_batons_grip's (the gun's grip is inside it), the thumb, the back of
    the hand, the forearm and a sleeve."""
    P = lambda v: at + R @ np.asarray(v, float)
    fw = R @ local_dir(tilt)
    wrist = at - fw * .07
    elbow = wrist - fw * .27
    return [prism(P([.004, -.012, .045]), P([.004, -.012, -.03]), .022, .024, 10, SKIN),   # the curled fingers, ahead of the grip
            prism(wrist, at - fw * .005 + R @ np.array([0, 0, -.01]), .028, .034, 10, SKIN),  # the palm / back of the hand
            ball(P([-.006, -.03, -.045]), .012, SKIN * .9, 8),                                  # the thumb along the frame
            prism(elbow, wrist, .043, .032, 10, SUIT)]


def frame_before(at, R):
    f = R @ np.array([0., -1, 0])                  # AIM AXIS HAND
    u = np.array([0, 1., 0]) - f * f[1]            # hero007_play gunUp: the world's up off the aim
    return at, f, nz(u)


def frame_after(at, R, tilt, gun, model):
    f, u = R @ local_dir(tilt), R @ local_dir(tilt + 90.)
    pf, pu = PALM[(gun, model)]
    return at - f * pf - u * pu, f, u


def builtin(gun, o, f, u, open_=False):
    r = np.cross(f, u)
    P = lambda fw, up, rt=0: o + f * fw + u * up + r * rt
    if gun == 'pistol':   # hero007_visual appendPistol
        return [prism(P(-.015, .035), P(-.042, -.065), .014, .014, 10, POLY),
                prism(P(-.03, .04), P(.12, .04), .011, .011, 10, POLY),
                prism(P(0, .03), P(.02, .006), .003, .003, 6, POLY), prism(P(.02, .006), P(.062, .03), .003, .003, 6, POLY),
                prism(P(.022, .03), P(.019, .014), .0028, .0028, 6, POLY * .5),
                prism(P(-.035, .062), P(.16, .062), .013, .013, 10, STEEL),
                ball(P(.15, .076), .0025, POLY * .2, 6), ball(P(-.028, .076), .0025, POLY * .2, 6)]
    parts = [prism(P(0, .04), P(-.035, -.07), .016, .016, 10, WOOD),   # appendSawedOff
             prism(P(.005, .03), P(.03, .008), .003, .003, 6, STEEL), prism(P(.03, .008), P(.065, .032), .003, .003, 6, STEEL),
             prism(P(-.04, .05), P(.07, .05), .02, .02, 10, STEEL)]
    hinge = P(.07, .045)
    a = np.radians(35. if open_ else 0.)
    bd, bu = f * np.cos(a) - u * np.sin(a), u * np.cos(a) + f * np.sin(a)
    parts.append(prism(hinge - bu * .006 + bd * .02, hinge - bu * .006 + bd * .17, .016, .016, 10, WOOD))
    for side in (-.011, .011):
        b0 = hinge + bu * .012 + r * side
        parts.append(prism(b0, b0 + bd * .30, .0105, .0105, 10, BLUED))
    return parts


def modelled(gun, mesh, o, f, u, open_=False):
    r = np.cross(f, u)
    W = lambda t: o + t[..., :1] * r + t[..., 1:2] * u - t[..., 2:3] * f   # appendModel
    if gun == 'pistol':
        t, c, _ = mesh['pistol']
        return [(W(t), c)]
    (tf, cf, _), (tb, cb, ex) = mesh['frame'], mesh['barrels']
    pv = np.asarray(ex['pivot'], float)
    a = np.radians(35. if open_ else 0.)
    bd, bu = f * np.cos(a) - u * np.sin(a), u * np.cos(a) + f * np.sin(a)
    hinge = o + r * pv[0] + u * pv[1] - f * pv[2]                          # appendSawedOffModel
    d = tb - pv
    return [(W(tf), cf), (hinge + d[..., :1] * r + d[..., 1:2] * bu - d[..., 2:3] * bd, cb)]


def scene(gun, model, which, mesh, open_=False, tilt=TILT['touch']):
    eye = np.array([0, 1.62, 0.])
    parts, info = [], []
    for h in (0, 1):
        s = 1 if h == 1 else -1
        at = np.array([.15 * s, 1.44, -.48])
        fore = np.array([-.16 * s, .05, -1.])         # aiming: the forearm out ahead, a little up, inward
        up = np.array([.10 * s, 1., .05])             # the thumb's side up
        at, R = grip_pose(h, at, fore, up, tilt)
        o, f, u = frame_before(at, R) if which == 'before' else frame_after(at, R, tilt, gun, model)
        parts += hand_parts(at, R, tilt)
        parts += modelled(gun, mesh, o, f, u, open_) if model else builtin(gun, o, f, u, open_)
        fw = R @ local_dir(tilt)
        pf, pu = PALM[(gun, model)]
        palm_world = o + f * pf + u * pu
        info.append(dict(h=h, at=at, R=R, off=angle(f, fw), aim=angle(f, R @ local_dir(30.)),
                         palm=np.linalg.norm(palm_world - at), roll=angle(u, R @ local_dir(tilt + 90.)), o=o))
    return eye, parts, info


def views(gun, model, which, mesh, out, open_=False):
    eye, parts, info = scene(gun, model, which, mesh, open_)
    look = '%s-%s' % ({'pistol': 'pistol', 'sawed': 'sawed-off'}[gun], 'model' if model else 'built-in')
    tag = which + ('-open' if open_ else '')
    label = ('BEFORE (along AIM AXIS HAND, upright to the world, grip front on the palm)' if which == 'before' else
             'AFTER (grip pose: barrel along the forearm, top to the thumb, grip middle in the palm)') + (' - broken open' if open_ else '')
    ims = []
    for h, name in ((0, 'LEFT hand'), (1, 'RIGHT hand')):
        i = info[h]
        notes = ('barrel %.0f deg off the forearm, %.0f deg off the Touch aim ray' % (i['off'], i['aim']),
                 'the look\'s grip middle %.1f cm from the palm, roll off the hand %.0f deg' % (i['palm'] * 100, i['roll']))
        im, _, _ = render(parts, eye + np.array([.06 if h else -.06, -.04, -.12]), i['at'] + np.array([0, .02, -.14]),
                          'PUNISHER %s - first person, %s' % (look, name), label, fov=34, notes=notes)
        ims.append(im)
    at = info[1]['at']
    im, _, _ = render(parts, at + np.array([.75, .03, -.12]), at + np.array([0, .02, -.12]), 'PUNISHER %s - side view (right hand)' % look,
                      label, fov=32, notes=('right: barrel %.0f deg off the forearm, %.0f deg off the Touch aim ray' % (info[1]['off'], info[1]['aim']),
                                            'left/right mirror: %s' % ('yes' if np.allclose(info[0]['o'] * [-1, 1, 1], info[1]['o'], atol=1e-6) else 'no')))
    ims.append(im)
    for k, im in enumerate(ims):
        im.save(os.path.join(out, '%s-%s-%s.png' % (look, tag, ('first-person-left', 'first-person-right', 'side')[k])))
    return ims, info, look


def main(models, out):
    os.makedirs(out, exist_ok=True)
    g = lambda p: tuple(np.asarray(x, float) if k < 2 else x for k, x in enumerate(read_glb(os.path.join(models, p))))
    mesh = {'pistol': g('Pistol/scene.glb'), 'frame': g('SawedOff/Frame/scene.glb'), 'barrels': g('SawedOff/Barrels/scene.glb')}
    report = []
    for gun in ('pistol', 'sawed'):
        for model in (False, True):
            rows = []
            for which in ('before', 'after'):
                ims, info, look = views(gun, model, which, mesh, out)
                rows += ims
                for i in info:
                    report.append('%s %s %s: barrel %.1f deg off the forearm, %.1f deg off the Touch aim ray, grip middle %.1f cm from the palm, roll %.1f deg' %
                                  (look, which, 'LR'[i['h']], i['off'], i['aim'], i['palm'] * 100, i['roll']))
            if gun == 'sawed':
                ims, _, _ = views(gun, model, 'after', mesh, out, open_=True)
                rows += ims
            sheet(rows, cols=3).save(os.path.join(out, '%s-before-after.png' % look))
    open(os.path.join(out, 'measurements.txt'), 'w').write('\n'.join(report) + '\n')
    print('\n'.join(report))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
