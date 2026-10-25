"""Version 0.0.5 sound effects, made from scratch with numpy/scipy (nothing sampled from the game or films).

python tools/sfx/make_sfx.py  -> assets/sfx/*.wav, 48 kHz, 16-bit, mono. Same seed, same files every run.
"""
import os
import numpy as np
from scipy import signal
from scipy.io import wavfile

SR = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'assets', 'sfx')
rng = np.random.default_rng(5005)


def t_of(sec):
    return np.arange(int(SR * sec)) / SR


def noise(sec):
    return rng.standard_normal(int(SR * sec))


def band(x, lo, hi, order=4):
    sos = signal.butter(order, [lo, hi], btype='bandpass', fs=SR, output='sos')
    return signal.sosfilt(sos, x)


def low(x, f, order=4):
    return signal.sosfilt(signal.butter(order, f, btype='lowpass', fs=SR, output='sos'), x)


def high(x, f, order=4):
    return signal.sosfilt(signal.butter(order, f, btype='highpass', fs=SR, output='sos'), x)


def fade(x, inn=0.002, out=0.01):
    a, b = int(SR * inn), int(SR * out)
    x = x.copy()
    if a:
        x[:a] *= np.linspace(0, 1, a)
    if b:
        x[-b:] *= np.linspace(1, 0, b)
    return x


def chirp(t, f0, f1, curve=1.0):
    f = f0 + (f1 - f0) * (t / t[-1]) ** curve
    return np.sin(2 * np.pi * np.cumsum(f) / SR)


def save(name, x, peak_db=-1.0):
    x = fade(x - np.mean(x))
    x = x / (np.max(np.abs(x)) + 1e-12) * 10 ** (peak_db / 20)
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name)
    wavfile.write(path, SR, (x * 32767).astype(np.int16))
    return path


def n_wave(sec, dur):
    """A sonic boom's pressure signature: a sharp jump up, a straight fall below zero, a sharp jump back."""
    x = np.zeros(int(SR * sec))
    n = int(SR * dur)
    x[:n] = np.linspace(1, -1, n)
    return x


def sonic_boom():
    sec = 1.85
    t = t_of(sec)
    x = np.zeros_like(t)
    for start, gain in ((0.0, 1.0), (0.11, 0.8)):              # the double crack (bow and tail shock)
        s = int(SR * start)
        nw = n_wave(sec - start, 0.075)
        x[s:] += gain * low(nw, 2200, 2)
    crack = high(noise(sec), 1500) * np.exp(-t / 0.018)          # air tearing on the edge
    x += 0.35 * crack
    rumble = low(noise(sec), 140, 4) * np.exp(-t / 0.55) * np.minimum(1, t / 0.04)
    x += 2.2 * rumble / (np.max(np.abs(rumble)) + 1e-9) * 0.6
    x += 0.5 * np.sin(2 * np.pi * np.cumsum(55 - 25 * t / sec) / SR) * np.exp(-t / 0.45)
    roll = low(noise(sec), 420, 2) * np.exp(-((t - 0.5) / 0.35) ** 2) * 0.25   # distant rolling echo
    x += roll / (np.max(np.abs(roll)) + 1e-9) * 0.2
    return save('sonic_boom.wav', np.tanh(1.6 * x), -0.5)


def repulsor_fire():
    sec = 0.42
    t = t_of(sec)
    whine = chirp(t, 520, 2300, 0.6) * np.exp(-t / 0.12)
    fm = np.sin(2 * np.pi * 1450 * t + 3.0 * np.sin(2 * np.pi * 95 * t)) * np.exp(-t / 0.08)
    hiss = band(noise(sec), 2500, 9000) * np.exp(-t / 0.05)
    thump = np.sin(2 * np.pi * np.cumsum(140 - 80 * t / sec) / SR) * np.exp(-t / 0.06)
    x = 0.55 * whine + 0.35 * fm + 0.35 * hiss / np.max(np.abs(hiss)) + 0.6 * thump
    return save('repulsor_fire.wav', np.tanh(1.3 * x), -1.0)


def chest_beam():
    sec = 1.7
    t = t_of(sec)
    charge = np.clip(t / 0.25, 0, 1)
    body_env = charge * np.where(t < 1.25, 1.0, np.exp(-(t - 1.25) / 0.15))
    f = 98 + 30 * charge
    saw = sum(signal.sawtooth(2 * np.pi * np.cumsum(f * d) / SR) for d in (1.0, 1.006, 0.994)) / 3
    saw = low(saw, 2600, 2)
    buzz = np.sin(2 * np.pi * np.cumsum(f * 4.02) / SR) * (0.6 + 0.4 * np.sin(2 * np.pi * 13 * t))
    rise = chirp(t[: int(SR * 0.3)], 300, 3000, 1.5) * np.linspace(0.2, 1, int(SR * 0.3))
    rise = np.concatenate([rise, np.zeros(len(t) - len(rise))])
    hiss = high(noise(sec), 3500) * 0.12
    x = body_env * (0.7 * saw + 0.35 * buzz + hiss) + 0.4 * rise
    return save('chest_beam.wav', np.tanh(1.5 * x), -1.0)


def beam_ready():
    sec = 0.62
    t = t_of(sec)
    x = np.zeros_like(t)
    for start, f in ((0.0, 880.0), (0.12, 1318.5)):            # a rising fifth: "ready"
        s = int(SR * start)
        tt = t[: len(t) - s]
        tone = sum(a * np.sin(2 * np.pi * f * m * tt) for m, a in ((1, 1.0), (2.0, 0.3), (3.01, 0.12)))
        x[s:] += tone * np.exp(-tt / 0.18) * np.minimum(1, tt / 0.004)
    return save('beam_ready.wav', x, -3.0)


def baton_whoosh():
    sec = 0.5
    t = t_of(sec)
    env = np.sin(np.pi * np.clip(t / sec, 0, 1)) ** 2
    n = noise(sec)
    x = np.zeros_like(t)
    hop = 480
    for i in range(0, len(t), hop):                         # a band that sweeps up then down, like the club passing
        c = 500 + 1600 * np.sin(np.pi * i / len(t))
        seg = band(n[max(0, i - 2048): i + hop], c * 0.7, c * 1.3, 2)[-min(hop, len(t) - i):]
        x[i: i + len(seg)] = seg
    x = low(x, 4000, 2) * env
    return save('baton_whoosh.wav', x, -2.0)


def baton_hit():
    sec = 0.34
    t = t_of(sec)
    click = high(noise(sec), 3000) * np.exp(-t / 0.004)
    thud = np.sin(2 * np.pi * np.cumsum(150 - 70 * t / sec) / SR) * np.exp(-t / 0.07)
    knock = band(noise(sec), 450, 900, 4) * np.exp(-t / 0.035)
    knock /= np.max(np.abs(knock)) + 1e-9
    ring = np.sin(2 * np.pi * 1180 * t) * np.exp(-t / 0.05) * 0.15
    x = 0.5 * click / (np.max(np.abs(click)) + 1e-9) + 0.9 * thud + 0.6 * knock + ring
    return save('baton_hit.wav', np.tanh(1.8 * x), -1.0)


ALL = [sonic_boom, repulsor_fire, chest_beam, beam_ready, baton_whoosh, baton_hit]

if __name__ == '__main__':
    for make in ALL:
        p = make()
        sr, d = wavfile.read(p)
        print(f'{os.path.relpath(p)}  {len(d) / sr:.2f} s  {sr} Hz')
