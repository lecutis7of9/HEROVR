#!/usr/bin/env python3
"""Version 0.0.62: the player's own repulsor sound (assets/sfx/source/user-repulsor.mp3, 1.59 s,
44.1 kHz stereo, sent by the player) as the hand blast (take "user", CMakeLists SPIDY_REPULSOR_TAKE),
and the chest beam made from it.

  repulsor_mcu_user.wav         the whole sound, 48 kHz mono 16-bit (the build-up, then the blast)
  repulsor_mcu_user_charge.wav  the last 0.12 s of its build-up (plays on the pull, the chord window)
  repulsor_mcu_user_fire.wav    its blast, from the onset (0.505 s) to the end
  chest_beam.wav                the blast pitched down 5 semitones, a sub boom under it, a longer
                                tail, light tanh saturation, louder; 1.70 s (0.0.5's length)

Loudness: each is peak-normalised to -1 dBFS (0.891, as every make_sfx.py sound) after a 30 Hz
high-pass; the chest beam's peak is 0.944 (-0.5 dBFS, as sonic_boom) with a soft limiter - nothing
clips. Needs ffmpeg (decode only) and numpy/scipy. Run: python3 tools/sfx/user_repulsor.py
"""
import os
import subprocess
import sys
import tempfile

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, resample_poly, sosfilt

HERE = os.path.dirname(os.path.abspath(__file__))
SFX = os.path.normpath(os.path.join(HERE, '..', '..', 'assets', 'sfx'))
SRC = os.path.join(SFX, 'source', 'user-repulsor.mp3')
SR = 48000
ONSET = .505        # where the blast starts in the player's file (measured: 0.51 s first peak over 0.5)
CHARGE = .12        # repulsor.hpp chordWindow, make_sfx.py BLAST_AT
PEAK = .891         # -1 dBFS
CHEST_PEAK = .944   # -0.5 dBFS
CHEST_SECONDS = 1.70


def decode(path):
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, 'raw.wav')
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', path, '-ar', str(SR), '-c:a', 'pcm_f32le', out], check=True)
        sr, x = wavfile.read(out)
    x = x.astype(np.float64)
    return x.mean(axis=1) if x.ndim == 2 else x  # L and R correlate 0.95: a plain sum loses nothing


def high(x, hz, order=2):
    return sosfilt(butter(order, hz, 'highpass', fs=SR, output='sos'), x)


def low(x, hz, order=2):
    return sosfilt(butter(order, hz, 'lowpass', fs=SR, output='sos'), x)


def fade(x, inn, out):
    x = x.copy()
    a, b = int(SR * inn), int(SR * out)
    if a:
        x[:a] *= np.linspace(0, 1, a)
    if b:
        x[-b:] *= np.linspace(1, 0, b)
    return x


def save(name, x, peak):
    x = x / (np.max(np.abs(x)) + 1e-12) * peak
    pcm = np.round(x * 32767).clip(-32767, 32767).astype(np.int16)
    path = os.path.join(SFX, name)
    wavfile.write(path, SR, pcm)
    return path, pcm


def chest_from(blast):
    # 1 lower: 5 semitones down (resampled, so it is longer as well: a bigger, slower blast)
    pitched = resample_poly(blast, 4, 3)          # x1.333 longer = 2^(4.98/12): ~5 semitones down
    n = int(SR * CHEST_SECONDS)
    t = np.arange(n) / SR
    body = np.zeros(n)
    body[:min(n, len(pitched))] = pitched[:n]
    # 2 longer sustain: a low-passed, decaying copy smeared after it (a simple tail, no reverb plugin)
    tail = np.zeros(n)
    for k, (delay, g) in enumerate(((.045, .55), (.11, .42), (.19, .3), (.29, .2))):
        d = int(SR * delay)
        tail[d:] += g * body[:n - d]
    body = body + low(tail, 2500)
    # 3 a sub boom: 68 Hz falling to 38 Hz, from the onset, decaying over ~0.9 s
    f = 38 + 30 * np.exp(-t / .25)
    boom = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / .55) * np.minimum(1, t / .004)
    body = body / (np.max(np.abs(body)) + 1e-12) + .55 * boom
    body = high(body, 30)
    # 4 a little saturation, then louder with a soft limiter (tanh): no clipping
    body = np.tanh(1.8 * body / np.max(np.abs(body)))
    return fade(body, .0, .25)


def loudest(y):
    w = int(SR * .05)
    return 20 * np.log10(max(np.sqrt(np.mean(y[i:i + w] ** 2)) for i in range(0, max(1, len(y) - w), w // 2)) + 1e-12)


def matched_drive(blast, target):
    y = blast / np.max(np.abs(blast))
    for drive in np.arange(.2, 4.0, .02):
        z = np.tanh(drive * y)
        if loudest(z / np.max(np.abs(z)) * PEAK) >= target:
            return float(drive)
    return 4.0


def main():
    if not os.path.exists(SRC):
        sys.exit('missing ' + SRC)
    x = high(decode(SRC), 30)
    on = int(SR * ONSET)
    # Loudness match: the blast's loudest 50 ms to take C's (-6.9 dBFS, the other blasts -5 to -7),
    # by a gentle tanh soft limiter (drive found here) and the -1 dBFS peak: louder, never clipped.
    drive = matched_drive(fade(x[on:], .002, .03), -6.9)
    shape = lambda v: np.tanh(drive * v) / np.tanh(drive)
    whole = shape(fade(x, .002, .03) / np.max(np.abs(x)))
    g = PEAK / (np.max(np.abs(whole)) + 1e-12)   # one gain for the whole and its blast
    made = []
    for name, part in (('repulsor_mcu_user.wav', whole),
                       ('repulsor_mcu_user_fire.wav', whole[on:] * fade(np.ones(len(x) - on), .002, 0))):
        pcm = np.round(part * g * 32767).clip(-32767, 32767).astype(np.int16)
        path = os.path.join(SFX, name)
        wavfile.write(path, SR, pcm)
        made.append((path, pcm))
    # The charge (the build-up's last 0.12 s) is quiet in the file: brought up to a 0.336 peak
    # (11,000), near take C's charge (13,329), so it is heard on the pull and passes the SFX gate.
    made.append(save('repulsor_mcu_user_charge.wav', fade(x[on - int(SR * CHARGE):on], .005, .003), .336))
    made.append(save('chest_beam.wav', chest_from(x[on:]), CHEST_PEAK))
    for path, pcm in made:
        y = pcm / 32768.0
        w = int(SR * .05)
        loud = max(np.sqrt(np.mean(y[i:i + w] ** 2)) for i in range(0, max(1, len(y) - w), w // 2))
        print(f'{os.path.relpath(path)}  {len(pcm) / SR:.3f} s  peak {int(np.max(np.abs(pcm)))}  '
              f'loudest 50 ms {20 * np.log10(loud + 1e-12):.1f} dBFS')


if __name__ == '__main__':
    main()
