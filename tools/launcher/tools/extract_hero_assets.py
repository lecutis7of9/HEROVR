#!/usr/bin/env python3
"""Builds the HERO launcher design's assets from the player's concept video (launcher-redesign).

    python3 tools/extract_hero_assets.py concept.mp4 [--out assets/hero] [--preview DIR]

Writes (all embedded in the exe by build.ps1 / build.sh):
  orb.bin    the orb: the video's own frames, cropped round the orb (soft round mask, on black), JPEG.
             Intro frames (orb centred, then shrinking to the left, exactly where it is in the video) and an
             idle loop whose last frames are crossfaded into the frames just before its first one, so the
             wrap is seamless.
  intro.wav  the video's own sound from 0 s to LOOP_START (swell, whoosh, the tonal hit).
  loop.wav   the ambient drone after it (LOOP_START..LOOP_END), level-evened, whose last XFADE seconds are an
             equal-power crossfade into the sound just before LOOP_START: intro -> loop -> loop ... is continuous.
--preview also writes intro.mp3 / loop.mp3 / ambient_2min.mp3 and a loop-check mp4 to DIR.

Needs ffmpeg, numpy and Pillow (not needed to build the launcher: the outputs are committed).
"""
import argparse, io, os, struct, subprocess, sys, tempfile, wave
import numpy as np
from PIL import Image

FPS = 24
W, H = 1280, 720
CY = 357.0                 # the orb's centre line in the video
IDLE_CX = 369.5            # where the orb settles
MASK_R = 280.0             # soft round mask radius at idle scale (ring ~207 px + halo + floor glow)
RING_R = 207.0
INTRO_END = 140            # frames 0..139 are the intro (centred orb, then the move left)
LOOP_END = 188             # frames 140..187 are the idle loop (the video's orb changes look after ~190)
LOOP_XF = 12               # frames crossfaded at the loop's end with frames 128..139
JPEG_Q = 80
STORE = 0.75              # frames are stored at 0.75x the video's pixels (the glow is soft; drawn scaled back)
STORE_MAX = 1.25          # and an orb bigger than 1.25x the idle one at 1.25x idle size
LOOP_START = 5.0           # s
LOOP_STOP = 8.6            # s (the video fades out after that)
XFADE = 0.8                # s
SR = 48000


def frames(video, tmp):
    subprocess.check_call(["ffmpeg", "-loglevel", "error", "-y", "-i", video, "-vf", "fps=%d" % FPS,
                           os.path.join(tmp, "%03d.png")])
    n = len([f for f in os.listdir(tmp) if f.endswith(".png")])
    return [os.path.join(tmp, "%03d.png" % (i + 1)) for i in range(n)]


def green(path):
    return np.asarray(Image.open(path).convert("RGB")).astype(np.float32)


# --- where the orb is in each frame: radial ring profile matched against a settled frame ---------
ANG = np.deg2rad(np.linspace(100, 260, 33))   # left half only (the menu comes in on the right)
RAD = np.arange(20, 320, 3.0)


def _sample(g, cx, s):
    X = cx + np.outer(RAD * s, np.cos(ANG)); Y = CY + np.outer(RAD * s, np.sin(ANG))
    xi = np.clip(X.round().astype(int), 0, W - 1); yi = np.clip(Y.round().astype(int), 0, H - 1)
    return g[yi, xi].mean(1)


def _corr(p, q):
    p = p - p.mean(); q = q - q.mean()
    return (p * q).sum() / np.sqrt((p * p).sum() * (q * q).sum())


def track(paths):
    T = _sample(green(paths[151])[:, :, 1], IDLE_CX, 1.0)
    out = []; prev = None
    for i in range(LOOP_END):
        g = green(paths[i])[:, :, 1]
        if prev is None: cxs, ss = np.arange(600, 670, 1.0), np.arange(1.2, 1.7, 0.01)
        else: cxs, ss = np.arange(prev[0] - 14, prev[0] + 14.1, 1.0), np.arange(prev[1] - 0.06, prev[1] + 0.061, 0.005)
        best = max(((_corr(_sample(g, cx, s), T), cx, s) for s in ss for cx in cxs))
        prev = (best[1], best[2]); out.append(prev)
    a = np.array(out)
    # smooth (the crops go back where they came from, so this only steadies the mask)
    k = np.ones(9) / 9
    sm = np.stack([np.convolve(np.pad(a[:, j], 4, mode="edge"), k, mode="valid") for j in range(2)], 1)
    sm[INTRO_END:] = (IDLE_CX, 1.0)
    return sm


