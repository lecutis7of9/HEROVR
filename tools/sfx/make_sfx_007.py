"""Version 0.0.7 HERO PROFILES' sound effects, made from scratch with numpy/scipy (nothing sampled from
any game or film). Only the 0.0.7 files are written; make_sfx.py's 0.0.5-0.0.62 WAVs are untouched.

python tools/sfx/make_sfx_007.py [OUT_DIR]  -> pistol_shot, shotgun_blast, dry_click, mag_drop, mag_seat,
holster, claw_snikt, heat_vision, freeze_breath .wav (48 kHz, 16-bit, mono; the same files every run).
CAPTAIN AMERICA adds shield_throw, shield_ricochet, shield_catch, shield_clang (their own random stream,
so the nine earlier files come out byte-identical).  python tools/sfx/make_sfx_007.py [OUT_DIR] [--cap]
writes only the shield's four with --cap.
"""
import os
import sys
import numpy as np
from scipy import signal
from scipy.io import wavfile

SR = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'assets', 'sfx')
rng = np.random.default_rng(7007)


def t_of(sec):
    return np.arange(int(SR * sec)) / SR


def noise(sec):
    return rng.standard_normal(int(SR * sec))


def band(x, lo, hi, order=4):
    return signal.sosfilt(signal.butter(order, [lo, hi], btype='bandpass', fs=SR, output='sos'), x)


def low(x, f, order=4):
    return signal.sosfilt(signal.butter(order, f, btype='lowpass', fs=SR, output='sos'), x)


def high(x, f, order=4):
    return signal.sosfilt(signal.butter(order, f, btype='highpass', fs=SR, output='sos'), x)


def norm(x):
    return x / (np.max(np.abs(x)) + 1e-12)


def save(name, x, peak_db=-1.0):
    x = x - np.mean(x)
    a, b = int(SR * .001), int(SR * .01)
    x[:a] *= np.linspace(0, 1, a)
    x[-b:] *= np.linspace(1, 0, b)
    x = norm(x) * 10 ** (peak_db / 20)
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name)
    wavfile.write(path, SR, (x * 32767).astype(np.int16))
    return path


def click(t, at, f, decay, level):
    tt = np.clip(t - at, 0, None)
    on = (t >= at).astype(float)
    return on * level * np.sin(2 * np.pi * f * tt) * np.exp(-tt / decay)


def pistol_shot():
    """A 9 mm pistol: a sharp crack, a short body thump, a slide's metallic clack and a little room tail."""
    sec = .45
    t = t_of(sec)
    crack = norm(high(noise(sec), 1500)) * np.exp(-t / .006)
    body = np.sin(2 * np.pi * np.cumsum(180 - 120 * np.clip(t / .08, 0, 1)) / SR) * np.exp(-t / .03)
    tail = norm(low(noise(sec), 1800)) * np.exp(-t / .12) * .25
    clack = click(t, .045, 2900, .006, .35) + click(t, .05, 4100, .004, .25)
    return save('pistol_shot.wav', np.tanh(2.5 * (crack + .9 * body + tail + clack)), -1.0)


def shotgun_blast():
    """A 12 gauge: a wide boom, a deep body, a long rolling tail."""
    sec = .8
    t = t_of(sec)
    crack = norm(high(noise(sec), 900)) * np.exp(-t / .012)
    body = np.sin(2 * np.pi * np.cumsum(110 - 70 * np.clip(t / .15, 0, 1)) / SR) * np.exp(-t / .07)
    tail = norm(low(noise(sec), 900)) * np.exp(-t / .25) * .45
    return save('shotgun_blast.wav', np.tanh(2.2 * (crack + 1.2 * body + tail)), -1.0)


def dry_click():
    """A hammer falling on nothing."""
    sec = .12
    t = t_of(sec)
    x = click(t, 0, 3400, .004, 1) + click(t, .002, 5200, .002, .6) + norm(band(noise(sec), 2000, 6000)) * np.exp(-t / .003) * .5
    return save('dry_click.wav', x, -4.0)


def mag_drop():
    """A magazine sliding out and clattering off the floor."""
    sec = .4
    t = t_of(sec)
    slide = norm(band(noise(sec), 1500, 4000)) * np.exp(-t / .02) * .5
    hit = click(t, .22, 1700, .01, .9) + click(t, .22, 2600, .008, .6) + click(t, .29, 2100, .006, .4)
    thud = norm(band(noise(sec), 200, 700)) * np.exp(-np.clip(t - .22, 0, None) / .03) * (t >= .22) * .6
    return save('mag_drop.wav', slide + hit + thud, -2.0)


def mag_seat():
    """A magazine (or shells) seating: a firm click-clack."""
    sec = .25
    t = t_of(sec)
    x = click(t, 0, 2200, .006, 1) + norm(band(noise(sec), 800, 3000)) * np.exp(-t / .008) * .6
    x += click(t, .09, 3000, .005, .8) + click(t, .09, 1400, .01, .5)
    return save('mag_seat.wav', x, -1.0)


def holster():
    """Leather and steel: a draw or a holster."""
    sec = .22
    t = t_of(sec)
    rub = norm(band(noise(sec), 300, 2500, 2)) * np.sin(np.pi * np.clip(t / .18, 0, 1)) ** 2 * .7
    x = rub + click(t, .15, 2500, .006, .5)
    return save('holster.wav', x, -3.0)


