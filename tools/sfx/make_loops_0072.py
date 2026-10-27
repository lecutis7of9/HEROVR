"""Version 0.0.72 SEAMLESS HEAT LOOPS (the user's 0.0.71 videos: SUPERMAN's heat vision "stutters" - 0.0.7 re-fired
a 0.6 s one-shot every 0.55 s). heat_vision.wav and freeze_breath.wav become 1.000 s loops that join end to start
with no seam: every tone has a whole number of cycles in the second, the noise is shaped in the frequency domain
over the whole loop (circular, so it wraps), and there is no fade in or out. sfx::loop plays them on repeat
(XAUDIO2_LOOP_INFINITE) for as long as the beam is held. Made from scratch with numpy (nothing sampled).

python tools/sfx/make_loops_0072.py [OUT_DIR] [--check FILE...]   (the same files every run)
"""
import os
import sys
import numpy as np
from scipy.io import wavfile

SR = 48000
N = SR  # 1.000 s: every frequency below is a whole number of Hz, so it wraps exactly
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'assets', 'sfx')
rng = np.random.default_rng(70072)
t = np.arange(N) / SR


def circ_band(lo, hi, tilt=0.0):
    """White noise band-passed over the whole loop (FFT): periodic, so the loop has no seam."""
    spec = np.fft.rfft(rng.standard_normal(N))
    f = np.fft.rfftfreq(N, 1 / SR)
    gain = ((f >= lo) & (f <= hi)).astype(float)
    edge = 0.15 * (hi - lo)
    gain = np.clip(np.minimum((f - lo + edge) / edge, (hi + edge - f) / edge), 0, 1) if edge > 0 else gain
    if tilt:
        gain *= (np.maximum(f, 1) / max(lo, 1)) ** -tilt
    x = np.fft.irfft(spec * gain, N)
    return x / (np.max(np.abs(x)) + 1e-12)


def circ_low(x, fc):
    spec = np.fft.rfft(x)
    f = np.fft.rfftfreq(N, 1 / SR)
    return np.fft.irfft(spec / np.sqrt(1 + (f / fc) ** 4), N)


def save(name, x, peak_db):
    x = x - np.mean(x)
    x = x / (np.max(np.abs(x)) + 1e-12) * 10 ** (peak_db / 20)
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name)
    wavfile.write(path, SR, np.round(x * 32767).astype(np.int16))
    return path


def heat_vision():
    """A steady searing hum: 95 Hz buzz and its octave, a sizzle that breathes at 13 Hz, a slow 2 Hz swell."""
    buzz = np.tanh(3 * np.sin(2 * np.pi * 95 * t)) * .3 + np.sin(2 * np.pi * 190 * t) * .4 + np.sin(2 * np.pi * 285 * t) * .08
    buzz = circ_low(buzz, 1200)
    sizzle = circ_band(3000, 9000) * (.4 + .3 * np.sin(2 * np.pi * 13 * t))
    swell = 1 + .08 * np.sin(2 * np.pi * 2 * t)
    return save('heat_vision.wav', (buzz + .5 * sizzle) * swell, -3.0)


def freeze_breath():
    """A steady cold rush: breathy band noise with a slow 1 Hz swell and fine glassy crackle (wrapped)."""
    rush = circ_band(500, 5000, .3) * (.85 + .15 * np.sin(2 * np.pi * 1 * t))
    crackle = np.zeros(N)
    for at in rng.uniform(0, 1, 14):
        f = rng.uniform(5000, 9000)
        tt = (t - at) % 1.0  # wraps around the loop's end
        crackle += rng.uniform(.15, .35) * np.sin(2 * np.pi * f * tt) * np.exp(-tt / .002)
    return save('freeze_breath.wav', rush + crackle, -3.0)


def seam(path):
    """The jump from the last sample to the first, against the loop's typical sample-to-sample step."""
    _, d = wavfile.read(path)
    x = d.astype(float)
    steps = np.abs(np.diff(x))
    return abs(x[0] - x[-1]) / (np.percentile(steps, 99) + 1e-9), len(x)


if __name__ == '__main__':
    if '--check' in sys.argv:
        bad = 0
        for p in sys.argv[sys.argv.index('--check') + 1:]:
            s, n = seam(p)
            print(f'{p}: {n} samples, seam {s:.2f} x the 99th-percentile step')
            bad += s > 1.0 or n != N
        sys.exit(1 if bad else 0)
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if args:
        OUT = args[0]
    for make in (heat_vision, freeze_breath):
        p = make()
        s, n = seam(p)
        print(f'{os.path.relpath(p)}  {n / SR:.3f} s  seam {s:.2f}')
