#!/usr/bin/env python3
"""Version 0.0.73 PUNISHER VAN SOUND: the player's own recording (owner t996u; sha256 below), a door slam,
the start-up and the engine running, becomes the van's two embedded sounds:
  assets/sfx/source/user-van-enter-start-engine.mp3 ->
    assets/sfx/van_start.wav        (sfx::vanStart: the slam and the start-up, once, from the file's start
                                     to the loop point A)
    assets/sfx/van_engine_loop.wav  (sfx::vanEngine: the running engine from A, L long, repeating
                                     seamlessly: its last X are crossfaded with the X just before A)
    assets/sfx/van_door_slam.wav    (sfx::vanDoorSlam: the slam alone, owner t997u - played again as he gets
                                     out, after the engine has faded)
Played on ONE voice, queued back to back (sfx_win.cpp engine), van_start runs straight into the loop at
sample A of the recording: no gap, no click. Nothing is invented: every sample is the recording's own
(decoded as it is - its own 24 kHz stereo, 16-bit, no gain, no filter), the crossfade aside.

THE SPLIT POINTS ARE MEASURED, NOT GUESSED (printed, and written to assets/sfx/van_engine_0073.json):
  - the slam: the first 2 ms frame 20 dB over the file's opening floor;
  - the slam's end: the first 20 ms frame from 0.1 s after its onset 15 dB under the slam's loudest (its
    first 0.3 s) - the gap before the starter's first crank (the cranks' tone is 250 - 500 Hz, the slam's
    120 - 250 Hz, measured) - at its rising zero crossing; the clip starts at the rising zero crossing 20 ms
    before the onset and its last 10 ms fade out (the gap's own floor, so no click);
  - the start-up's end: the first time after which every 0.25 s window for 2 s holds the steady engine's
    level (within 1 dB of the median of 10 s .. the end) and its tone (spectral centroid within 6 %);
  - A: the rising zero crossing of the mid signal nearest that time;
  - L: of 7 .. 12 s, the length whose X before its end best matches (normalized correlation of the engine's
    firing band, 20 - 120 Hz: its 30 Hz fundamental and 60 / 90 Hz, measured) the X before A - so the
    crossfade joins the engine's pulses in phase;
  - X: 0.2 s, split at 120 Hz (a zero-phase low pass; the high part is the rest, so the two add back to the
    recording exactly): the in-phase firing band crossfades linearly (when its match is >= 0.5), the
    uncorrelated rest equal-power - so the join neither dips nor swells.

  python3 tools/sfx/van_engine_0073.py          write the WAVs and the json
  python3 tools/sfx/van_engine_0073.py --check  make them again and compare (exit 1 on any difference)
Needs ffmpeg, numpy and scipy."""
import hashlib, json, os, subprocess, sys, tempfile

import numpy as np
import scipy.io.wavfile as wavfile
import scipy.signal as signal

HERE = os.path.dirname(os.path.abspath(__file__))
SFX = os.path.normpath(os.path.join(HERE, '..', '..', 'assets', 'sfx'))
SOURCE = 'user-van-enter-start-engine.mp3'
DIGEST = 'd650044451f7c846ba0c7ad5c05332f1e2024d0b18fa8c42327addfd8cc033fc'
START, LOOP, SLAM, REPORT = 'van_start.wav', 'van_engine_loop.wav', 'van_door_slam.wav', 'van_engine_0073.json'
XFADE_S, LOOP_MIN_S, LOOP_MAX_S, BAND = .2, 7., 12., (20., 120.)


def sha(path):
    return hashlib.sha256(open(path, 'rb').read()).hexdigest()


def decode(src):
    with tempfile.TemporaryDirectory(prefix='sm2073_van_') as d:
        p = os.path.join(d, 'x.wav')
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', src, '-map_metadata', '-1', '-fflags', '+bitexact',
                        '-flags:a', '+bitexact', '-c:a', 'pcm_s16le', '-f', 'wav', p], check=True)
        rate, x = wavfile.read(p)
    if x.ndim == 1:
        x = x[:, None]
    return rate, x


