"""Version 0.0.72 (test build 3): SIMULATED previews, not game capture.

  van_interior.png   the driver's view from van072::seatEye: the van body, the built cab and the steering wheel
                     drawn through the game's own reader + placed drawer (spidy_model_dump), the wheel turned
                     by van072::wheelAxes at 0 and at a quarter turn, the hands on the rim
  hammer_select.png  the THOR HAMMER setting (combat38 option text) with each pick's model-check render
  side_by_side/      the model-check side-by-sides (copied)

  python3 tools/models072/preview_072c.py <Models folder> <model-check folder> <out folder>
"""
import math, os, shutil, sys, tempfile
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import model_check as mc  # noqa: E402

SIM = 'SIMULATED (THE MOD\'S OWN READER + DRAWER, SOFTWARE RASTER), NOT GAME CAPTURE'
# van072.hpp (Version 0.0.72 MEASURED): the seat's eye, the wheel's hub, its axis tilted back 25 degrees
SEAT_EYE = np.array([-.45, 1.60, -.55])
WHEEL_HUB = np.array([-.45, 1.18, -1.08])
WHEEL_RADIUS = .19


def norm(v):
    return v / np.linalg.norm(v)


def wheel_axes(angle):  # van072::wheelAxes
    a = norm(np.array([0, .42, .91]))
    up0 = norm(np.array([0, 1, 0]) - a * a[1])
    up = up0 * math.cos(angle) + np.array([1, 0, 0]) * math.sin(angle)
    return up, a


def place(v, at, forward, up):  # hero007::appendPlacedModel
    f = norm(forward)
    u = norm(up - f * np.dot(up, f))
    r = np.cross(f, u)
    p = v[:, 0:3].astype(np.float64)
    out = v.copy().astype(np.float64)
    out[:, 0:3] = at + p[:, :1] * r + p[:, 1:2] * u - p[:, 2:3] * f
    return out


def hand(at, toward):  # a dark glove box on the rim (made up, like the 0.0.72b previews)
    s = .035
    c = np.array([[x, y, z] for x in (-s, s) for y in (-s, s) for z in (-s, s)]) + at
    faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    tris = []
    for a, b, cc, d in faces:
        tris += [c[a], c[b], c[cc], c[a], c[cc], c[d]]
    v = np.zeros((len(tris), 11))
    v[:, 0:3] = tris
    v[:, 3:6] = mc.s2l(np.array([.08, .08, .09]))
    return v


def perspective(v, eye, target, fov=100):
    back = norm(eye - target)
    right = norm(np.cross([0, 1, 0], back))
    up = np.cross(back, right)
    d = v[:, 0:3] - eye
    x, y, z = d @ right, d @ up, d @ back
    tris_z = z.reshape(-1, 3)
    keep = (tris_z < -.05).all(1)
    k = 1 / math.tan(math.radians(fov) / 2)
    with np.errstate(divide='ignore', invalid='ignore'):
        P = np.stack([x / -z * k, y / -z * k, 1 / -z], 1)  # 1/distance: linear on screen, nearer is larger
    P = P.reshape(-1, 3, 3)[keep]
    return P, keep


def render_view(v, layers, eye, target, size=720):
    T, keep = perspective(v, eye, target)
    C = v[:, 3:6].reshape(-1, 3, 3)[keep]
    S = v[:, 7:11].reshape(-1, 3, 4)[keep]

    def shade(t, l0, l1, l2):
        lin = l0[:, None] * C[t, 0] + l1[:, None] * C[t, 1] + l2[:, None] * C[t, 2]
        layer = int(round(S[t, 0, 2]))
        if S[t, 0, 3] == 1 and 0 <= layer < len(layers):
            uv = l0[:, None] * S[t, 0, :2] + l1[:, None] * S[t, 1, :2] + l2[:, None] * S[t, 2, :2]
            tx = mc.sample(layers[layer], uv)
            return np.c_[lin * mc.s2l(tx[:, :3]), tx[:, 3]]
        return lin

    img, _ = mc.raster(T, shade, size=size, bg=(.30, .36, .45))
    return img


