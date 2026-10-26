# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""What a game VR session's report says about comfort: the frame rate while immersive and the game's own
settings that blur, smear or shake the eyes' picture or cost the most frame rate. No Windows calls, so the
tests run anywhere."""
import re


def play_rate(samples, window=2.0):
    """New stereo pairs a second while immersive, against the headset's frames a second: medians over
    `window`-second stretches of consecutive immersive samples (single samples are 0.05 s apart and too coarse).
    Reused pairs fill the headset frames between new ones; an uneven 2-3 frame cadence is felt as judder."""
    windows, start, last = [], None, None
    for s in samples:
        if s.get('presentation') != 'immersive' or s.get('status') != 3 or 'unique_submitted' not in s:
            start = last = None
            continue
        if start is None:
            start = last = s
            continue
        last = s
        seconds = s['seconds']-start['seconds']
        if seconds >= window:
            windows.append(((s['unique_submitted']-start['unique_submitted'])/seconds,
                            (s['frames']-start['frames'])/seconds))
            start = s
    if not windows:
        return None
    new, headset = sorted(w[0] for w in windows), sorted(w[1] for w in windows)
    median = lambda v: v[len(v)//2]
    result = dict(windows=len(windows), seconds=round(len(windows)*window, 1), new_pairs_fps=round(median(new), 1),
                  headset_fps=round(median(headset), 1), lowest_new_pairs_fps=round(new[len(new)//10], 1))
    result['headset_frames_per_pair'] = round(result['headset_fps']/result['new_pairs_fps'], 2) \
        if result['new_pairs_fps'] else None
    return result


GAME_SETTING = re.compile(r'\[Settings\]\s+([A-Za-z][A-Za-z0-9 \-]*?): (.*?)\s*$')


def game_settings(log):
    """The game's own graphics settings from its log's [Settings] block (the newest one), by the names the
    game writes there, e.g. 'Motion Blur': '1.000000'."""
    found, inside = {}, False
    for line in log.splitlines():
        if 'Display + Graphics Settings' in line:
            inside = not inside
            if inside:
                found = {}
        elif inside and (m := GAME_SETTING.search(line)) and m[2]:
            found[m[1].strip()] = m[2]
    return found


# Game settings that blur, smear or shake the eyes' picture (comfort), and those that cost the most frame
# rate, with what to set them to: (name in the game's log, values that call for advice, advice).
COMFORT_SETTINGS = (
    ('Motion Blur', lambda v: _number(v) > 0, 'set it to 0: it smears the picture as your head and Spider-Man '
     'move'),
    ('Film Grain Strength', lambda v: _number(v) > 0, 'set it to 0: grain shimmers differently in each eye'),
    ('Chromatic Aberration', lambda v: v == 'On', 'Off: colour fringes at the edges'),
    ('Vignette', lambda v: v == 'On', 'Off: darkened edges in the lenses'),
    ('Screen Shake', lambda v: v == 'On', 'Off: shakes your view without your head moving'),
    ('Lens Flares', lambda v: v == 'On', 'Off: flares drawn as on a camera lens'),
    ('Ambient Occlusion', lambda v: v not in ('Off', ''), 'try Off: if shadows still seem to slide over '
     'buildings as you turn your head, this screen-space shading is the likely cause; it also costs frame rate'),
)
SPEED_SETTINGS = (
    ('Shadow Quality', ('Ultra', 'Very High', 'VeryHigh'), 'High (each eye renders its own shadows)'),
    ('Screen Space Reflections', ('High', 'Very High', 'VeryHigh', 'Ultra'), 'Low or Off'),
    ('Level of Detail', ('Ultra', 'Very High', 'VeryHigh'), 'High'),
    ('Crowd Detail', ('Ultra', 'Very High', 'VeryHigh'), 'High or Medium'),
    ('Hair Quality', ('Ultra', 'Very High', 'VeryHigh'), 'High'),
)


def _number(text):
    try:
        return float(text)
    except (TypeError, ValueError):
        return 0.0


# Version 0.0.6 (Update 39.1): the one RenderScale measurement there is (tools/launch-game-vr.ps1: an RTX 4080
# at 90 Hz gave 45 new pictures a second at 100% and 33 at 125%). With frame time = fixed + per-pixel and the
# pixels growing with the square of the scale, 1/45 s = a + b and 1/33 s = a + 1.5625 b: about 65% of a frame
# scales with the pixels (b), 35% does not (a). Every gain derived from it is an ESTIMATE (one GPU, one scene).
PIXEL_SHARE = (1/33 - 1/45) / 0.5625 / (1/45)


def estimated_fps(fps, scale_from, scale_to):
    """An ESTIMATE of the new pictures a second at RenderScale `scale_to` from `fps` at `scale_from`."""
    if not fps or not scale_from or not scale_to:
        return None
    frame = lambda scale: (1 - PIXEL_SHARE) + PIXEL_SHARE * (scale / 100) ** 2
    return round(fps * frame(scale_from) / frame(scale_to), 1)


def performance_advice(rate, render_scale=None, timing=None):
    """Version 0.0.6: what would raise or even out the frame rate, from the session's own numbers. Never
    changes a setting; the gains are estimates and say so."""
    lines = []
    if not rate or not rate.get('headset_fps') or not rate.get('new_pairs_fps'):
        return lines
    hz, fps, scale = rate['headset_fps'], rate['new_pairs_fps'], render_scale or 100
    copy = (timing or {}).get('copy_gpu') or {}
    work = ((timing or {}).get('sharpen') or {}).get('gpu_ms_mean') or 0
    if copy.get('count') and copy.get('mean_ms', 0) - work >= 3 and fps < hz * 0.95:
        lines.append(f"The graphics card is the limit: the headset's copy waited {copy['mean_ms'] - work:.1f} ms "
                     "on average behind the game's frame on the GPU (its own work is "
                     f"{work:.2f} ms). Fewer pixels or lighter game settings raise the frame rate; the CPU is not "
                     'what holds it.')
    # A lower refresh where the game's rate divides it evenly: each picture then stays exactly 2 frames.
    for lower in (90, 80, 72):
        if lower < hz * 0.97 and lower / 2 * 0.95 <= fps < hz / 2 * 0.95:
            lines.append(f'At {hz:.0f} Hz each of your {fps:.0f} pictures a second stays 2 or 3 headset frames '
                         f'unevenly (judder). Set the headset (Virtual Desktop/Quest Link/SteamVR) to {lower} Hz: '
                         f'the game already makes the {lower // 2} a second that needs, so each picture stays '
                         'exactly 2 frames. Nothing in the game changes.')
            break
    if fps < hz / 2 * 0.95:
        for to in (90, 80):
            if to < scale:
                est = estimated_fps(fps, scale, to)
                lines.append(f'-RenderScale {to} (now {scale}): about {est:.0f} new pictures a second (ESTIMATE, '
                             f'+{(est / fps - 1) * 100:.0f}%, from one RTX 4080 measurement), slightly softer.')
    return lines


def comfort_advice(rate, settings, render_scale=None, timing=None):
    """Console lines about the session's play frame rate and the game settings to change for comfort."""
    lines = []
    if rate:
        lines.append(f"While immersive the game made {rate['new_pairs_fps']} new pictures a second "
                     f"(slowest tenth: {rate['lowest_new_pairs_fps']}) for a headset showing {rate['headset_fps']}: "
                     f"each picture stayed for {rate['headset_frames_per_pair']} headset frames.")
        if rate['headset_fps'] and rate['new_pairs_fps'] < rate['headset_fps']/2*0.95:
            if render_scale and render_scale > 100:
                lines.append(f'The game could not keep up: -RenderScale {render_scale} renders both eyes larger '
                             'than the headset asks for. Try -RenderScale 100 (or 80) for smoother motion.')
            else:
                lines.append('The game kept up with less than half the headset rate, so each picture stays for 2 '
                             'or 3 headset frames unevenly (judder). Lower the settings below, -RenderScale '
                             '(e.g. 80), or the headset refresh rate (72 Hz).')
        lines += performance_advice(rate, render_scale, timing)
    copy = (timing or {}).get('pair_to_copy')
    if copy and copy.get('count'):
        lines.append(f"From the game's eye pictures to the headset's copy of them: {copy['mean_ms']:.0f} ms on "
                     f"average, {copy['max_ms']:.0f} ms at most.")
    advice = [f'{name} ({settings[name]}): {text}' for name, wants, text in COMFORT_SETTINGS
              if name in settings and wants(settings[name])]
    if rate and rate['headset_fps'] and rate['new_pairs_fps'] < rate['headset_fps']*0.6:
        advice += [f'{name} ({settings[name]}): {text}, for frame rate' for name, values, text in SPEED_SETTINGS
                   if settings.get(name) in values]
    if advice:
        lines.append("For comfort, change these in the game's Graphics/Display settings (names as the game's log "
                     'has them):')
        lines += ['  '+line for line in advice]
    return lines
