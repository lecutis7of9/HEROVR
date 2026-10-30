"""Start the game with a small desktop view for VR, and restore the display settings afterwards.

In immersive VR the game's own view must keep running: the engine does occlusion,
key-light shadow setup, and auto-exposure for it, and Spidy places it at the head
so that work fits the eyes. Its pixels are not needed. A small window keeps that
work and drops most of its rendering cost (a 3440 x 1440 desktop renders about a
fifth of the pixels of two 3072 x 3264 eyes). The game reads these values once at
startup. The previous values are saved in reports/ and written back once the game
has closed.

Every VR session also turns off five game settings, whatever its window (SESSION):
frame generation, vertical sync, ray-traced reflections, NVIDIA Reflex, and the game's
Windows.Gaming.Input, which under SteamVR takes the controllers away from Spidy's
XInput controller. It puts the game's Aspect Ratio at Auto. They are saved and
restored with the window's. So are the game's depth of field, its sharpening and its camera
effects (motion blur, film grain, chromatic aberration, vignette), which a session turns off
unless told to keep them (PICTURE).
"""
import argparse
import ctypes
import json
import pathlib
import sys
import winreg

ROOT = pathlib.Path(__file__).resolve().parents[1]
KEY = r"Software\Insomniac Games\Marvel's Spider-Man 2\Graphics"
INPUT_KEY = r"Software\Insomniac Games\Marvel's Spider-Man 2\Input"
BACKUP = ROOT/'reports'/'desktop-view-before-vr.json'
NAMES = ('Fullscreen', 'ExclusiveFullscreen', 'WindowMaximized', 'WindowLeft', 'WindowTop',
         'WindowWidth', 'WindowHeight', 'UserWindowWidth', 'UserWindowHeight')