def frames_db(m, rate, seconds):
    hop = max(1, int(rate * seconds))
    n = len(m) // hop
    seg = m[:n * hop].reshape(n, hop)
    return 20 * np.log10(np.sqrt((seg ** 2).mean(1)) + 1e-9), hop


def centroids(m, rate, seconds):
    hop = int(rate * seconds)
    n = len(m) // hop
    f = np.fft.rfftfreq(hop, 1 / rate)
    w = np.hanning(hop)
    out = np.empty(n)
    for i in range(n):
        s = np.abs(np.fft.rfft(m[i * hop:(i + 1) * hop] * w))
        out[i] = (s * f).sum() / (s.sum() + 1e-12)
    return out


def analyse(rate, x):
    m = x.astype(np.float64).mean(1) / 32768
    # the slam: the first 2 ms frame 20 dB over the opening floor (the quietest tenth of the first 0.25 s)
    db2, hop2 = frames_db(m, rate, .002)
    opening = db2[:int(.25 / .002)]
    floor = np.percentile(opening[opening > -150], 10) if (opening > -150).any() else -90.
    slam_i = int(np.argmax(db2 > floor + 20))
    slam = slam_i * hop2 / rate
    def rising_near(i, reach):
        w = m[max(0, i - reach):i + reach]
        z = np.nonzero((w[:-1] < 0) & (w[1:] >= 0))[0]
        return max(0, i - reach) + int(z[np.argmin(np.abs(z + max(0, i - reach) - i))]) + 1 if len(z) else i
    db20, hop20 = frames_db(m, rate, .02)
    g0, top = int((slam + .1) / .02), float(db20[int(slam / .02):int((slam + .3) / .02)].max())
    gap = next(i for i in range(g0, len(db20)) if db20[i] <= top - 15)
    slam_end = rising_near(gap * hop20 + hop20 // 2, rate // 200)
    slam_begin = rising_near(max(0, int((slam - .02) * rate)), rate // 200)
    # the steady engine: 0.25 s windows from 10 s to the end
    db, hop = frames_db(m, rate, .25)
    cent = centroids(m, rate, .25)
    first = int(10 / .25)
    ref_db, ref_c = float(np.median(db[first:])), float(np.median(cent[first:]))
    ok = (np.abs(db - ref_db) < 1.) & (np.abs(cent - ref_c) / ref_c < .06)
    run = int(2 / .25)
    settle_i = next(i for i in range(int(slam / .25) + 1, len(ok) - run) if ok[i:i + run].all())
    settle = settle_i * hop / rate
    # A: the rising zero crossing of the mid signal nearest the settle time
    s0 = int(settle * rate)
    window = m[s0 - rate // 50:s0 + rate // 50]
    rising = np.nonzero((window[:-1] < 0) & (window[1:] >= 0))[0]
    a = s0 - rate // 50 + int(rising[np.argmin(np.abs(rising - rate // 50))]) + 1
    # L: the X before A against the X before A+L, in the engine's low band
    sos = signal.butter(4, BAND, 'bandpass', fs=rate, output='sos')
    low = signal.sosfiltfilt(sos, m)
    xn = int(XFADE_S * rate)
    ref = low[a - xn:a]
    ref = (ref - ref.mean()) / (np.linalg.norm(ref - ref.mean()) + 1e-12)
    lo, hi = int(LOOP_MIN_S * rate), int(LOOP_MAX_S * rate)
    if a + hi > len(m):
        hi = len(m) - a
    span = low[a + lo - xn:a + hi]
    corr = signal.correlate(span, ref, mode='valid')  # corr[k]: span[k:k+xn] . ref
    # normalize each candidate window
    c2 = np.convolve(span ** 2, np.ones(xn), 'valid')
    c1 = np.convolve(span, np.ones(xn), 'valid')
    norm = np.sqrt(np.maximum(c2 - c1 ** 2 / xn, 1e-18))
    ncc = corr / norm
    k = int(np.argmax(ncc))
    length = lo + k
    # the loop's own level holds the steady engine's (no transient inside it)
    ldb, _ = frames_db(m[a:a + length], rate, .05)
    return dict(rate=int(rate), channels=int(x.shape[1]), samples=int(len(m)), seconds=round(len(m) / rate, 4),
                opening_floor_db=round(float(floor), 1), slam_s=round(slam, 4),
                slam_clip_samples=[int(slam_begin), int(slam_end)],
                slam_clip_s=[round(slam_begin / rate, 5), round(slam_end / rate, 5)],
                slam_gap_db=round(float(db20[gap]), 1), startup_settled_s=round(settle, 3),
                steady_db=round(ref_db, 2), steady_centroid_hz=round(ref_c, 1),
                loop_start_sample=int(a), loop_start_s=round(a / rate, 5),
                loop_samples=int(length), loop_s=round(length / rate, 5),
                crossfade_samples=xn, crossfade='split 120 Hz: low linear, rest equal-power' if ncc[k] >= .5 else 'equal-power',
                join_correlation=round(float(ncc[k]), 4),
                loop_level_range_db=[round(float(ldb.min()), 1), round(float(ldb.max()), 1)])


def build(rate, x, r):
    a, length, xn = r['loop_start_sample'], r['loop_samples'], r['crossfade_samples']
    xf = x.astype(np.float64)
    start = x[:a]
    loop = xf[a:a + length].copy()
    t = (np.arange(xn) + .5) / xn
    pin, pout = np.sin(t * np.pi / 2)[:, None], np.cos(t * np.pi / 2)[:, None]
    tail, before = xf[a + length - xn:a + length], xf[a - xn:a]
    if r['crossfade'].startswith('split'):
        sos = signal.butter(4, BAND[1], 'low', fs=rate, output='sos')
        # filtered over a margin either side, so the cut's ends carry no filter edge
        pad = rate // 10
        def low_of(i0):
            seg = xf[i0 - pad:i0 + xn + pad]
            return signal.sosfiltfilt(sos, seg, axis=0)[pad:pad + xn]
        tl, bl = low_of(a + length - xn), low_of(a - xn)
        lin = t[:, None]
        mixed = (tl * (1 - lin) + bl * lin) + ((tail - tl) * pout + (before - bl) * pin)
    else:
        mixed = tail * pout + before * pin
    # its last X: the tail fading out, the X just before A fading in (which runs on into A: loop[0])
    loop[length - xn:] = mixed
    loop = np.clip(np.round(loop), -32768, 32767).astype(np.int16)
    b, e = r['slam_clip_samples']
    slam = xf[b:e].copy()
    fade = min(len(slam), rate // 100)
    slam[len(slam) - fade:] *= np.linspace(1, 0, fade)[:, None]
    slam = np.clip(np.round(slam), -32768, 32767).astype(np.int16)
    return start.astype(np.int16), loop, slam


def write(path, rate, data):
    wavfile.write(path, rate, data)


def main(check=False):
    src = os.path.join(SFX, 'source', SOURCE)
    if not os.path.exists(src) or sha(src) != DIGEST:
        print("MISSING OR CHANGED: the player's sound " + src)
        return 1
    rate, x = decode(src)
    r = analyse(rate, x)
    start, loop, slam = build(rate, x, r)
    print(json.dumps(r, indent=1))
    outs = {START: start, LOOP: loop, SLAM: slam}
    if not check:
        for name, data in outs.items():
            write(os.path.join(SFX, name), rate, data)
            print(name, os.path.getsize(os.path.join(SFX, name)), 'bytes')
        with open(os.path.join(SFX, REPORT), 'w') as f:
            json.dump(r, f, indent=1)
            f.write('\n')
        return 0
    bad = 0
    with tempfile.TemporaryDirectory(prefix='sm2073_van_') as d:
        for name, data in outs.items():
            p = os.path.join(d, name)
            write(p, rate, data)
            same = open(p, 'rb').read() == open(os.path.join(SFX, name), 'rb').read()
            print(('SAME ' if same else 'DIFFERENT ') + name)
            bad += not same
    stored = json.load(open(os.path.join(SFX, REPORT)))
    if stored != r:
        print('DIFFERENT ' + REPORT)
        bad += 1
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main('--check' in sys.argv[1:]))