def claw_snikt():
    """Three blades springing out: a fast metal scrape up into a ringing lock."""
    sec = .4
    t = t_of(sec)
    k = np.clip(t / .07, 0, 1)
    scrape = norm(band(noise(sec), 2500, 9000)) * (k * np.exp(-np.clip(t - .07, 0, None) / .02))
    ring = sum(a * np.sin(2 * np.pi * f * t) for f, a in ((2380, 1), (3710, .6), (5230, .35)))
    ring = ring * np.exp(-np.clip(t - .07, 0, None) / .09) * (t >= .07)
    return save('claw_snikt.wav', .8 * scrape + .7 * ring, -1.0)


def heat_vision():
    """A searing hum: a low buzz with a crackling, sizzling top."""
    sec = .6
    t = t_of(sec)
    buzz = np.sign(np.sin(2 * np.pi * 95 * t)) * .3 + np.sin(2 * np.pi * 190 * t) * .4
    sizzle = norm(band(noise(sec), 3000, 9000)) * (.4 + .3 * np.sin(2 * np.pi * 13 * t))
    env = np.minimum(1, t / .03)
    return save('heat_vision.wav', low(buzz, 1200, 2) * env + .5 * sizzle * env, -3.0)


def freeze_breath():
    """A cold rushing blast: breathy noise swelling, glassy ice crackles on top."""
    sec = .7
    t = t_of(sec)
    rush = norm(band(noise(sec), 500, 5000, 2)) * np.sin(np.pi * np.clip(t / sec, 0, 1)) ** .7
    crackle = np.zeros_like(t)
    for at in rng.uniform(.05, .6, 18):
        crackle += click(t, at, rng.uniform(5000, 9000), .002, rng.uniform(.2, .5))
    return save('freeze_breath.wav', rush + crackle, -3.0)


rng_cap = np.random.default_rng(7008)  # CAPTAIN AMERICA's own stream (see the module docstring)


def cnoise(sec):
    return rng_cap.standard_normal(int(SR * sec))


def ring(t, at, partials, decay):
    tt = np.clip(t - at, 0, None)
    return (t >= at) * sum(a * np.sin(2 * np.pi * f * tt) * np.exp(-tt / (decay * d)) for f, a, d in partials)


def shield_throw():
    """The shield leaving the hand: a swelling whoosh, chopped by the flat spin (about 10 turns a
    second, two edges a turn: a 20 Hz flutter) over a low gyroscopic hum."""
    sec = .6
    t = t_of(sec)
    swell = np.sin(np.pi * np.clip(t / sec, 0, 1)) ** 1.5
    whoosh = norm(band(cnoise(sec), 400, 3200, 2)) * swell
    flutter = .55 + .45 * np.sin(2 * np.pi * 20 * t) ** 2
    hum = np.sin(2 * np.pi * np.cumsum(140 - 30 * t / sec) / SR) * swell * .35
    return save('shield_throw.wav', whoosh * flutter + hum, -2.0)


def shield_ricochet():
    """The shield striking a body and leaving for the next: a bright inharmonic strike (a thick metal
    disc) and the whine of it spinning away, the pitch falling (Doppler)."""
    sec = .7
    t = t_of(sec)
    strike = ring(t, 0, ((1180, 1, 1), (2650, .7, .7), (4130, .45, .5), (5870, .3, .35)), .09)
    hit = norm(band(cnoise(sec), 150, 900)) * np.exp(-t / .015) * .9
    f = 2600 - 1500 * np.clip((t - .05) / .5, 0, 1)
    whine = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-np.clip(t - .05, 0, None) / .2) * (t >= .05) * .35
    return save('shield_ricochet.wav', np.tanh(1.8 * (strike + hit + whine)), -1.0)


def shield_catch():
    """The shield back on the arm: a firm thump into the straps, a leather creak, a muted ring."""
    sec = .45
    t = t_of(sec)
    thump = np.sin(2 * np.pi * np.cumsum(95 - 40 * np.clip(t / .06, 0, 1)) / SR) * np.exp(-t / .045)
    creak = norm(band(cnoise(sec), 600, 2400, 2)) * np.exp(-np.clip(t - .03, 0, None) / .05) * (t >= .03) * .35
    mute = ring(t, .005, ((820, 1, 1), (1910, .5, .6)), .06) * .35
    return save('shield_catch.wav', thump + creak + mute, -2.0)


def shield_clang():
    """A round or a hit stopped by the shield: a hard ping and a long, shimmering ring (the shield
    takes no damage)."""
    sec = 1.1
    t = t_of(sec)
    ping = norm(high(cnoise(sec), 2500)) * np.exp(-t / .004)
    body = ring(t, 0, ((1520, 1, 1), (1534, .8, 1), (3310, .6, .7), (5020, .35, .5), (7240, .2, .35)), .32)
    return save('shield_clang.wav', np.tanh(1.5 * (ping * .8 + body)), -1.0)


CAP = [shield_throw, shield_ricochet, shield_catch, shield_clang]

# Version 0.0.72: heat_vision.wav and freeze_breath.wav are tools/sfx/make_loops_0072.py's seamless loops now
# (sfx::loop); the 0.0.7 one-shots above stay for the record but are no longer written.
ALL = [pistol_shot, shotgun_blast, dry_click, mag_drop, mag_seat, holster, claw_snikt] + CAP

if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if a != '--cap']
    if args:
        OUT = args[0]
    for make in (CAP if '--cap' in sys.argv else ALL):
        p = make()
        sr, d = wavfile.read(p)
        print(f'{os.path.relpath(p)}  {len(d) / sr:.3f} s  {sr} Hz')