# The window's rows and shape. VR's HUD panel draws the game's HUD at the window's size
# (src/native_hud.cpp), and the game sizes its HUD by the window's height: 1920 x 1080 gives the
# HUD half again the pixels of the 1720 x 720 (the desktop's shape) of October 9's fifth build,
# whose HUD a Quest 3 player still found soft, and its 16:9 layout, as on a console, puts the same
# HUD on a panel as wide with a third more height, so all of it shows a third larger. Next to two
# 3840 x 4080 eyes, the frame draws about 3% more pixels than at 1720 x 720. No more than three
# quarters of the desktop's height.
HEIGHT = 1080
ASPECT = 16/9
# Update 33 (-SmallerDesktopView): 1280 x 720, 44% of 1080's pixels. The game's own view still runs in
# full (occlusion, key-light shadow setup, auto-exposure; the SM2 port never rests it, see
# src/game_xr.cpp), only at fewer pixels: next to two 3072 x 3264 (3840 x 4080) eyes a frame draws 5% (3.5%) fewer
# pixels. The game reads the window once at startup, so it takes effect at the next launch. Its cost:
# the HUD panel, drawn at the window's size, is that much softer (as October 9's 720-row window).
SMALLER_HEIGHT = 720
# What every VR session changes, whatever its window: (key, values, create). A value the game never
# saved is created only where `create` is set, and deleted again afterwards.
# - Frame generation (DLSS or FSR) off: Spidy draws the eyes itself, and generated frames change how
#   the game presents its window (which the headset's game screen copies) on a queue of their own.
#   Every tested session had it off.
# - Vertical sync off: with it the game waits for the monitor before each new frame, so it runs at the
#   monitor's rate divided by a whole number (120, 60, 40, 30 a second on a 120 Hz display) whatever the
#   headset's: the October 10 session, started with it on, ran 36-45 ms frames. The headset's own frames
#   do not wait for the game's.
# - Windows.Gaming.Input off: in the Steam Link session of October 7 the game found two Steam virtual
#   gamepads (Valve 28de:11ff) there, then logged "XInput disabled, disconnecting controller" and never
#   read Spidy's XInput controller again (pad_reads 0): its menus did not move. At 0 the game skips
#   Windows.Gaming.Input altogether (Spider-Man.exe 1d14c89); XInput and DualSense (libScePad) stay.
# - Depth of field off (DOFQuality 0) and sharpening at its lowest (Sharpness 1, where the game's slider
#   stores its 0), since October 12: the game blurs by distance from its own camera's focus, which in the
#   headset is not where you look (the 3D game screen's cutscenes most of all), and its sharpening pass
#   rings and shimmers on edges at the eyes' resolution. `keep` in prepare_session leaves either as the
#   player has it (the launcher's "Game's depth of field" and "Game's sharpening").
# - Ray-traced reflections off (RTReflections 0), since October 13. The eyes cannot have them (the game
#   takes another way through its reflection pass, for which an eye's buffers have no targets), and the
#   game's own view, the only one that had them, rests in VR play (restGameView in src/stereo_probe.cpp).
#   With them on, the first frame that view drew again hung the graphics device: a menu, the map or a
#   cutscene ended the game with its display driver error (DXGI_ERROR_DEVICE_HUNG). In two players'
#   reports (October 11-13, an RTX 4070 Ti and an RTX 5080) every session ended on its first such frame,
#   three more players ended the error by turning the setting off, and none of the 30 game logs kept
#   here with it off has the error. With it off the eyes follow Screen Space Reflections. A game that
#   runs with it on all the same keeps its own view rendering (ray_traced_view in run_game_vr.py).
# - Aspect Ratio at Auto (ForcedAspectRatio 0), since October 13's third build. The value is the choice
#   in the game's Display settings, counted from Auto, and the option reads and writes nothing else
#   (Spider-Man.exe 1cc1e70 and 1cca6d0; AspectRatio beside it is a number of its own). On October 11
#   two players' games ended as VR started, and each had VR once the option was back at Auto, which a
#   third player had advised. One of the two named his choice, 16:9, and his report of that day has the
#   graphics device gone by the 30th eye picture, in a 1920 x 1080 window. It was not run here with a
#   fixed shape, so what the game does to an eye with one is not known.
# - NVIDIA Reflex off (ReflexMode 0, the choice counted from Off), since October 18. Reflex paces the
#   game by the frames it presents to the monitor, and in VR play Spidy holds back the presents beyond
#   the monitor's rate (native_gpu::holdPresent, since October 11) while the headset paces the eyes, so
#   it has nothing to pace. On October 16 a player's game (RTX 4070 SUPER, Reflex On + Boost) lost its
#   graphics device by the 19th eye picture in every session (DXGI_ERROR_ACCESS_DENIED), on Virtual
#   Desktop and SteamVR alike, on every preset; the one session played here with Reflex on (October 10,
#   Low Latency, without Boost) predates the held presents. Whether Reflex is the cause is not known.
# - Motion blur, film grain, chromatic aberration and vignette off (MotionBlurScale 0, FilmGrainStrength 0,
#   ChromaticAberration 0, Vignette 0), since Update 11. The names and their encoding are the player's own:
#   the October 22 report (game-vr-20261022-205656) has the game's registry beside its log's [Settings]
#   block, and MotionBlurScale 1000 / FilmGrainStrength 1000 are its "Motion Blur 1.000000" and "Film Grain
#   Strength 1.000000" (the game's sliders store a thousandth, as FOVCorrection 1000 is 1.0 and Sharpness
#   1 its 0.001), ChromaticAberration 1 and Vignette 1 its "On" (toggles store 0 for Off, as DOFQuality,
#   AmbientOcclusionQuality, RTShadows and VSync there). Grain is new noise in every eye image, blur smears
#   a turning head's view, and both, with the colour fringes and the darkened lens edge, are the game's
#   own camera effects, which a headset's lenses do not need. `keep` leaves them ('comfort').
PICTURE = dict(depth_of_field=('DOFQuality',), sharpening=('Sharpness',),
               comfort=('MotionBlurScale', 'FilmGrainStrength', 'ChromaticAberration', 'Vignette'))
# - SM2 VR (from SM2VR 0.0.27): Spider-Man 2's other ray tracing off too (RTShadows, RTAmbientOcclusion,
#   RTInteriors, RayReconstruction): the eyes' views have none of the game's ray-tracing data, and a session
#   with ray tracing on lost the graphics device (DXGI_ERROR_ACCESS_DENIED) as the first eye images were drawn.
SESSION = ((KEY, dict(DLSSG=0, FrameGen=0, VSync=0, DOFQuality=0, Sharpness=1, RTReflections=0,
                      RTShadows=0, RTAmbientOcclusion=0, RTInteriors=0, RayReconstruction=0,
                      ForcedAspectRatio=0, ReflexMode=0, MotionBlurScale=0, FilmGrainStrength=0,
                      ChromaticAberration=0, Vignette=0), False),
           (INPUT_KEY, dict(EnableWindowsGamingInput=0), True))