def masked(path, cx, s):
    """The frame's orb on black: soft round mask, and halo pixels brighter than the halo (bits of the menu
    or the logo sliding in) pulled down to it. Returns (rgb crop, x, y)."""
    a = green(path)
    R = MASK_R * s; ring = RING_R * s
    x0, y0 = int(np.floor(cx - R)), int(np.floor(CY - R)); x1, y1 = int(np.ceil(cx + R)), int(np.ceil(CY + R))
    x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, W), min(y1, H)
    c = a[y0:y1, x0:x1].copy()
    yy, xx = np.mgrid[y0:y1, x0:x1]
    d = np.sqrt((xx - cx) ** 2 + (yy - CY) ** 2)
    ang = np.degrees(np.arctan2(yy - CY, xx - cx))
    halo = (d > ring * 1.1) & ~((ang > 55) & (ang < 125))      # not the floor glow under the orb
    rb = (d / 4).astype(int)
    lum = c.max(2)
    lim = np.zeros(rb.max() + 1, np.float32)
    for r in np.unique(rb[halo]):
        sel = halo & (rb == r)
        lim[r] = np.median(lum[sel]) * 1.5 + 8
    over = halo & (lum > lim[rb])
    f = np.where(over, lim[rb] / np.maximum(lum, 1), 1.0)
    c *= f[:, :, None]
    m = np.clip((R - d) / (0.14 * R), 0, 1); m = m * m * (3 - 2 * m)
    c *= m[:, :, None]
    return c, x0, y0


def warp_to_idle(c, x, y, cx, s):
    """A crop of a nearly-settled frame moved/scaled onto the idle geometry (for the loop crossfade)."""
    im = Image.fromarray(np.clip(c, 0, 255).astype(np.uint8))
    k = 1.0 / s
    nw, nh = max(1, int(round(im.width * k))), max(1, int(round(im.height * k)))
    im = im.resize((nw, nh), Image.LANCZOS)
    nx = IDLE_CX - (cx - x) * k; ny = CY - (CY - y) * k
    return np.asarray(im).astype(np.float32), nx, ny


def idle_canvas(parts):
    R = MASK_R
    x0, y0 = int(np.floor(IDLE_CX - R)), int(np.floor(CY - R))
    size = int(np.ceil(2 * R))
    out = np.zeros((size, size, 3), np.float32)
    for c, w, x, y in parts:
        ox, oy = int(round(x - x0)), int(round(y - y0))
        h_, w_ = c.shape[:2]
        sx0, sy0 = max(0, -ox), max(0, -oy); dx0, dy0 = max(0, ox), max(0, oy)
        ww = min(w_ - sx0, size - dx0); hh = min(h_ - sy0, size - dy0)
        out[dy0:dy0 + hh, dx0:dx0 + ww] += c[sy0:sy0 + hh, sx0:sx0 + ww] * w
    return out, x0, y0


def jpeg(c, s=1.0):
    """JPEG of a crop, stored at STORE x its pixels (and an orb bigger than STORE_MAX x idle at STORE_MAX)
    (the record keeps its full-size placement; the launcher scales it back)."""
    im = Image.fromarray(np.clip(c + 0.5, 0, 255).astype(np.uint8))
    k = STORE * min(1.0, STORE_MAX / s)
    if k < 1:
        im = im.resize((max(1, int(round(im.width * k))), max(1, int(round(im.height * k)))), Image.LANCZOS)
    b = io.BytesIO()
    im.save(b, "JPEG", quality=JPEG_Q, optimize=True)
    return b.getvalue()


