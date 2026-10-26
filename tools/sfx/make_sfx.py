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


# Version 0.0.6 (Update 39.0.2) REPULSOR: three takes on one shape (A CLASSIC, B HEAVY, C TIGHT; the
# one shipped is CMakeLists.txt's SPIDY_REPULSOR_TAKE). Each take is cut at the blast (0.12 s) into
# <take>_charge.wav, played on the press so the whine fills the chord wait, and <take>_fire.wav,
# played when the blast leaves the palm (the thump). Played back to back they are the whole take.
#   charge whine (rising, filtered, quiet) -> deep thump + air crack at 0.12 s -> short electric crackle.
def _whine(t, f0, f1, sec, level):
    """A turbine-like charge: two detuned rising partials and a shimmer band, swelling then cut."""
    k = np.clip(t / sec, 0, 1)
    f = f0 + (f1 - f0) * k ** 1.6
    ph = 2 * np.pi * np.cumsum(f) / SR
    tone = np.sin(ph) + .35 * np.sin(2.003 * ph) + .12 * np.sin(3.01 * ph)
    shimmer = band(noise(len(t) / SR), 4000, 11000, 2) * .25
    env = (k ** 1.5) * (t < sec) + (t >= sec) * np.exp(-(t - sec) / .012)
    return level * env * (tone + shimmer)


def _thump(t, at, f0, f1, decay, level):
    """The blast: a falling sine kick, a click of air and a pressure burst, all starting at `at`."""
    x = np.zeros_like(t)
    s = int(SR * at)
    tt = t[: len(t) - s]
    kick = np.sin(2 * np.pi * np.cumsum(f1 + (f0 - f1) * np.exp(-tt / .018)) / SR) * np.exp(-tt / decay)
    click = high(noise(len(tt) / SR), 2500) * np.exp(-tt / .0025)
    click /= np.max(np.abs(click)) + 1e-9
    press = band(noise(len(tt) / SR), 180, 1600, 2) * np.exp(-tt / .03)
    press /= np.max(np.abs(press)) + 1e-9
    x[s:] = level * (1.0 * kick + .45 * click + .5 * press)
    return x


def _crackle(t, at, length, density, level, seed):
    """Electric crackle: sparse sharp ticks, thinning out, through a bright band, plus a fading buzz."""
    r = np.random.default_rng(seed)
    x = np.zeros_like(t)
    s, e = int(SR * at), min(len(t), int(SR * (at + length)))
    for i in range(s, e):
        k = (i - s) / max(1, e - s)
        if r.random() < density * 100 * (1 - k) ** 1.5 / SR:
            x[i] = r.choice([-1, 1]) * (.4 + .6 * r.random()) * (1 - k)
    ticks = band(x, 1800, 9500, 2)
    ticks /= np.max(np.abs(ticks)) + 1e-9
    tt = np.maximum(t - at, 0)
    buzz = signal.sawtooth(2 * np.pi * 120 * t) * (t >= at) * np.exp(-tt / (length * .35))
    buzz = band(buzz, 200, 2400, 2)
    buzz /= np.max(np.abs(buzz)) + 1e-9
    return level * (ticks + .18 * buzz)


def _repulsor(name, sec, whine, thump, crackle, drive):
    t = t_of(sec)
    x = _whine(t, *whine) + _thump(t, *thump) + _crackle(t, *crackle)
    x = high(x, 30, 2)          # no DC or sub rumble the headset speakers can't play
    return save(name, np.tanh(drive * x), -1.0)


def repulsor_mcu_a():   # A "CLASSIC": balanced whine, solid thump, short crackle
    return _repulsor('repulsor_mcu_a.wav', .50, (900, 3600, .12, .30), (.12, 130, 62, .075, 1.0),
                     (.13, .22, 3.5, .30, 61), 1.4)


def repulsor_mcu_b():   # B "HEAVY": lower whine, deeper longer thump, longer crackle
    return _repulsor('repulsor_mcu_b.wav', .62, (650, 2800, .12, .26), (.12, 115, 52, .11, 1.15),
                     (.14, .32, 4.5, .32, 62), 1.6)


def repulsor_mcu_c():   # C "TIGHT": brighter, quicker, little crackle (good for rapid fire)
    return _repulsor('repulsor_mcu_c.wav', .38, (1200, 4600, .12, .24), (.12, 150, 72, .05, .95),
                     (.125, .13, 3, .24, 63), 1.3)


REPULSOR_0_0_6 = [repulsor_mcu_a, repulsor_mcu_b, repulsor_mcu_c]
BLAST_AT = .12  # the chord window (repulsor.hpp chordWindow): where the thump starts


def split_take(path):
    """<take>.wav -> <take>_charge.wav (0..BLAST_AT, a 3 ms fade out) + <take>_fire.wav (BLAST_AT..end)."""
    sr, d = wavfile.read(path)
    cut = int(sr * BLAST_AT)
    charge = d[:cut].astype(np.float64)
    n = int(sr * .003)
    charge[-n:] *= np.linspace(1, 0, n)
    stem = path[:-4]
    out = []
    for name, x in ((stem + '_charge.wav', charge.astype(np.int16)), (stem + '_fire.wav', d[cut:])):
        wavfile.write(name, sr, x)
        out.append(name)
    return out


ALL = [sonic_boom, repulsor_fire, chest_beam, beam_ready, baton_whoosh, baton_hit]

if __name__ == '__main__':
    import sys
    made = []
    if '--preview-repulsor' not in sys.argv:
        made += [make() for make in ALL]
    # 0.0.6's takes start from the seed again, so they are the same files whether made alone or after ALL.
    rng = np.random.default_rng(5005)
    for make in REPULSOR_0_0_6:
        p = make()
        made += [p] + split_take(p)
    for p in made:
        sr, d = wavfile.read(p)
        print(f'{os.path.relpath(p)}  {len(d) / sr:.3f} s  {sr} Hz')