# - Version 0.0.62 (the player chooses, live.txt lens_flares = off_in_vr; the default leaves it): the game's
#   lens flares off for the session. Car and bridge headlights' flares showed through walls in the headset at
#   night: the game tests a flare's light against the monitor camera's depth, not each eye's (best lead, not
#   proven without the headset). LensFlares is the name in the game's registry (the October 25 report,
#   game-vr-20261025-130446: LensFlares = 1 beside its log's "Lens Flares: On", CONFIRMED); 0 for Off is by
#   the other toggles' encoding (CANDIDATE). Backed up and put back after the session as SESSION's are.
# - 0.0.73 AA GAME AA TAA (the default with -EngineTaa and live.txt image_style = spidyvr; the launcher's KEEP MY
#   GAME AA or --keep-game-aa leaves the player's): the game's Anti-Aliasing at TAA with no upscaler for the
#   session (AA 2, UpscaleMethod 0), the path SpidyVR's per-eye TAA runs on. Test 5 (game-vr-20261029-012428) ran
#   DLAA/DLSS Quality (AA 2, UpscaleMethod 2): DLSS biased 266 texture samplers negative for its 864 x 480 render
#   (the eyes share them at 100%, where that sparkles) and evaluated every frame for the monitor view only (SL
#   WATCH: viewport 0 alone; the eyes never pass through Streamline). Encoding CONFIRMED beside the game's log in
#   31 reports kept here: AA 2 + UpscaleMethod 0 is "Anti aliasing: TAA / Upscale method: None" (23 of them),
#   AA 2 or 3 + UpscaleMethod 2 is "DLAA / DLSS" (8). Read by the game at launch (restart); put back after.
# - 0.0.74 test 4 AA MODE CLEAN (the default; aa_mode = off leaves it): the game's screen shake off for the
#   session. ScreenShake is a name in the game's registry set (CONFIRMED: the string beside LensFlares,
#   ChromaticAberration and FilmGrainStrength in Spider-Man2.exe 2.810.0.0); 0 for Off by the other toggles'
#   encoding (CANDIDATE, as LensFlares'). Backed up and put back after the session as SESSION's are.
SESSION_OFF = dict(lens_flares=dict(LensFlares=0), game_aa=dict(AA=2, UpscaleMethod=0),
                   screen_shake=dict(ScreenShake=0))


def session_list(off=()):
    """SESSION, with the SESSION_OFF choices named in `off` added to the graphics key's."""
    extra = {name: value for choice in off for name, value in SESSION_OFF[choice].items()}
    return tuple((key, dict(wanted, **extra) if key == KEY else wanted, create) for key, wanted, create in SESSION)


class Registry:
    """The game's keys; only REG_DWORD values are read or written (by default NAMES in the graphics key)."""

    def read(self, names=NAMES, key=KEY):
        values = {}
        try:
            opened = winreg.OpenKey(winreg.HKEY_CURRENT_USER, key)
        except FileNotFoundError:
            return values
        with opened:
            for name in names:
                try:
                    value, kind = winreg.QueryValueEx(opened, name)
                except FileNotFoundError:
                    continue
                if kind == winreg.REG_DWORD:
                    values[name] = value
        return values

    def write(self, values, key=KEY):
        """None deletes a value: the session created it."""
        with winreg.CreateKeyEx(winreg.HKEY_CURRENT_USER, key, 0, winreg.KEY_SET_VALUE) as opened:
            for name, value in values.items():
                if value is None:
                    try:
                        winreg.DeleteValue(opened, name)
                    except FileNotFoundError:
                        pass
                else:
                    winreg.SetValueEx(opened, name, 0, winreg.REG_DWORD, value)


def desktop():
    """Primary display size in physical pixels."""
    user32 = ctypes.windll.user32
    user32.SetProcessDPIAware()
    return user32.GetSystemMetrics(0), user32.GetSystemMetrics(1)


