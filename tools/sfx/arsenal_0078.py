"""HERO VR 0.0.78 PUNISHER ARSENAL sounds, made from the player's own recordings already in assets/sfx:

  suppressed_shot  pistol_shot_user.wav: band-limited (the can's thump, no crack), its tail cut to 0.9 s
  rifle_shot       pistol_shot_user.wav's crack over shotgun_blast_user.wav's body, cut to 1.2 s
  pump_rack        pistol_reload.wav's two slide clacks (0.40 s - 1.10 s): back, then home

Each is written at 48 kHz stereo 16-bit, its leading silence trimmed (the transient 1 ms in), at -20 LUFS max
momentary loudness (ITU-R BS.1770 K-weighting, 400 ms windows) with the sample peak capped at -1 dBFS.
Run from the repository root: python3 tools/sfx/arsenal_0078.py (numpy + scipy).
"""
import math
import os
import sys

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, lfilter, resample_poly, sosfilt

RATE = 48000
TARGET_LUFS = -20.0
PEAK_CAP_DB = -1.0
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
SFX = os.path.join(ROOT, "assets", "sfx")


def load(name):
    rate, data = wavfile.read(os.path.join(SFX, name + ".wav"))
    data = data.astype(np.float64) / 32768.0
    if data.ndim == 1:
        data = np.stack([data, data], axis=1)
    if rate != RATE:
        g = math.gcd(rate, RATE)
        data = resample_poly(data, RATE // g, rate // g, axis=0)
    return data


def k_weight(x, rate=RATE):
    # BS.1770 pre-filter (high shelf) and RLB high-pass, designed for the rate (pyloudnorm's formulas).
    def shelf(G, Q, fc):
        A = 10 ** (G / 40.0)
        w0 = 2 * math.pi * fc / rate
        alpha = math.sin(w0) / (2 * Q)
        b = [A * ((A + 1) + (A - 1) * math.cos(w0) + 2 * math.sqrt(A) * alpha),
             -2 * A * ((A - 1) + (A + 1) * math.cos(w0)),
             A * ((A + 1) + (A - 1) * math.cos(w0) - 2 * math.sqrt(A) * alpha)]
        a = [(A + 1) - (A - 1) * math.cos(w0) + 2 * math.sqrt(A) * alpha,
             2 * ((A - 1) - (A + 1) * math.cos(w0)),
             (A + 1) - (A - 1) * math.cos(w0) - 2 * math.sqrt(A) * alpha]
        return np.array(b) / a[0], np.array(a) / a[0]

    def highpass(Q, fc):
        w0 = 2 * math.pi * fc / rate
        alpha = math.sin(w0) / (2 * Q)
        b = [(1 + math.cos(w0)) / 2, -(1 + math.cos(w0)), (1 + math.cos(w0)) / 2]
        a = [1 + alpha, -2 * math.cos(w0), 1 - alpha]
        return np.array(b) / a[0], np.array(a) / a[0]

    b, a = shelf(4.0, 1 / math.sqrt(2), 1500.0)
    y = lfilter(b, a, x, axis=0)
    b, a = highpass(0.5, 38.0)
    return lfilter(b, a, y, axis=0)


def max_momentary(x, rate=RATE):
    y = k_weight(x, rate)
    win, hop = int(0.4 * rate), int(0.1 * rate)
    if len(y) < win:
        y = np.concatenate([y, np.zeros((win - len(y), y.shape[1]))])
    best = -200.0
    for s in range(0, len(y) - win + 1, hop):
        z = np.sum(np.mean(y[s:s + win] ** 2, axis=0))
        if z > 0:
            best = max(best, -0.691 + 10 * math.log10(z))
    return best


def trim(x, threshold_db=-40.0, lead_ms=1.0):
    level = np.max(np.abs(x), axis=1)
    hit = np.nonzero(level > 10 ** (threshold_db / 20.0))[0]
    start = max(0, int(hit[0]) - int(lead_ms * RATE / 1000)) if len(hit) else 0
    return x[start:]


def fade_out(x, seconds):
    n = min(len(x), int(seconds * RATE))
    x = x.copy()
    x[len(x) - n:] *= np.linspace(1, 0, n)[:, None] ** 2
    return x


def level(x):
    x = trim(x)
    gain_db = TARGET_LUFS - max_momentary(x)
    peak = np.max(np.abs(x))
    gain_db = min(gain_db, PEAK_CAP_DB - 20 * math.log10(peak))
    return x * 10 ** (gain_db / 20.0)


def suppressed_shot():
    x = load("pistol_shot_user")
    x = trim(x)[: int(0.9 * RATE)]
    sos = butter(4, [90, 1900], btype="band", fs=RATE, output="sos")
    thump = sosfilt(sos, x, axis=0)
    click = sosfilt(butter(2, 3500, btype="high", fs=RATE, output="sos"), x[: int(0.012 * RATE)], axis=0)
    thump[: len(click)] += 0.25 * click
    return fade_out(thump, 0.5)


def rifle_shot():
    crack = trim(load("pistol_shot_user"))[: int(1.2 * RATE)]
    body = trim(load("shotgun_blast_user"))[: int(1.2 * RATE)]
    body = sosfilt(butter(2, 2500, btype="low", fs=RATE, output="sos"), body, axis=0)
    n = min(len(crack), len(body))
    crack = crack[:n] * np.exp(-np.arange(n) / (0.15 * RATE))[:, None]
    return fade_out(0.6 * crack + 0.5 * body[:n], 0.6)


def pump_rack():
    x = load("pistol_reload")
    return fade_out(x[int(0.40 * RATE): int(1.10 * RATE)], 0.08)


def write(name, x):
    x = level(x)
    pcm = np.clip(np.round(x * 32767), -32768, 32767).astype(np.int16)
    wavfile.write(os.path.join(SFX, name + ".wav"), RATE, pcm)
    peak = 20 * math.log10(np.max(np.abs(pcm)) / 32768.0)
    print(f"{name}: {len(pcm) / RATE:.3f} s, {max_momentary(pcm / 32768.0):.1f} LUFS max momentary, peak {peak:.1f} dBFS")


def main():
    write("suppressed_shot", suppressed_shot())
    write("rifle_shot", rifle_shot())
    write("pump_rack", pump_rack())
    return 0


if __name__ == "__main__":
    sys.exit(main())