def van_interior(models, out, work):
    d, summary = mc.run_dump(models, work)
    names = list(summary['slots'].keys())
    get = {}
    layers = None
    for i, slot in enumerate(names, start=1):
        if slot.startswith('Vehicle/PunisherVan'):
            v, layers = mc.load_dump(d, i, summary)
            get[slot] = v
    need = ['Vehicle/PunisherVan', 'Vehicle/PunisherVan/Interior', 'Vehicle/PunisherVan/SteeringWheel']
    missing = [n for n in need if n not in get or not len(get[n])]
    cells, lines = [], []
    for title, angle in (('WHEEL STRAIGHT', 0.0), ('WHEEL A QUARTER TURN RIGHT', math.pi / 2)):
        if missing:
            img = np.full((720, 720, 3), .1)
        else:
            up, toward = wheel_axes(angle)
            wheel = place(get[need[2]], WHEEL_HUB, -toward, up)
            r = np.cross(-toward, up)
            grips = [WHEEL_HUB + WHEEL_RADIUS * (math.cos(a) * r + math.sin(a) * up) for a in (math.radians(160), math.radians(20))]
            allv = np.concatenate([get[need[0]], get[need[1]], wheel] + [hand(g, toward) for g in grips])
            img = render_view(allv, layers, SEAT_EYE, WHEEL_HUB + np.array([0, .05, -1.2]))
        cells.append((title, img))
    W = 720 * 2 + 30
    canvas = Image.new('RGB', (W, 720 + 110), (16, 20, 24))
    dr = ImageDraw.Draw(canvas)
    dr.text((10, 6), 'PUNISHER VAN: THE DRIVER\'S VIEW (van072::seatEye) - body (snafuj, black) + BUILT cab + BUILT wheel', fill=(245, 197, 24))
    for k, (title, img) in enumerate(cells):
        canvas.paste(Image.fromarray((img * 255).astype(np.uint8)), (10 + k * 730, 26))
        dr.text((14 + k * 730, 30), title, fill=(255, 255, 255))
    lines = ['MODEL MISSING: ' + ', '.join(missing)] if missing else [
        'Both hands on the rim (made-up gloves); the wheel turns about its own tilted axis (van072::wheelAxes).',
        'RT accelerates, LT reverses, both brake; quick tap of left X summons. EXPERIMENTAL, off by default.',
        'Driving: engine preview only - the body carry is NOT WIRED (CANDIDATE).']
    for i, s in enumerate(lines + [SIM]):
        dr.text((10, 752 + 16 * i), s, fill=(255, 190, 80) if s == SIM else (220, 220, 220))
    canvas.save(os.path.join(out, 'van_interior.png'))
    return not missing


def hammer_select(check, out):
    picks = [('BOOOOP', 'Boooooop', 'Hammer_BOOOOP.png', '#1 DEFAULT'), ('DEVILSEYE', 'TheDevilsEye', 'Hammer_DEVILSEYE.png', '#5'),
             ('LOVE THUNDER', 'Jesus Osco', 'Hammer_LOVEANDTHUNDER.png', '#6'), ('FCCOOPER', 'fccooper', 'Hammer_FCCOOPER.png', '#3')]
    T = 300
    canvas = Image.new('RGB', (40 + 4 * (T + 20), 520), (14, 16, 22))
    dr = ImageDraw.Draw(canvas)
    dr.rectangle([20, 20, canvas.width - 20, 120], fill=(28, 32, 42), outline=(245, 197, 24))
    dr.text((36, 34), 'COMBAT  >  THOR HAMMER', fill=(160, 170, 190))
    dr.text((36, 58), '<   BOOOOP   >', fill=(245, 197, 24))
    dr.text((36, 86), 'Picks which Mjolnir model Thor draws from his back, no restart.', fill=(220, 220, 220))
    for k, (val, artist, png, tag) in enumerate(picks):
        x = 20 + k * (T + 20)
        p = os.path.join(check, png)
        if os.path.exists(p):
            im = Image.open(p)
            im = im.crop((10, 30, 10 + mc.SIZE, 30 + mc.SIZE)).resize((T, T))  # OURS panel
            canvas.paste(im, (x, 140))
        else:
            dr.text((x + 10, 280), 'MODEL MISSING', fill=(255, 90, 90))
        dr.rectangle([x, 140, x + T, 140 + T], outline=(245, 197, 24) if k == 0 else (70, 76, 90), width=3 if k == 0 else 1)
        dr.text((x + 6, 446), '%s  %s' % (tag, val), fill=(255, 255, 255))
        dr.text((x + 6, 464), 'by %s, CC BY 4.0' % artist, fill=(170, 180, 200))
    dr.text((20, 494), 'Starts sheathed on his back; reach over the shoulder and grip to draw. Throw, recall, lightning unchanged.  ' + SIM,
            fill=(255, 190, 80))
    canvas.save(os.path.join(out, 'hammer_select.png'))


def main():
    models, check, out = sys.argv[1:4]
    os.makedirs(os.path.join(out, 'side_by_side'), exist_ok=True)
    for f in sorted(os.listdir(check)):
        if f.endswith('.png') and (f.startswith('Hammer_') or f.startswith('Vehicle_')):
            shutil.copy(os.path.join(check, f), os.path.join(out, 'side_by_side', f))
    hammer_select(check, out)
    work = tempfile.mkdtemp(prefix='sm2072_prev_')
    try:
        ok = van_interior(models, out, work)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    with open(os.path.join(out, 'README.txt'), 'w') as f:
        f.write('Version 0.0.72 test build 3 previews. ' + SIM + '.\n')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