def small_window(screen_width, screen_height, height=HEIGHT, aspect=ASPECT):
    """Centred 16:9 window `height` rows tall, within three quarters of the desktop's height and its width."""
    height = min(height, screen_height*3//4)
    width = round(height*aspect)
    if width > screen_width:
        width = screen_width
        height = round(width/aspect)
    return dict(Fullscreen=0, ExclusiveFullscreen=0, WindowMaximized=0,
                WindowLeft=(screen_width-width)//2, WindowTop=(screen_height-height)//2,
                WindowWidth=width, WindowHeight=height, UserWindowWidth=width, UserWindowHeight=height)


def pending(backup=BACKUP):
    return backup.exists()


def session_changes(backup=BACKUP):
    """The player's own values of the settings a session replaced (SESSION), by name, from the backup a
    launch left; empty without one. None: a value the session made."""
    if not backup.exists():
        return {}
    session = json.loads(backup.read_text()).get('session', [])
    return {name: value for entry in session for name, value in entry['values'].items()}


def _saved(values, names, created=False):
    """Values a backup may hold: known names with REG_DWORD numbers, and None for one the session created."""
    return isinstance(values, dict) and all(
        name in names and ((created and value is None) or (isinstance(value, int) and 0 <= value < 1 << 32))
        for name, value in values.items())


def restore(registry=None, backup=BACKUP):
    """Write back the values saved before VR. Returns False when nothing was saved."""
    if not backup.exists():
        return False
    saved = json.loads(backup.read_text())
    values = saved.get('values') if saved.get('key') == KEY else None
    # Backups before October 7's SteamVR fix hold the window's values only.
    session = saved.get('session', [])
    allowed = {key: (tuple(wanted), create) for key, wanted, create in session_list(tuple(SESSION_OFF))}
    known = isinstance(session, list) and all(
        isinstance(entry, dict) and entry.get('key') in allowed and
        _saved(entry.get('values'), *allowed[entry['key']]) for entry in session)
    if not _saved(values, NAMES) or not known:
        raise RuntimeError(f'Unrecognized display backup: {backup}')
    registry = registry or Registry()
    # What the session wrote (backups since October 22): a session setting the player changed in the game
    # while VR ran no longer holds that value, and stays as the player chose it.
    written = saved.get('written', {})
    if not isinstance(written, dict) or not all(key in allowed and _saved(ours, allowed[key][0])
                                                for key, ours in written.items()):
        raise RuntimeError(f'Unrecognized display backup: {backup}')
    # Only what differs now: where Windows refused the launch's changes, the user's values are still there,
    # and writing them again would be refused too.
    for key, wanted in [(KEY, values)]+[(entry['key'], entry['values']) for entry in session]:
        current = registry.read(tuple(wanted), key)
        ours = written.get(key, {})
        changed = {name: value for name, value in wanted.items() if current.get(name) != value
                   and not (name in ours and name in current and current[name] != ours[name])}
        if changed:
            registry.write(changed, key)
    backup.unlink()
    return True


def session_written(backup=BACKUP):
    """What the launch wrote for VR, by name (empty without a backup)."""
    if not backup.exists():
        return {}
    return {n: v for ours in (json.loads(backup.read_text()).get('written') or {}).values() for n, v in ours.items()}


def kept_in_game(registry=None, backup=BACKUP):
    """Session settings the player changed in the game since the launch wrote them: {name: value now}.
    restore leaves these as they are."""
    if not backup.exists():
        return {}
    saved = json.loads(backup.read_text())
    registry = registry or Registry()
    kept = {}
    for key, ours in (saved.get('written') or {}).items():
        current = registry.read(tuple(ours), key)
        kept.update({name: current[name] for name in ours if name in current and current[name] != ours[name]})
    return kept


def snapshot(registry=None, keys=(KEY, INPUT_KEY)):
    """Every REG_DWORD value of the game's keys, read only, for the session report: which names this
    game saves and how, beside the [Settings] block of its log (session_comfort.game_settings)."""
    if registry is not None:
        return {key: registry.read_all(key) for key in keys}
    values = {}
    for key in keys:
        found = {}
        try:
            opened = winreg.OpenKey(winreg.HKEY_CURRENT_USER, key)
        except FileNotFoundError:
            values[key] = found
            continue
        with opened:
            index = 0
            while True:
                try:
                    name, value, kind = winreg.EnumValue(opened, index)
                except OSError:
                    break
                index += 1
                if kind == winreg.REG_DWORD:
                    found[name] = value
        values[key] = found
    return values


GAME_KEY = r"Software\Insomniac Games\Marvel's Spider-Man 2"
# The game's own UI > HUD options, by the names in its exe (Update 38.5 research). They are not in the exe's
# registry name table (the Graphics block lists picture and window names only), so the game most likely keeps
# them with its profile. We never write them: this only looks, so a report can confirm where they live.
GAME_HUD_NAMES = ('UIHideHUDEnabled', 'UIMinimapHUDEnabled', 'UICrimeHUDEnabled', 'UITrickHUDEnabled',
                  'UIAppActivityHUDEnabled', 'HUDBackgroundEnabled', 'UIHUDParallaxEnabled',
                  'UIControlHintsEnabled')


def hud_option_probe(registry=None, root=GAME_KEY, names=GAME_HUD_NAMES):
    """Read only: {subkey: {name: value}} for any game HUD option found in the registry under the game's
    key and its subkeys. Empty when the game keeps them elsewhere (expected)."""
    found = {}
    if registry is not None:
        for key in registry.keys_under(root):
            hit = {n: v for n, v in registry.read_all(key).items() if n in names}
            if hit:
                found[key] = hit
        return found
    pending_keys = [root]
    while pending_keys:
        key = pending_keys.pop()
        try:
            opened = winreg.OpenKey(winreg.HKEY_CURRENT_USER, key)
        except OSError:
            continue
        with opened:
            index = 0
            while True:
                try:
                    pending_keys.append(key + '\\' + winreg.EnumKey(opened, index))
                except OSError:
                    break
                index += 1
            index = 0
            hit = {}
            while True:
                try:
                    name, value, kind = winreg.EnumValue(opened, index)
                except OSError:
                    break
                index += 1
                if name in names:
                    hit[name] = value
            if hit:
                found[key] = hit
    return found


def shrink(registry=None, backup=BACKUP, screen=None, height=HEIGHT):
    """Save the current values (once) and select the small window. Returns the values written."""
    registry = registry or Registry()
    current = registry.read()
    if not current:
        return None  # the game has not saved display settings yet
    if not backup.exists():
        backup.parent.mkdir(parents=True, exist_ok=True)
        backup.write_text(json.dumps(dict(key=KEY, values=current), indent=2)+'\n')
    wanted = {name: value for name, value in small_window(*(screen or desktop()), height=height).items() if name in current}
    registry.write(wanted)
    return wanted


def prepare_session(registry=None, backup=BACKUP, keep=(), off=()):
    """Turn off what SESSION lists, adding the user's values to the backup (made here if the window
    keeps the user's settings). keep: PICTURE names ('depth_of_field', 'sharpening', 'comfort') to leave as the
    player has them; off: SESSION_OFF names ('lens_flares', 'game_aa') turned off (or set) too. Returns the values written."""
    registry = registry or Registry()
    session = session_list(off)
    kept = {value for name in keep for value in PICTURE[name]}
    saved = json.loads(backup.read_text()) if backup.exists() else dict(key=KEY, values={})
    if 'session' not in saved:
        saved['session'] = []
        for key, wanted, create in session:
            current = registry.read(tuple(wanted), key)
            saved['session'].append(dict(key=key, values={name: current.get(name) for name in wanted
                                                          if (name in current or create) and name not in kept}))
        backup.parent.mkdir(parents=True, exist_ok=True)
        backup.write_text(json.dumps(saved, indent=2)+'\n')
    before = {entry['key']: entry['values'] for entry in saved['session']}
    written = {}
    by_key = {}
    for key, wanted, _ in session:
        values = {name: value for name, value in wanted.items() if name in before.get(key, {})}
        if values:
            by_key[key] = values
            written.update(values)
    if saved.get('written') != by_key:
        saved['written'] = by_key
        backup.parent.mkdir(parents=True, exist_ok=True)
        backup.write_text(json.dumps(saved, indent=2)+'\n')
    for key, values in by_key.items():
        registry.write(values, key)
    return written


def prepare_launch(small=True, registry=None, backup=BACKUP, screen=None, keep=(), height=HEIGHT, off=()):
    """Called with the game closed, just before it starts. A previous session's values are restored
    first, so a backup always holds the user's own settings. keep: as prepare_session's. Returns the
    window's values written (None when the window keeps the user's settings).

    Raises OSError when a settings change or its backup fails. Any changes made during this attempt
    are restored where possible; if restoration also fails, the backup stays for a later retry."""
    registry = registry or Registry()
    restore(registry, backup)
    try:
        window = shrink(registry, backup, screen, height) if small else None
        prepare_session(registry, backup, keep, off)
    except OSError:
        try:
            restore(registry, backup)
        except OSError:
            pass  # the backup stays; it is restored after the game closes, or at the next launch
        raise
    return window


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--restore', action='store_true', help='write back the display values saved before VR')
    a = p.parse_args()
    if not a.restore:
        print('Restore pending.' if pending() else 'No display values are waiting to be restored.')
        return 0
    from capture_game_state import find_game
    try:
        find_game()
        print('Close Spider-Man first; it may save its window settings when it exits.', file=sys.stderr)
        return 1
    except RuntimeError as error:
        if not str(error).startswith('Spider-Man 2 is not running.'):
            raise
    print('Restored your display settings.' if restore() else 'No display values are waiting to be restored.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
