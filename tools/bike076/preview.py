# HERO VR 0.0.76 HARLEYS converter previews (Made by Agent Mulder): system python (numpy, PIL), Blender has no PIL.
#   python3 tools/bike076/preview.py <job.npz>    renders the views the converter asked for into PNGs
import sys, math, json
import numpy as np

def render(path, S, N, C, view, px=900, marks=()):
    from PIL import Image, ImageDraw
    if view == 'side':   ax, ay, dirv = 1, 2, np.array([1., 0, 0])     # from the right: forward to the right
    elif view == 'top':  ax, ay, dirv = 0, 1, np.array([0, 0, 1.])
    elif view == 'front': ax, ay, dirv = 0, 2, np.array([0, 1., 0])
    u, v = S[:, ax], S[:, ay]
    if view == 'side': depth = -S[:, 0]
    elif view == 'top': depth = -S[:, 2]
    else: depth = -S[:, 1]; u = -u
    if view == 'front': u = S[:, 0] * -1
    lo_u, hi_u, lo_v, hi_v = u.min(), u.max(), v.min(), v.max()
    sc = (px - 20) / max(hi_u - lo_u, hi_v - lo_v)
    W, Hh = int((hi_u - lo_u) * sc) + 20, int((hi_v - lo_v) * sc) + 20
    x = ((u - lo_u) * sc + 10).astype(int); y = (Hh - 1 - ((v - lo_v) * sc + 10)).astype(int)
    order = np.argsort(-depth)            # far first, near overwrite
    img = np.full((Hh, W, 3), 255, np.uint8)
    shade = .35 + .65 * np.abs(N @ dirv)
    col = np.clip(C * shade[:, None], 0, 255).astype(np.uint8)
    img[y[order], x[order]] = col[order]
    im = Image.fromarray(img); d = ImageDraw.Draw(im)
    for p, rgb in marks:
        p = np.asarray(p, float)
        uu = {'side': p[1], 'top': p[0], 'front': -p[0]}[view]; vv = {'side': p[2], 'top': p[1], 'front': p[2]}[view]
        X = (uu - lo_u) * sc + 10; Y = Hh - 1 - ((vv - lo_v) * sc + 10)
        d.ellipse([X - 4, Y - 4, X + 4, Y + 4], outline=rgb, width=2)
    im.save(path)

def eye_render(path, S, N, C, eye, look, fov=80, px=900, marks=()):
    from PIL import Image, ImageDraw
    f = np.asarray(look, float) - eye; f /= np.linalg.norm(f)
    r = np.cross(f, [0, 0, 1.]); r /= np.linalg.norm(r); up = np.cross(r, f)
    q = S - eye; z = q @ f; keep = z > .05
    q, z, Nn, Cc = q[keep], z[keep], N[keep], C[keep]
    t = 1 / math.tan(math.radians(fov / 2))
    x = ((q @ r) / z * t * .5 + .5) * px; y = (.5 - (q @ up) / z * t * .5) * px
    ok = (x >= 0) & (x < px) & (y >= 0) & (y < px)
    x, y, z, Nn, Cc = x[ok].astype(int), y[ok].astype(int), z[ok], Nn[ok], Cc[ok]
    order = np.argsort(-z)
    img = np.full((px, px, 3), 200, np.uint8)
    shade = .35 + .65 * np.abs((Nn * f).sum(1))
    img[y[order], x[order]] = np.clip(Cc * shade[:, None], 0, 255).astype(np.uint8)[order]
    im = Image.fromarray(img); d = ImageDraw.Draw(im)
    for p, rgb in marks:
        qq = np.asarray(p, float) - eye; zz = qq @ f
        if zz <= 0: continue
        X = ((qq @ r) / zz * t * .5 + .5) * px; Y = (.5 - (qq @ up) / zz * t * .5) * px
        d.ellipse([X - 5, Y - 5, X + 5, Y + 5], outline=rgb, width=2)
    im.save(path)


def main(job):
    J = np.load(job, allow_pickle=True)
    S, N, C = J['S'], J['N'], J['C']
    for v in json.loads(str(J['views'])):
        marks = [(m[0], tuple(m[1])) for m in v.get('marks', [])]
        if v['view'] == 'eye': eye_render(v['path'], S, N, C, np.array(v['eye']), np.array(v['look']), marks=marks)
        else: render(v['path'], S, N, C, v['view'], marks=marks)
        print('PREVIEW', v['path'])

if __name__ == '__main__':
    main(sys.argv[1])