def build_orb(paths, out):
    geo = track(paths)
    recs = []
    for i in range(INTRO_END):
        cx, s = geo[i]
        c, x, y = masked(paths[i], cx, s)
        recs.append((x, y, c.shape[1], c.shape[0], cx, s, jpeg(c, s)))
    loop = []
    n = LOOP_END - INTRO_END
    for k in range(n):
        f = INTRO_END + k
        c, x, y = masked(paths[f], IDLE_CX, 1.0)
        if k >= n - LOOP_XF:
            # the loop's end turns into the frames just before its start (frames 128..139, cut at the idle
            # place: the orb has settled to within a pixel or two there), so the wrap is the video's own
            # frame 139 -> 140 step
            j = k - (n - LOOP_XF)
            t = (j + 1.0) / LOOP_XF; t = t * t * (3 - 2 * t)
            pc, px, py = masked(paths[INTRO_END - LOOP_XF + j], IDLE_CX, 1.0)
            c = c * (1 - t) + pc * t
        c, x, y = idle_canvas([(c, 1.0, x, y)])
        loop.append((x, y, c.shape[1], c.shape[0], IDLE_CX, 1.0, jpeg(c, 1.0)))
    with open(os.path.join(out, "orb.bin"), "wb") as f:
        f.write(b"HORB")
        f.write(struct.pack("<7i", 1, FPS, len(recs), len(loop), W, H, 0))
        for x, y, w, h, cx, s, data in recs + loop:  # x, y, w, h = where it goes in the 1280x720 video; the JPEG may be smaller
            f.write(struct.pack("<4i2fi", x, y, w, h, float(cx), float(s), len(data)))
            f.write(data)
    return recs, loop


# --- sound ---------------------------------------------------------------------------------------
def read_audio(video):
    raw = subprocess.check_output(["ffmpeg", "-loglevel", "error", "-i", video, "-vn", "-ac", "2", "-ar", str(SR),
                                   "-f", "s16le", "-"])
    return np.frombuffer(raw, np.int16).reshape(-1, 2).astype(np.float64) / 32768.0


def envelope(x, win):
    p = (x ** 2).mean(1)
    k = np.ones(win) / win
    return np.sqrt(np.convolve(np.pad(p, (win // 2, win - win // 2 - 1), mode="edge"), k, mode="valid"))


def build_audio(video, out):
    x = read_audio(video)
    A, B, X = int(LOOP_START * SR), int(LOOP_STOP * SR), int(XFADE * SR)
    env = envelope(x, int(0.6 * SR))
    target = float(np.median(env[A:A + int(1.5 * SR)]))
    g = np.ones(len(x))
    flat = np.clip(target / np.maximum(env, 1e-6), 0.5, 4.0)
    ramp0 = A - int(0.6 * SR)
    t = np.clip((np.arange(len(x)) - ramp0) / float(A - ramp0), 0, 1); t = t * t * (3 - 2 * t)
    g = (1 - t) * 1.0 + t * flat
    F = x * g[:, None]
    intro = F[:A]
    loop = F[A:B].copy()
    n = len(loop)
    i = np.arange(X) / float(X)
    fo, fi = np.cos(i * np.pi / 2), np.sin(i * np.pi / 2)
    loop[n - X:] = F[B - X:B] * fo[:, None] + F[A - X:A] * fi[:, None]
    mono = np.abs(x[:, 0] - x[:, 1]).max() < 2 / 32768.0      # the concept video's sound is the same on both sides
    for name, data in (("intro.wav", intro), ("loop.wav", loop)):
        wav(os.path.join(out, name), data[:, :1] if mono else data)
    return intro, loop


def wav(path, data):
    with wave.open(path, "wb") as w:
        w.setnchannels(data.shape[1]); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((np.clip(data, -1, 1) * 32767).round().astype("<i2").tobytes())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("video"); ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "assets", "hero"))
    ap.add_argument("--preview")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        paths = frames(a.video, tmp)
        if len(paths) < LOOP_END: sys.exit("expected at least %d frames, got %d" % (LOOP_END, len(paths)))
        recs, loop = build_orb(paths, a.out)
    intro, lp = build_audio(a.video, a.out)
    print("orb.bin: %d intro + %d loop frames, %.2f MB" % (len(recs), len(loop), os.path.getsize(os.path.join(a.out, "orb.bin")) / 1e6))
    for n in ("intro.wav", "loop.wav"):
        print("%s: %.2f MB" % (n, os.path.getsize(os.path.join(a.out, n)) / 1e6))
    if a.preview:
        os.makedirs(a.preview, exist_ok=True)
        long = np.concatenate([intro] + [lp] * int(np.ceil(120 * SR / len(lp))))[:120 * SR]
        tmpw = os.path.join(a.preview, "_ambient.wav"); wav(tmpw, long)
        for src, dst in ((os.path.join(a.out, "intro.wav"), "intro.mp3"), (os.path.join(a.out, "loop.wav"), "loop.mp3"), (tmpw, "ambient_2min.mp3")):
            subprocess.check_call(["ffmpeg", "-loglevel", "error", "-y", "-i", src, "-c:a", "libmp3lame", "-b:a", "192k", os.path.join(a.preview, dst)])
        os.remove(tmpw)


if __name__ == "__main__":
    main()
