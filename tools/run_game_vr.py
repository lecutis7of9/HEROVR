"""Launch/attach native VR, with optional bounded tests and automatic eye resolution."""
import argparse
import ctypes as c
from ctypes import wintypes
import hashlib
import json
import math
import os
import pathlib
import re
import shutil
import struct
import subprocess
import sys
import time
import signal
import threading
import uuid
import zlib
import _thread
from collections import deque
from capture_game_state import Game, find_game, open_process, close
from bridge_game import (ROOT, HOOKS, prepare, snapshot as bridge_snapshot, GameNeedsRestart, other_build_text,
                         spidy_folder, staged)
from inspect_game import PE, EXPECTED_VERSION
import live_settings
import std_handles
import address_finder
import eye_lod
import shadow_camera
import shadow_distance
import far_shadows
import update37_records
import player_check
import mod_check  # 0.0.74 test 3
import gpu_check
import start_errors
from session_comfort import comfort_advice, game_settings, play_rate
from observe_game import call_remote, call_with_payload, modules
from probe_stereo_gpu import MAX_EYE_SIZE, discover_queue, snapshot as gpu_snapshot, save_eye_images
from probe_stereo import (frame_snapshot, render_memory_snapshot, snapshot as stereo_snapshot, encode_mip_bias,
                          mip_bias_snapshot, eye_camera_line)
from probe_native_rays import snapshot as ray_snapshot
from eye_shadows import snapshot as shadow_snapshot, line as eye_shadow_line
from probe_game_swing import snapshot as swing_snapshot
from probe_native_motion import snapshot as motion_snapshot
from vr_launcher import (LauncherLock, alive, bring_to_front, wait_for_game, enlarge_render_memory, elevated,
                         RENDER_MEMORY_HOOKS, NeedsAdministrator, NEEDS_ADMINISTRATOR_EXIT, CLOSE_GAME_EXIT,
                         close_game)
import vr_display
import xr_runtime

# Spider-Man2.exe 2.810.0.0 entries (from SM2VR 0.0.27), checked to be unpatched before VR starts.
GAME_HOOKS = (*HOOKS, 0x4b42ca0, 0x35184e0, 0x3517bb0, 0x12de950, 0x350e880,
              0x2c9c780, 0x2c96f80, 0x2c656d0, 0x2c32a70, 0x2d03240,
              0x2c99e30, 0x2c99ec0, 0x2c6cb70, 0x2b59660, 0x2d016d0, 0x7ca2f0, 0x2d01600,
              # The web shooter: the camera's update (shots go out on the main thread), the weapons' muzzle
              # and the shots' events (what they strike).
              0xa24e90, 0x373e260, 0xf55310,
              # Slow motion: the update of the game's time system (game_time.hpp).
              0x2db5e90,
              # The HUD in VR (native_hud.hpp): its panel's placement, its texture's stream, the render command
              # that draws its second movie, and the projections of its world markers.
              0x9e53d0, 0x2610e40, 0x223a280, 0x346e730, 0x346e7c0)
# The render scale's range in percent (spidy::minimumRenderScale and maximumRenderScale).
RENDER_SCALES = (50, 200)
# Upscaling: the percent of the eye's size, per side, the eyes' scene is rendered at, and its default
# (spidy::minimumEyeUpscale, maximumEyeUpscale and defaultEyeUpscale). SM2 port, Update 24: the default is
# 100, because Spider-Man 2's eyes have no temporal upscaler (SpidyEyeTemporal stores 0) and always render at
# full size; the old 67 changed nothing but looked like a 67% render in every log.
UPSCALES, DEFAULT_UPSCALE = (50, 100), 100
SUPERSAMPLES = (100, 200)  # native_eye_gpu.hpp minimumSupersample, maximumSupersample
# How often the game's window gets the left eye in immersive VR: presents a second, 0 for every frame
# (include/spidy/monitor_rate.hpp).
MONITOR_RATES, DEFAULT_MONITOR_RATE = (0, 240), 30  # Update 37.2: 30 again (as Update 36)
# What a session takes as a T-pose calibration, millimetres (body_calibration.hpp): the eye height and the arm
# length (from Spider-Man's shoulder at the player's size to the wrist) the body is sized to.
EYE_HEIGHTS_MM, ARM_LENGTHS_MM = (1000, 2500), (250, 1200)
# Eye snapshots kept per session (the newest), and the header fields saved with each.
EYE_SHOTS = 72
EYE_SHOT_FIELDS = ('count', 'generation', 'serial', 'width', 'height', 'flat_screen', 'speed_mps',
                   'web_hand_gap_m', 'game_web', 'wrist_px', 'rope_start_px')


GAME_NAME = "Marvel's Spider-Man 2"  # its folder in Documents, and its log's name there
GAME_MEMORY_FIELDS = {'Working set': 'working_set_mb', 'Page file': 'commit_mb', 'Video Usage': 'video_usage_mb',
                      'Video Budget': 'video_budget_mb', 'Tex Usage': 'texture_usage_mb',
                      'Tex Budget': 'texture_budget_mb', 'Demoted': 'demoted_mb', 'fps': 'fps'}


def flat_fallback_line(reason):
    """Update 24.1: the one console line said whenever VR ends on an error and the game goes back to its flat screen."""
    reason = (reason or '').strip() or 'no reason given'
    return f'VR STOPPED - the game is back on its flat screen. Reason: {reason}'


# 0.0.5 hotfix 3 (B3ARVR, Virtual Desktop): VR lost mid-game cannot start again inside the same game process, and
# the game must never be left on its flat screen without the player being told what to do.
VR_LOST_HELP = ('VR cannot come back inside this game process. Close Spider-Man 2, open Virtual Desktop in your '
                'headset (or start SteamVR / Quest Link), then press PLAY VR: the launcher offers to close the game '
                'for you.')
# How long PLAY VR waits for a headset before giving up; the game is never started without one.
HEADSET_WAIT = 300
HEADSET_RETRY = 3
HEADSET_MISSING = ('Headset not connected: open Virtual Desktop in your headset first (or start SteamVR / Quest '
                   'Link). Waiting for it - the game is not started without a headset; Stop VR cancels.')


def documents_folder():
    """The user's Documents folder, wherever OneDrive or the user has moved it."""
    known, path = (c.c_ubyte*16).from_buffer_copy(uuid.UUID('FDD39AD0-238F-46AF-ADB4-6C85480369C7').bytes_le), c.c_void_p()
    shell, ole = c.windll.shell32, c.windll.ole32
    shell.SHGetKnownFolderPath.argtypes = [c.c_void_p, wintypes.DWORD, wintypes.HANDLE, c.POINTER(c.c_void_p)]
    ole.CoTaskMemFree.argtypes = [c.c_void_p]
    if shell.SHGetKnownFolderPath(known, 0, None, c.byref(path)) or not path.value:
        return pathlib.Path.home()/'Documents'
    try:
        return pathlib.Path(c.wstring_at(path.value))
    finally:
        ole.CoTaskMemFree(path)


def game_memory(log):
    """The memory and frame rate line the game writes to its log about once a minute."""
    rows = []
    for line in log.splitlines():
        if '[Render] Working set:' not in line:
            continue
        found = dict(re.findall(r'(%s): ?([0-9.]+)' % '|'.join(GAME_MEMORY_FIELDS), line))
        if len(found) == len(GAME_MEMORY_FIELDS):
            rows.append(dict(time=line[:8], **{GAME_MEMORY_FIELDS[k]: float(v) for k, v in found.items()}))
    return rows


def keep_game_log(report, source=None):
    """Copy the game's log beside the report; the game overwrites it the next time it starts.

    Returns the copy's path and the game's memory lines, or (None, []) without a readable log.
    """
    source = source or documents_folder()/GAME_NAME/(GAME_NAME+'.log')
    try:
        log = source.read_text(encoding='utf-8', errors='replace')
        copy = report.with_name(report.stem+'-game.log')
        copy.write_text(log, encoding='utf-8')
    except OSError:
        return None, []
    return copy, game_memory(log)


class MemoryStatus(c.Structure):
    _fields_ = [('dwLength', wintypes.DWORD), ('dwMemoryLoad', wintypes.DWORD), ('ullTotalPhys', c.c_uint64),
                ('ullAvailPhys', c.c_uint64), ('ullTotalPageFile', c.c_uint64), ('ullAvailPageFile', c.c_uint64),
                ('ullTotalVirtual', c.c_uint64), ('ullAvailVirtual', c.c_uint64),
                ('ullAvailExtendedVirtual', c.c_uint64)]


def free_commit_mb():
    """Memory Windows can still promise to programs (RAM plus page file, minus what is committed)."""
    status = MemoryStatus(dwLength=c.sizeof(MemoryStatus))
    if not c.windll.kernel32.GlobalMemoryStatusEx(c.byref(status)):
        return None
    return status.ullAvailPageFile >> 20


class ProcessMemory(c.Structure):
    _fields_ = [('cb', wintypes.DWORD), ('PageFaultCount', wintypes.DWORD), ('PeakWorkingSetSize', c.c_size_t),
                ('WorkingSetSize', c.c_size_t), ('QuotaPeakPagedPoolUsage', c.c_size_t),
                ('QuotaPagedPoolUsage', c.c_size_t), ('QuotaPeakNonPagedPoolUsage', c.c_size_t),
                ('QuotaNonPagedPoolUsage', c.c_size_t), ('PagefileUsage', c.c_size_t),
                ('PeakPagefileUsage', c.c_size_t), ('PrivateUsage', c.c_size_t)]


def process_commit_mb(pid):
    """Memory Windows has promised a process (its private bytes), or None if it cannot be asked."""
    process = open_process(0x1000, False, pid)
    if not process:
        return None
    try:
        counters = ProcessMemory(cb=c.sizeof(ProcessMemory))
        query = c.windll.kernel32.K32GetProcessMemoryInfo
        query.argtypes = [wintypes.HANDLE, c.POINTER(ProcessMemory), wintypes.DWORD]
        return counters.PrivateUsage >> 20 if query(process, c.byref(counters), counters.cb) else None
    finally:
        close(process)


def scaled_eye_size(width, height, percent, limit=MAX_EYE_SIZE):
    """The eye size for `percent` of the runtime's recommended `width` x `height`, as spidy::scaledEyeSize
    computes it: the recommendation itself at 100, else in multiples of 8, shrunk with its shape to fit
    `limit` (the session's runtime may allow less)."""
    w, h = float(width), float(height)
    if percent != 100:
        w = max(64., math.floor(w*percent/800+.5)*8)
        h = max(64., math.floor(h*percent/800+.5)*8)
    fit = min(1., min(limit, MAX_EYE_SIZE)/w, min(limit, MAX_EYE_SIZE)/h)
    if fit < 1:
        w, h = math.floor(w*fit/8+1e-6)*8, math.floor(h*fit/8+1e-6)*8
    return int(w), int(h)


def eye_render_size(width, height, percent):
    """The size an eye's scene is rendered at for `percent` of a `width` x `height` eye, as
    spidy::eyeRenderSize computes it: in multiples of 8."""
    if percent >= 100:
        return width, height
    return max(64, (width*percent//100+7) & ~7), max(64, (height*percent//100+7) & ~7)


def public_version():
    """The public version (VERSION beside tools/, semver: "0.0.1"); None without the file."""
    try:
        return (ROOT/'VERSION').read_text(encoding='utf-8').strip() or None
    except OSError:
        return None


# Version 0.0.6: the on-screen credit (console, holo, overlay): this build's maker only.
MADE_BY = 'Made by Agent Mulder'


# HERO VR 0.0.72 test build 5: the installer (INSTALL-TEST-BUILD.ps1) writes this beside tools/ with the
# build's commit and every DLL's sha256, so the console says which build runs, from which folder, and
# whether its DLLs are the ones the build shipped (a mixed install is named, never guessed).
TEST_MARKER = 'HERO-VR-TEST-BUILD.txt'


def test_build_lines(root=None):
    """The console's first lines: the test build and its commit, the mod folder, a backup-copy warning,
    and MIXED INSTALL for a DLL that is not the build's. [] for a release install in a normal folder."""
    root = pathlib.Path(root or ROOT)
    lines = []
    try:
        text = (root/TEST_MARKER).read_text(encoding='utf-8')
    except OSError:
        text = None
    if text is not None:
        fields, hashes = {}, {}
        for line in text.splitlines():
            key, sep, value = line.partition('=')
            if not sep:
                continue
            key, value = key.strip(), value.strip()
            if key.startswith('sha256 '):
                hashes[key[7:].strip()] = value.lower()
            else:
                fields[key] = value
        # 0.0.73 test 5: the same words as the wrist and the mod's start line (HERO VR 0.0.73 TEST 5 <commit>)
        lines.append(f"HERO VR {fields.get('version', '0.0.72')} TEST {fields.get('build', '?')} {fields.get('commit', 'unknown')}"
                     f" - mod folder {root}")
        mixed = []
        for name, digest in sorted(hashes.items()):
            try:
                actual = hashlib.sha256((root/name).read_bytes()).hexdigest()
            except OSError:
                actual = None
            if actual != digest:
                mixed.append(name + (' missing' if actual is None else ' differs'))
        if mixed:
            lines.append('MIXED INSTALL: ' + ', '.join(mixed) + ' - run INSTALL-TEST-BUILD.bat from the test build again')
    if 'backup' in str(root).lower():
        lines.append(f'NOTE: this is a BACKUP copy of the mod ({root}). Changes made to your main folder do not run here.')
    return lines


def version_line():
    """The log's and the report's header: "Version 0.0.1 (build Update 38.5)"."""
    return f'Version {public_version() or "unknown (VERSION missing)"} (build {port_version() or "unknown"})'


def port_version():
    """The update these tools came with (PORT-VERSION beside tools/): the report says which one ran, so a
    session from files that were not replaced is told apart (the October 22 Update 7 report had Update 6's
    module hashes and report fields)."""
    try:
        return (ROOT/'PORT-VERSION').read_text(encoding='utf-8').strip() or None
    except OSError:
        return None


def rendering_line(width, height, percent, size, recommended, upscale=100, antialiased=True):
    """The console's line on the eye size the session renders, beside what the headset asked for, and on
    the size the eyes' scene is rendered at where that is smaller (upscaling)."""
    line = f'Rendering {width} x {height} pixels per eye'
    if size:
        line += ' (a square size instead of the headset\'s)'
    elif recommended:
        wanted = scaled_eye_size(*recommended, percent)
        of = f"{percent}% of the headset's {recommended[0]} x {recommended[1]}"
        line += f' ({of})' if (width, height) == wanted else \
            f' ({of} would be {wanted[0]} x {wanted[1]}, more than the VR runtime takes)'
    if not antialiased:
        return f'{line}, without anti-aliasing.'
    scene = eye_render_size(width, height, upscale)
    if scene == (width, height):
        return f'{line}, anti-aliased.'
    return f'{line}: the scene at {scene[0]} x {scene[1]} ({upscale}% per side), anti-aliased and upscaled.'


def eye_reflections(setting, refused=False, ray_traced=0):
    """Whether the eyes get the targets the game's screen-space reflections need, and the console's line.

    They follow the game's own setting (Graphics > Screen Space Reflections; `setting` is its saved value,
    0 for Off, None where the game never saved one) as it is when the session starts: an eye's buffers are
    made once. With it off, or with --no-eye-reflections (`refused`), the eyes are as before October 11's
    second build. So they are with ray-traced reflections on (`ray_traced`, the saved RTReflections): the
    game then takes another way through the pass, for which an eye's buffers lack the targets, and that
    was never run in the eyes."""
    if refused:
        return False, 'Reflections: none in the eyes (--no-eye-reflections).'
    if ray_traced:
        return False, ("Reflections: none in the eyes, as Ray-Traced Reflections is on in the game's graphics "
                       'settings and the eyes cannot have those. With it off and Screen Space Reflections on, '
                       'start VR again for reflections in the eyes.')
    if setting == 0:
        return False, ("Reflections: none in the eyes, as Screen Space Reflections is off in the game's "
                       'graphics settings. Turn it on there and start VR again to have them.')
    return True, ("Reflections: the game's screen-space reflections, in each eye (the game's Screen Space "
                  'Reflections setting, as it is when VR starts).')


# The console's line where a session turned the player's Ray-Traced Reflections off (vr_display.SESSION
# has why).
RAY_TRACING_OFF = ('Ray-Traced Reflections: off for VR. The eyes cannot have them, and with them on the game '
                   'ended with its display driver error (DXGI_ERROR_DEVICE_HUNG) as a menu, the map or a '
                   'cutscene opened. Your setting returns when the game closes.')


# The console's line where a session put the player's Aspect Ratio at Auto (vr_display.SESSION has why).
ASPECT_RATIO_AUTO = ("Aspect Ratio: Auto for VR. With a fixed shape chosen in the game's display settings, "
                     "players' games ended as VR started. Your setting returns when the game closes.")

# The console's line where a session turned the player's NVIDIA Reflex off (vr_display.SESSION has why).
REFLEX_OFF = ("NVIDIA Reflex: off for VR. The headset paces the game's frames, not the monitor, and one "
              "player's game with it on lost its graphics device as VR started. Your setting returns when the "
              'game closes.')


def device_removed(log):
    """The reason the game logged for losing its graphics device ('0x887A002B'), or None. Its own dialog
    says only "A problem has occurred with your display driver" with the same number."""
    found = re.search(r'Device removed detected \((0x[0-9A-Fa-f]{8})', log or '')
    return '0x'+found.group(1)[2:].upper() if found else None


def game_adapters(log):
    """The graphics adapters the game listed at its start, in its order. The October 16 player whose
    device was lost as VR began had his one RTX 4070 SUPER listed twice (adapters 0 and 1, NVIDIA's own
    list naming one), which no game log kept here has."""
    return re.findall(r'\[Render\]\s+Adapter \d+:\s*\n[^\n]*Description\s*:\s*([^\r\n]*?)\s*$', log or '', re.M)


# What the reasons the game logs are (DXGI's names).
DEVICE_REASONS = {'0x887A0005': 'DXGI_ERROR_DEVICE_REMOVED', '0x887A0006': 'DXGI_ERROR_DEVICE_HUNG',
                  '0x887A0007': 'DXGI_ERROR_DEVICE_RESET', '0x887A0020': 'DXGI_ERROR_DRIVER_INTERNAL_ERROR',
                  '0x887A002B': 'DXGI_ERROR_ACCESS_DENIED', '0x8007000E': 'E_OUTOFMEMORY'}


def device_lost_advice(reason, play_frames, clear=True):
    """The console's lines after a session in which the game lost its graphics device (the game shows
    "A problem has occurred with your display driver"). `play_frames`: frames of VR play before it (the
    report's final `tracked`). Ray-Traced Reflections, a fixed Aspect Ratio, frame generation and NVIDIA
    Reflex are off for VR already (vr_display.SESSION); what is left to try is what the player can change."""
    name = DEVICE_REASONS.get(reason, 'unknown')
    when = 'as VR play began' if play_frames is not None and play_frames < 300 else 'in VR'
    if clear and reason in start_errors.DEVICE_LOST:
        # 0.0.74 test 3 CLEARER ERRORS (-NoClearErrors keeps the words below): the fix steps, in order.
        return start_errors.device_lost_steps(reason, name, when)
    return (f'The game lost its graphics device {when} ({reason}, {name}): that is its "problem has occurred '
            'with your display driver" message. Try these one at a time, pressing START VR after each: '
            'OPTIONS > Normal camera on the monitor: On; in the game\'s Display settings, Monitor: your main '
            'display; the newest graphics driver. If it happens again, post the session report in #spidy-bugs '
            'on Discord (press Reports, zip the three newest game-vr- files).')

# Windows' own libraries, by name. A program loads a library from its own folder first, so a file with
# one of these names beside Spider-Man.exe is neither Windows' nor the game's, whose folder holds none of
# them: it is how another mod, an overlay or a trainer gets into the game, and it takes the calls the
# game meant for Windows. REAL_VR: what the R.E.A.L. VR mod keeps there beside its dxgi.dll. With that
# mod in the game a player's headset stayed black on SteamVR and on Virtual Desktop alike: in the six
# sessions of October 10-12 that reached VR, none of 1,270 to 4,899 frames had a picture for the
# headset, slow motion never found the game's time scale where it looks (9702), and once the eyes were
# on another graphics device than the game's (3502). His folder and the game's module list of October
# 13 showed the mod's dxgi.dll loaded.
WRAPPERS = ('dxgi.dll', 'd3d12.dll', 'd3d11.dll', 'dinput8.dll', 'version.dll', 'winmm.dll', 'winhttp.dll',
            'xinput1_3.dll', 'xinput1_4.dll', 'xinput9_1_0.dll', 'dsound.dll', 'opengl32.dll')
REAL_VR = ('realvr.ini', 'realvr64.log', 'realconfig.bat', 'realrepo')


def other_mods(folder):
    """What else loads into the game from its own `folder`, by the names the files have there: (the
    R.E.A.L. VR mod's files, any other library that stands in for one of Windows'). One of the two is
    empty: R.E.A.L. VR's own library is among the first."""
    try:
        names = {entry.name.lower(): entry.name for entry in pathlib.Path(folder).iterdir()}
    except OSError:
        return [], []
    wrappers = [names[name] for name in WRAPPERS if name in names]
    real = [names[name] for name in REAL_VR if name in names]
    return (wrappers+real, []) if wrappers and real else ([], wrappers)


def real_vr_refusal(folder, files):
    """The error that ends a session in a game the R.E.A.L. VR mod is in."""
    return (f"The R.E.A.L. VR mod is in the game's folder ({', '.join(files)}). It loads into the game with "
            "Spidy and takes the game's picture for itself: the headset stays black and the controllers do "
            f'nothing. Close the game, move those files out of {folder} (Verify integrity of game files in '
            'Steam leaves them there), then press START VR.')


def other_mods_warning(folder, files):
    """The console's line on any other library in the game's folder that loads into the game."""
    return (f"WARNING: another mod or tool loads into the game from its folder ({', '.join(files)}). Spidy "
            'has not been run beside it. If the headset stays black or the game closes, close the game, move '
            f"{'it' if len(files) == 1 else 'them'} out of {folder} and press START VR again.")


def ray_traced_view(ray_traced):
    """Whether the game's own view keeps rendering in VR play, and the console's line (None: nothing to say).

    `ray_traced` is the saved RTReflections of the game that runs. A session turns it off in a game it
    starts (vr_display.SESSION), so it is on only in a game Spidy did not start, or one whose settings
    Windows kept from Spidy. There the view must not rest: the first frame it drew again hung the graphics
    device. It renders as with --game-monitor-view, about a fifth fewer frames."""
    if not ray_traced:
        return False, None
    return True, ('Ray-Traced Reflections is on in this game, so its own view keeps rendering beside the eyes '
                  '(about a fifth fewer frames): with that view resting, the game ended with its display '
                  'driver error as a menu opened. In a game Spidy starts itself the setting is off for VR: '
                  'close the game and press START VR.')


def command_text(command, timeout=10):
    """What a Windows command printed, '' where it could not run. Its text is in the console's code page,
    not Python's (UTF-8 under the launcher): read as Python's, one letter outside ASCII in it (a Windows
    in another language, an adapter's or a network's name) left no output at all, and no session of 0.2.13
    started on such a PC. A byte that is no letter there either becomes a question mark."""
    try:
        printed = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True, timeout=timeout,
                                 creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0)).stdout
    except (OSError, subprocess.SubprocessError):
        return ''
    return (printed or b'').decode('oem', 'replace')


# A Steam Frame's own link. The headset makes a 6 GHz network of its own and the PC joins it through the
# adapter from the headset's box: it carries the headset's picture and is not the PC's way to the router.
# A Frame player's PC on October 13 was on "frame_CBC1602B" beside its network cable, and the session told
# them to turn the PC's Wi-Fi off. The name's form is from that one report.
HEADSET_LINK = re.compile(r'frame_[0-9A-Fa-f]{6,}$')


def network_links(wlan=None, addresses=None):
    """How this PC is on the network: the Wi-Fi network it is connected to (None without one) and whether
    a cable carries an address too. A headset's own network the PC is on (HEADSET_LINK) is no such Wi-Fi:
    it is named as `headset_link`. `wlan` and `addresses` are `netsh wlan show interfaces` and `ipconfig`
    output (run here when not given). Both are read by their English words: a Windows in another language
    shows neither."""
    wlan = command_text(['netsh', 'wlan', 'show', 'interfaces']) if wlan is None else wlan
    addresses = command_text(['ipconfig']) if addresses is None else addresses
    wifi = headset = None
    # One block for each Wi-Fi adapter, from its Name line on.
    for block in re.split(r'^(?=[ \t]*Name[ \t]*:)', wlan, flags=re.M):
        if not re.search(r'^\s*State\s*:\s*connected\s*$', block, re.M):
            continue
        name = re.search(r'^\s*SSID\s*:\s*(.*?)\s*$', block, re.M)
        name = name.group(1) if name and name.group(1) else 'Wi-Fi'
        if HEADSET_LINK.match(name):
            headset = headset or name
        else:
            wifi = wifi or name
    cable = False
    for block in re.split(r'^(?=\S)', addresses, flags=re.M):
        if block.startswith('Ethernet adapter') and 'Bluetooth' not in block.splitlines()[0] and \
                re.search(r'IPv4 Address', block):
            cable = True
    links = dict(wifi=wifi, cable=cable)
    if headset:
        links['headset_link'] = headset
    return links


def link_warning(links, runtime_name):
    """The console's warning for a PC that streams to the headset over Wi-Fi, or None.

    A wireless headset's picture comes over the network. From a PC that is on Wi-Fi itself every picture
    crosses the air twice, and the stream stalls for a second or two at a time whatever the game's frame
    rate: Virtual Desktop then holds each of the game's frames for a tenth of a second (`stream_stalls`).
    On October 11 the session with the PC on its cable alone had none; the three with it on Wi-Fi had 5 to
    8 each."""
    if not links.get('wifi') or not re.search(r'virtual desktop|steamvr|alvr', runtime_name or '', re.I):
        return None
    if links.get('cable'):
        return (f"WARNING: this PC is on Wi-Fi ({links['wifi']}) as well as on a network cable. A headset that "
                "connected over the Wi-Fi keeps using it, and its picture then stalls for a second or two at a "
                "time. Turn this PC's Wi-Fi off and connect the headset again.")
    return (f"WARNING: this PC is on Wi-Fi ({links['wifi']}), not on a network cable. A wireless headset's "
            'picture then crosses the Wi-Fi twice and stalls for a second or two at a time, whatever the frame '
            'rate. Connect this PC to the router by cable.')


def stream_stalls(samples):
    """Rounds of the headset's loop that the VR runtime held for about a tenth of a second (its submit
    timed out: Virtual Desktop was not taking frames), from the samples' `xr_ms`; and their seconds.
    Version 0.0.6 (Update 39.1): only rounds while immersive count. Reports rep0022 and rep0050 had 183 and
    131 such rounds, but all except 1 each came while nothing was presented (loading, or the headset
    taken off at the end: the runtime then holds every frame); those are `outside_rounds`."""
    held = lambda s: s.get('xr_ms') and s['xr_ms'][0] >= 90 and \
        s['xr_ms'][XR_STAGES.index('end_frame')]+s['xr_ms'][XR_STAGES.index('wait_frame')] >= 80
    playing = lambda s: s.get('presentation', 'immersive') == 'immersive'
    rounds = {s.get('frames') for s in samples if held(s) and playing(s)}
    outside = {s.get('frames') for s in samples if held(s) and not playing(s)} - rounds
    return dict(rounds=len(rounds), seconds=round(len(rounds)/10, 1), outside_rounds=len(outside),
                outside_seconds=round(len(outside)/10, 1))


# What the headset's loop spent on its newest frame, in milliseconds, in a session's samples (`xr_ms`).
XR_STAGES = ('frame', 'wait_frame', 'tracking', 'prepare', 'acquire', 'copy_wait', 'hand_overlay', 'release',
             'end_frame')


def frame_rates(samples):
    """Measure active presentation; reused images are not new scene frames."""
    active = [s for s in samples if s.get('status') == 3 and s.get('submitted', 0) > 0]
    if len(active) < 2:
        return None
    # Older workers kept status=3 during cleanup. Ignore trailing samples of
    # the same stopped frame, while retaining dropped frames during active work.
    counter = 'frames' if all('frames' in s for s in active) else 'submitted'
    while len(active) > 1 and active[-1][counter] == active[-2][counter]:
        active.pop()
    if len(active) < 2:
        return None
    first, last = active[0], active[-1]
    seconds = last['seconds'] - first['seconds']
    if seconds <= 0:
        return None
    result = dict(sample_seconds=seconds, submitted_fps=(last['submitted']-first['submitted'])/seconds)
    for name, field in (('new_scene_fps', 'unique_submitted'), ('reused_fps', 'reused_submitted')):
        result[name] = (last[field]-first[field])/seconds if field in first and field in last else None
    return result


def accepted(result):
    """A stopped worker alone is not a successful presentation or clean test."""
    final, gpu, swing = (result.get(name) or {} for name in ('final', 'gpu', 'swing'))
    return bool(final.get('status') == 4 and not final.get('error', 1) and final.get('submitted', 0) > 0 and
                gpu.get('status') == 2 and not gpu.get('error', 1) and gpu.get('pairs', 0) > 0 and
                0 < gpu.get('fence', 0) <= gpu.get('completed', 0) < 0xffffffffffffffff and
                not swing.get('error', 0) and not any(s.get('error') for s in result.get('samples', [])) and
                not any(s.get('error') for s in result.get('swing_samples', [])) and
                not any(s.get('error') for s in result.get('motion_samples', [])) and
                result.get('game_entries_restored') and result.get('xr_stop') == 0 and result.get('bridge_stop') == 0)


def wait_for_headset(ask, wait=HEADSET_WAIT, retry=HEADSET_RETRY, clock=time.monotonic, sleep=time.sleep,
                     say=lambda line: print(line, flush=True)):
    """0.0.5 hotfix 3: ask for the headset until it answers or `wait` seconds pass (then the last error is raised,
    so the game is never started without a headset). A missing headset is said once, in plain words."""
    deadline = clock()+wait
    told = False
    while True:
        try:
            return ask()
        except RuntimeError as error:
            if clock()+retry > deadline:
                raise RuntimeError(f'{error} No headset within {wait // 60} minutes: the game was not started. Open '
                                   'Virtual Desktop in your headset first, then press PLAY VR again.') from error
            if not told:
                say(HEADSET_MISSING)
                told = True
            sleep(retry)


def preflight(requested):
    """The session's OpenXR runtime, once it has a headset: the one asked for, or by default ('auto') the one
    the headset is connected to (xr_runtime.detect). No session is started. Returns (manifest, what the
    report says about it)."""
    automatic = not requested or str(requested).lower() == 'auto'
    if not automatic:
        manifest = xr_runtime.choose(requested)
        runtime = xr_runtime.name(manifest)
        print(f'VR runtime: {runtime} ({manifest}).', flush=True)

        def ask():
            found, said = xr_runtime.probe(manifest)
            if not found:
                raise RuntimeError(f'{said} Connect your headset in {runtime} before starting Spidy VR.')
            return manifest, said
        manifest, said = wait_for_headset(ask)
    else:
        manifest, said = wait_for_headset(lambda: xr_runtime.detect(say=lambda line: print(line, flush=True)))
        runtime = xr_runtime.name(manifest)
        print(f'VR runtime: {runtime} ({manifest}), found automatically.', flush=True)
    print(said, flush=True)
    headset = next((line[len('Headset available: '):].split(';')[0] for line in said.splitlines()
                    if line.startswith('Headset available: ')), None)
    eye = re.search(r'^Recommended eye 0: (\d+)x(\d+)', said, re.M)
    return manifest, dict(name=runtime, manifest=str(manifest), automatic=automatic, headset=headset,
                          recommended_eye=[int(eye[1]), int(eye[2])] if eye else None)


class _Echo:
    """A console stream that writes to the session's console log as well."""

    def __init__(self, stream, log):
        self.stream, self.log = stream, log

    def write(self, text):
        if self.stream:
            self.stream.write(text)
        try:
            self.log.write(text)
            self.log.flush()
        except (OSError, ValueError):
            pass  # a full disk must not end the session
        return len(text)

    def flush(self):
        if self.stream:
            self.stream.flush()

    def __getattr__(self, name):
        return getattr(self.stream, name)


def keep_console(report):
    """Everything the session prints goes to <report>-console.log too. The launcher's window used to be the
    only place it went: the October 7 Steam Frame player's reports folder said nothing about why VR never
    started. Returns the log's path, or None when it cannot be written."""
    path = report.with_name(report.stem+'-console.log')
    try:
        report.parent.mkdir(parents=True, exist_ok=True)
        log = path.open('a', encoding='utf-8')
    except OSError:
        return None
    sys.stdout, sys.stderr = _Echo(sys.stdout, log), _Echo(sys.stderr, log)
    return path


class Startup:
    """How far a session got before VR started. A session that ends there writes this as its report, with the
    game's log and the modules in the game's process (overlays and capture tools among them)."""

    def __init__(self, output):
        self.output, self.stage, self.reported, self.fields = output, 'finding the headset', False, {}

    def ended(self, error, modules_of=modules):
        if self.reported or (isinstance(error, KeyboardInterrupt) and 'game_pid' not in self.fields):
            return  # written already, or cancelled before anything started
        report = dict(stage=self.stage, error=str(error) or type(error).__name__,
                      cancelled=isinstance(error, KeyboardInterrupt), vr_started=False,
                      free_commit_mb=free_commit_mb(), **self.fields)
        try:
            if 'game_pid' in self.fields:
                copy, memory = keep_game_log(self.output)
                report.update(game_log=str(copy) if copy else None, game_memory=memory)
                try:
                    report['game_modules'] = sorted({m['name'] for m in modules_of(self.fields['game_pid'])})
                except (OSError, RuntimeError, ValueError):
                    report['game_modules'] = None  # the game has closed
            self.output.parent.mkdir(parents=True, exist_ok=True)
            self.output.write_text(json.dumps(report, indent=2, default=str)+'\n')
            self.reported = True
            print(f"VR did not start ({self.stage}). Report: {self.output.resolve()}", flush=True)
        except (OSError, ValueError):
            pass


def runtime_path(manifest):
    """The runtime manifest as the game XR worker's config carries it: UTF-16, 260 characters."""
    encoded = str(manifest).encode('utf-16-le')
    if len(encoded) > 518:
        raise RuntimeError(f'The OpenXR runtime path is too long: {manifest}')
    return encoded.ljust(520, b'\0')


def watch_stop_event(name):
    """Treat the named event as Ctrl+C. A launcher window without a console stops VR this way."""
    kernel = c.WinDLL('kernel32', use_last_error=True)
    kernel.OpenEventW.restype, kernel.OpenEventW.argtypes = wintypes.HANDLE, [wintypes.DWORD, wintypes.BOOL,
                                                                             wintypes.LPCWSTR]
    kernel.WaitForSingleObject.restype = wintypes.DWORD
    kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    handle = kernel.OpenEventW(0x00100000, False, name)  # SYNCHRONIZE
    if not handle:
        raise c.WinError(c.get_last_error())

    def wait():
        if kernel.WaitForSingleObject(handle, 0xFFFFFFFF) == 0:
            _thread.interrupt_main()
    threading.Thread(target=wait, name='stop-event', daemon=True).start()


# game_screen_3d: the game screen with a scene's own two eye images on it (a cutscene in stereo).
PRESENTATIONS = {0: 'none', 1: 'immersive', 2: 'flat', 3: 'game_screen', 4: 'game_screen_3d'}
# Why a frame had no gameplay (game_xr.cpp GateReason).
GATE_REASONS = ((1, 'no_player'), (2, 'bridge_stopped'), (4, 'no_camera_commit'), (8, 'other_camera'),
                (16, 'tracking'))
# Camera movers (Camera2 vtables, image offsets) the gate has names for.
CAMERA_MOVERS = {0x538bf38: 'follow', 0x538c040: 'combat', 0x538c3a8: 'death', 0x538c418: 'exterior',
                 0x538be30: 'look', 0x538c5c8: 'melee_animation',  # SM2VR found no look_game mover
                 0x538c760: 'relative_animation', 0x538c998: 'turret', 0x538cab8: 'vehicle',
                 0x85da1a0: 'photo_mode'}


# What the VR settings (the SPIDY VR tab in the game's Settings, and X) changed, as XrData reports it. A session
# that ends with other values than it started with prints them on one line, from which the launcher
# starts the next session (launcher_text.hpp, headsetSettings).
SETTINGS_LINE = 'VR settings from the headset: '


def start_settings(a):
    """The VR settings a session starts with, as its samples' vr_settings report them; with them the player's T-pose
    calibration (millimetres, 0: none) and whether a session without one asks for it at its first gameplay."""
    return dict(aim_markers=not a.no_aim_markers, web_grab=not a.no_web_grab, air_webs=not a.no_air_webs,
                web_shooter=not a.no_web_shooter, punch=not a.no_punch, body=not a.no_body,
                swing_speed=round(a.swing_speed, 1), weight=a.weight,
                snap_turn=a.snap_turn, smooth_turn=a.smooth_turn, haptics=a.haptics, screen_size=a.screen_size,
                flips=a.flips, flip_speed=a.flip_speed, trigger_webs=a.trigger_webs, hud=a.hud,
                stand_on_walls=not a.no_stand_on_walls, screen_depth=a.screen_depth, arcade_webs=a.arcade_webs,
                body_haptics=a.body_haptics, eye_height_mm=a.eye_height, arm_length_mm=a.arm_length,
                calibration_prompt=not a.no_calibration_prompt)


def settings_line(start, sample):
    """The line for the launcher, or None while the settings are as the session started."""
    now = (sample or {}).get('vr_settings')
    if not now or now == start:
        return None
    return SETTINGS_LINE + ' '.join(f'{k}={int(round(v))}' for k, v in now.items())


PAD_IGNORED = ("WARNING: the game is not reading the VR controllers on its screen. Keep the game window in front "
               "on the desktop. If it is in front, close the game and start VR again from Spidy's launcher: a game "
               "Spidy starts reads controllers through XInput only, and one started otherwise can take SteamVR's "
               'virtual gamepads instead. If the buttons stay dead and the game shows keyboard keys in its prompts, '
               'turn Steam Input off for the game: in Steam, right-click the game > Properties > Controller > '
               'Disable Steam Input.')


def pad_ignored(watch, sample, quiet=5):
    """True once: the game screen showed and a button was held while the game had not read Spidy's controller
    for `quiet` seconds. `watch` (reads, since, warned) carries what earlier samples showed."""
    if watch['warned'] or not sample.get('pad_installed'):
        return False
    if sample.get('pad_reads') != watch['reads']:
        watch.update(reads=sample.get('pad_reads'), since=sample['seconds'])
        return False
    if sample.get('presentation') in ('game_screen', 'game_screen_3d') and \
            sample.get('pad_buttons', '0x0') != '0x0' and \
            sample['seconds']-watch['since'] >= quiet:
        watch['warned'] = True
        return True
    return False


# The layout the VR runtime uses for a hand's controller (controller_layout.hpp ControllerKind).
CONTROLLER_KINDS = {0: None, 1: 'touch', 2: 'index', 3: 'steam_frame', 4: 'runtime'}


# Under SteamVR a Quest's left menu button is SteamVR's own (its dashboard), and so is an Index controller's
# system button: the game's pause comes from both thumbsticks held in (game_pad::PausePress).
STEAMVR_PAUSE = (' SteamVR keeps the menu button for its own dashboard: to pause the game, hold both thumbsticks '
                 'in for a moment.')


def controllers_line(controllers, runtime=None):
    """What the session log says about the layout the VR runtime uses for the controllers in the
    player's hands (left, right), or None while it has not said. runtime: the VR runtime's name."""
    kinds = {kind for kind in controllers if kind}
    if not kinds:
        return None
    if kinds == {'steam_frame'}:
        return ("Controllers: Steam Frame, with their own layout: A, B, X and Y on the right are Spidy's A, B, X "
                "and Y, Menu pauses, View opens the game menu and the D-pad is the game's (down heals).")
    pause = STEAMVR_PAUSE if runtime == 'SteamVR' else ''
    if 'runtime' in kinds:
        return ("Controllers: the VR runtime uses a layout of its own for them, so Spidy's buttons may sit "
                'elsewhere. Its controller settings show where.' + pause)
    names = {'touch': 'Touch', 'index': 'Index', 'steam_frame': 'Steam Frame'}
    return f"Controllers: the {' and '.join(sorted(names[kind] for kind in kinds))} layout." + pause


# A demand as prompt_buttons::pack lays it out: the pad's button bits, then each trigger, the hold, and
# two bits an axis of each stick (0 no way, 1 back or left, 2 forward or right).
PAD_BUTTONS = ((0x1, 'D-pad up'), (0x2, 'D-pad down'), (0x4, 'D-pad left'), (0x8, 'D-pad right'),
               (0x10, 'Start'), (0x20, 'Back'), (0x40, 'L3'), (0x80, 'R3'), (0x100, 'LB'), (0x200, 'RB'),
               (0x1000, 'A'), (0x2000, 'B'), (0x4000, 'X'), (0x8000, 'Y'))
PAD_STICKS = ((19, 'left stick left', 'left stick right'), (21, 'left stick back', 'left stick forward'),
              (23, 'right stick left', 'right stick right'), (25, 'right stick back', 'right stick forward'))
# What VR play can send on its own, without the pad layer or the interact button carrying it
# (prompt_buttons::reachableInPlay): each bumper and each trigger only with its pair.
PLAY_BUTTONS = 0x1000 | 0x2000 | 0x8000 | 0x10 | 0x20 | 0x2 | 0x80 | 0x100 | 0x200


def demand_name(word):
    """What the game asked for, from a packed demand; None for nothing."""
    if not word:
        return None
    names = [name for bit, name in PAD_BUTTONS if word & bit]
    if word >> 16 & 1:
        names.append('LT')
    if word >> 17 & 1:
        names.append('RT')
    for shift, back, forward in PAD_STICKS:
        way = word >> shift & 3
        if way:
            names.append(back if way == 1 else forward)
    # A QTE that asks for a stick to be moved any way, and a prompt that named
    # a key or a mouse button (prompt_buttons packs those in bits 27 to 29).
    if word >> 27 & 1:
        names.append('left stick (any way)')
    if word >> 28 & 1:
        names.append('right stick (any way)')
    if word >> 29 & 1:
        names.append('a key or a mouse button')
    if not names:
        return None
    return ' + '.join(names) + (' (hold)' if word >> 18 & 1 else '')


def reachable_in_play(word):
    """Whether VR play sends that demand as it stands (prompt_buttons::reachableInPlay)."""
    if not word:
        return True
    if word >> 23 & 15 or word >> 28 & 1:                 # the right stick turns the player in play
        return False
    if (word >> 16 & 1) != (word >> 17 & 1):              # a trigger reaches the game only with its pair
        return False
    shoulders = word & 0x300
    if shoulders and shoulders != 0x300:
        return False
    return not word & 0xffff & ~PLAY_BUTTONS


def prompts_line(word):
    """What the session log says when the game asks a player for a button, or None for nothing."""
    asked = demand_name(word)
    if not asked:
        return None
    own = '' if reachable_in_play(word) else ' (VR play has no button of its own for it)'
    return (f'The game is asking for {asked}{own}: the interact button sends it, and clicking both '
            'thumbsticks latches the whole pad.')


# The T-pose calibration's phases and what its panel asks for (body_calibration.hpp Phase, Hint).
CALIBRATION_PHASES = {0: 'none', 1: 'waiting', 2: 'holding', 3: 'done'}
CALIBRATION_HINTS = {0: None, 1: 'tracking', 2: 'stand_up', 3: 'arms_out', 4: 'straight', 5: 'look_ahead',
                     6: 'triggers', 7: 'still'}


# Why the view rig could not place a frame of play (GameTrackingRig::Inactive bits).
TRACKING_GAP_REASONS = ((1, 'not_playing'), (2, 'unfocused'), (4, 'no_head'), (8, 'no_feet'), (16, 'no_camera'),
                        (32, 'frame_time'), (64, 'stale_display'), (128, 'no_eyes'))


def snapshot(game, address):
    for _ in range(8):
        raw = game.read(address, 1176)
        if len(raw) != 1176:
            return None
        if struct.unpack_from('<3I', raw) != (0x53585244, 39, 1176):
            raise RuntimeError('Game XR protocol mismatch')
        if struct.unpack_from('<Q', raw, 16)[0] & 1 or raw[16:24] != game.read(address+16, 8):
            continue
        result = dict(zip(('frames', 'tracked', 'submitted', 'dropped', 'left_hands', 'right_hands', 'serial', 'generation'),
                          struct.unpack_from('<8Q', raw, 24)))
        result.update(status=struct.unpack_from('<I', raw, 12)[0],
                      native_keys=struct.unpack_from('<I', raw, 88)[0],
                      error=struct.unpack_from('<I', raw, 92)[0],
                      head=struct.unpack_from('<16f', raw, 96),
                      hands=[struct.unpack_from('<16f', raw, 160+i*64) for i in range(2)],
                      message=raw[288:544].split(b'\0', 1)[0].decode('utf-8', 'replace'))
        result.update(zip(('unique_submitted','reused_submitted'),struct.unpack_from('<2Q',raw,544)))
        result.update(zip(('flat_screen','toggles','eye_width','eye_height'),struct.unpack_from('<4I',raw,560)))
        # What the last headset frame showed; game_screen frames showed the game's own camera on the
        # virtual screen because gameplay was unavailable (menus, hint cards, cutscenes, animated cameras).
        # Then the B presses the game got as its B (the dodge).
        presentation, dodges = struct.unpack_from('<2I', raw, 576)
        # screen_3d_submitted: the game-screen frames that showed a scene in stereo (among screen_submitted).
        result.update(presentation=PRESENTATIONS.get(presentation, presentation), dodges=dodges,
                      screen_submitted=struct.unpack_from('<Q', raw, 584)[0],
                      screen_3d_submitted=struct.unpack_from('<Q', raw, 808)[0])
        # Why gameplay was unavailable, and which camera the game used meanwhile (a scene that
        # stays on the screen although it is played: its camera is missing from the gate).
        gate, mover = struct.unpack_from('<2I', raw, 592)
        players, record = struct.unpack_from('<2Q', raw, 600)
        pad_buttons, pad_installed = struct.unpack_from('<2I', raw, 616)
        result.update(gate=[name for bit, name in GATE_REASONS if gate & bit],
                      camera_mover=CAMERA_MOVERS.get(mover, hex(mover) if mover else None),
                      players=players, player_record=hex(record), pad_buttons=hex(pad_buttons),
                      pad_installed=bool(pad_installed), pad_reads=struct.unpack_from('<Q', raw, 624)[0])
        # Camera commits, and those on the player by the follow or combat camera. Both rising while
        # the gate says other_camera: another camera commits after the player's every frame.
        result.update(zip(('camera_commits', 'player_commits'), struct.unpack_from('<2Q', raw, 632)))
        # Y presses the game got as its Y (interact, web strike), whether the aim markers show, and the
        # markers drawn so far (one per hand per headset frame).
        interacts, aim_markers = struct.unpack_from('<2I', raw, 648)
        # The walks a held Y began: the player stepping up to what the game's Y is for.
        result.update(interact_steps=struct.unpack_from('<I', raw, 800)[0])
        # Pushes of the right stick the game got as its D-pad down (heal) and as its LT + RT (the zip to a point),
        # with the zips sent as a hold of it was let go in the game's aim mode (ZipPress), counted again on their
        # own in zip_releases: the press went out a second time, to the point the aim held.
        result.update(zip(('heals', 'zips'), struct.unpack_from('<2I', raw, 816)))
        result.update(zip_releases=struct.unpack_from('<I', raw, 852)[0])
        # The game's own camera, which decides what its Y takes, against where the head looks: the angle between
        # them now in degrees (the head to the camera's right: more than 0; 0 while it is not steered), and the
        # times the virtual controller's right stick began to turn the camera there.
        look_error, look_turns = struct.unpack_from('<fI', raw, 824)
        result.update(look_error=round(look_error, 1), look_turns=look_turns)
        # The scans a trigger pulled at the temple began (the game's R3), and the Charge Jumps a held A
        # began (its RT + A).
        result.update(zip(('scans', 'charge_jumps'), struct.unpack_from('<2I', raw, 832)))
        # What the game asked a player for, read through its own UI (game_prompts.hpp): its calls into
        # Flash and the prompt and QTE interface's among them ('calls' at 0 while 'invokes' rises says the
        # names no longer match the game), prompts raised and taken away, texts that named a button and
        # texts rewritten for VR; what it is asking for now and the last thing it asked for, the asks VR
        # play could not have sent on its own (what the interact button carries) and texts that came with
        # the keyboard's icons, which name no pad button at all. Then the presses of the interact button
        # that sent an ask instead of the game's Y, and the pad layer's latches.
        invokes, calls, raised, cleared, texts, rewrites = struct.unpack_from('<6Q', raw, 856)
        demand, last_demand, beyond_play, keyboard_texts = struct.unpack_from('<4I', raw, 904)
        context_presses, pad_entries, pad_layer, wall_interacts = struct.unpack_from('<4I', raw, 920)
        # What the controllers themselves sent, beside what reached the game: a button in neither was
        # never pressed. ControllerButton bits, as xr_session gives them.
        vr_now, vr_played = struct.unpack_from('<2I', raw, 936)
        # Presses of the interact that handed the player back to the game's own wall crawl, because the
        # swing held him on a wall of its own and the game was offering nothing (game_pad::WallHandover).
        wall_handovers, handovers_offered = struct.unpack_from('<2I', raw, 944)
        # The eyes held out of what the world is built of (HeadClearance, version 37 in the slots of the
        # room around the player): frames they were moved more than a centimetre, and the most they were.
        head_push_frames, head_push_most_mm = struct.unpack_from('<2I', raw, 952)
        # The game's own wall crawl, now that the view turns onto it as it does onto a wall of the swing's.
        # Frames it held the player, and of those the frames the view had turned. Then what the game made of
        # the stick there (CrawlStick): the mean turn from the way sent to the way taken, in the surface's own
        # axes, how alike those turns were (near 1 a frame fixed against the surface, which the stick wants
        # rotating by that turn; near 0 no fixed frame at all, and the mover wants driving instead) and the
        # mean speed taken. The crawl is the only hold the game itself calls standing on a wall, so it is the
        # only one whose interacts work there.
        crawl_frames, crawl_stood, crawl_stick = struct.unpack_from('<3I', raw, 960)
        crawl_turn, crawl_agreement, crawl_speed = struct.unpack_from('<3f', raw, 972)
        # The head stopping short of the surface over the player (headroom): frames it was brought down, and
        # the least room there ever was over his feet. Inside the vent it stood at the player's own eye
        # height over its floor and so out through its roof.
        head_dropped, = struct.unpack_from('<I', raw, 984)
        headroom_least, = struct.unpack_from('<f', raw, 988)
        # Handing him from the swing's own wall to the game's, indoors (WallToGame): tries, those the game
        # answered by taking him - a vent that works - those it did not and the swing took the wall straight
        # back, and the frames he was driven into the wall. `handed` against `recovered` is the whole answer
        # to whether the game catches a player let go of at a wall.
        wtg_tries, wtg_handed, wtg_recovered, wtg_push = struct.unpack_from('<4I', raw, 992)
        result.update(wall_to_game=dict(tries=wtg_tries, handed=wtg_handed, recovered=wtg_recovered,
                                        push_frames=wtg_push))
        # The game's own crawl under the player at rest on the swing's wall (game_warp): crawls asked for,
        # those its state machine took, refused, faulted, and how the hand-overs ended (the crawl took him,
        # or the swing had him back); frames of the game's own wall run (its RT with the stick up the wall)
        # and frames the view held its surface through a gap in the swing's reports.
        (w_asked, w_applied, w_refused, w_faulted, w_taken, w_declined, w_run,
         w_grace) = struct.unpack_from('<8I', raw, 1008)
        result.update(warp=dict(asked=w_asked, applied=w_applied, refused=w_refused, faulted=w_faulted,
                                taken=w_taken, declined=w_declined, game_run_frames=w_run,
                                hold_grace_frames=w_grace))
        # Frames of play the view rig could not place, and every reason seen (one shorter than half a second
        # keeps the view standing where it stood); the game's crawl on a floor (a vent, a duct), the frames
        # the head was held at the crawl's own height there, and the frames the body knelt under a held head;
        # the game camera's look and up at the latest frame - the frame the game's crawl takes its stick in.
        gap_frames, gap_reasons, floor_frames, cap_frames, kneel_frames = struct.unpack_from('<5I', raw, 1040)
        camera_forward = struct.unpack_from('<3f', raw, 1064)
        camera_up = struct.unpack_from('<3f', raw, 1076)
        result.update(tracking_gaps=dict(frames=gap_frames,
                                         reasons=[name for bit, name in TRACKING_GAP_REASONS if gap_reasons & bit]),
                      floor_crawl=dict(frames=floor_frames, capped_frames=cap_frames, kneel_frames=kneel_frames),
                      game_camera=dict(forward=[round(v, 3) for v in camera_forward],
                                       up=[round(v, 3) for v in camera_up]))
        # The game's interactions on Spidy's wall (game_offers, offer_pointing): frames a hand pointed at one the
        # game would let the player use and at one to come closer to; presses of the interact that asked for a
        # use, those the game refused at once, those that came to the game's interact and of those the ones the
        # game had picked the offer for first, those given up (its crawl did not take him); the most offers it
        # gathered near him at once.
        # And whether its hooks went in (off, hooked, failed - with the reason: 9601 another game build).
        (o_point, o_reach, o_asks, o_refused, o_presses, o_picked, o_failed,
         o_seen) = struct.unpack_from('<8I', raw, 1088)
        o_state, = struct.unpack_from('<I', raw, 1060)
        # The module's own counts: updates run with the crawl's mask, offers checked against the crawl's pose,
        # masked updates whose gather checked any offer and the most at once, spot searches and those that found
        # one, and the offers near the player at the end.
        (o_masked, o_posed, o_gathered, o_most, o_searches, o_spots, o_near,
         o_retries) = struct.unpack_from('<8I', raw, 1120)
        result.update(offers=dict(hooks={0: 'off', 1: 'hooked', 3: 'failed'}.get(o_state & 0xff, o_state & 0xff),
                                  hook_error=o_state >> 8, point_frames=o_point, reach_frames=o_reach, asks=o_asks,
                                  refused=o_refused, presses=o_presses, picked=o_picked, failed=o_failed,
                                  most_seen=o_seen, masked_updates=o_masked, posed_checks=o_posed,
                                  gathers=o_gathered, most_checked=o_most, spot_searches=o_searches,
                                  spots_found=o_spots, near_now=o_near, press_retries=o_retries))
        # Spider-Sense in the headset (spider_sense.hpp): warnings that showed, frames that drew a mark over an
        # attacker and in the sightline (one out of view), tingles sent to the controllers, marks a counter broke,
        # and the game's dodge pressed while a warning stood (what a red mark asks for).
        s_shown, s_frames, s_sight, s_tingles, s_broken, s_dodges = struct.unpack_from('<6I', raw, 1152)
        result.update(sense_headset=dict(shown=s_shown, mark_frames=s_frames, sight_frames=s_sight,
                                         tingles=s_tingles, broken=s_broken, dodges=s_dodges))
        result.update(crawl=dict(frames=crawl_frames, stood=crawl_stood, stick_samples=crawl_stick,
                                 stick_turn_degrees=round(math.degrees(crawl_turn), 1),
                                 stick_agreement=round(crawl_agreement, 3),
                                 stick_speed=round(crawl_speed, 2)),
                      headroom=dict(dropped_frames=head_dropped, least=round(headroom_least, 2)),
                      head_clearance=dict(pushed_frames=head_push_frames, most_m=head_push_most_mm / 1000))
        result.update(vr_buttons=controller_buttons(vr_now), vr_buttons_played=controller_buttons(vr_played))
        result.update(prompts=dict(invokes=invokes, calls=calls, raised=raised, cleared=cleared, texts=texts,
                                   rewrites=rewrites, asking=demand_name(demand),
                                   last_asked=demand_name(last_demand), beyond_play=beyond_play,
                                   keyboard_texts=keyboard_texts, context_presses=context_presses,
                                   pad_entries=pad_entries, pad_layer=bool(pad_layer),
                                   wall_interacts=wall_interacts, wall_handovers=wall_handovers,
                                   wall_handovers_offered=handovers_offered),
                      prompt_demand=demand, prompt_last_demand=last_demand)
        # The layout the VR runtime uses for each hand's controller (left, right; None until it says), and
        # the layouts it took Spidy's controls for. A Steam Frame on 'touch' plays SteamVR's translation.
        left, right, layouts = struct.unpack_from('<3I', raw, 840)
        result.update(controllers=[CONTROLLER_KINDS.get(kind, kind) for kind in (left, right)],
                      controller_layouts=[name for kind, name in CONTROLLER_KINDS.items()
                                          if name and layouts & 1 << kind])
        result.update(interacts=interacts, aim_markers=bool(aim_markers),
                      markers=struct.unpack_from('<Q', raw, 656)[0])
        # The VR settings now (the SPIDY VR tab in the game's Settings changes them),
        # the tab's changes, the times the game built its Settings with it, whether its hooks are in, and
        # why it is missing (game_menu.hpp: 93xx-94xx not hooked, 95xx not built).
        flags, snap_turn, haptics, screen_size = struct.unpack_from('<4I', raw, 664)
        swing_speed, changes = struct.unpack_from('<fI', raw, 680)
        smooth_turn, = struct.unpack_from('<I', raw, 704)
        # The weight (percent of real gravity) and the HUD (0 off, 1-3 small to large).
        weight, hud = struct.unpack_from('<2I', raw, 744)
        # The T-pose calibration: its phase and what its panel asks for, the hold (0-1), the eye height and arm
        # length the body is sized to (mm, 0: the headset's running height and Spider-Man's own arms),
        # calibrations finished and skipped this session, whether a session without one asks for none at its
        # first gameplay (bit 0), and each arm's reach at the last one.
        phase, hint, progress = struct.unpack_from('<2If', raw, 752)
        eye_height, arm_length, calibrations, skips, calibration_flags = struct.unpack_from('<5I', raw, 764)
        reach = struct.unpack_from('<2f', raw, 784)
        # The flip speed: degrees a second at full tilt of the left stick. Then how far the view leans from the
        # world's up now, degrees: 90 while the player stands on a wall (STAND ON WALLS).
        flip_speed, view_tilt = struct.unpack_from('<If', raw, 792)
        # The GAME SCREEN 3D row: 0 off (cutscenes flat), 1 low, 2 medium, 3 high.
        screen_depth, = struct.unpack_from('<I', raw, 804)
        result.update(vr_settings=dict(aim_markers=bool(aim_markers), web_grab=bool(flags & 1),
                                       air_webs=bool(flags & 8), web_shooter=bool(flags & 16),
                                       punch=bool(flags & 2), body=bool(flags & 4),
                                       swing_speed=round(swing_speed, 1), weight=weight, snap_turn=snap_turn,
                                       smooth_turn=smooth_turn, haptics=haptics, screen_size=screen_size,
                                       flips=bool(flags & 32), flip_speed=flip_speed,
                                       trigger_webs=bool(flags & 64), hud=hud,
                                       stand_on_walls=bool(flags & 128),
                                       screen_depth=screen_depth, arcade_webs=bool(flags & 512),
                                       eye_height_mm=eye_height, arm_length_mm=arm_length,
                                       calibration_prompt=not calibration_flags & 1),
                      setting_changes=changes,
                      calibration=dict(phase=CALIBRATION_PHASES.get(phase, phase),
                                       hint=CALIBRATION_HINTS.get(hint, hint), progress=round(progress, 3),
                                       done=calibrations, skipped=skips, reach_m=[round(r, 3) for r in reach]))
        menu_tabs, menu_installed, menu_status = struct.unpack_from('<Q2I', raw, 688)
        result.update(menu_tabs=menu_tabs, menu_installed=bool(menu_installed), menu_status=menu_status)
        # Walls and ceilings the game held the player on (its wall crawl): the stretches and frames of play
        # on one, the player's up now (its actor's), the head's stand-off from the surface now, and on the
        # last frame on one the head's height over it as placed from the feet, before and after the stand-off.
        entries, frames = struct.unpack_from('<IQ', raw, 708)
        up = struct.unpack_from('<3f', raw, 720)
        stand_off, height, clearance = struct.unpack_from('<3f', raw, 732)
        result.update(surface=dict(entries=entries, frames=frames, hero_up=[round(v, 3) for v in up],
                                   stand_off_m=round(stand_off, 3), head_height_m=round(height, 3),
                                   head_clearance_m=round(clearance, 3), view_tilt_deg=round(view_tilt, 1)))
        return result
    return None


HERO_STATES = {0: 'unknown', 1: 'valid', 2: 'no_instance', 3: 'unregistered', 4: 'bad_transform'}
WEB_STATES = {0: 'off', 1: 'waiting', 2: 'game_webs', 3: 'failed'}


def lens_degrees(left, right, top, bottom):
    """Horizontal and vertical field of view of native lens tangents."""
    return [math.degrees(math.atan(right) - math.atan(left)), math.degrees(math.atan(bottom) - math.atan(top))]


BODY_STATES = {0: 'off', 1: 'waiting', 2: 'active', 3: 'failed'}
BODY_PROBLEMS = {0: None, 1: 'no_hero', 2: 'no_rig', 3: 'unknown_rig', 4: 'bad_rest_pose', 5: 'bad_instance'}


def body_snapshot(game, address):
    """The player's body on the hero (native_body::Status): whether it turns the hero's joints, how far each
    wrist and the head joint land from the controllers and the headset (metres), how far the hero turned
    between its pose job and the render (radians), which the body is off by while the hero turns, how often
    the hero's pose jobs changed rig (the body's blend restarted at each change before October 6 evening), the
    body's scale and its arms' on top of it, and whether those are from the player's T-pose calibration."""
    for _ in range(8):
        raw = game.read(address, 144)
        if len(raw) != 144:
            return None
        if struct.unpack_from('<3I', raw) != (0x53424453, 2, 144):
            raise RuntimeError('Body protocol mismatch')
        if struct.unpack_from('<Q', raw, 16)[0] & 1 or raw[16:24] != game.read(address+16, 8):
            continue
        v = struct.unpack('<4Iq3Q2I2Q3fI2ff2fIQ2IdfI', raw)
        return dict(state=BODY_STATES.get(v[3], v[3]), jobs=v[5], hero_jobs=v[6], solved=v[7],
                    problem=BODY_PROBLEMS.get(v[8], v[8]), joints=v[9], weight=round(v[12], 3),
                    scale=round(v[13], 4), yaw=round(v[14], 4), grounded=bool(v[15]),
                    hand_error_m=[round(v[16], 4), round(v[17], 4)], head_error_m=round(v[18], 4),
                    turn_last=round(v[19], 5), turn_max=round(v[20], 5), rig=hex(v[10]), rig_switches=v[21],
                    renders=v[22], hero_jobs_last_frame=v[23], hero_jobs_max=v[24], solve_ms=round(v[25], 3),
                    arm_scale=round(v[26], 4), calibrated=bool(v[27]))
    return None


BLOW_KINDS = ('hit', 'heavy', 'launch', 'juggle', 'slam', 'finisher')
CUE_WAYS = ('none', 'left', 'right', 'up', 'down')


def rope_snapshot(game, address):
    """Physical webs (game_swing::Ropes): the bends made round edges and taken off again, the webs cut off
    because something stayed in the way, the steps a web paid out while the game walked the player, each
    hand's web now (its bends, the rope beyond the last and all of it, metres), the times a hand took hold of the
    other hand's web, whether both hold one now, and (version 3) the thin or loose things arcade webs went
    through: a shot past one, a swing's line coming onto one."""
    for _ in range(8):
        raw = game.read(address, 96)
        if len(raw) != 96:
            return None
        magic, version, size = struct.unpack_from('<3I', raw)
        if (magic, size) != (0x53575250, 96) or version not in (2, 3):
            raise RuntimeError('Rope protocol mismatch')
        if struct.unpack_from('<Q', raw, 16)[0] & 1 or raw[16:24] != game.read(address+16, 8):
            continue
        wraps, unwraps, cut, paid_out = struct.unpack_from('<4Q', raw, 24)
        return dict(physical=bool(struct.unpack_from('<I', raw, 12)[0]), wraps=wraps, unwraps=unwraps, cut=cut,
                    paid_out=paid_out, bends=list(struct.unpack_from('<2I', raw, 56)),
                    free_m=[round(x, 2) for x in struct.unpack_from('<2f', raw, 64)],
                    whole_m=[round(x, 2) for x in struct.unpack_from('<2f', raw, 72)],
                    joins=struct.unpack_from('<Q', raw, 80)[0], shared=bool(struct.unpack_from('<I', raw, 88)[0]),
                    passed=struct.unpack_from('<I', raw, 92)[0] if version >= 3 else 0)
    return None
STRIKE_PHASES = ('idle', 'flying', 'hanging')


def strike_snapshot(game, address):
    """Fighting across the room (game_punch::Strikes): the web strikes opened, landed and missed, every blow
    landed (fists' and strikes') and of what kind, the thugs' flights given a blow's velocity, the strike in
    progress, the flow's count, and the latest strike and blow. One enemy at a time: `locked` is the enemy a blow
    locked the player onto, `knockouts` those his blows knocked out, `swung` the strikes a fist opened by
    travelling the called way, `strike_called` the strikes' blows that answered a call, and `passed_over` the
    swings that pointed at another enemy than the marked one and opened nothing."""
    for _ in range(8):
        raw = game.read(address, 328)
        if len(raw) != 328:
            return None
        if struct.unpack_from('<3I', raw) != (0x53535452, 3, 328):
            raise RuntimeError('Strike protocol mismatch')
        if struct.unpack_from('<Q', raw, 16)[0] & 1 or raw[16:24] != game.read(address+16, 8):
            continue
        opened, landed, missed, held = struct.unpack_from('<4Q', raw, 24)
        blows, heavies, launches, juggles, slams, finishers, flown, unflown = struct.unpack_from('<8Q', raw, 56)
        phase, hand, flow, enemies = struct.unpack_from('<4I', raw, 120)
        distance, seconds = struct.unpack_from('<2f', raw, 156)
        strength, damage = struct.unpack_from('<2f', raw, 188)
        kind, count, last_hand, by_strike, last_called = struct.unpack_from('<5I', raw, 196)
        # Called blows: the calls made, answered and taken away by another blow; and the call that shows
        # now (on whom, which way its arrow points, whether it calls the finisher, how much time it has).
        calls, called, missed_calls, cue_target = struct.unpack_from('<4Q', raw, 216)
        cue_way, cue_finisher, cue_left = struct.unpack_from('<2If', raw, 260)
        locked, knockouts = struct.unpack_from('<2Q', raw, 272)
        cue_locked, = struct.unpack_from('<I', raw, 300)
        swung, strike_called, passed_over = struct.unpack_from('<3Q', raw, 304)
        return dict(status=struct.unpack_from('<I', raw, 12)[0], opened=opened, landed=landed, missed=missed,
                    calls=calls, called=called, missed_calls=missed_calls, last_called=bool(last_called),
                    cue=dict(target=hex(cue_target), way=CUE_WAYS[cue_way] if cue_way < len(CUE_WAYS) else cue_way,
                             finisher=bool(cue_finisher), left=round(cue_left, 2), locked=bool(cue_locked),
                             point=[round(x, 3) for x in struct.unpack_from('<3f', raw, 248)]) if cue_target
                    else None,
                    locked=hex(locked), knockouts=knockouts,
                    knockout_point=[round(x, 3) for x in struct.unpack_from('<3f', raw, 288)],
                    swung=swung, strike_called=strike_called, passed_over=passed_over,
                    held=held, blows=blows, heavies=heavies, launches=launches, juggles=juggles, slams=slams,
                    finishers=finishers, flown=flown, unflown=unflown,
                    phase=STRIKE_PHASES[phase] if phase < len(STRIKE_PHASES) else phase, hand=hand, flow=flow,
                    enemies=enemies, target=hex(struct.unpack_from('<Q', raw, 136)[0]),
                    destination=[round(x, 3) for x in struct.unpack_from('<3f', raw, 144)],
                    distance=round(distance, 2), flight_seconds=round(seconds, 3),
                    last_point=[round(x, 3) for x in struct.unpack_from('<3f', raw, 164)],
                    last_direction=[round(x, 3) for x in struct.unpack_from('<3f', raw, 176)],
                    last_strength=round(strength, 3), last_damage=round(damage, 1),
                    last_kind=BLOW_KINDS[kind] if kind < len(BLOW_KINDS) else kind, last_count=count,
                    last_hand=last_hand, last_by_strike=bool(by_strike))
    return None


SENSE_ANSWERS = ('none', 'counter', 'dodge', 'jump')
# BotAttackDodgeData's Properties, as the game's reflection table names the bits (spider_sense.hpp attack::).
ATTACK_FLAGS = ('dodgeable', 'counterable', 'never_beaten', 'dodge_button', 'jump_button', 'dodge_left',
                'dodge_right', 'dodge_away', 'dodge_toward', 'no_auto_camera', 'reversable', 'camera_on_jump',
                'no_ranged_fx')


def attack_flags(flags):
    """An attack's flags by name; bits above the game's own thirteen as one number."""
    names = [name for bit, name in enumerate(ATTACK_FLAGS) if flags >> bit & 1]
    return names + ([hex(flags & ~0x1fff)] if flags & ~0x1fff else [])


def sense_snapshot(game, address):
    """Spider-Sense (game_sense::Data): the attacks the game warned the player of, read from the hero's own
    HeroDodgeManager at each input sample. `read` of `samples` found the manager; `managers` counts a new one per
    hero; `attacks` it holds now and `most_attacks` at once. `warnings` went off, `asked` by what they asked for
    (counter, dodge, jump), `unknown` named no actor in play. Counters: `thrown` by a fist thrown at the
    attacker, `landed` by a blow landing during his warning, `perfect` in its last fifth of a second;
    `counter_blows` the blows after them, `breaks` the flinches sent as the fist went out. `standing` the warnings
    now, soonest blow first; `census` what the game's warnings were made of (each set of flags, how often, and
    the longest warning it gave, game seconds)."""
    for _ in range(8):
        raw = game.read(address, 632)
        if len(raw) != 632:
            return None
        # Telemetry only: a mismatch is reported, never a reason to end the session (prompt_snapshot's rule).
        if struct.unpack_from('<3I', raw) != (0x53534e53, 1, 632):
            magic, version, size = struct.unpack_from('<3I', raw)
            return dict(protocol_mismatch=dict(magic=hex(magic), version=version, bytes=size))
        if struct.unpack_from('<Q', raw, 16)[0] & 1 or raw[16:24] != game.read(address+16, 8):
            continue
        samples, read, faults = struct.unpack_from('<3Q', raw, 24)
        managers, most, attacks, count = struct.unpack_from('<4I', raw, 48)
        manager, now = struct.unpack_from('<Qd', raw, 64)
        warnings, *asked = struct.unpack_from('<5Q', raw, 80)
        unknown, thrown, landed, perfect, blows, breaks, last = struct.unpack_from('<7Q', raw, 120)
        last_point = struct.unpack_from('<3f', raw, 176)
        last_left, last_hand = struct.unpack_from('<fI', raw, 188)
        standing = []
        for i in range(min(count, 6)):
            at = 200+56*i
            attacker, = struct.unpack_from('<Q', raw, at)
            head = struct.unpack_from('<3f', raw, at+8)
            to_impact, warned, flags, state = struct.unpack_from('<2f2I', raw, at+32)
            answer, timed, countered = struct.unpack_from('<3B', raw, at+48)
            left, = struct.unpack_from('<f', raw, at+52)
            standing.append(dict(attacker=hex(attacker), head=[round(x, 2) for x in head],
                                 to_impact=round(to_impact, 3) if timed else None, warned=round(warned, 3),
                                 flags=attack_flags(flags), state=hex(state),
                                 answer=SENSE_ANSWERS[answer] if answer < 4 else answer,
                                 countered=bool(countered), left=round(left, 3)))
        flags = struct.unpack_from('<8I', raw, 536)
        counts = struct.unpack_from('<8I', raw, 568)
        leads = struct.unpack_from('<8f', raw, 600)
        census = [dict(flags=attack_flags(f), bits=hex(f), count=n, longest=round(lead, 3) if lead >= 0 else None)
                  for f, n, lead in zip(flags, counts, leads) if n]
        return dict(status=struct.unpack_from('<I', raw, 12)[0], samples=samples, read=read, faults=faults,
                    managers=managers, most_attacks=most, attacks=attacks, manager=hex(manager),
                    clock=round(now, 3), warnings=warnings, asked=dict(zip(SENSE_ANSWERS, asked)),
                    unknown=unknown, thrown=thrown, landed=landed, perfect=perfect, counter_blows=blows,
                    breaks=breaks, last_counter=hex(last), last_point=[round(x, 2) for x in last_point],
                    last_left=round(last_left, 3) if last_left >= 0 else None,
                    last_hand=('left', 'right', 'blow')[last_hand] if last_hand < 3 else last_hand,
                    standing=standing, census=census)
    return None


def punch_snapshot(game, address):
    """Punches (game_punch::Data): landed per hand, the game's damage requests issued and dropped, the bots
    within reach of a fist, and the latest punch. unnamed: the issued requests (punches, web pulls and web
    balls alike) that went out without their damager, to a bot whose attack responses have no AwarenessDB
    to look one up in (the game crashes on a damager there)."""
    for _ in range(8):
        raw = game.read(address, 168)
        if len(raw) != 168:
            return None
        if struct.unpack_from('<3I', raw) != (0x53505544, 1, 168):
            raise RuntimeError('Punch protocol mismatch')
        if struct.unpack_from('<Q', raw, 16)[0] & 1 or raw[16:24] != game.read(address+16, 8):
            continue
        status = struct.unpack_from('<I', raw, 12)[0]
        samples, punches, issued, dropped = struct.unpack_from('<4Q', raw, 24)
        bots, error = struct.unpack_from('<2I', raw, 56)
        hands = []
        for i in range(2):
            count, target, strength, speed, knockback, busy = struct.unpack_from('<2Q2f2I', raw, 64+i*32)
            hands.append(dict(punches=count, last_target=hex(target), last_strength=round(strength, 3),
                              speed=round(speed, 2), last_knockback=knockback, busy=bool(busy)))
        point, direction = struct.unpack_from('<3f', raw, 128), struct.unpack_from('<3f', raw, 140)
        damage, speed = struct.unpack_from('<2f', raw, 152)
        return dict(status=status, samples=samples, punches=punches, issued=issued, dropped=dropped,
                    unnamed=struct.unpack_from('<Q', raw, 160)[0], bots=bots,
                    error=error, hands=hands, last_point=[round(x, 3) for x in point],
                    last_direction=[round(x, 3) for x in direction], last_damage=round(damage, 1),
                    last_speed=round(speed, 2))
    return None


GADGETS = ('none', 'web_shooter', 'impact_web', 'web_bomb', 'electric_web', 'trip_mine', 'concussion_blast',
           'spider_drone', 'suspension_matrix', 'game_web_shooter')


def gadgets_of(raw):
    """The player's gadgets in game_shooter::Data (v4): the weapon manager, the equipped weapon's id, why none was
    read, each gadget with its place on the wheel and its charges, the latest equip's result and how many were asked
    for, the equipped gadget's shots from the hand, and the pulls that found it without a charge."""
    manager, count, error, equipped, equip_result, equips = struct.unpack_from('<Q2I2HI', raw, 192)
    owned = []
    for i in range(min(count, 8)):
        weapon, kind, place, ammo, most = struct.unpack_from('<H4B', raw, 216 + i * 8)
        owned.append(dict(gadget=GADGETS[kind] if kind < len(GADGETS) else kind, weapon=weapon, place=place,
                          ammo=ammo, most=most))
    gadget_shots, empty_pulls = struct.unpack_from('<2Q', raw, 280)
    return dict(weapon_manager=hex(manager), gadget_error=error, equipped=equipped, gadgets=owned,
                equip_result=equip_result, equips=equips, gadget_shots=gadget_shots, empty_pulls=empty_pulls)


def shooter_snapshot(game, address):
    """Web-shooter shots (game_shooter::Data): pulls that asked for a shot, shots the game fired and requests it
    dropped, those aimed at a thug and those whose target the game took, the hero's gadget, the main-thread frames
    the module saw, shots per hand, the latest shot, and what the shots struck (v2): shots that hit anything,
    webbing blows dealt to thugs, props knocked, and the latest actor hit (0x0: the world)."""
    for _ in range(8):
        raw = game.read(address, 296)
        if len(raw) != 296:
            return None
        if struct.unpack_from('<3I', raw) != (0x53484f44, 4, 296):
            raise RuntimeError('Shooter protocol mismatch')
        if struct.unpack_from('<Q', raw, 16)[0] & 1 or raw[16:24] != game.read(address+16, 8):
            continue
        status = struct.unpack_from('<I', raw, 12)[0]
        samples, requested, fired, dropped, targeted, resolved, weapon, frames = struct.unpack_from('<8Q', raw, 24)
        bots, error = struct.unpack_from('<2I', raw, 88)
        hands = [dict(shots=shots, last_target=hex(target))
                 for shots, target in (struct.unpack_from('<2Q', raw, 96+i*16) for i in range(2))]
        origin, aim_point = struct.unpack_from('<3f', raw, 128), struct.unpack_from('<3f', raw, 140)
        collisions, webbed, pushed, last_hit = struct.unpack_from('<4Q', raw, 160)
        return dict(status=status, samples=samples, requested=requested, fired=fired, dropped=dropped,
                    targeted=targeted, resolved=resolved, weapon=hex(weapon), frames=frames, bots=bots, error=error,
                    hands=hands, last_origin=[round(x, 3) for x in origin],
                    last_aim_point=[round(x, 3) for x in aim_point],
                    last_shot=hex(struct.unpack_from('<Q', raw, 152)[0]), collisions=collisions, webbed=webbed,
                    pushed=pushed, last_hit=hex(last_hit), **gadgets_of(raw))
    return None


def eye_lod_sample(game, xr):
    """Update 28.1: SpidyEyeLodData, None if it cannot be read (a failure here only loses EYE LOD's record)."""
    try:
        return eye_lod.snapshot(game, xr['SpidyEyeLodData']) if 'SpidyEyeLodData' in xr else None
    except Exception:
        return None


def shadow_distance_sample(game, xr):
    """Update 33: SpidyShadowDistanceData, None if it cannot be read (a failure here only loses its record)."""
    try:
        return shadow_distance.snapshot(game, xr['SpidyShadowDistanceData']) if 'SpidyShadowDistanceData' in xr else None
    except Exception:
        return None


def far_shadows_sample(game, xr):
    """Update 35: SpidyFarShadowsData, None if it cannot be read (a failure here only loses its record)."""
    try:
        return far_shadows.snapshot(game, xr['SpidyFarShadowsData']) if 'SpidyFarShadowsData' in xr else None
    except Exception:
        return None


def shadow_camera_sample(game, xr):
    """Update 31: SpidyShadowCameraData, None if it cannot be read (a failure here only loses its record)."""
    try:
        return shadow_camera.snapshot(game, xr['SpidyShadowCameraData']) if 'SpidyShadowCameraData' in xr else None
    except Exception:
        return None


def appearance_snapshot(game, address):
    for _ in range(8):
        raw = game.read(address, 432)
        if len(raw) != 432:
            return None
        if struct.unpack_from('<3I', raw) != (0x53415044, 7, 432):
            raise RuntimeError('Native appearance protocol mismatch')
        if struct.unpack_from('<Q', raw, 16)[0] & 1 or raw[16:24] != game.read(address+16, 8):
            continue
        state = struct.unpack_from('<I', raw, 12)[0]
        left, right, srgb_left, srgb_right, actor = struct.unpack_from('<5Q', raw, 24)
        hidden_frames, hides, restores = struct.unpack_from('<3Q', raw, 64)
        handle, flags = struct.unpack_from('<2I', raw, 88)
        near_left, near_right, near, model = struct.unpack_from('<4Q', raw, 96)
        near_distance, near_radius = struct.unpack_from('<2f', raw, 128)
        near_handle, near_flags = struct.unpack_from('<2I', raw, 136)
        anchored, rejected = struct.unpack_from('<2Q', raw, 144)
        anchor_last, anchor_max = struct.unpack_from('<2f', raw, 160)
        anchor_sum = struct.unpack_from('<d', raw, 168)[0]
        web_state, web_live = struct.unpack_from('<2I', raw, 176)
        web_creates, web_releases, web_failures, web_updates = struct.unpack_from('<4Q', raw, 184)
        active_aligned, active_rejected = struct.unpack_from('<2Q', raw, 216)
        active_bounds = struct.unpack_from('<4f', raw, 232)
        active_shift = struct.unpack_from('<f', raw, 248)[0]
        exposure_shared = struct.unpack_from('<I', raw, 252)[0]
        shared_hero, lag_last, lag_max, lag_sum = struct.unpack_from('<Q2fd', raw, 256)
        web_start_last, web_start_max = struct.unpack_from('<2f', raw, 280)
        gap_frames, gap_last, gap_max, gap_sum = struct.unpack_from('<Q2fd', raw, 288)
        active_lag_frames, active_lag_last, active_lag_max = struct.unpack_from('<Q2f', raw, 312)
        eye_scale, eye_detail, detail_frames, request_jobs, request_raise, impostor_frames = \
            struct.unpack_from('<2f2QfI', raw, 328)
        lens = struct.unpack_from('<8f', raw, 360)
        asked = struct.unpack_from('<8f', raw, 392)
        lens_frames = struct.unpack_from('<Q', raw, 424)[0]
        return dict(hidden_avatar=[left, right], srgb_overlay=[srgb_left, srgb_right], player_actor=hex(actor),
                    hero_state=HERO_STATES.get(state, state), hero_handle=hex(handle), hero_flags=hex(flags),
                    native_hidden_frames=hidden_frames, native_hides=hides, native_restores=restores,
                    near_instances=[near_left, near_right], near_instance=hex(near), near_model=hex(model),
                    near_distance=near_distance, near_radius=near_radius, near_handle=hex(near_handle),
                    near_flags=hex(near_flags), anchored_frames=anchored, anchor_rejected=rejected,
                    anchor_last_m=anchor_last, anchor_max_m=anchor_max,
                    anchor_mean_m=anchor_sum/anchored if anchored else 0.0,
                    web_state=WEB_STATES.get(web_state, web_state), web_live=[bool(web_live & 1), bool(web_live & 2)],
                    web_creates=web_creates, web_releases=web_releases, web_failures=web_failures,
                    web_updates=web_updates, active_view_aligned=active_aligned,
                    active_view_rejected=active_rejected,
                    active_view_fov_deg=lens_degrees(*active_bounds) if active_aligned else None,
                    active_view_shift_m=active_shift,
                    # Eye images exposed from the game's own view (two a frame); 0 = each eye on its own.
                    exposure_shared=exposure_shared, shared_hero_frames=shared_hero,
                    hero_lag_last_m=lag_last, hero_lag_max_m=lag_max,
                    hero_lag_mean_m=lag_sum/shared_hero if shared_hero else 0.0,
                    web_start_error_m=web_start_last if web_start_last >= 0 else None,
                    web_start_error_max_m=web_start_max if web_start_max >= 0 else None,
                    # Tracked web shooter to the game's rope in the same image; near zero when webs leave the hands.
                    web_hand_gap_frames=gap_frames, web_hand_gap_last_m=gap_last, web_hand_gap_max_m=gap_max,
                    web_hand_gap_mean_m=gap_sum/gap_frames if gap_frames else 0.0,
                    active_view_lag_frames=active_lag_frames, active_view_lag_last_m=active_lag_last,
                    active_view_lag_max_m=active_lag_max,
                    # The engine's distance scale for the left eye's lens (1 for its 68-degree lens) and the
                    # multiplier the eyes got for it; 0 frames = the eyes kept their lenses' own distances.
                    eye_distance_scale=round(eye_scale, 4), eye_detail=round(eye_detail, 4),
                    eye_detail_frames=detail_frames,
                    # Eye model jobs whose texture requests were sized for the eye's image rather than its
                    # rendered scene, and how many times stronger the last one asked (1 / upscale).
                    texture_request_jobs=request_jobs, texture_request_raise=round(request_raise, 4),
                    # Frames the left eye chose which far buildings draw their full models (lit windows),
                    # standing in for the game's own view while it does not render; 0 = the game's view did.
                    impostor_frames=impostor_frames,
                    # Each eye's lens (left, right, top, bottom tangents; top is up and negative) as the game's
                    # constructor stored it, beside the tangents asked for from the headset's field of view:
                    # equal pairs mean the game kept the headset's lens, asymmetry and all (eye_lens_error).
                    eye_lens_frames=lens_frames,
                    eye_lens=[[round(v, 5) for v in lens[i*4:i*4+4]] for i in range(2)] if lens_frames else None,
                    eye_lens_asked=[[round(v, 5) for v in asked[i*4:i*4+4]] for i in range(2)]
                    if lens_frames else None,
                    eye_lens_error=round(max(abs(a-b) for a, b in zip(lens, asked)), 6) if lens_frames else None)
    return None


def eye_snapshot(game, address):
    """Header of the newest copy of the left eye as shown in the headset (include/spidy/eye_snapshot.hpp)."""
    for _ in range(8):
        raw = game.read(address, 112)
        if len(raw) != 112:
            return None
        if struct.unpack_from('<3I', raw) != (0x53455353, 1, 112):
            raise RuntimeError('Eye snapshot protocol mismatch')
        sequence = struct.unpack_from('<Q', raw, 16)[0]
        if sequence & 1 or raw[16:24] != game.read(address+16, 8):
            continue
        flags = struct.unpack_from('<I', raw, 12)[0]
        count, pixels, generation, serial = struct.unpack_from('<4Q', raw, 24)
        width, height, row_pitch, flat = struct.unpack_from('<4I', raw, 56)
        speed, gap = struct.unpack_from('<2f', raw, 72)
        points = struct.unpack_from('<8f', raw, 80)
        pixel = lambda i: [points[i], points[i+1]] if points[i] >= 0 and points[i+1] >= 0 else None
        return dict(sequence=sequence, count=count, pixels=pixels, generation=generation, serial=serial,
                    width=width, height=height, row_pitch=row_pitch, flat_screen=bool(flat), speed_mps=speed,
                    web_hand_gap_m=gap if gap >= 0 else None, game_web=[bool(flags & 1), bool(flags & 2)],
                    wrist_px=[pixel(0), pixel(2)], rope_start_px=[pixel(4), pixel(6)])
    return None


def write_rgb_png(path, width, height, rgb):
    def chunk(name, payload):
        return struct.pack('>I', len(payload))+name+payload+struct.pack('>I', zlib.crc32(name+payload))
    rows = b''.join(b'\0'+rgb[y*width*3:(y+1)*width*3] for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>2I5B', width, height, 8, 2, 0, 0, 0)) +
                     chunk(b'IDAT', zlib.compress(rows, 3))+chunk(b'IEND', b''))


def rgb_rows(data, row_pitch, left, top, width, height, step=1):
    """RGB bytes of every `step`-th pixel of a rectangle of RGBA rows. Returns (columns, rows, bytes)."""
    out = []
    columns = len(range(0, width, step))
    for y in range(top, top+height, step):
        row = data[y*row_pitch+left*4:y*row_pitch+(left+width)*4]
        rgb = bytearray(columns*3)
        for channel in range(3):
            rgb[channel::3] = row[channel::4*step]
        out.append(bytes(rgb))
    return columns, len(out), b''.join(out)


def remove_eye_files(folder, files):
    for name in files.values():
        if isinstance(name, str):
            (folder/name).unlink(missing_ok=True)


def crop_origin(center, size, limit):
    """Left or top edge of a `size`-pixel window around `center`, kept inside 0..limit."""
    return max(0, min(int(round(center))-size//2, limit-size))


def save_eye_snapshot(game, address, shot, folder, name, view=544, crop=512):
    """Save the newest eye copy: the whole image reduced to about `view` pixels, and a full-resolution
    `crop` around a hand that holds a game web. Returns the file names, or None if the copy was replaced
    or unreadable."""
    width, height, pitch = shot['width'], shot['height'], shot['row_pitch']
    if not (64 <= width <= MAX_EYE_SIZE and 64 <= height <= MAX_EYE_SIZE and width*4 <= pitch <= MAX_EYE_SIZE*4+4096) \
            or shot['pixels'] < 0x10000:
        return None
    size = pitch*(height-1)+width*4
    data = game.read(shot['pixels'], size)
    again = eye_snapshot(game, address)
    if len(data) != size or not again or (again['count'], again['sequence']) != (shot['count'], shot['sequence']):
        return None
    folder.mkdir(parents=True, exist_ok=True)
    files = {}
    step = max(1, math.ceil(max(width, height)/view))
    columns, rows, rgb = rgb_rows(data, pitch, 0, 0, width, height, step)
    write_rgb_png(folder/f'{name}-view.png', columns, rows, rgb)
    files['view'] = f'{name}-view.png'
    files['view_scale'] = step
    for hand, label in enumerate(('left', 'right')):
        point = shot['rope_start_px'][hand] or (shot['wrist_px'][hand] if shot['game_web'][hand] else None)
        if not point or not (0 <= point[0] < width and 0 <= point[1] < height):
            continue
        side = min(crop, width, height)
        left, top = crop_origin(point[0], side, width), crop_origin(point[1], side, height)
        columns, rows, rgb = rgb_rows(data, pitch, left, top, side, side)
        write_rgb_png(folder/f'{name}-{label}-hand.png', columns, rows, rgb)
        files[f'{label}_hand'] = f'{name}-{label}-hand.png'
        files[f'{label}_hand_origin'] = [left, top]
    return files


HISTORY_BYTES = 96  # Update 34.2: version 2 (64 before)
HISTORY_FAULTS = {1: 'root signature', 2: 'shader', 3: 'pipeline state', 4: 'descriptor heaps', 5: 'pictures'}


def history_snapshot(game, address):
    """The eye history (live.txt history): its percent at the end, pairs blended and started again, pairs
    without a noted view, the GPU milliseconds a measured pass took, and why it could not be built."""
    raw = game.read(address, HISTORY_BYTES)
    if len(raw) != HISTORY_BYTES or struct.unpack_from('<3I', raw) != (0x53454844, 2, HISTORY_BYTES):
        return None
    percent, fault, trail = struct.unpack_from('<3I', raw, 12)
    blended, restarted, unviewed = struct.unpack_from('<3Q', raw, 24)
    gpu_ms, timed = struct.unpack_from('<dQ', raw, 48)
    moving, cuts, travel_sum, travel_max, keep_min = struct.unpack_from('<2Qd2f', raw, 64)
    return dict(percent=percent, fault=HISTORY_FAULTS.get(fault, fault) if fault else None, blended_pairs=blended,
                restarted_pairs=restarted, unviewed_pairs=unviewed,
                gpu_ms_mean=round(gpu_ms/timed, 3) if timed else None, measured=timed,
                trail=trail, moving_pairs=moving, motion_cuts=cuts,
                travel_mean_m=round(travel_sum/(blended + restarted - unviewed), 4) if blended + restarted > unviewed else None,
                travel_max_m=round(travel_max, 4), keep_min=round(keep_min, 3))


SUMMARY_SETTINGS = ('aa', 'history', 'sharpen', 'web_style', 'reach', 'monitor')


def settings_summary(changes):
    """Update 21: the console's word on the graphics settings changed this session (live_changes, from live.txt
    and from the wrist hologram), in order, each with the value in force at the end."""
    seen = {}
    for change in changes:
        if change.get('name') in SUMMARY_SETTINGS:
            seen.setdefault(change['name'], []).append(change)
    if not seen:
        return ['Graphics settings: none changed this session (live.txt or the hologram).']
    lines = []
    for name in SUMMARY_SETTINGS:
        if name in seen:
            values = [live_settings.format_value(name, c['value']) for c in seen[name]]
            sources = sorted({c.get('source', 'live.txt') for c in seen[name]})
            lines.append(f"Graphics settings: {name} changed {len(values)} time(s) ({', '.join(sources)}): "
                         f"{' -> '.join(values[-8:])}; {values[-1]} at the end.")
    return lines


def history_line(h):
    """The console's word on the eye history (history_snapshot)."""
    if h['fault']:
        return f"Eye history: could not be built ({h['fault']}); the eyes were shown without it."
    if not h['blended_pairs'] and not h['restarted_pairs']:
        return 'Eye history: off all session (live.txt "history = 50" turns it on).'
    gpu = f", {h['gpu_ms_mean']:.2f} ms GPU a pair" if h['gpu_ms_mean'] is not None else ''
    motion = ''
    if h.get('trail') is not None:  # Update 34.2
        motion = (f" Motion: trail {h['trail']}%, {h.get('moving_pairs', 0)} pairs thinned for motion, "
                  f"{h.get('motion_cuts', 0)} cut for speed, least kept {h.get('keep_min')}, "
                  f"fastest {h.get('travel_max_m')} m a pair.")
    return (f"Eye history: {h['blended_pairs']} pairs blended, {h['restarted_pairs']} started again "
            f"(a cut or fast turn){gpu}; {h['percent']}% at the end.{motion}")


# Update 34.2: SpidyEngineTaaData (include/spidy/engine_taa.hpp Data): what ENGINE TAA did, for the report
# (Update 34/34.1 only said it to OutputDebugString: run341's report could not tell when it was on).
ENGINE_TAA_FORMAT = '<16I2Q8f6IQ'
ENGINE_TAA_BYTES = struct.calcsize(ENGINE_TAA_FORMAT)
ENGINE_TAA_STATES = {0: 'off', 1: 'on', 2: 'unavailable'}
ENGINE_TAA_REASONS = {0: None, 1: 'not prepared (-EngineTaa)', 2: "game AA not TAA", 3: 'mask differs',
                      4: 'no TAA targets', 5: 'targets gone', 6: 'slow (EYE AA instead)', 7: 'VR ended', 8: 'no eyes'}
JITTER_MODES = {0: 'OURS', 1: 'GAME', 2: 'PIXELS', 3: 'NONE'}


def engine_taa_snapshot(game, address):
    """ENGINE TAA's record: state, the jitter mode and scale, how often on/refused/off, and the jitter seen (ours
    and the game camera's, newest and largest: ~0.5 is pixels, ~0.0005 clip units at a 2000-pixel eye)."""
    raw = game.read(address, ENGINE_TAA_BYTES)
    if len(raw) != ENGINE_TAA_BYTES:
        return None
    v = struct.unpack_from(ENGINE_TAA_FORMAT, raw)
    if v[:3] != (0x41544e45, 1, ENGINE_TAA_BYTES):
        return None
    packed = v[3]
    return dict(state=ENGINE_TAA_STATES.get(packed & 0xff, packed & 0xff),
                reason=ENGINE_TAA_REASONS.get(packed >> 8 & 0xff, packed >> 8 & 0xff),
                jitter_mode=JITTER_MODES.get(v[4], v[4]), jitter_scale=v[5],
                game_aa=None if v[6] == 0xffffffff else v[6], mask_seen=v[7], widened=v[8],
                turned_on=v[9], refused=v[10], went_off=v[11],
                last_refusal=ENGINE_TAA_REASONS.get(v[12], v[12]), last_off=ENGINE_TAA_REASONS.get(v[13], v[13]),
                render=[v[14], v[15]], jittered_lenses=v[16], on_frames=v[17],
                eye_jitter=[round(x, 7) for x in v[18:20]], game_jitter=[round(x, 7) for x in v[20:22]],
                game_jitter_max=[round(x, 7) for x in v[22:24]], eye_jitter_max=[round(x, 7) for x in v[24:26]],
                targets=[v[26], v[27]], mode_changes=v[28], sequence=v[32])


def engine_taa_line(t):
    """The console's word on ENGINE TAA (engine_taa_snapshot)."""
    why = f" ({t['reason']})" if t['reason'] else ''
    return (f"ENGINE TAA: {t['state'].upper() if isinstance(t['state'], str) else t['state']}{why}; jitter "
            f"{t['jitter_mode']} at {t['jitter_scale']}%; on {t['turned_on']}x, refused {t['refused']}x, "
            f"off {t['went_off']}x; {t['on_frames']} frames on; largest jitter ours "
            f"{t['eye_jitter_max'][0]:.6f}/{t['eye_jitter_max'][1]:.6f}, game camera "
            f"{t['game_jitter_max'][0]:.6f}/{t['game_jitter_max'][1]:.6f}; targets L{t['targets'][0]} R{t['targets'][1]}.")


# 0.0.73 TAA SHAKE: SpidyEngineTaaEyes (include/spidy/engine_taa.hpp Eyes): what each eye's view holds where
# the game's TemporalAa reads it - the jitter stored, the size it is taken back with, the pixels that makes, the
# history stamps and the current frame's weight from them (1 = the history thrown away), read every 250 ms.
ENGINE_TAA_EYES_FORMAT = '<4I4f4I6I2f2I4f2I2IQ'
ENGINE_TAA_EYES_BYTES = struct.calcsize(ENGINE_TAA_EYES_FORMAT)


def jitter_level(mode, scale):
    """TAA JITTER's wrist word (engine_taa::levelText): FULL / HALF / OFF, the mode by name, else the percent."""
    if mode != 'OURS':
        return str(mode)
    return {100: 'FULL', 50: 'HALF', 0: 'OFF'}.get(scale, f'{scale}%')


def engine_taa_eyes_snapshot(game, address):
    """The per-eye TAA record (None until ENGINE TAA has been on, or from a probe without it)."""
    if not address:
        return None
    raw = game.read(address, ENGINE_TAA_EYES_BYTES)
    if len(raw) != ENGINE_TAA_EYES_BYTES:
        return None
    v = struct.unpack_from(ENGINE_TAA_EYES_FORMAT, raw)
    if v[:3] != (0x45544e45, 1, ENGINE_TAA_EYES_BYTES) or not v[3]:
        return None
    mode = JITTER_MODES.get(v[26], v[26])
    eyes = {}
    for i, side in enumerate(('left', 'right')):
        eyes[side] = dict(stored=[round(x, 8) for x in v[4 + 2 * i:6 + 2 * i]], taa_size=list(v[8 + 2 * i:10 + 2 * i]),
                          pixels=[round(x, 4) for x in v[22 + 2 * i:24 + 2 * i]], cut=v[12 + i],
                          cut_age=v[14 + i], stamp_age=v[16 + i], weight=round(v[18 + i], 4),
                          history_reset_samples=v[28 + i])
    return dict(samples=v[3], render=[v[20], v[21]], jitter_mode=mode, jitter_scale=v[27],
                jitter=jitter_level(mode, v[27]), sequence=v[30], **eyes)


def engine_taa_eyes_line(e):
    """The console's word on the per-eye TAA record."""
    def eye(name):
        t = e[name]
        return (f"{name} {t['pixels'][0]:+.3f}/{t['pixels'][1]:+.3f} px at {t['taa_size'][0]}x{t['taa_size'][1]}, "
                f"weight {t['weight']}, {t['history_reset_samples']} resets")
    return (f"TAA JITTER {e['jitter']}: eyes render {e['render'][0]}x{e['render'][1]}; newest {eye('left')}; "
            f"{eye('right')} ({e['samples']} samples).")


# 0.0.74 test 2b TAA DIAGNOSTICS: SpidyTaaDiag (include/spidy/taa_diag074.hpp Diag), every frame while the
# option is ON: per eye the jitter sent vs stored, the history ages and weight, shared or per-eye buffer sets.
TAA_DIAG_FRAME = 'I2f4f2i2i2f'
TAA_DIAG_RING = 16
TAA_DIAG_FORMAT = '<4IQ2I2I4I2f2I' + TAA_DIAG_FRAME * TAA_DIAG_RING + 'Q'
TAA_DIAG_BYTES = struct.calcsize(TAA_DIAG_FORMAT)


def taa_diag_snapshot(game, address):
    """The TAA DIAGNOSTICS record (None while it is off or missing)."""
    if not address:
        return None
    raw = game.read(address, TAA_DIAG_BYTES)
    if len(raw) != TAA_DIAG_BYTES:
        return None
    v = struct.unpack_from(TAA_DIAG_FORMAT, raw)
    if v[:3] != (0x44415454, 1, TAA_DIAG_BYTES) or not v[3] or not v[4]:
        return None
    head = v[15]
    frames = []
    for k in range(TAA_DIAG_RING):
        o = 17 + 13 * ((head + k) % TAA_DIAG_RING)
        f = v[o:o + 13]
        if not f[0] and not any(f[1:]):
            continue
        frames.append(dict(frame=f[0], sent=[round(x, 8) for x in f[1:3]],
                           left=dict(stored=[round(x, 8) for x in f[3:5]], cut_age=f[7], stamp_age=f[9], weight=round(f[11], 4)),
                           right=dict(stored=[round(x, 8) for x in f[5:7]], cut_age=f[8], stamp_age=f[10], weight=round(f[12], 4))))
    return dict(frames_recorded=v[4], resets=[v[5], v[6]], stamp_changes=[v[7], v[8]], shared_jitter_frames=v[9],
                shared_set_frames=v[10], per_eye_set_frames=v[11], sent_mismatch_frames=v[12],
                stored_over_sent=[round(v[13], 3), round(v[14], 3)], last_frames=frames)


def taa_diag_line(d):
    """The console's word on TAA DIAGNOSTICS."""
    n = max(1, d['frames_recorded'])
    history = 'per-eye' if d['per_eye_set_frames'] >= d['shared_set_frames'] else 'SHARED'
    return (f"TAA DIAGNOSTICS: {d['frames_recorded']} frames; history thrown away L {100 * d['resets'][0] / n:.0f}% "
            f"R {100 * d['resets'][1] / n:.0f}%; stamp moved L {d['stamp_changes'][0]} R {d['stamp_changes'][1]}; "
            f"stored/sent {d['stored_over_sent'][0]}x/{d['stored_over_sent'][1]}x; {history} history buffers; "
            f"same jitter both eyes {100 * d['shared_jitter_frames'] / n:.0f}%; {d['sent_mismatch_frames']} unexplained.")


# 0.0.74 test 2b TAA HOLD: SpidyTaaHoldData (include/spidy/taa_hold074.hpp Data), read from the eye JOBS
# TemporalAa itself reads: zero_ages counts jobs whose history the game was about to throw away.
TAA_HOLD_FORMAT = '<4I8Q6iI4xQ'
TAA_HOLD_BYTES = struct.calcsize(TAA_HOLD_FORMAT)


def taa_hold_snapshot(game, address):
    """The TAA HOLD record (None before the first eye job or when missing)."""
    if not address:
        return None
    raw = game.read(address, TAA_HOLD_BYTES)
    if len(raw) != TAA_HOLD_BYTES:
        return None
    v = struct.unpack_from(TAA_HOLD_FORMAT, raw)
    if v[:3] != (0x444c4854, 1, TAA_HOLD_BYTES) or not (v[4] or v[5]):
        return None
    return dict(on=bool(v[3]), jobs=[v[4], v[5]], held=[v[6], v[7]], zero_ages=[v[8], v[9]], cuts=[v[10], v[11]],
                last_frame=[v[12], v[13]], last_cut_age=[v[14], v[15]], last_stamp_age=[v[16], v[17]])


def taa_hold_line(h):
    """The console's word on TAA HOLD."""
    def pct(a, b):
        return f"{100 * a / max(1, b):.0f}%"
    return (f"TAA HOLD {'ON' if h['on'] else 'OFF'}: history about to be thrown away on L {pct(h['zero_ages'][0], h['jobs'][0])} "
            f"R {pct(h['zero_ages'][1], h['jobs'][1])} of eye jobs; held L {h['held'][0]} R {h['held'][1]}; "
            f"game cuts L {h['cuts'][0]} R {h['cuts'][1]}.")


# 0.0.74 test 3 TAA LINK / JITTER GUARD: SpidyTaaLinkData (include/spidy/taa_link074.hpp Data). Whether each eye's
# TAA history slot moves with the frame, as the game's own history check (0x2c98290) needs it to.
TAA_LINK_FORMAT = '<4I6Q8QQ4iQ'
TAA_LINK_BYTES = struct.calcsize(TAA_LINK_FORMAT)


def taa_link_snapshot(game, address):
    """The TAA LINK record (None before the first eye job or when missing)."""
    if not address:
        return None
    raw = game.read(address, TAA_LINK_BYTES)
    if len(raw) != TAA_LINK_BYTES:
        return None
    v = struct.unpack_from(TAA_LINK_FORMAT, raw)
    if v[:3] != (0x4b4e4c54, 1, TAA_LINK_BYTES) or not (v[4] or v[5]):
        return None
    return dict(guard=bool(v[3]), jobs=[v[4], v[5]], no_slot=[v[6], v[7]], refused=[v[8], v[9]],
                ages=[list(v[10:14]), list(v[14:18])], guarded_lenses=v[18], last_index=[v[19], v[20]],
                last_age=[v[21], v[22]], sequence=v[23])


def taa_link_line(t):
    """The console's word on the eyes' TAA history link."""
    def pct(a, b):
        return f"{100 * a / max(1, b):.0f}%"
    eyes = []
    for i, name in enumerate('LR'):
        a = t['ages'][i]
        refused = t['no_slot'][i] + t['refused'][i]
        eyes.append(f"{name} refused {pct(refused, t['jobs'][i])} (age 0/1/2/3+: {a[0]}/{a[1]}/{a[2]}/{a[3]})")
    return (f"TAA LINK: {'; '.join(eyes)}; JITTER GUARD {'ON' if t['guard'] else 'OFF'}, "
            f"{t['guarded_lenses']} lenses held still.")


# Update 34.1: SpidyXrStart's options field: its offset in the config and the ENGINE TAA bit.
OPTIONS_OFFSET = struct.calcsize('<4I7Q2If')
ENGINE_TAA_OPTION = 0x800000
# 0.0.74 test 3 MILES TAA (live.txt miles_taa, on by default): the eyes are made with the game's TAA targets and
# their scene at 67%, which the game's TemporalAa resolves to the full eye (include/spidy/miles_taa074.hpp).
MILES_TAA_OPTION = 0x1000000
# 0.0.74 test 4 MAIN VIEW SMALL (live.txt main_view_small, on by default): the game's own view at 50% per side, its
# TAA resolving it (include/spidy/taa_gate074.hpp); read when VR starts.
MAIN_VIEW_SMALL_OPTION = 0x2000000


def eye_aa_line(aa, asked):
    """The console's word on the eye anti-aliasing (timing_snapshot's eye_aa)."""
    if aa['fault']:
        return (f"Eye anti-aliasing: could not run ({aa['fault_reason']}); the eyes were shown as the game drew "
                'them.')
    if not aa['smoothed_pairs']:
        return 'Eye anti-aliasing: off.' if asked == 'off' else 'Eye anti-aliasing: no eye pair was smoothed.'
    cost = (f"{aa['gpu_ms_mean']:.2f} ms of GPU time per pair, both eyes (at most {aa['gpu_ms_max']:.2f})"
            if aa['measured'] else 'GPU time not measured')
    if aa['mode'] == 'downsample':
        return (f"Eye supersampling: {aa['smoothed_pairs']} eye pairs filtered down to the headset's size "
                f"(Mitchell-Netravali, linear light; no FXAA), {cost}.")
    return f"Eye anti-aliasing: FXAA on {aa['smoothed_pairs']} eye pairs, {cost}; now {aa['mode']}."


EYE_AA_MODES={0:'off',1:'fxaa',2:'downsample'}
EYE_AA_FAULTS={1:'root signature',2:'shader compile (d3dcompiler_47)',3:'pipeline',4:'descriptor heaps',
               5:'eye image not readable by shaders',6:'resources'}
# Update 16: why the headset sharpening switched itself off (native_eye_gpu.hpp eyeSharpenFault).
SHARPEN_FAULTS={1:'root signature',2:'shader compile (d3dcompiler_47)',3:'pipeline',4:'descriptor heaps',
                5:'eye image not readable by shaders',6:'headset image not drawable'}


def sharpen_line(sharp):
    """One console line on the headset sharpening (Update 16)."""
    if sharp['fault']:
        return (f"Headset sharpening: switched off ({sharp['fault_reason']}); the eyes were copied unsharpened, "
                f"{sharp['sharpened_pairs']} pairs sharpened before.")
    if not sharp['percent']:
        return 'Headset sharpening: off (no pass).'
    cost = (f"GPU {sharp['gpu_ms_mean']:.2f} ms mean, {sharp['gpu_ms_max']:.2f} max over {sharp['measured']} pairs"
            if sharp['measured'] else 'GPU time not measured')
    return f"Headset sharpening: {sharp['percent']}%, {sharp['sharpened_pairs']} pairs sharpened; {cost}."


def timing_snapshot(game, address):
    """The XR loop's stage times (XrTimingData, xr_timing.hpp). Version 2 adds copy_gpu (the headset copy's GPU
    finish after its submission) and pair_to_copy (after the game submitted the eye pair it copied); version 3
    the eye AA pass, version 4 (Update 16) the headset sharpening pass."""
    names=('frame','wait_frame','tracking','prepare','acquire','copy_wait','hand_overlay','release','end_frame',
           'display_period','copy_gpu','pair_to_copy','eye_aa_gpu')
    for _ in range(8):
        raw=game.read(address,504)
        if len(raw)<24:
            return None
        layout=struct.unpack_from('<3I',raw)
        if layout not in ((0x5358544d,1,344),(0x5358544d,2,408),(0x5358544d,3,456),(0x5358544d,4,504)):
            raise RuntimeError('XR timing protocol mismatch')
        if len(raw)<layout[2]:
            return None
        if struct.unpack_from('<Q',raw,16)[0]&1 or raw[16:24]!=game.read(address+16,8):
            continue
        result={}
        for i,name in enumerate(names[:min(layout[2]-24,13*32)//32]):
            count,total,maximum,last=struct.unpack_from('<Q3d',raw,24+32*i)
            result[name]=dict(count=count,mean_ms=total/count if count else 0,max_ms=maximum,last_ms=last)
        if layout[1]>=3:
            # The eye AA pass (native_eye_gpu.hpp eyeAATiming): mode now, why it stopped, pairs smoothed.
            mode,fault,pairs=struct.unpack_from('<2IQ',raw,24+13*32)
            result['eye_aa']=dict(mode=EYE_AA_MODES.get(mode,mode),fault=fault,
                                  fault_reason=EYE_AA_FAULTS.get(fault,f'code {fault}') if fault else None,
                                  smoothed_pairs=pairs,gpu_ms_mean=result['eye_aa_gpu']['mean_ms'],
                                  gpu_ms_max=result['eye_aa_gpu']['max_ms'],measured=result['eye_aa_gpu']['count'])
        if layout[1]>=4:
            # The headset sharpening (eyeSharpenTiming): percent now, why it stopped, pairs sharpened.
            count,total,maximum,last=struct.unpack_from('<Q3d',raw,456)
            percent,fault,pairs=struct.unpack_from('<2IQ',raw,488)
            result['sharpen']=dict(percent=percent,fault=fault,
                                   fault_reason=SHARPEN_FAULTS.get(fault,f'code {fault}') if fault else None,
                                   sharpened_pairs=pairs,gpu_ms_mean=total/count if count else 0,
                                   gpu_ms_max=maximum,measured=count)
        return result
    return None


# Slow motion's counters (SlowMotionData in game_xr.cpp); a change of one is a sample.
def steam_snapshot(game, address):
    """The guard on the game's Steam ticket call (game_steam.hpp): whether it is in (status 9801: the game held
    no Steam interface yet), the tickets asked for, those from the game's call on its way to the main menu, of
    those the ones that came with something for an identity (the game passes none: its stack's), and the latest
    such something."""
    raw = game.read(address, 56)
    if len(raw) != 56 or struct.unpack_from('<3I', raw) != (0x53544b54, 1, 56):
        return None
    installed, status = struct.unpack_from('<2I', raw, 12)
    calls, from_game, cleared, identity = struct.unpack_from('<4Q', raw, 24)
    return dict(installed=bool(installed), status=status, calls=calls, from_game=from_game, cleared=cleared,
                last_identity=hex(identity))


SLOW_MOTION_COUNTS = ('starts', 'stops', 'refused', 'emptied', 'interrupted')
SLOW_MOMENTS = ('blow', 'called_blow', 'air_pull', 'perfect_dodge')


def slow_motion_snapshot(game, address):
    """Slow motion: the player's presses, focus and the game's time (status 0: the game's time is hooked)."""
    for _ in range(8):
        raw = game.read(address, 120)
        if len(raw) != 120:
            return None
        if struct.unpack_from('<3I', raw) != (0x574f4c53, 2, 120):
            raise RuntimeError('Slow motion protocol mismatch')
        if struct.unpack_from('<Q', raw, 16)[0] & 1 or raw[16:24] != game.read(address+16, 8):
            continue
        result = dict(status=struct.unpack_from('<I', raw, 12)[0])
        counts = struct.unpack_from('<6I', raw, 24)
        result.update(zip(SLOW_MOTION_COUNTS, counts[:5]), active=bool(counts[5]))
        # Real seconds in it so far, focus and how far into it now (0-1), the time scale asked of the
        # game; the game's own scale and its world's in its last time update, and Havok's step then.
        result.update(zip(('seconds', 'focus', 'blend', 'wanted', 'game_scale', 'world_scale', 'physics_step'),
                          (round(v, 6) for v in struct.unpack_from('<7f', raw, 48))))
        result['physics_scaled'] = bool(struct.unpack_from('<I', raw, 76)[0])
        result.update(zip(('updates', 'slowed_updates'), struct.unpack_from('<2Q', raw, 80)))
        # Moments (time slowed by itself): those begun by kind, whether one is on, and its time scale.
        moments = struct.unpack_from('<4I', raw, 96)
        active, scale = struct.unpack_from('<If', raw, 112)
        result.update(moments=dict(zip(SLOW_MOMENTS, moments)), moment_active=bool(active),
                      moment_scale=round(scale, 3))
        return result
    return None


# Body haptics on bHaptics devices (game_xr.cpp HapticsData; bhaptics_output.hpp's Status, body_haptics.hpp's
# Device bits and Kind order).
HAPTICS_STATUS = ('off', 'starting', 'no_library', 'no_app', 'no_player', 'waiting', 'connected', 'playing', 'failed',
                  'rejected')
HAPTIC_DEVICES = ((1, 'vest'), (2, 'left_sleeve'), (4, 'right_sleeve'), (8, 'visor'))
HAPTIC_KINDS = ('web_shot', 'web_attach', 'web_taken', 'web_release', 'zip', 'grab_caught', 'grab_yanked', 'grab_held',
                'grab_let_go', 'air_pull', 'web_pull', 'wall_land', 'wall_jump', 'landing', 'leap_charge', 'leap',
                'punch', 'heavy_blow', 'finisher', 'strike_rush', 'knockout', 'counter', 'spider_sense',
                'sense_dodge', 'hit', 'heal', 'slow_start', 'slow_stop', 'perfect_dodge', 'dodge', 'calibrated',
                'scan', 'gadget', 'phone', 'wall_grip')
HAPTICS_BYTES = 116 + 4*len(HAPTIC_KINDS)
# The groups the SPIDY VR tab's switches leave on (body_haptics.hpp's Group bits).
HAPTIC_GROUPS = ((1, 'webs'), (2, 'wind'), (4, 'combat'), (8, 'movement'))
# The output thread's ticks a second (bhaptics_output.hpp's tickMs).
HAPTIC_TICKS = 25


def haptics_snapshot(game, address):
    """Body haptics: what the connection to the bHaptics Player does (no_player for a PC without one), BODY
    HAPTICS now (percent; `written` once the session has said), the devices on now and at any time, the Player's
    connections, events posted, frames sent to a device and refused, blows the player took (the aimed ones placed
    by the attacker the game warned of), landings, the seconds of VR play with the Player connected and each
    device's share of them with something playing and its strongest motor's mean over those (percent), the
    groups the tab's switches leave on, and the events by kind that came (played or not)."""
    for _ in range(8):
        raw = game.read(address, HAPTICS_BYTES)
        if len(raw) != HAPTICS_BYTES:
            return None
        if struct.unpack_from('<3I', raw) != (0x53485044, 2, HAPTICS_BYTES):
            raise RuntimeError('Body haptics protocol mismatch')
        sequence = struct.unpack_from('<Q', raw, 16)[0]
        if sequence & 1 or raw[16:24] != game.read(address+16, 8):
            continue
        status = struct.unpack_from('<I', raw, 12)[0]
        level, devices, seen, connects = struct.unpack_from('<4I', raw, 24)
        events, frames, refused = struct.unpack_from('<3Q', raw, 40)
        hits, aimed, landings, live = struct.unpack_from('<4I', raw, 64)
        on = struct.unpack_from('<4I', raw, 80)
        peaks = struct.unpack_from('<4I', raw, 96)
        kinds = struct.unpack_from(f'<{len(HAPTIC_KINDS)}I', raw, 112)
        groups = struct.unpack_from('<I', raw, 112 + 4*len(HAPTIC_KINDS))[0]
        played = {name: dict(share=round(on[i]/live, 3), mean_peak=round(peaks[i]/on[i], 1))
                  for i, (_, name) in enumerate(HAPTIC_DEVICES) if on[i] and live}
        return dict(written=sequence > 0,
                    status=HAPTICS_STATUS[status] if status < len(HAPTICS_STATUS) else status, level=level,
                    devices=[name for bit, name in HAPTIC_DEVICES if devices & bit],
                    devices_seen=[name for bit, name in HAPTIC_DEVICES if seen & bit], connects=connects,
                    events=events, frames=frames, refused=refused, hits=hits, aimed_hits=aimed, landings=landings,
                    live_seconds=round(live/HAPTIC_TICKS, 1), played=played,
                    groups=[name for bit, name in HAPTIC_GROUPS if groups & bit],
                    kinds={kind: n for kind, n in zip(HAPTIC_KINDS, kinds) if n})
    return None


# Update 20: the wrist hologram's gesture and the MUSIC player (game_xr.cpp WristHudTelemetry).
WRIST_HUD_BYTES = 192
WRIST_HUD_STATE = ((1, 'hud_setting_on'), (2, 'hud_failed'), (4, 'hud_menu_failed'), (8, 'controllers_play_vr'),
                   (16, 'open'), (32, 'gestures_play'), (64, 'gameplay'), (128, 'flat_screen'), (256, 'pad_layer'),
                   (512, 'phone_open'), (1024, 'wheel_open'), (2048, 'calibrating'), (4096, 'hands_tracked'),
                   (8192, 'headset_viewing'))
WRIST_HUD_REACH_M = .15


def wrist_hud_snapshot(game, address):
    """The wrist hologram (Update 20): what its gesture saw - the pointing hand's distance from the hologram
    arm's forearm now and at its nearest (metres; None while a hand is untracked or the controllers do not play
    VR), that hand's trigger pulls (in reach, out of reach, inside the rearm, while not playing) and the distance
    at the last, the toggles and F8's presses, toggles and refusals, the state now and at the last refused one,
    and the MUSIC player's songs, song, play state and note (`music_read` once the player said)."""
    for _ in range(8):
        raw = game.read(address, WRIST_HUD_BYTES)
        if len(raw) != WRIST_HUD_BYTES:
            return None
        if struct.unpack_from('<3I', raw) != (0x53574844, 1, WRIST_HUD_BYTES):
            raise RuntimeError('Wrist hologram protocol mismatch')
        sequence = struct.unpack_from('<Q', raw, 16)[0]
        if sequence & 1 or raw[16:24] != game.read(address+16, 8):
            continue
        state = struct.unpack_from('<I', raw, 12)[0]
        distance, nearest, pressed, opacity = struct.unpack_from('<4f', raw, 24)
        (presses, in_reach, out_of_reach, not_playing, rearming, toggles, key_toggles, key_presses, key_refused,
         blocked, tracks) = struct.unpack_from('<11I', raw, 40)
        track = struct.unpack_from('<i', raw, 84)[0]
        music = struct.unpack_from('<I', raw, 88)[0]
        note = raw[92:192].split(b'\0', 1)[0].decode('utf-8', 'replace')
        metres = lambda v: round(v, 3) if v >= 0 and v == v else None
        return dict(written=sequence > 0, state=[name for bit, name in WRIST_HUD_STATE if state & bit],
                    distance_m=metres(distance), nearest_m=metres(nearest), press_distance_m=metres(pressed),
                    reach_m=WRIST_HUD_REACH_M, opacity=round(opacity, 2), presses=presses,
                    presses_in_reach=in_reach, presses_out_of_reach=out_of_reach,
                    presses_not_playing=not_playing, presses_rearming=rearming, toggles=toggles,
                    f8_presses=key_presses, f8_toggles=key_toggles, f8_refused=key_refused,
                    refused_state=[name for bit, name in WRIST_HUD_STATE if blocked & bit],
                    music_read=bool(music & 8), music_available=bool(music & 1), music_playing=bool(music & 2),
                    music_paused=bool(music & 4), music_tracks=tracks, music_track=track, music_note=note)
    return None


def wrist_hud_lines(now, before):
    """The console's lines for what changed in the wrist hologram since `before` (Update 20)."""
    before = before or {}
    hand = 'pointing'
    lines = []
    if now['presses'] > before.get('presses', 0):
        d = now['press_distance_m']
        if d is None:
            lines.append('Wrist hologram: trigger pulled with a hand untracked - not measured.')
        else:
            lines.append(f'Wrist hologram: {hand} trigger pulled {d*100:.0f} cm from the hologram forearm '
                         f'({"in reach" if d <= WRIST_HUD_REACH_M else "needs 15 cm or nearer"}).')
    if now['presses_not_playing'] > before.get('presses_not_playing', 0) and now['presses_not_playing'] <= 5:
        lines.append('Wrist hologram: a trigger pull was not taken - the controllers were not playing VR '
                     f'({", ".join(now["refused_state"]) or "no state"}).')
    if now['toggles'] != before.get('toggles', 0):
        lines.append(f'Wrist hologram: {"opened" if "open" in now["state"] else "closed"} '
                     f'(toggle {now["toggles"]}).')
    if now['f8_refused'] > before.get('f8_refused', 0):
        lines.append('Wrist hologram: F8 refused - it opens only in VR gameplay with the phone, wheel and pad '
                     f'layer closed (state: {", ".join(now["refused_state"]) or "none"}).')
    if now['music_read'] and not before.get('music_read'):
        lines.append(f'Music: {now["music_tracks"]} song(s) found'
                     + ('' if now['music_available'] else ', player off')
                     + (f' - {now["music_note"]}' if now['music_note'] else '') + '.')
    elif now['music_note'] and now['music_note'] != before.get('music_note'):
        lines.append(f'Music: {now["music_note"]}')
    return lines


def stage_haptics(digest):
    """bHaptics' library beside the staged stereo module (reports/stereo-modules/<digest>), which loads it from its
    own folder once body haptics are on (bhaptics_output.hpp). Without it a session plays no body haptics, and says
    so; a copy the game already holds stays."""
    source = ROOT/'build/windows-ninja/bhaptics_library.dll'
    target = ROOT/'reports/stereo-modules'/digest/source.name
    if not source.exists():
        return
    try:
        if not target.exists() or target.read_bytes() != source.read_bytes():
            shutil.copyfile(source, target)
    except OSError as error:
        print(f'WARNING: the bHaptics library could not be staged ({error}): no body haptics this session.',
              flush=True)


# The prompt and QTE interface's eight names, in the order PromptData counts them.
PROMPT_CALL_NAMES = ('ShowPlaceablePrompt', 'HidePlaceablePrompt', 'SetPlaceablePromptProgress',
                     'UpdateInWorldPrompt', 'POIInteractAvailable', 'AddQTE', 'UpdateQTE', 'ClearQTE',
                     'ShowText', 'HideText')


# The VR controllers' own buttons (ControllerButton, tracking.hpp). On Touch, A and B are the right
# controller's and X and Y the left one's, so a session where B arrives and Y never does says which hand.
CONTROLLER_BUTTONS = ((1, 'a'), (2, 'b'), (4, 'x'), (8, 'y'), (16, 'menu'), (32, 'view'),
                      (64, 'dpad_up'), (128, 'dpad_down'), (256, 'dpad_left'), (512, 'dpad_right'))


def controller_buttons(bits):
    """The names of the controller buttons in `bits`."""
    return [name for bit, name in CONTROLLER_BUTTONS if bits & bit]


def prompt_snapshot(game, address):
    """What the game asked a player for (game_prompts.hpp PromptData v2): its calls into Flash and the prompt
    and QTE interface's among them, prompts raised and taken away, texts that named a button and texts
    rewritten for VR, what it is asking for now and last, the asks VR play could not have sent on its own,
    and texts that came with the keyboard's icons.

    Then the two lists that say where a prompt's button and text actually arrive, no prompt text having
    reached the localized lookup on October 13: `recent`, the newest prompt calls with their arguments, and
    `census`, every distinct name the game's UI called with how often and one example of its arguments."""
    for _ in range(8):
        raw = game.read(address, 12112)
        if len(raw) != 12112:
            return None
        # A mismatch here is reported, never raised. Everything in this struct is
        # diagnostics, and on October 13 raising it ended three sessions: the module
        # still said version 1 while this reader wanted 2 (the census raised the size
        # but not the version), so the loop died and the game dropped out of VR to
        # the flat screen. Nothing a player is doing is worth that.
        magic, version, size = struct.unpack_from('<3I', raw)
        if (magic, version, size) != (0x53505244, 4, 12112):
            return dict(protocol_mismatch=dict(magic=hex(magic), version=version, bytes=size,
                                               wanted=dict(magic=hex(0x53505244), version=4, bytes=12112)))
        if struct.unpack_from('<Q', raw, 16)[0] & 1 or raw[16:24] != game.read(address+16, 8):
            continue
        text_at = lambda at, size: raw[at:at+size].split(b'\0', 1)[0].decode('utf-8', 'replace')
        result = dict(zip(('invokes', 'calls', 'raised', 'cleared', 'texts', 'rewrites'),
                          struct.unpack_from('<6Q', raw, 24)))
        demand, last_demand, beyond_play, keyboard_texts = struct.unpack_from('<4I', raw, 72)
        result.update(asking=demand_name(demand), last_asked=demand_name(last_demand),
                      beyond_play=beyond_play, keyboard_texts=keyboard_texts)
        lines = struct.unpack_from('<I', raw, 88)[0]
        counts = struct.unpack_from('<10I', raw, 96)
        result['by_name'] = {name: n for name, n in zip(PROMPT_CALL_NAMES, counts) if n}
        # One timeline: the game's prompt calls and this session's own presses, in order,
        # each stamped with the milliseconds since the first of them ('>>' marks ours).
        result['timeline'] = [t for t in (text_at(136+i*128, 128) for i in range(min(lines, 32))) if t]
        names = struct.unpack_from('<I', raw, 4232)[0]
        calls = struct.unpack_from('<48I', raw, 4240)
        census = []
        for i in range(min(names, 48)):
            example = text_at(4432+i*160, 160)
            if example:
                census.append(dict(call=example, times=calls[i]))
        result['census'] = sorted(census, key=lambda c: -c['times'])
        return result
    return None


def hud_snapshot(game, address):
    """The HUD in VR (native_hud::Data v3): its panel in front of the head, where the lazy follow turned it, its
    texture at the window's size, its second movie (markers, subtitles, prompts) drawn on it, markers projected
    onto it, and the HUD setting (size 0 off, 1-3 small to large) with what leaving it out of the eyes did."""
    for _ in range(8):
        raw = game.read(address, 192)
        if len(raw) != 192:
            return None
        if struct.unpack_from('<3I', raw) != (0x53485544, 3, 192):
            raise RuntimeError('HUD protocol mismatch')
        if struct.unpack_from('<Q', raw, 16)[0] & 1 or raw[16:24] != game.read(address+16, 8):
            continue
        result = dict(zip(('game_frames', 'placed_frames', 'rejected_frames'), struct.unpack_from('<3Q', raw, 40)))
        result.update(game_scale=[round(v, 4) for v in struct.unpack_from('<3f', raw, 64)])
        result.update(zip(('game_distance', 'view_half_width', 'distance', 'half_width', 'offset_mismatch_m'),
                          (round(v, 4) for v in struct.unpack_from('<5f', raw, 76))))
        result.update(texture=list(struct.unpack_from('<2I', raw, 96)),
                      game_texture=list(struct.unpack_from('<2I', raw, 104)))
        result.update(zip(('texture_resizes', 'texture_restores', 'layer_frames', 'marker_projections'),
                          struct.unpack_from('<4Q', raw, 112)))
        layer_status, restored = struct.unpack_from('<2I', raw, 144)
        result.update(layer_status=layer_status, restored=bool(restored))
        result['size'] = struct.unpack_from('<I', raw, 152)[0]
        # Markers are projected before the game moves the player, from the head foreseen for the frame: the
        # largest distance from it to the frame's head (0 from builds before October 10's fourth).
        result['marker_mismatch_m'] = round(struct.unpack_from('<f', raw, 156)[0], 4)
        result.update(zip(('hidden_frames', 'hidden_draws', 'followed_frames'), struct.unpack_from('<3Q', raw, 160)))
        result.update(zip(('follow_angle', 'follow_angle_max'),
                          (round(v, 2) for v in struct.unpack_from('<2f', raw, 184))))
        return result
    return None


def wait_for_renderer(game, process, render_data, timeout=180, problems=None, tell_every=20, tick=None):
    """The game's graphics queue, as soon as the game draws frames (its intro, seconds after it starts).

    `render_data` is the render memory module's telemetry, when that module is loaded: its frame count
    shows the renderer running before the queue is looked for. `problems` collects why each look found
    no queue (for the session's report); the console hears the latest every `tell_every` seconds. `tick()`
    runs each look (0.0.74 test 3: the game's module list for CLEARER ERRORS, MOD CHECK's watch); it never stops it.
    """
    deadline = time.monotonic()+timeout
    problem = 'the game drew no frames'
    told, fronted = time.monotonic(), False
    # SM2 port: the render memory module's frame count is only a hint. Should it count no frames (its
    # hooks not reached in this build), look for the queue anyway after `frame_patience` seconds.
    frame_patience = time.monotonic()+30
    while alive(game) and time.monotonic() < deadline:
        if tick:
            try:
                tick()
            except Exception:  # noqa: BLE001 - a watch never ends the start
                pass
        memory = render_memory_snapshot(game, render_data) if render_data else None
        if render_data and (not memory or memory['frames'] < 30) and time.monotonic() < frame_patience:
            time.sleep(.25)
            continue
        try:
            return discover_queue(game, process)
        except TimeoutError as error:
            # A call into the game that never returned: asking again would leave another one waiting there.
            raise RuntimeError(f"The game did not answer Spidy's render probe: {error}") from error
        except GameNeedsRestart:
            raise  # another build's render probe in the game: asking for three minutes changes nothing
        except (OSError, RuntimeError) as error:
            # Startup can show no queue, or a second one, for a moment; a module list can fail meanwhile.
            problem = str(error)
            if problem == 'Render discovery start: 1001':
                # SM2 port: 1001 is the probe refusing this process as not Spider-Man2.exe. That never
                # changes while the game runs, so stop now rather than retry for three minutes.
                raise RuntimeError("Spidy's render probe refused this game process (code 1001: not "
                                   'Spider-Man2.exe). Rebuild with tools\\build.ps1 -Observer and restart the game.') from error
            if problems is not None and problem not in problems[-1:]:
                problems.append(problem)
            if 'no graphics work' in problem and not fronted:
                fronted = True  # once: it taps Alt
                bring_to_front(game.pid)  # a minimized game may draw nothing
            if time.monotonic()-told >= tell_every:
                print(f"Still looking for the game's graphics queue: {problem}", flush=True)
                told = time.monotonic()
            time.sleep(.5)
    if not alive(game):
        raise RuntimeError('The game closed while starting.')
    raise RuntimeError(f'No game graphics queue within {timeout:.0f} seconds: {problem.rstrip(".")}.')


def game_running():
    try:
        find_game()
        return True
    except RuntimeError as error:
        return not str(error).startswith('Spider-Man 2 is not running.')


# Spidy's modules start once per game process and stay in it until the game exits. The XR module goes in
# just before VR starts, so a game that has it has had its VR session: a second start returns 1000.
XR_MODULE = 'spidy_stereo_probe.dll'
VR_RAN = ('VR already ran in this game, and Spidy VR starts once per game launch. Close Spider-Man, then '
          'press START VR: the launcher starts the game again.')


def had_vr(pid, modules_of=modules):
    """Whether VR already ran in the game process `pid` (STOP VR, or VR that ended, leaves the game running)."""
    try:
        return any(m['name'].lower() == XR_MODULE for m in modules_of(pid))
    except (OSError, RuntimeError):
        return False  # closing, or still being set up; the XR start refuses a second session anyway


# The modules a session loads into the game, each staged in its folder under reports/ (bridge_game.prepare):
# the render memory module first, as the game starts, then the render probe, the bridges and the XR module.
SESSION_MODULES = (('spidy_render_memory.dll', 'render-memory-modules'), ('spidy_render_probe.dll', 'render-modules'),
                   ('spidy_bridge.dll', 'bridge-modules'), ('spidy_ray_bridge.dll', 'ray-modules'),
                   ('spidy_movement_bridge.dll', 'motion-modules'), (XR_MODULE, 'stereo-modules'))


def other_build(pid, modules_of=modules, root=ROOT):
    """Another Spidy build's module in the game process `pid`: (True, the Spidy folder it came from, None when
    its path says none), or None when every Spidy module there is this folder's build. A game left running
    while Spidy was updated, or one another Spidy folder put VR in, holds such a module until it exits, and
    each of this build's then fails to go in (flardy, October 16: "Different bridge build is resident")."""
    try:
        loaded = {m['name'].lower(): m['path'] for m in modules_of(pid)}
    except (OSError, RuntimeError):
        return None  # closing, or still being set up
    for name, staging in SESSION_MODULES:
        if name not in loaded:
            continue
        try:
            digest = hashlib.sha256((root/'build/windows-ninja'/name).read_bytes()).hexdigest()
        except OSError:
            digest = None
        if digest is None or not staged(loaded[name], root/'reports'/staging, digest):
            return True, spidy_folder(pathlib.Path(loaded[name]).resolve())
    return None


def restart_reason(pid, modules_of=modules):
    """Why the running game `pid` cannot start VR until it is closed and started again (VR ran in it, or
    another build's modules are in it), or None."""
    found = other_build(pid, modules_of)
    if found:
        return other_build_text(found[1])
    return VR_RAN if had_vr(pid, modules_of) else None


# SM2: SpidyStart (src/game_bridge.cpp) names the MinHook hook that failed: 1200 + 20 * hook + status to create
# it, 1300 + 20 * hook + status to enable it (hook 0 keeps the old 1200 + status).
BRIDGE_HOOK_NAMES = ('input commit 0x334d950', 'input clear 0x31c8c20', 'digital query 0x31c5060',
                     'analog query 0x31c4e40', 'combat camera update 0x9907c0')
MINHOOK_STATUS = ('MH_OK', 'MH_ERROR_ALREADY_INITIALIZED', 'MH_ERROR_NOT_INITIALIZED', 'MH_ERROR_ALREADY_CREATED',
                  'MH_ERROR_NOT_CREATED', 'MH_ERROR_ENABLED', 'MH_ERROR_DISABLED', 'MH_ERROR_NOT_EXECUTABLE',
                  'MH_ERROR_UNSUPPORTED_FUNCTION', 'MH_ERROR_MEMORY_ALLOC', 'MH_ERROR_MEMORY_PROTECT',
                  'MH_ERROR_MODULE_NOT_FOUND', 'MH_ERROR_FUNCTION_NOT_FOUND')


def bridge_start_text(code):
    """SpidyStart's code, with the hook and MinHook status a 12xx or 13xx code stands for."""
    for step, start in (('create', 1200), ('enable', 1300)):
        hook, status = divmod(code - start + 1, 20)
        status -= 1  # MH_UNKNOWN (-1) sits just below its hook's block
        if 0 <= hook < len(BRIDGE_HOOK_NAMES) and -1 <= status < len(MINHOOK_STATUS):
            name = MINHOOK_STATUS[status] if status >= 0 else 'MH_UNKNOWN'
            text = f'{code} ({step} hook {hook}, {BRIDGE_HOOK_NAMES[hook]}: {name})'
            if status == 8:
                text += ' - MinHook cannot copy this entry: run tools/bootstrap.ps1 -Observer and rebuild'
            return text
    return str(code)


def exit_code(error):
    """What a session that could not start tells the launcher's window: 2, or the code at which it offers to
    start again as administrator, or to close the game and start VR again."""
    if isinstance(error, NeedsAdministrator):
        return NEEDS_ADMINISTRATOR_EXIT
    return CLOSE_GAME_EXIT if isinstance(error, GameNeedsRestart) else 2


# Motion blur, film grain, chromatic aberration and vignette left this list in Update 11: their registry names
# are confirmed (vr_display.PICTURE 'comfort') and every session turns them off.
UNCONFIRMED_COMFORT = ('Screen Shake Off', 'Lens Flares Off', 'Ambient Occlusion Off',
                       'Screen Space Reflections Low/Off', 'Shadow Quality High', 'Level of Detail High',
                       'Crowd Detail Medium/High', 'Hair Quality High')


def settle_mods(running):
    """0.0.74 test 3 MOD CHECK: the mod loaders a session moved aside go back when the game is closed; while it
    runs, a small waiter puts them back as it closes (and the next START VR does, whatever happens)."""
    try:
        if not running:
            mod_check.restore(ROOT/'reports')
        elif mod_check.pending(ROOT/'reports'):
            mod_check.restore_when_closed(find_game(), ROOT/'reports')
    except Exception:  # noqa: BLE001 - the session's end never fails on it
        pass


def settle_display(running):
    """Restore the game settings saved for a VR launch (vr_display) once the game has closed."""
    if not vr_display.pending() or getattr(settle_display, 'deferred', False):
        return
    if running:
        settle_display.deferred = True
        print('Your game settings (display, frame generation, controllers) are restored after the game closes, '
              r'the next time Spidy VR starts it (or run tools\vr_display.py --restore).', flush=True)
        return
    try:
        kept = vr_display.kept_in_game()
        if kept:
            print('Kept as you changed them in the game during VR: '+', '.join(f'{n} = {v}' for n, v in kept.items())
                  + '.', flush=True)
        if vr_display.restore():
            print('Restored your game settings (display, frame generation, controllers).', flush=True)
    except OSError as error:
        settle_display.deferred = True
        print(f'WARNING: Windows refused restoring your game settings ({error}); the next time Spidy VR '
              r'starts the game it tries again (or run tools\vr_display.py --restore).', flush=True)


def picture_kept(a):
    """The game's picture settings a session leaves as the player has them (vr_display.PICTURE names)."""
    return tuple(name for name, kept in (('depth_of_field', a.game_depth_of_field or a.keep_game_settings),
                                         ('sharpening', a.game_sharpening or a.keep_game_settings),
                                         ('comfort', a.keep_game_settings)) if kept)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--seconds', type=float, default=0,help='0 runs until game exit or Ctrl+C; 2..25 runs a timed test')
    p.add_argument('--size', type=int, default=0,help='0 uses the runtime recommendation; otherwise a square override')
    p.add_argument('--render-scale', type=int, default=100,
                   help="Eye resolution in percent of the runtime's recommendation, per side (50-200; default "
                        '100). Above 100 is sharper and costs frame rate and memory.')
    p.add_argument('--mip-bias', default='0.5',
                   help="Texture mip bias for the samplers the game biases negatively for its upscaler or "
                        "anti-aliasing, which the eyes do not get (-2..1; default 0.5 since Update 23, was 0; 'game' keeps the game's "
                        'own and only reports it). Higher is calmer and softer: less moire and sparkle on '
                        'distant textures. Also changes the monitor picture. Takes effect only when the '
                        'launcher starts the game.')
    p.add_argument('--anisotropy', type=int, default=16,
                   help='Anisotropic filtering (1-16; default 16; 0 keeps the game\'s) for the same samplers: the '
                        "trilinear ones become anisotropic and lower anisotropy is raised to this. Steadier "
                        'roofs, streets and walls seen at a slant. Takes effect only when the launcher starts '
                        'the game.')
    p.add_argument('--upscale', type=int, default=DEFAULT_UPSCALE,
                   help="Percent of the eye's size, per side, the eyes' scene is rendered at (50..100; default "
                        "%d). Spider-Man 2: no effect, the eyes always render at full size (no temporal "
                        'upscaler in its eye views).' % DEFAULT_UPSCALE)
    p.add_argument('--no-eye-antialiasing', action='store_true',
                   help='The eyes get no anti-aliasing and no upscaling: each is its scene as rendered, at the '
                        "eye's size (builds before October 10's eighth did; jagged and slower)")
    p.add_argument('--eye-aa', type=str.lower, choices=('off', 'fxaa'), default='fxaa',
                   help="The mod's own anti-aliasing of each eye image on its way to the headset (default fxaa: "
                        "FXAA 3.11 quality, once per new eye pair; off: the eyes as the game drew them). Uses no "
                        "game addresses; live.txt 'aa = 0|1' switches it during the session")
    p.add_argument('--supersample', type=int, default=100, metavar='PERCENT',
                   help="The eyes are rendered at this percent of the headset's eye size per side (100-200; "
                        "default 100: none) and each pair is filtered down to it (a Mitchell-Netravali filter "
                        "in linear light, in place of FXAA): fine repeated detail such as rows of fins no longer "
                        "makes curved moire. 150 renders 2.25 times the pixels, so fewer frames a second. Uses no "
                        'game addresses')
    p.add_argument('--game-monitor-view', action='store_true',
                   help='The game renders a view of its own on your head for the monitor, as builds before '
                        "October 10's eighth did (about a fifth fewer frames); without it the monitor shows the "
                        'left eye')
    p.add_argument('--monitor-rate', type=int, default=DEFAULT_MONITOR_RATE,
                   help='How often the game window gets your left eye in immersive VR: pictures a second (1..240; '
                        'default %d), or 0 for every frame, as builds before October 11 did. Each one costs the '
                        'graphics card time the headset could have.' % DEFAULT_MONITOR_RATE)
    p.add_argument('--no-eye-reflections', action='store_true',
                   help="The eyes without the game's screen-space reflections, as builds before October 11's "
                        "second did. Without it they follow the game's own Screen Space Reflections setting")
    p.add_argument('--own-shadows', action='store_true',
                   help="Each eye makes its own set of the sun's shadow maps, as builds before October 11 did; "
                        'without it both use one set, fitted to a lens covering both (fewer render commands, '
                        'shadow edges a little softer)')
    p.add_argument('--building-reach', type=int, default=150,  # Update 37.2: 150 again (as Update 36)
                   help="How far away the far buildings draw their full models (lit windows, the rooms behind them) "
                        "in percent of the game's own distance, 145 m (50-400; default 150). More costs a little "
                        'frame rate and video memory')
    p.add_argument('--eye-lens-distances', action='store_true',
                   help="The eyes keep the distances their own wide lenses give the game, as builds before "
                        'October 19 did: models get their detail, people and props appear and textures sharpen '
                        "at about half the distance the flat game has them, and an upscaled eye's textures are "
                        'asked for at its smaller rendered size (about 2 percent more frames)')
    p.add_argument('--auto-launch',action='store_true')
    p.add_argument('--close-game', action='store_true',
                   help='A running game (VR ran or was lost in it, another build is in it, or it was started '
                        'flat) is closed as its close button does, then started again (the launcher asks the player '
                        'first)')
    p.add_argument('--stop-after',type=float,default=0,help='End an untimed session after this many seconds for validation')
    p.add_argument('--capture-images', action='store_true',help='Enable diagnostic CPU eye readback (adds overhead)')
    p.add_argument('--overlay-webs', action='store_true',help="Draw Spidy's overlay webs instead of the game's web lines")
    p.add_argument('--no-web-grab', action='store_true', help='Webs never catch props or thugs; they only swing')
    p.add_argument('--no-air-webs', action='store_true',
                   help='A web that meets nothing within reach misses, instead of holding in open air 100 m out')
    p.add_argument('--flips', action='store_true',
                   help='Experimental: in the air the left stick turns you over, and a tap of A flips you')
    p.add_argument('--flip-speed', type=int, default=180,
                   help='How fast a flip turns you at full tilt of the left stick, in degrees a second (90..480; '
                        'default 180)')
    p.add_argument('--game-depth-of-field', action='store_true',
                   help="Keep the game's own Depth of Field setting; without it a session turns it off (in the "
                        'headset the blur is not where you look)')
    p.add_argument('--game-sharpening', action='store_true',
                   help="Keep the game's own Sharpness setting; without it a session sets it to its lowest")
    p.add_argument('--keep-game-aa', action='store_true',
                   help="0.0.73 AA: keep the game's own Anti-Aliasing and upscaler (DLAA, DLSS) in VR; without it "
                        "a session with --engine-taa and GAME TAA SPIDYVR sets TAA with no upscaler, put back "
                        'after. Read when the game starts')
    p.add_argument('--keep-game-settings', action='store_true',
                   help="Keep the game's own picture settings (depth of field, sharpness): the session then "
                        'changes only what VR needs (frame generation, vertical sync, ray tracing, Reflex, '
                        'aspect ratio, XInput)')
    p.add_argument('--no-stand-on-walls', action='store_true',
                   help='Walking a wall or stopped on it, your view stays upright beside it instead of turning so '
                        'that the wall is your floor')
    p.add_argument('--no-wall-run', action='store_true',
                   help="A player who flies into a wall is the game's, as before: its wall crawl holds him still, "
                        'then sticks him to the wall (for comparison; no setting offers it)')
    p.add_argument('--trigger-webs', action='store_true',
                   help='The trigger shoots and holds webs, and the grip reels in and shoots web balls (the two '
                        'swapped)')
    p.add_argument('--arcade-webs', action='store_true',
                   help='Arcade webs (WEB STYLE: ARCADE): webs go through lamp posts, signs, cars and other thin or '
                        'loose things, and swings stay above the street')
    p.add_argument('--no-body', action='store_true',
                   help="Keep the hero hidden in VR (gloves drawn over the image) instead of your own body")
    p.add_argument('--no-punch', action='store_true', help='Fists pass through thugs instead of punching them')
    p.add_argument('--no-web-shooter', action='store_true',
                   help="A free hand's trigger shoots nothing, instead of the game's web-shooter web balls")
    p.add_argument('--no-prompt-help', action='store_true',
                   help="Leave the game's own button prompts alone: Spidy neither reads which button they "
                        'ask for nor names the VR control in them, and the interact button is the game\'s Y '
                        'as before (the pad layer still gives the whole controller)')
    p.add_argument('--no-aim-markers', action='store_true',
                   help="Start with the aim markers (where each hand's web would land) hidden; AIM MARKERS in the "
                        "SPIDY VR tab shows them")
    p.add_argument('--no-eye-occlusion', action='store_true',
                   help='Each eye draws everything in its view, hidden behind buildings or not (builds before '
                        'October 6 did; about half the frame rate)')
    p.add_argument('--stock-monitor-view', action='store_true',
                   help="Keep the stock camera as the game's active view; culling and shadows then follow it, not the head")
    p.add_argument('--swing-speed', type=float, default=32, help='Native swing speed cap in m/s (default 32)')
    p.add_argument('--weight', type=int, default=100,
                   help='How heavy you are while webs fly you, in percent of real gravity (40..300; default 100)')
    p.add_argument('--eye-height', type=int, default=0,
                   help="Your eye height in millimetres from a T-pose calibration (with --arm-length): Spider-Man's "
                        'body is sized to it. 0 (default): none yet, the first gameplay asks for one')
    p.add_argument('--arm-length', type=int, default=0,
                   help="Your arm in millimetres from a T-pose calibration, from Spider-Man's shoulder at your size "
                        'to the wrist (with --eye-height)')
    p.add_argument('--no-calibration-prompt', action='store_true',
                   help='Without a calibration, do not ask for the T-pose at the first gameplay (the SPIDY VR tab '
                        'in the game\'s Settings still offers it)')
    p.add_argument('--snap-turn', type=int, default=30,
                   help='Degrees a flick of the right stick turns you (0: no snap turning; default 30)')
    p.add_argument('--smooth-turn', type=int, default=120,  # 0.0.73: smooth turning by default
                   help='Degrees a second the right stick turns you while held over, at full tilt '
                        '(0: it snap turns instead; default 120)')
    p.add_argument('--haptics', type=int, default=100, help='Controller vibration in percent (default 100)')
    p.add_argument('--body-haptics', type=int, default=100,
                   help='How strongly a bHaptics vest, sleeves and visor play, in percent (0: off; default 100). '
                        'They play through the bHaptics Player, which has to be running')
    p.add_argument('--screen-size', type=int, choices=(0, 1, 2), default=1,
                   help='The game screen for menus, cutscenes and flat mode: 0 small, 1 medium, 2 large')
    p.add_argument('--screen-depth', type=int, choices=(0, 1, 2, 3), default=2,
                   help='The depth cutscenes have on the game screen (the game renders them once for each eye): '
                        '0 off (flat), 1 low, 2 medium (default), 3 high, everything as far away as it is from '
                        "the game's camera. Spider-Man 2: cutscenes stay flat for now (the HUD panel hook is not found "
                        'yet, so no stereo scene is asked for)')
    p.add_argument('--hud', type=int, choices=(0, 1, 2, 3), default=2,
                   help="The game's HUD in the headset: 0 off, 1 small (50 degrees across), 2 medium (60, the "
                        'default), 3 large (70)')
    # 0.0.74 test 3: the start's safety checks (each on unless switched off).
    p.add_argument('--no-mod-check', action='store_true',
                   help="Do not look for other mods' loaders (winmm.dll, dinput8.dll...) in the game's folder")
    p.add_argument('--move-mod-loaders', action='store_true',
                   help="Move those loaders into the game folder's _hero-vr-moved for this session (the launcher "
                        'asks first); they go back when the game closes')
    p.add_argument('--no-gpu-check', action='store_true',
                   help='Do not set Windows\' high-performance graphics preference for the game and SteamVR on a PC '
                        'with two graphics adapters (puts back the old values)')
    p.add_argument('--no-clear-errors', action='store_true',
                   help="Do not read the game's crash line or give the graphics-device fix steps")
    p.add_argument('--full-desktop-view', action='store_true',
                   help="Start the game with its own display settings instead of a small VR window")
    p.add_argument('--smaller-desktop-view', action='store_true',
                   help='Update 33: a 1280 x 720 desktop window instead of 1920 x 1080 (about 3-5%% fewer pixels '
                        'a frame; the HUD panel is softer). Read by the game at launch')
    p.add_argument('--engine-taa', action='store_true',
                   help="Update 34: make the eyes' buffers with the game's TAA targets, so the wrist's "
                        'ENGINE TAA can be turned on (set the game\'s Anti-Aliasing to TAA first). Read when '
                        'VR starts')
    p.add_argument('--output', type=pathlib.Path, default=ROOT/'reports/game-vr.json')
    p.add_argument('--xr-runtime', default='auto',
                   help='OpenXR runtime manifest, or auto (default): the runtime the headset is connected to '
                        "(Virtual Desktop, then one whose VR service runs, then Windows' active runtime)")
    p.add_argument('--stop-event', help='Named event that stops VR as Ctrl+C does (for a launcher window)')
    a = p.parse_args()
    # Update 37: live.txt eye_occlusion = 0 turns the eyes' occlusion off from the next VR start (the
    # eyes' occlusion is made when VR starts: it cannot change in a session).
    try:
        if live_settings.parse((ROOT/'live.txt').read_text(encoding='utf-8-sig', errors='replace'))[0].get(
                'eye_occlusion') == 0:
            a.no_eye_occlusion = True
            print('live.txt: eye_occlusion = 0: the eyes start without occlusion culling.', flush=True)
    except OSError:
        pass
    # Version 0.0.62: live.txt lens_flares = off_in_vr (the hologram's LENS FLARES, RESTART) turns the game's
    # lens flares off for this session (vr_display.SESSION_OFF; put back when VR closes). 0.0.73 lights: OFF IN
    # VR is the default (no lens_flares line too), and a live.txt at the old default moves once (migrate_073t5,
    # run here: this is read before session()'s migrations).
    session_off = ()
    migrated_073t5 = live_settings.migrate_073t5(ROOT/'live.txt')
    if migrated_073t5:
        print(migrated_073t5, flush=True)
    try:
        live_text = (ROOT/'live.txt').read_text(encoding='utf-8-sig', errors='replace')
    except OSError:
        live_text = None
    if live_settings.lens_flares_off(live_text):
        session_off = ('lens_flares',)
        if live_settings.screen_shake_off(live_text):
            print('LENS FLARES: OFF IN VR this session (AA MODE CLEAN; aa_mode = off + restart VR for your own).',
                  flush=True)
        print('LENS FLARES: OFF IN VR this session (0.0.73 lights default: no flare strobing or headlight '
              'ghosts; the game\'s own setting is put back when VR closes; LENS FLARES GAME + restart VR to '
              'keep them).', flush=True)
    else:
        print('LENS FLARES: GAME this session (the game\'s own lens flares; LENS FLARES OFF IN VR + restart VR '
              'if they strobe).', flush=True)
    # 0.0.73 AA GAME AA TAA (vr_display.SESSION_OFF['game_aa']): with -EngineTaa and GAME TAA SPIDYVR (the
    # defaults) the game's Anti-Aliasing is TAA with no upscaler while VR runs, as SpidyVR's per-eye TAA has it;
    # the player's own (DLAA, DLSS) comes back when the game closes. --keep-game-aa keeps it during VR too.
    # 0.0.73 release: GAME TAA starts OFF; a live.txt at the test builds' SPIDYVR moves once (migrate_0073, run
    # here before image_style is read for GAME AA TAA and the mip bias). GAME AA TAA follows GAME TAA: OFF leaves
    # the game's own Anti-Aliasing alone.
    migrated_0073 = live_settings.migrate_0073(ROOT/'live.txt')
    if migrated_0073:
        print(migrated_0073, flush=True)
    # 0.0.74 test 2 SPIDYVR PICTURE: the picture settings set once to SpidyVR's (after migrate_0073, which would
    # move image_style spidyvr back to off in a live.txt older than 0.0.73), the eye supersample to 100.
    migrated_074t2 = live_settings.migrate_074t2(ROOT/'live.txt', ROOT/'next-launch.txt')
    if migrated_074t2:
        print(migrated_074t2, flush=True)
    # 0.0.74 test 4 AA MODE CLEAN (default on): the game's ScreenShake 0 while VR runs (SESSION_OFF['screen_shake'],
    # put back after), and no game TAA in the eyes, so GAME AA TAA does not apply.
    clean = live_settings.aa_mode_in_file(ROOT/'live.txt')
    if live_settings.screen_shake_off(live_text):
        session_off = tuple(session_off) + ('screen_shake',)
        print('SCREEN SHAKE: OFF this session (AA MODE CLEAN; the game\'s own is put back when VR closes; '
              'aa_mode = off + restart VR keeps it).', flush=True)
    game_taa_look = 'off' if clean else live_settings.image_style_in_file(ROOT/'live.txt')
    if live_settings.game_aa_taa(getattr(a, 'engine_taa', False), game_taa_look, getattr(a, 'keep_game_aa', False)):
        session_off = tuple(session_off) + ('game_aa',)
        print("GAME AA: TAA, no upscaler, this session (0.0.73 AA: the path SpidyVR's per-eye TAA uses; DLAA/DLSS "
              "biased the textures the eyes share and ran for the monitor alone); your own is put back when the "
              'game closes (-KeepGameAa keeps it; restart).', flush=True)
    elif getattr(a, 'engine_taa', False) and game_taa_look == 'spidyvr':
        print("GAME AA: your own this session (-KeepGameAa); GAME TAA works best with the game's Anti-Aliasing "
              'at TAA and Upscale Method None.', flush=True)
    elif getattr(a, 'engine_taa', False):
        print("GAME AA: your own this session (GAME TAA OFF, the 0.0.73 default: GAME AA TAA only runs with GAME TAA "
              'SPIDYVR; turn it on in GRAPHICS > QUALITY and restart VR to use it).', flush=True)
    a.session_off = session_off   # session() reads it: it is not a local there (0.0.62 NameError at 'starting the game')
    # SM2 VR (from SM2VR 0.0.27): a Spider-Man 2 that renders no view of its own writes a swapchain back
    # buffer that is not the current one, and the graphics device is lost (DXGI_ERROR_ACCESS_DENIED). The
    # game always renders its own (monitor) view until that is understood.
    a.game_monitor_view = True
    try: encode_mip_bias(a.mip_bias)
    except ValueError: p.error("--mip-bias must be 'game' or a number from -2 to 1")
    if not 0 <= a.anisotropy <= 16:
        p.error('--anisotropy must be 0 to 16')
    if a.stop_after and (a.seconds or not 5<=a.stop_after<=300):
        p.error('--stop-after requires --seconds 0 and a value from 5 to 300')
    if (a.seconds!=0 and not 2 <= a.seconds <= 25) or (a.size!=0 and not 64 <= a.size <= MAX_EYE_SIZE) or \
            not 1 <= a.swing_speed <= 65:
        p.error(f'Use 0 or 2..25 seconds, 0 or 64..{MAX_EYE_SIZE} pixels, and 1..65 m/s')
    if not RENDER_SCALES[0] <= a.render_scale <= RENDER_SCALES[1] or (a.size and a.render_scale != 100):
        p.error('Use a render scale of %d..%d%%, and not with --size' % RENDER_SCALES)
    if not UPSCALES[0] <= a.upscale <= UPSCALES[1]:
        p.error('Use an upscale of %d..%d%%' % UPSCALES)
    if not SUPERSAMPLES[0] <= a.supersample <= SUPERSAMPLES[1] or (a.size and a.supersample != 100):
        p.error('Use a supersample of %d..%d%%, and not with --size' % SUPERSAMPLES)
    # Update 15: the eye supersample the wrist hologram chose for this launch (NEXT LAUNCH), unless one is given.
    # Update 23: once, an older next-launch.txt's supersample above 100 goes back to OFF (100).
    if (reset := live_settings.migrate_next_launch(ROOT/'next-launch.txt')):
        print(reset, flush=True)
    # 0.0.74 test 3 SPIDYVR STOCK (default on): SpidyVR's eye size (100%), whatever next-launch.txt chose.
    stock_eyes = not a.size and live_settings.stock_in_file(ROOT/'live.txt')
    if stock_eyes and a.supersample != live_settings.STOCK_SUPERSAMPLE:
        print(f'0.0.74 SPIDYVR STOCK: eye supersample {live_settings.STOCK_SUPERSAMPLE}% (SpidyVR\'s; '
              f'{a.supersample}% asked; spidyvr_stock = off keeps it).', flush=True)
        a.supersample = live_settings.STOCK_SUPERSAMPLE
    if a.supersample == 100 and not a.size and not stock_eyes:
        chosen = live_settings.next_launch_supersample(ROOT/'next-launch.txt')
        if chosen and chosen != 100:
            a.supersample = chosen
            print(f'Eye supersample {chosen}% from next-launch.txt (chosen on the wrist hologram; delete the file '
                  'or set it back to 100 there to return to the default).', flush=True)
    if not 50 <= a.building_reach <= 400:
        p.error('Use a building reach of 50..400% of the game\'s distance')
    if not MONITOR_RATES[0] <= a.monitor_rate <= MONITOR_RATES[1]:
        p.error('Use a monitor rate of %d..%d a second (0: every frame)' % MONITOR_RATES)
    if not 0 <= a.snap_turn <= 90 or not 0 <= a.smooth_turn <= 360 or not 0 <= a.haptics <= 100:
        p.error('Use 0..90 degrees of snap turn, 0..360 degrees a second of smooth turn and 0..100% vibration')
    if not 0 <= a.body_haptics <= 100:
        p.error('Use 0..100% body haptics')
    if not 40 <= a.weight <= 300:
        p.error('Use a weight of 40..300% of real gravity')
    if not 90 <= a.flip_speed <= 480:
        p.error('Use a flip speed of 90..480 degrees a second')
    if (a.eye_height == 0) != (a.arm_length == 0) or a.eye_height and (
            not EYE_HEIGHTS_MM[0] <= a.eye_height <= EYE_HEIGHTS_MM[1] or
            not ARM_LENGTHS_MM[0] <= a.arm_length <= ARM_LENGTHS_MM[1]):
        p.error('Give both --eye-height (%d..%d mm) and --arm-length (%d..%d mm), or neither'
                % (*EYE_HEIGHTS_MM, *ARM_LENGTHS_MM))
    # Update 24: the folder (writable, not Program Files or OneDrive, not too long), Microsoft's C++ runtime for an
    # older build, the game in every Steam library and its version, and Windows' OpenXR runtime: plain words and
    # a stop before anything is written or started when VR cannot work.
    player_check.before_session(ROOT, launching=a.auto_launch and not game_running())
    keep_console(a.output)
    startup = Startup(a.output)
    # 0.0.74 test 3 MOD CHECK: what an earlier session moved aside and could not put back (the game was still
    # running) goes back first. TWO-GPU CHECK switched off: Windows' old preferences come back.
    try:
        mod_check.restore(ROOT/'reports', game_running())
        if getattr(a, 'no_gpu_check', False):
            gpu_check.restore(ROOT/'reports')
    except Exception as error:  # noqa: BLE001 - never stops a session
        print(f'MOD CHECK / TWO-GPU CHECK: could not put files or settings back ({error}).', flush=True)
    for line in test_build_lines():  # test build 5: which build, which folder, a mixed install
        print(line, flush=True)
    print(f'SM2 VR {version_line()} - {MADE_BY}', flush=True)  # Update 38.5: the public version first  # Update 30.1 (Update 31 from PORT-VERSION)
    print(f'This session\'s report will be written to: {a.output.resolve()}', flush=True)
    try:
        return session(a, startup, session_off=session_off)
    except BaseException as error:
        startup.ended(error)
        raise


def crash_while_loading(folder, game_modules, startup, attempts=6, log_path=None):
    """0.0.74 test 3 CLEARER ERRORS: the plain words for a game that died while starting, from its log's crash
    line (the game may still be writing it: a few tries), or None without one."""
    found = None
    for attempt in range(attempts):
        try:
            source = log_path or documents_folder()/GAME_NAME/(GAME_NAME+'.log')
            found = start_errors.crash(pathlib.Path(source).read_text(encoding='utf-8', errors='replace'))
        except (OSError, ValueError):
            found = None
        if found or attempt == attempts-1:
            break
        time.sleep(.5)
    if not found:
        return None
    loaders = mod_check.found(folder)
    startup.fields['game_crash'] = dict(found, address=hex(found['address']), loaders=loaders,
                                        module=(start_errors.module_at(found['address'], game_modules) or [None])[0])
    return start_errors.crash_message(found, game_modules, loaders, game_folder=folder)


def session(a, startup, session_off=()):
    """One VR session from the headset check to the report; `startup` follows it until VR starts.

    `session_off`: the game settings turned off only while VR runs (vr_display.SESSION_OFF names, from
    main()'s live.txt read, e.g. ('lens_flares',)); () keeps the game's own. 0.0.7 fix: it was read from
    main()'s locals, a NameError on every real launch."""
    if a.stop_event:
        watch_stop_event(a.stop_event)
    # First: nothing later can start VR in such a game.
    startup.stage = 'checking the running game'
    # An administrator's game is closed to a session without those rights (NeedsAdministrator).
    startup.fields['administrator'] = elevated()
    if game_running():
        pid = find_game()
        reason = restart_reason(pid)
        if a.close_game:
            # The launcher asked the player first (kCloseGameExit). A game started by another Spidy folder left
            # the player's own display settings in that folder's backup: back they go before this one saves any.
            startup.stage = 'closing the game'
            found = other_build(pid)
            # 0.0.5 hotfix 3: the launcher asked the player, so a game left over (VR lost, or started flat) closes
            # too; VR can only start in a game this session starts.
            print('Closing Spider-Man as its close button does (it saves as it quits)...', flush=True)
            if not close_game(pid):
                raise RuntimeError('Spider-Man did not close within 30 seconds. Quit it from its own menu, then press '
                                   'START VR.')
            print('Spider-Man closed.', flush=True)
            elsewhere = found and found[1] and found[1].resolve() != ROOT
            if elsewhere:
                try:
                    vr_display.restore(backup=found[1]/'reports'/vr_display.BACKUP.name)
                except (OSError, RuntimeError, ValueError):
                    pass  # that folder's own next session restores them
        elif reason:
            raise GameNeedsRestart(reason)
    startup.stage = 'finding the headset'
    manifest, runtime = preflight(a.xr_runtime)
    startup.fields['xr_runtime'] = runtime
    links = network_links()
    startup.fields['network'] = links
    if link_warning(links, runtime.get('name')):
        print(link_warning(links, runtime.get('name')), flush=True)
    if not getattr(a, 'no_gpu_check', False):
        # 0.0.74 test 3 TWO-GPU CHECK: before the game and SteamVR's compositor pick a graphics adapter.
        startup.stage = 'checking the graphics adapters'
        try:
            exe = player_check.find_game()[0]
            startup.fields['two_gpu'] = gpu_check.check(exe, manifest, ROOT/'reports',
                                                        local_appdata=os.environ.get('LOCALAPPDATA'),
                                                        log=lambda line: print(line, flush=True))
        except Exception as error:  # noqa: BLE001 - VR starts as before
            startup.fields['two_gpu'] = f'{type(error).__name__}: {error}'
    startup.stage = 'starting the game'
    moved_loaders = []

    def move_mod_loaders():
        # 0.0.74 test 3 MOD CHECK: the player said yes in the launcher (-MoveModLoaders). Only for a game this
        # session starts: a running game has them loaded already.
        if getattr(a, 'no_mod_check', False) or not getattr(a, 'move_mod_loaders', False):
            return
        try:
            exe = player_check.find_game()[0]
            if exe:
                moved_loaders.extend(mod_check.move_aside(pathlib.Path(exe).parent, ROOT/'reports',
                                                          log=lambda line: print(line, flush=True)))
                startup.fields['mod_loaders_moved'] = list(moved_loaders)
            else:
                print('MOD CHECK: the game\'s folder was not found, so nothing was moved.', flush=True)
        except Exception as error:  # noqa: BLE001 - VR starts as before
            print(f'MOD CHECK: could not move the mod loaders ({error}).', flush=True)

    def prepare_display():
        # Read only: every value the game saves, beside its log's [Settings] block in the report, so the
        # names of its other picture settings (motion blur, film grain...) can be confirmed (SM2TODO).
        try:
            startup.fields['game_registry'] = vr_display.snapshot()
        except OSError as error:
            startup.fields['game_registry'] = str(error)
        # Read only (38.5): does the game keep its UI > Hide HUD options in the registry? Never written.
        try:
            startup.fields['game_hud_options'] = vr_display.hud_option_probe() or 'not in the registry'
        except OSError as error:
            startup.fields['game_hud_options'] = str(error)
        print('Tip: to hide the game\'s own HUD (health, focus, mini-map) in VR, turn on Settings > UI > '
              'Hide HUD in the game. It applies at once; prompts, subtitles and menus stay.', flush=True)
        try:
            values = vr_display.prepare_launch(small=not a.full_desktop_view, keep=picture_kept(a), off=tuple(session_off) or getattr(a, 'session_off', ()),
                                               height=vr_display.SMALLER_HEIGHT if a.smaller_desktop_view
                                               else vr_display.HEIGHT)
        except OSError as error:
            # VR runs without them: the window only saves GPU time, and the two settings matter where
            # frame generation is on or under SteamVR.
            startup.fields['game_settings_refused'] = str(error)
            print(f"WARNING: Spidy could not apply the game's VR settings ({error}). "
                  'The game starts with its current settings: turn '
                  'frame generation off in its graphics settings, and a smaller window leaves more of the GPU '
                  "for the headset. Under SteamVR the game's menus may not follow the VR controllers.",
                  flush=True)
            if vr_display.pending():
                print('Some saved game settings still need restoring; Spidy will retry after the game closes.',
                      flush=True)
            return
        if values:
            print(f"Desktop view: {values['WindowWidth']} x {values['WindowHeight']} window to save GPU time; "
                  'your display settings return when the game closes.', flush=True)
            if a.smaller_desktop_view:
                print('Smaller desktop view (-SmallerDesktopView): set at launch, so it applies to this launch '
                      'and is undone by the next launch without it; the game view still runs in full, at '
                      'fewer pixels; the HUD panel is softer.', flush=True)
        elif a.smaller_desktop_view:
            print('Smaller desktop view: not applied (the game keeps its own window settings).', flush=True)
        print('Game settings for VR: frame generation off, vertical sync off (the game would wait for the '
              "monitor), controllers through XInput only (SteamVR's virtual gamepads would take the menus from "
              'the VR controllers); they return when the game closes.', flush=True)
        changed = vr_display.session_changes()
        if changed.get('RTReflections'):
            print(RAY_TRACING_OFF, flush=True)
        if changed.get('ForcedAspectRatio'):
            print(ASPECT_RATIO_AUTO, flush=True)
        if changed.get('ReflexMode'):
            print(REFLEX_OFF, flush=True)
        kept = picture_kept(a)
        off = [text for name, text in (('depth_of_field', 'depth of field off'),
                                       ('sharpening', 'sharpening at its lowest'),
                                       ('comfort', 'motion blur, film grain, chromatic aberration and vignette off'))
               if name not in kept]
        if off:
            print(f"Game picture for VR: {' and '.join(off)} (-KeepGameSettings keeps the game's own); "
                  'they return when the game closes, unless you change them in the game meanwhile.', flush=True)
        print('Changed for VR: ' + ', '.join(f'{n} {v}' for n, v in vr_display.session_written().items())
              + '.', flush=True)
        # SM2TODO: these have no registry name and encoding confirmed beside the game's log yet (motion blur,
        # grain, chromatic aberration and vignette had, in the October 22 report, and are written now), so the
        # launcher leaves them alone. game_registry in the report maps them.
        print('You may also set these yourself in Settings > Graphics (the launcher leaves them as you set them): ' + ', '.join(UNCONFIRMED_COMFORT) + '.', flush=True)

    render_module = {}
    # 0.0.73 IMAGE: SPIDYVR (live.txt image, the default) starts the textures at mip bias 0 when the launcher
    # passes the old default 0.5; CLASSIC keeps 0.5; any other -MipBias is the player's and is kept.
    # 0.0.73 AA: GAME TAA SPIDYVR (live.txt image_style, the default) with -EngineTaa starts at 0 for either
    # look (no +0.5 blur under a TAA); image_style = off gives IMAGE's choice back.
    image_look = live_settings.image_in_file(ROOT/'live.txt')
    game_taa_look = live_settings.image_style_in_file(ROOT/'live.txt')
    mip_bias_asked = live_settings.image_mip_bias(a.mip_bias, image_look, game_taa_look, bool(getattr(a, 'engine_taa', False)))
    _, render_preset_name = live_settings.render_preset_in_file(ROOT/'live.txt')
    if render_preset_name == 'AA MODE CLEAN':
        # 0.0.74 test 4 AA MODE CLEAN: +0.5 on the samplers the game biases negatively, with --anisotropy (16x by
        # default); a -MipBias other than the default is the player's and is kept.
        mip_bias_asked = a.mip_bias if str(a.mip_bias).strip() != '0.5' else live_settings.CLEAN_MIP_BIAS
        print(f'0.0.74 AA MODE CLEAN: texture mip bias {mip_bias_asked} with {a.anisotropy}x anisotropic filtering '
              'on the samplers the game biases for its upscaler (aa_mode = off uses your own; restart).', flush=True)
    elif render_preset_name and str(mip_bias_asked).strip() != '0':
        # 0.0.74 test 2 SPIDYVR PICTURE / test 3 SPIDYVR STOCK: SpidyVR adds no mip bias (CONFIRMED);
        # spidyvr_stock = off with spidyvr_picture = custom keeps yours.
        print(f'0.0.74 {render_preset_name}: texture mip bias 0 (the launcher passed {a.mip_bias}; '
              'spidyvr_stock = off and spidyvr_picture = custom keep it; restart).', flush=True)
        mip_bias_asked = '0'
    elif mip_bias_asked != a.mip_bias:
        if game_taa_look == 'spidyvr' and getattr(a, 'engine_taa', False) and image_look != 'spidyvr':
            print(f'0.0.73 GAME TAA SPIDYVR: texture mip bias {mip_bias_asked} under the game\'s TAA (the '
                  f'launcher passed {a.mip_bias}; live.txt image_style = off gives IMAGE CLASSIC\'s back; restart).',
                  flush=True)
        else:
            print(f'0.0.73 IMAGE {image_look.upper()}: texture mip bias {mip_bias_asked} (the launcher passed '
                  f'{a.mip_bias}; image = classic keeps it).', flush=True)
    try:
        mip_bias = encode_mip_bias(mip_bias_asked, a.anisotropy)
    except (ValueError, TypeError):
        mip_bias = encode_mip_bias(a.mip_bias, a.anisotropy)

    def render_memory_module(pid):
        # In time only in a game that has just started: it creates its render memory once. That is the graphics
        # card's memory for each frame's drawing, not the PC's (a player with 32 GB read it so on October 16).
        try:
            render_module.update(enlarge_render_memory(pid, mip_bias=mip_bias))
        except GameNeedsRestart:
            raise
        except (OSError, RuntimeError) as error:
            print(f"WARNING: Spidy's module for the game's per-frame graphics memory is unavailable ({error}).",
                  flush=True)

    # VR starts with the game: its intro, menus and loading show on a screen in the headset until
    # there is gameplay. The worker finds each player itself (a loaded save, a respawn, a character
    # switch); before October 5's seventh build the launcher found one, once, and VR stayed on the
    # screen after any reload.
    if a.auto_launch:
        def prepare_game():
            move_mod_loaders()
            prepare_display()
        game = wait_for_game(prepare=prepare_game, early=render_memory_module, ready=lambda game: True)
    else:
        game = Game(find_game())
        render_memory_module(game.pid)
    startup.fields['game_pid'] = game.pid
    startup.fields['steam_build'] = game.steam_build
    if not game.steam_build:
        print(f'This Spider-Man2.exe is version {EXPECTED_VERSION} but not Steam\'s file (an Epic Games Store copy?). '
              'Spidy tries it: should the game\'s code differ where Spidy works with it, VR stops with a code '
              'to tell us.', flush=True)
    # Before anything of Spidy's goes into the game: another VR mod there leaves the headset black.
    startup.stage = "checking the game's folder"
    real_vr, others = other_mods(game.path.parent)
    startup.fields['other_mods'] = real_vr+others
    if real_vr:
        raise RuntimeError(real_vr_refusal(game.path.parent, real_vr))
    told_back = []

    def tell_came_back():
        # 0.0.74 test 3 MOD CHECK: a moved loader back as the game starts (a mod manager redeploying it).
        back = [name for name in mod_check.came_back(game.path.parent, moved_loaders) if name not in told_back]
        if back:
            told_back.extend(back)
            startup.fields['mod_loaders_came_back'] = list(told_back)
            print(mod_check.came_back_text(back), flush=True)
    if moved_loaders:
        tell_came_back()
    others = [name for name in others if name not in told_back]
    if others:
        print(other_mods_warning(game.path.parent, others), flush=True)
    game_modules, modules_read = [], [0.0]

    def watch_start():
        # 0.0.74 test 3: the game's module list every 3 seconds (CLEARER ERRORS maps a crash address with it).
        if moved_loaders:
            tell_came_back()
        if not getattr(a, 'no_clear_errors', False) and time.monotonic()-modules_read[0] >= 3:
            modules_read[0] = time.monotonic()
            listed = modules(game.pid)
            if listed:
                game_modules[:] = listed
    try:
        watch_start()
    except Exception:  # noqa: BLE001
        pass
    startup.stage = "looking for the game's graphics queue"
    process = bridge = xr = rays = None
    bridge_active = xr_active = False
    previous_signal=None
    session_report = safe_report = console_handler = None
    report_written = False
    closing, flushed = threading.Event(), threading.Event()
    try:
        pe = PE(game.path.read_bytes())
        original_entries = {rva:pe.bytes(rva,16) for rva in (*GAME_HOOKS, *RENDER_MEMORY_HOOKS)}
        if any(game.read(game.base+rva,16) != original_entries[rva] for rva in GAME_HOOKS):
            raise RuntimeError('A required game entry is already patched. Start a fresh game process.')
        process = open_process(0x0400 | 0x0010 | 0x0020 | 0x0008 | 0x0002, False, game.pid)
        if not process:
            raise c.WinError(c.get_last_error())
        render_data = render_module.get('SpidyRenderMemoryData')
        try:
            queue = wait_for_renderer(game, process, render_data, problems=startup.fields.setdefault('queue_search', []),
                                      tick=watch_start)
        except RuntimeError as error:
            if str(error) != 'The game closed while starting.' or getattr(a, 'no_clear_errors', False):
                raise
            clearer = crash_while_loading(game.path.parent, game_modules, startup)
            if not clearer:
                raise
            raise RuntimeError(clearer) from error
        startup.fields['queue'] = hex(queue)
        startup.stage = "loading Spidy's modules into the game"
        if not bring_to_front(game.pid):
            print('Click the game window once: the game pauses while another window is in front.', flush=True)
        bridge, bridge_hash = prepare(game.pid, process,
            names=('SpidyStart', 'SpidyStop', 'SpidySubmit', 'SpidyRetarget', 'SpidyBridgeData',
                   'SpidyBridgeSample'))
        # No player yet: the VR worker hands the bridge each one it finds.
        config = struct.pack('<4I3Q', 0x53424346, 1, 40, game.pid, game.base, 0, 0)
        code = call_with_payload(process, bridge['SpidyStart'], config)
        if code:
            raise RuntimeError(f'Input bridge start: {bridge_start_text(code)}')
        bridge_active = True
        bridge_module = next(m['base'] for m in modules(game.pid) if m['name'].lower() == 'spidy_bridge.dll')
        rays, ray_hash = prepare(game.pid, process, ROOT/'build/windows-ninja/spidy_ray_bridge.dll',
            ROOT/'reports/ray-modules', ('SpidyRayStart', 'SpidyRaySubmit', 'SpidyRayStop',
                                       'SpidyRaySample', 'SpidyRayData', 'SpidySwingData', 'SpidyGrabData',
                                       'SpidyPunchData', 'SpidyStrikeData', 'SpidyShooterData', 'SpidyRopeData',
                                       'SpidySenseData'))
        # Imported here: probe_game_grab imports from this module.
        from probe_game_grab import grab_snapshot
        ray_module = next(m['base'] for m in modules(game.pid) if m['name'].lower() == 'spidy_ray_bridge.dll')
        motion, motion_hash = prepare(game.pid,process,ROOT/'build/windows-ninja/spidy_movement_bridge.dll',
            ROOT/'reports/motion-modules',('SpidyMotionStart','SpidyMotionSubmit','SpidyMotionSample',
                                         'SpidyMotionStop','SpidyMotionData'))
        motion_module = next(m['base'] for m in modules(game.pid) if m['name'].lower() == 'spidy_movement_bridge.dll')
        xr, xr_hash = prepare(game.pid, process, ROOT/'build/windows-ninja/spidy_stereo_probe.dll',
            ROOT/'reports/stereo-modules', ('SpidyXrStart', 'SpidyXrStop', 'SpidyXrKeepAlive', 'SpidyXrData',
                                           'SpidyGpuData', 'SpidyXrTimingData', 'SpidyAppearanceData',
                                           'SpidyStereoFrames', 'SpidyXrSnapshot', 'SpidyStereoData',
                                           'SpidyBodyData', 'SpidySlowMotionData', 'SpidyHudData',
                                           'SpidySteamData', 'SpidyPromptData', 'SpidyImpostorReach',
                                           'SpidyHapticsData', 'SpidyMonitorRate', 'SpidyEyeAA',
                                           'SpidyEyeSupersample', 'SpidyEyeHistory', 'SpidyEyeHistoryData',
                                           'SpidyWebWidth', 'SpidyWebTravel', 'SpidyWebSpeed', 'SpidyWebStart', 'SpidyEyeDebug', 'SpidyShadowData', 'SpidyWristHud', 'SpidyWristHudHand',
                                           'SpidyWristHudScale', 'SpidyHudSettings', 'SpidyEyeSharpen',
                                           'SpidyReticles', 'SpidyWristHudHeight', 'SpidyWristHudText',
                                           'SpidyWristHudTheme', 'SpidyWristHudTextColor', 'SpidyWebStyle',
                                           'SpidyWebColor', 'SpidyMusicVolume', 'SpidyMusicVisualizer',
                                           'SpidyMusicTrack', 'SpidyWristHudData',
                                           'SpidyAddressFinder', 'SpidyAddressFinderData',
                                           'SpidyAddressFinderReport', 'SpidyEyeLod',
                                           'SpidyEyeLodData', 'SpidyShadowCamera',
                                           'SpidyShadowCameraData', 'SpidyShadowDistanceData',
                                           'SpidyEyeTrail', 'SpidyEngineTaaJitter', 'SpidyEngineTaaJitterScale',
                                           'SpidyEngineTaaData', 'SpidyEngineTaaEyes', 'SpidyFarShadowsData',
                                           'SpidyTaaDiag',  # 0.0.74 test 2b TAA DIAGNOSTICS
                                           'SpidyTaaHoldData',  # 0.0.74 test 2b TAA HOLD
                                           'SpidyTaaLinkData',  # 0.0.74 test 3 TAA LINK
                                           # Update 37: every live.txt export (so none can be missing
                                           # at the first edit) and the new telemetry.
                                           *sorted({e for _, e, _ in live_settings.SETTINGS.values() if e}),
                                           # Update 37.2: every record update37_records reads (the
                                           # WORLD LOG, BLOOD, PERF, COLOR, FOVEATED and NPC FADE ones
                                           # were missing, so their report sections stayed empty).
                                           *sorted({export for export, *_ in update37_records.RECORDS.values()})))
        stage_haptics(xr_hash)
        saved = vr_display.Registry().read(('ScreenSpaceReflections', 'RTReflections'))
        reflections, reflections_line = eye_reflections(saved.get('ScreenSpaceReflections'), a.no_eye_reflections,
                                                        saved.get('RTReflections', 0))
        game_view, game_view_line = ray_traced_view(saved.get('RTReflections', 0))
        # SM2 port: as in SM2VR 0.0.27, the probe stores 0 whatever SpidyEyeTemporal and SpidyEyeShadows
        # are given (SM2VR 0.0.3-0.0.5): no eye temporal AA or upscaling, no extra reflection targets, and
        # each eye makes its own shadows. Say so, so the console and the report match what the eyes get.
        reflections_line = ("Reflections: the eyes' extra reflection targets are off "
                           "(not ported to Spider-Man 2; SM2VR has them off too).")
        miles_taa = live_settings.miles_taa_in_file(ROOT/'live.txt')
        print('MILES TAA: ' + ('ON - the game\'s TAA in each eye (Miles Morales VR\'s path), the eyes\' scene at 67% '
                               'resolved by it; miles_taa = off in live.txt for the full eye size (restart).'
                               if miles_taa else 'OFF (live.txt miles_taa).'), flush=True)
        main_small = live_settings.main_view_small_in_file(ROOT/'live.txt')
        print('MAIN VIEW SMALL: ' + ('ON - the flat-screen view at 50% (its TAA resolves it), more GPU time for the eyes; '
                                     'main_view_small = off in live.txt and restart for the full size.'
                                     if main_small else 'OFF (live.txt main_view_small).'), flush=True)
        config = struct.pack('<4I7Q2IfI', 0x53585243, 24, 664, game.pid, game.base, queue,
                             bridge_module, ray_module, motion_module, 0, 0,
                             int(a.seconds*1000), a.size, a.swing_speed,
                             int(a.capture_images) | (2 if a.overlay_webs else 0) |
                             (4 if a.stock_monitor_view else 0) | (8 if a.no_web_grab else 0) |
                             (16 if a.no_eye_occlusion else 0) | (32 if a.no_body else 0) |
                             (64 if a.no_punch else 0) | (128 if a.no_aim_markers else 0) |
                             (256 if a.no_air_webs else 0) | (512 if a.no_web_shooter else 0) |
                             (1024 if a.no_calibration_prompt else 0) | (2048 if a.flips else 0) |
                             (4096 if a.trigger_webs else 0) | (8192 if a.no_stand_on_walls else 0) |
                             (16384 if a.no_wall_run else 0) | (0x8000 if a.no_eye_antialiasing else 0) |
                             (0x10000 if a.game_monitor_view or game_view else 0) |
                             (0x20000 if a.own_shadows else 0) | (0 if reflections else 0x40000) |
                             (0x100000 if a.no_prompt_help else 0) | (0x80000 if a.eye_lens_distances else 0) |
                             (0x200000 if a.arcade_webs else 0) | (0x400000 if a.eye_aa == 'off' else 0) |
                             (ENGINE_TAA_OPTION if a.engine_taa else 0) |
                             (MILES_TAA_OPTION if miles_taa else 0) |
                             (MAIN_VIEW_SMALL_OPTION if main_small else 0)) + \
            runtime_path(manifest) + struct.pack('<14I', a.snap_turn, a.haptics, a.screen_size, a.smooth_turn,
                                                 a.render_scale, a.weight, a.eye_height, a.arm_length, a.hud,
                                                 a.flip_speed, a.upscale, a.monitor_rate, a.screen_depth,
                                                 a.body_haptics)
        # Before the start: the eyes' size is chosen when their views are made.
        if a.engine_taa:
            print("ENGINE TAA (-EngineTaa): the eyes get the game's TAA targets; turn ENGINE TAA on from the "
                  "wrist's GRAPHICS page (the game's Anti-Aliasing must be TAA; it turns itself off, and "
                  'EYE AA on, if it fails).', flush=True)
        if call_remote(process, xr['SpidyEyeSupersample'], a.supersample):
            raise RuntimeError(f'Eye supersample {a.supersample}% refused')
        # 0.0.7 MIRROR PERSIST FIX: what is made when VR starts (MIRROR VIEW TEST) is set before the start.
        for name, value in live_settings.vr_start_settings(ROOT/'live.txt').items():
            _, export, text = live_settings.SETTINGS[name]
            if export in xr and not call_remote(process, xr[export], live_settings.export_value(name, value)):
                print(f'live.txt (kept, before VR starts): {text.format(value)}', flush=True)
        startup.stage = 'starting VR'
        code = call_with_payload(process, xr['SpidyXrStart'], config)
        if code == 1001 and a.engine_taa:
            # Update 34.1: a game_xr DLL older than this launcher (or Update 34's, whose
            # check refused the ENGINE TAA bit) refuses the bit: start VR without it.
            print('WARNING: the VR module refused -EngineTaa (an older build?); starting VR without ENGINE TAA. '
                  'Rebuild (build.ps1) to use it.', flush=True)
            config = bytearray(config)
            options, = struct.unpack_from('<I', config, OPTIONS_OFFSET)
            struct.pack_into('<I', config, OPTIONS_OFFSET, options & ~ENGINE_TAA_OPTION)
            config = bytes(config)
            code = call_with_payload(process, xr['SpidyXrStart'], config)
        if code:
            raise GameNeedsRestart(VR_RAN) if code == 1000 else RuntimeError(
                f'Game XR start: {code}' + (' (the VR module refused the launcher\'s settings: the DLL and the '
                                            'launcher are from different builds; run build.ps1)' if code == 1001 else ''))
        xr_active = True
        # The far buildings' full models, while the left eye chooses them for the game's resting view.
        if call_remote(process, xr['SpidyImpostorReach'], a.building_reach):
            print(f'WARNING: the building reach {a.building_reach}% was refused; the eyes keep 150%.', flush=True)
        # Update 20: a live.txt still on the old default 90s web look moves to messy, once.
        migrated = live_settings.migrate_web_style(ROOT/'live.txt')
        if migrated:
            print(migrated, flush=True)
        print('Wrist hologram: bring the right hand within about 15 cm of the left wrist or forearm and pull the '
              'right trigger (hud_hand = right mirrors it), or press F8 on the PC keyboard.', flush=True)
        # Settings the player changes during the session: saving live.txt applies them (tools/live_settings.py).
        live_file = live_settings.LiveFile(ROOT/'live.txt', {'reach': a.building_reach, 'monitor': a.monitor_rate,
                                                              'aa': 0 if a.eye_aa == 'off' else 1,
                                                              'history': 0, 'web_width': 1.3,
                                                              'web_travel': 1, 'web_speed': 180,
                                                              'hud': 1, 'hud_hand': 'left', 'hud_scale': 1.0,
                                                              'hud_height': 9, 'hud_text': 100,
                                                              'hud_theme': 'spiderman', 'hud_text_color': 'theme',
                                                              'web_style': 'messy',
                                                              **{f'web_color_{style}': shades[0] for style, shades
                                                                 in live_settings.WEB_SHADES.items()},
                                                              'music_volume': 70, 'music_visualizer': 'original',
                                                              'music_track': 0})
        live_changes = []
        history_last = None  # Update 21: history_snapshot, kept from the samples
        engine_taa_last = engine_taa_said = None  # Update 34.2
        taa_eyes_last = taa_eyes_said = None  # 0.0.73 TAA SHAKE
        taa_diag_said = -1e9  # 0.0.74 test 2b TAA DIAGNOSTICS
        taa_hold_said = -1e9  # 0.0.74 test 2b TAA HOLD
        taa_link_said = -1e9  # 0.0.74 test 3 TAA LINK
        shadows_last = None  # Update 26: shadow_snapshot, kept the same way (the exit path's report lacked it)
        finder_last = None  # Update 27: address_finder.snapshot
        finder_written_gen = None  # finished generation already written to reports/
        finder_partial_at = 0.0  # Update 27.2: last partial write (monotonic)
        finder_partial_gen = None

        def flush_address_finder_on_exit():
            nonlocal finder_written_gen, finder_last
            reports_dir = ROOT / 'reports'
            if 'SpidyAddressFinderData' not in xr:
                return
            finder = None
            try:
                if game:
                    finder = address_finder.snapshot(game, xr['SpidyAddressFinderData'])
            except Exception:
                finder = None
            finder = finder or finder_last
            if not finder:
                print(address_finder.missing_line(reports_dir, 'session end; no finder snapshot'),
                      flush=True)
                return
            if finder.get('generation') == finder_written_gen and finder.get('status') in (2, 3):
                return
            rtext = ''
            try:
                if game:
                    rtext = address_finder.read_report(game, xr['SpidyAddressFinderReport'])
            except Exception:
                rtext = ''
            jpath, tpath = address_finder.write_files(
                reports_dir, finder, rtext, partial=finder.get('status') == 1)
            rlines = address_finder.parse_report_lines(rtext)
            print(address_finder.done_line(finder, jpath, address_finder.almost_count(rlines),
                                           address_finder.shadow_count(rlines)) + ' (session end)', flush=True)
            print(f'ADDRESS FINDER text: {tpath}', flush=True)
            print(f'ADDRESS FINDER folder: {reports_dir} '
                  f'(also address-finder-latest.json / .txt)', flush=True)
            finder_written_gen = finder.get('generation')
        body_said = ('off', None)
        # Update 19: the wrist hologram's MUSIC page plays the songs in this folder.
        music_line = live_settings.ensure_music_folder(ROOT)
        if music_line:
            print(music_line, flush=True)
        # Update 37.2: an existing live.txt gets the 37.2 start values once (EYE AA, FAR SHADOWS
        # EYES+CACHE, sharpen 140 ...); a value chosen after that stays.
        migrated_372 = live_settings.migrate_372(ROOT/'live.txt')
        if migrated_372:
            print(migrated_372, flush=True)
        # Version 0.0.72: the model looks start on (the shipped Models), once over an old live.txt.
        migrated_072 = live_settings.migrate_072(ROOT/'live.txt')
        if migrated_072:
            print(migrated_072, flush=True)
        # Update 38.2: a 38.1 live.txt's COMBAT numbers kept meaning what they did (values reordered).
        migrated_382 = live_settings.migrate_382(ROOT/'live.txt')
        if migrated_382:
            print(migrated_382, flush=True)
        # Update 38.3: the double ball and web grab / yank fixes set once (run382's complaints).
        migrated_383 = live_settings.migrate_383(ROOT/'live.txt')
        if migrated_383:
            print(migrated_383, flush=True)
        # Update 38.5: BALL LOOK GAME ONLY moved to WRIST once (the ball from the pulled hand's wrist).
        migrated_385 = live_settings.migrate_385(ROOT/'live.txt')
        if migrated_385:
            print(migrated_385, flush=True)
        # Version 0.0.2: BALL LOOK moved to OURS once (one mod ball per press from the pulled wrist).
        migrated_386 = live_settings.migrate_386(ROOT/'live.txt')
        if migrated_386:
            print(migrated_386, flush=True)
        # Version 0.0.73 test 3: a setting an older build wrote at its old default moves to the new one
        # once (SHADOW MATCH ON, PUNISHER VAN ON: fixes the player asked for ship on); old numbers keep their old meaning.
        migrated_073t3 = live_settings.migrate_073t3(ROOT/'live.txt')
        if migrated_073t3:
            print(migrated_073t3, flush=True)
        # 0.0.73 IMAGE: the test 2/3 AA values the build wrote (aa 1, sharpen 140) move to SPIDYVR's once.
        migrated_073t4 = live_settings.migrate_073t4(ROOT/'live.txt')
        if migrated_073t4:
            print(migrated_073t4, flush=True)
        # Update 15: the settings the wrist hologram (or live.txt) kept last time, applied now.
        kept, _ = live_settings.persisted(ROOT/'live.txt')
        for name, value in kept.items():
            if live_file.current.get(name) == value:
                continue
            _, export, text = live_settings.SETTINGS[name]
            if export is None:  # Update 37: read at the start (eye_occlusion), nothing to send
                live_file.applied(name, value)
                continue
            if call_remote(process, xr[export], live_settings.export_value(name, value)):
                print(f'live.txt (kept): {text.format(value)} refused; the default stays.', flush=True)
            else:
                live_file.applied(name, value)
                print(f'live.txt (kept): {text.format(value)}', flush=True)
        # 0.0.74 test 2 SPIDYVR PICTURE (default on): SpidyVR's picture over whatever live.txt kept (live.txt
        # keeps the player's own for spidyvr_picture = custom); the DLL holds the same while it is on.
        render_preset, render_preset_name = live_settings.render_preset_in_file(ROOT/'live.txt')
        if render_preset_name == 'AA MODE CLEAN':
            # 0.0.74 test 4 AA MODE CLEAN (default on): docs/AA-MODE-CLEAN-074.md; aa_mode = off uses your own.
            sent = []
            for name, value in render_preset.items():
                _, export, text = live_settings.SETTINGS[name]
                if export not in xr or call_remote(process, xr[export], live_settings.export_value(name, value)):
                    continue
                live_file.applied(name, value)
                kept[name] = value
                sent.append(f'{name} = {value}')
            print('0.0.74 AA MODE CLEAN: ON - no game TAA in the eyes, FXAA every eye frame, the per-eye history '
                  'at 80, a light CAS, building reach 150 (' + ', '.join(sent) + '); aa_mode = off uses your own.',
                  flush=True)
        elif render_preset_name == 'SPIDYVR STOCK':
            # 0.0.74 test 3 SPIDYVR STOCK (default on): SpidyVR's render path - every HERO VR render layer of
            # live.txt at SpidyVR's value (docs/TAA-074-test3.md); spidyvr_stock = off uses your own.
            sent = []
            for name, value in render_preset.items():
                _, export, text = live_settings.SETTINGS[name]
                if export not in xr or call_remote(process, xr[export], live_settings.export_value(name, value)):
                    continue
                live_file.applied(name, value)
                kept[name] = value
                sent.append(f'{name} = {value}')
            print('0.0.74 SPIDYVR STOCK: ON - SpidyVR\'s own render path: the game\'s TAA in each eye with '
                  'SpidyVR\'s jitter and nothing of HERO VR\'s AA, TAA, history, sharpening, colour, LOD or light '
                  'tweaks (' + ', '.join(sent) + '); spidyvr_stock = off uses your own.', flush=True)
        elif render_preset_name:
            sent = []
            for name, value in live_settings.PICTURE_PRESET.items():
                _, export, text = live_settings.SETTINGS[name]
                if export not in xr or call_remote(process, xr[export], live_settings.export_value(name, value)):
                    continue
                live_file.applied(name, value)
                kept[name] = value
                sent.append(f'{name} = {value}')
            print('0.0.74 SPIDYVR PICTURE: ON - the game\'s TAA in each eye with SpidyVR\'s full jitter, no EYE AA, '
                  'FXAA, CAS or SHARPEN MATCH, mip bias 0 (' + ', '.join(sent) + '); spidyvr_picture = custom '
                  'uses your own.', flush=True)
        else:
            print('0.0.74 SPIDYVR PICTURE: CUSTOM - your own picture settings (live.txt).', flush=True)
        # Update 37: the run36 closing settings are the new defaults, for whatever live.txt does not keep.
        # 0.0.73 IMAGE: aa and sharpen follow the look (SPIDYVR 0 and 0, CLASSIC 1 and 140).
        image_now = live_settings.image_of(kept)
        update37_defaults = live_settings.image_defaults(live_settings.UPDATE37_DEFAULTS, image_now)
        for name, value in update37_defaults.items():
            if name in kept:
                continue
            _, export, text = live_settings.SETTINGS[name]
            if call_remote(process, xr[export], live_settings.export_value(name, value)):
                print(f'Update 37 default: {text.format(value)} refused; the old default stays.', flush=True)
            else:
                live_file.applied(name, value)
        print('Update 37 defaults (unless live.txt keeps others): ' + ', '.join(
            f'{k} = {v}' for k, v in update37_defaults.items()), flush=True)
        # Update 38: the COMBAT defaults (the recommended fixes), for whatever live.txt does not keep.
        for name, value in live_settings.UPDATE38_DEFAULTS.items():
            if name in kept:
                continue
            _, export, text = live_settings.SETTINGS[name]
            if export not in xr:
                print(f'Update 38 default: {text.format(value)} not in this DLL; the built-in default stays.',
                      flush=True)
                continue
            if call_remote(process, xr[export], live_settings.export_value(name, value)):
                print(f'Update 38 default: {text.format(value)} refused; the built-in default stays.', flush=True)
            else:
                live_file.applied(name, value)
        print('Update 38 COMBAT defaults (all live, no restart): ' + ', '.join(
            f'{k} = {v}' for k, v in live_settings.UPDATE38_DEFAULTS.items()), flush=True)
        # Update 37.2: the settings that must start on, as the session now runs them.
        print(live_settings.startup_line(live_file.current), flush=True)
        print(live_settings.image_line(image_now, live_file.current), flush=True)
        # Update 24: what makes far detail shimmer that the player set (strong sharpening), and a flicker
        # test (eye_debug) still in live.txt.
        for line in live_settings.start_notes(ROOT/'live.txt'):
            print(line, flush=True)
        hud_pending = {}
        # Update 24: the session HIGH / LOW / AVG FPS (the VR SETTINGS page's boxes), read every few seconds
        # so the report has the last even if the game is closed first.
        session_fps_last, session_fps_read = None, 0.0

        def read_session_fps():
            """live_settings.session_fps from the mod now; None if it cannot say."""
            if 'SpidyHudSettings' not in xr:
                return None
            try:
                codes = [call_remote(process, xr['SpidyHudSettings'], s) for s in live_settings.SESSION_FPS_SELECTORS]
            except (OSError, RuntimeError, TimeoutError):
                return None
            return live_settings.session_fps(*codes)

        def hud_changes():
            """{name: value} the wrist hologram's pages changed since the last call (SpidyHudSettings)."""
            if 'SpidyHudSettings' not in xr:
                return {}
            changed = live_settings.decode_hud_settings(3, call_remote(process, xr['SpidyHudSettings'], 3))
            if not changed:
                return {}
            values = {}
            for selector in sorted({live_settings.SELECTOR[k] for k in changed}):
                values.update(live_settings.decode_hud_settings(
                    selector, call_remote(process, xr['SpidyHudSettings'], selector)) or {})
            return {k: values[k] for k in changed if k in values and (k != 'supersample' or values[k])}
        print(f'Live settings: save {ROOT/"live.txt"} with lines like "reach = 100", "monitor = 15", "aa = 0" or '
              '"history = 50" or "web_width = 1.3" to change the building reach, the monitor rate, the eye '
              'anti-aliasing, the eye history (shimmer) or the webs\' thickness in cm now ("web_travel = 0" and "web_speed = 120" set the web shot\'s flight, "hud = 0", '
              '"hud_hand = right", "hud_scale = 1.2", "hud_height = 12", "hud_text = 150", "hud_theme = deutan" and "hud_text_color = yellow" set the wrist hologram; "web_style = messy" and "web_color = frost" the overlay webs\' look; "music_volume = 50" and "music_visualizer = tunnel" the MUSIC page\'s); a live.txt already there is read once you save it again (the render scale '
              'needs a relaunch).', flush=True)
        started = time.monotonic()
        samples = deque(maxlen=12000)
        ray_samples = deque(maxlen=12000)
        swing_samples = deque(maxlen=12000)
        # The web grab: a sample when a counter or a hand's grab changes, else once a second.
        grab_samples = deque(maxlen=12000)
        motion_samples = deque(maxlen=12000)
        eye_shots = deque()
        eye_folder = a.output.with_name(a.output.stem+'-eyes')
        shot_count = 0
        appearance = None
        eye_lod_last, eye_lod_said = None, None  # Update 28.1: EYE LOD's state, printed once per change
        eye_lod_timeline = []  # Update 29: (seconds, status) per change, in every report
        shadow_camera_last, shadow_camera_said = None, 0  # Update 31: SHADOW CAMERA, one line per change
        shadow_camera_modes = {}  # Update 31: the game view's cascades as last seen under each mode
        shadow_distance_last, shadow_distance_said = None, None  # Update 33: SHADOW DISTANCE, one line per change
        far_shadows_last, far_shadows_said = None, None  # Update 35: FAR SHADOWS, one line per change
        update37_last = {}  # Update 37: view gates, combat, hand climbing, health, mirror records
        trace_journal = None  # Update 37: the WORLD LOG's events, appended to reports/world-trace-*.jsonl
        # The player's body and fists: the latest of each, and a sample when punches land.
        body = punch = None
        punch_samples = deque(maxlen=2000)
        strikes, strike_samples = None, deque(maxlen=2000)
        sense, sense_samples = None, deque(maxlen=2000)
        # Read while the game runs: a session that ends with the game has nothing to read them from.
        ropes = steam = None
        # The web shooter: the latest, and a sample when a shot is asked for, fired or dropped.
        shooter = None
        shooter_samples = deque(maxlen=2000)
        # Slow motion: the latest, and a sample when it starts, ends or a press is refused.
        slow_motion = None
        slow_motion_samples = deque(maxlen=2000)
        # The HUD: the latest (native_hud::Data), for a report written after the game closed.
        hud_last = None
        # Body haptics: the latest, once the session has written it.
        haptics_last = None
        wrist_hud_last = None
        prompt_last = None
        eye_jobs = None
        timing = None
        render_memory = None
        # Update 23: the mip bias as seen early on, so a report written when the game closes first (the
        # October 23 one) still says what it was.
        mip_bias_early = None
        lowest_commit = start_commit = free_commit_mb()
        printed_ring = False
        previous = None
        vr_lost = False  # 0.0.5 hotfix 3: VR ended while the game keeps running
        printed_controllers = None
        printed_prompt = 0
        printed_dimensions=False
        renewed=0
        stop_requested=False
        settings_start = start_settings(a)
        # The game's reads of Spidy's controller: none while buttons are held on the game screen means its
        # menus cannot be played (the October 7 Steam Link session: the game had turned XInput off).
        pad_watch = dict(reads=None, since=0.0, warned=False)

        # How the eyes are rendered: upscaled from `upscale` percent of their size per side and anti-aliased
        # (or neither), whether the game renders a view of its own for the monitor as well, how often the
        # monitor gets the left eye otherwise, whether both eyes use one set of sun shadow maps, and whether
        # they judge distances as the game's 68-degree lens does (appearance's eye_detail says by how much).
        eye_options = dict(upscale=100, antialiased=False,  # SM2 port: see reflections above
                           game_monitor_view=bool(a.game_monitor_view or a.stock_monitor_view or game_view),
                           monitor_rate=a.monitor_rate, shared_shadows=False,
                           reflections=False, ray_traced=game_view, game_distances=not a.eye_lens_distances,
                           building_reach=a.building_reach, eye_aa=a.eye_aa, supersample=a.supersample,
                           render_size=None)

        def hand_over_settings(sample):
            """Tell the launcher what the VR settings were left at, for the next session. BODY HAPTICS rides its
            own struct, as written last."""
            if sample and sample.get('vr_settings') and haptics_last:
                sample = dict(sample, vr_settings=dict(sample['vr_settings'], body_haptics=haptics_last['level']))
            line = settings_line(settings_start, sample)
            if line:
                print(line, flush=True)

        def prompt_section(samples):
            """The report's prompts section, from the two structs that carry it.

            The census, the timeline and the game's own calls come from PromptData (`prompt_last`); the counters
            the session keeps - the interact's presses on a wall, its hand-overs to the game, the pad layer -
            ride XrData and reach here in the last sample. The report wants both, and only taking the first is
            how the hand-over counters went missing from the October 13 session that was meant to measure them.
            """
            out = dict(prompt_last or {})
            for key, value in ((samples[-1].get('prompts') if samples else None) or {}).items():
                out.setdefault(key, value)
            return out

        def session_report(**extra):
            """Everything sampled so far. It is written however the session ends."""
            # port_version and bridge_hash too: the October 22 report written here had neither, so whether
            # Update 9 ran had to be told from its eye_aa fields.
            # launch_args too (Update 11): the October 22 report covered one of three launches, which only its
            # eye_occlusion samples told apart.
            return dict(version=public_version(), port_version=port_version(), bridge_hash=bridge_hash, launch_args=sys.argv[1:],
                        live_changes=live_changes, pid=game.pid, eye_size=a.size,
                        render_scale=a.render_scale, supersample=a.supersample, swing_speed=a.swing_speed,
                        eyes=eye_options, vr_settings=settings_start, mip_bias=mip_bias_early,
                        game_settings_refused=startup.fields.get('game_settings_refused'),
                        game_registry=startup.fields.get('game_registry'),
                        xr_runtime=runtime, motion_hash=motion_hash,
                        xr_hash=xr_hash, ray_hash=ray_hash, samples=list(samples),
                        swing_samples=list(swing_samples), motion_samples=list(motion_samples),
                        grab_samples=list(grab_samples), body=body, punch=punch,
                        punch_samples=list(punch_samples), strikes=strikes, strike_samples=list(strike_samples),
                        sense=sense, sense_samples=list(sense_samples),
                        ropes=ropes, steam_guard=steam,
                        shooter=shooter, shooter_samples=list(shooter_samples),
                        slow_motion=slow_motion, slow_motion_samples=list(slow_motion_samples), hud=hud_last,
                        haptics=haptics_last, wrist_hud=wrist_hud_last, prompts=prompt_section(list(samples)),
                        ray_samples=list(ray_samples), appearance=appearance, eye_jobs=eye_jobs,
                        render_memory=render_memory, timing=timing, network=links, eye_history=history_last,
                        eye_shadows=shadows_last,
                        stream_stalls=stream_stalls(list(samples)),
                        free_commit_mb=dict(start=start_commit, lowest=lowest_commit),
                        session_fps=session_fps_last,
                        eye_snapshots=list(eye_shots), eye_snapshot_folder=str(eye_folder) if eye_shots else None,
                        frame_rates=frame_rates(list(samples)), final=samples[-1] if samples else None,
                        # Update 29: the 28.1 report written here (the game closed) had no EYE LOD record.
                        **eye_lod.report_fields(eye_lod_last), eye_lod_timeline=list(eye_lod_timeline),
                        **shadow_camera.report_fields(shadow_camera_last, shadow_camera_modes),
                        **shadow_distance.report_fields(shadow_distance_last),
                        **far_shadows.report_fields(far_shadows_last), update37=update37_last, **extra)

        lost_device = {}

        advised = []  # the comfort advice is printed once a session
        summarised = []  # and the settings summary (Update 21)

        def write_report(result):
            # Always leaves a report at a.output: a part that fails is noted in it, not raised.
            nonlocal report_written
            a.output.parent.mkdir(parents=True, exist_ok=True)
            try:
                # Texture and video memory pressure and the game's own frame rate, minute by minute.
                copy, memory = keep_game_log(a.output)
                result.update(game_log=str(copy) if copy else None, game_memory=memory)
                try:
                    log = copy.read_text(encoding='utf-8', errors='replace') if copy else ''
                except OSError:
                    log = ''
                reason = device_removed(log)
                result['game_adapters'] = game_adapters(log)
                result['game_settings'] = game_settings(log)
                result['play_rate'] = play_rate(result.get('samples') or [])
                result['comfort_advice'] = comfort_advice(result['play_rate'], result['game_settings'],
                                                          result.get('render_scale'), result.get('timing'))
                if result['comfort_advice'] and not advised:
                    advised.append(True)
                    print('\n'.join(result['comfort_advice']), flush=True)
                if reason:
                    result['device_removed'] = reason
                    lost_device.update(reason=reason, frames=(result.get('final') or {}).get('tracked'))
            except Exception as error:  # noqa: BLE001 - the report matters more than the game log
                result['game_log_error'] = f'{type(error).__name__}: {error}'
            if not summarised:
                # Update 21: whichever way the session ended (the October 23 report was written after the
                # game was closed, and said nothing of the hologram's settings or the eye history).
                summarised.append(True)
                try:
                    for line in settings_summary(result.get('live_changes') or []):
                        print(line, flush=True)
                    if (said := eye_camera_line((result.get('eye_jobs') or {}).get('eye_camera'))):
                        print(said, flush=True)
                    if (kept := result.get('eye_history')):
                        print(history_line(kept), flush=True)
                    if (taa := result.get('engine_taa')):
                        print(engine_taa_line(taa), flush=True)
                    if (eyes := result.get('engine_taa_eyes')):
                        print(engine_taa_eyes_line(eyes), flush=True)
                    if (shadows := eye_shadow_line(result.get('eye_shadows'))):
                        print(shadows, flush=True)
                except Exception as error:  # noqa: BLE001 - the report matters more than the summary
                    result['summary_error'] = f'{type(error).__name__}: {error}'
            try:
                text = json.dumps(result, indent=2, default=str)+'\n'
            except (TypeError, ValueError) as error:
                text = json.dumps(dict(vr_started=True, report_error=f'{type(error).__name__}: {error}',
                                       game_exited=result.get('game_exited'), error=str(result.get('error'))),
                                  indent=2)+'\n'
            a.output.write_text(text)
            report_written = startup.reported = True

        def safe_report(**extra):
            # The session report, or what is left of it when the game's closing makes its reads fail.
            try:
                return session_report(**extra)
            except Exception as error:  # noqa: BLE001
                return dict(vr_started=True, report_error=f'{type(error).__name__}: {error}',
                            pid=game.pid, **{k: (str(v) if v is not None else None) for k, v in extra.items()})

        def tell_device_lost():
            # Once, after the report that found it.
            reason = lost_device.pop('reason', None)
            if reason:
                print(device_lost_advice(reason, lost_device.get('frames'), clear=not getattr(a, 'no_clear_errors', False)), flush=True)

        def request_stop(_signal,_frame):
            nonlocal stop_requested
            stop_requested=True
            print('Stopping VR and restoring game hooks...',flush=True)
        previous_signal=signal.signal(signal.SIGINT,request_stop)

        def console_event(kind):
            # Closing the console window (2), logoff (5), or shutdown (6) ends this process within
            # about five seconds. Hold Windows off until the report is on disk.
            if kind not in (2, 5, 6):
                return False
            closing.set()
            flushed.wait(4.5)
            return True
        console_handler = c.WINFUNCTYPE(wintypes.BOOL, wintypes.DWORD)(console_event)
        c.windll.kernel32.SetConsoleCtrlHandler(console_handler, True)
        loop_error = None
        try:
            while (not stop_requested and not closing.is_set() and
                   (not a.seconds or time.monotonic()-started < a.seconds+40) and alive(game)):
                if a.stop_after and time.monotonic()-started>=a.stop_after: break
                if not a.seconds and time.monotonic()-renewed>=1:
                    if call_remote(process,xr['SpidyXrKeepAlive']): break
                    renewed=time.monotonic()
                    changes, problems = live_file.poll()
                    for problem in problems:
                        print(f'live.txt: {problem}', flush=True)
                    for name, value in changes.items():
                        _, export, text = live_settings.SETTINGS[name]
                        if export is None:  # Update 37: a RESTART setting (eye_occlusion)
                            live_file.applied(name, value)
                            print(f'live.txt: {text.format(value)}. {live_settings.RESTART[name]}', flush=True)
                            continue
                        code = call_remote(process, xr[export], live_settings.export_value(name, value))
                        if code:
                            print(f'live.txt: {text.format(value)} refused ({code})', flush=True)
                        else:
                            live_file.applied(name, value)
                            live_changes.append(dict(seconds=round(time.monotonic()-started, 1), name=name,
                                                     value=value, source='live.txt'))
                            print(f'live.txt: {text.format(value)} now', flush=True)
                    # Update 15: what the wrist hologram's pages set, written back here in this process (the
                    # mod's render thread does no file I/O): live.txt for the live ones, next-launch.txt for
                    # the eye supersample (RESTART REQUIRED). Only the settings the pages changed are written.
                    hud_pending.update(hud_changes())
                    if time.monotonic() - session_fps_read >= 5:
                        session_fps_read = time.monotonic()
                        session_fps_last = read_session_fps() or session_fps_last
                    live_now = {k: v for k, v in hud_pending.items() if k != 'supersample'}
                    if live_now and live_file.adopt(live_now):
                        print('Wrist hologram: kept in live.txt: ' + ', '.join(
                            f'{k} = {live_settings.format_value(k, v)}' for k, v in live_now.items()), flush=True)
                        # Update 21: the hologram's changes in the report too. The October 23 report's
                        # live_changes was empty although the pages had changed history, sharpen and aa
                        # many times: only live.txt edits were listed. The value is the one the mod reports
                        # in force (SpidyHudSettings), so a refused change cannot be listed as applied.
                        for k, v in live_now.items():
                            live_changes.append(dict(seconds=round(time.monotonic()-started, 1), name=k,
                                                     value=v, source='hologram'))
                        for k in live_now:
                            hud_pending.pop(k)
                    if 'supersample' in hud_pending:
                        if live_settings.write_next_launch(ROOT/'next-launch.txt', hud_pending['supersample']):
                            print(f"Wrist hologram: eye supersample {hud_pending['supersample']}% at the next launch "
                                  '(RESTART REQUIRED; kept in next-launch.txt).', flush=True)
                        hud_pending.pop('supersample')
                sample = snapshot(game, xr['SpidyXrData'])
                if sample:
                    sample['seconds'] = time.monotonic()-started
                    appearance = appearance_snapshot(game, xr['SpidyAppearanceData']) or appearance
                    sample['appearance'] = appearance
                    eye_lod_last = eye_lod_sample(game, xr) or eye_lod_last
                    eye_lod.timeline_step(eye_lod_timeline, sample['seconds'], eye_lod_last)
                    sample['eye_lod'] = eye_lod_last['status'] if eye_lod_last else None
                    said = eye_lod.console_line(eye_lod_last)
                    if said and eye_lod.said_key(eye_lod_last) != eye_lod_said:
                        eye_lod_said = eye_lod.said_key(eye_lod_last)
                        print(said, flush=True)
                    shadow_camera_last = shadow_camera_sample(game, xr) or shadow_camera_last
                    shadow_camera.mode_step(shadow_camera_modes, shadow_camera_last)
                    sample['shadow_camera'] = shadow_camera_last['mode'] if shadow_camera_last else None
                    lines, shadow_camera_said = shadow_camera.new_lines(shadow_camera_last, shadow_camera_said)
                    shadow_distance_last = shadow_distance_sample(game, xr) or shadow_distance_last
                    sample['shadow_distance'] = shadow_distance_last['state'] if shadow_distance_last else None
                    if shadow_distance.said_key(shadow_distance_last) != shadow_distance_said:
                        shadow_distance_said = shadow_distance.said_key(shadow_distance_last)
                        lines = lines + [shadow_distance.console_line(shadow_distance_last)]
                    far_shadows_last = far_shadows_sample(game, xr) or far_shadows_last
                    sample['far_shadows'] = far_shadows_last['mode'] if far_shadows_last else None
                    if far_shadows.said_key(far_shadows_last) != far_shadows_said:
                        far_shadows_said = far_shadows.said_key(far_shadows_last)
                        lines = lines + [far_shadows.console_line(far_shadows_last)]
                    update37_now = update37_records.snapshots(game, xr)
                    update37_last = {k: v or update37_last.get(k) for k, v in update37_now.items()}
                    if trace_journal is None:
                        trace_journal = update37_records.TraceJournal(
                            ROOT / 'reports' / f"world-trace-{time.strftime('%Y%m%d-%H%M%S')}.jsonl")
                    trace_journal.add(update37_now.get('world_trace'))
                    health_now = update37_last.get('health') or {}
                    sample['health_status'] = health_now.get('status')
                    sample['health_hp'] = health_now.get('hp')
                    for line in lines:
                        print(line, flush=True)
                    body = body_snapshot(game, xr['SpidyBodyData']) or body
                    if body and (body['state'], body['problem']) != body_said:
                        body_said = (body['state'], body['problem'])
                        if body['state'] == 'failed':
                            print('Body: did not start (its game hooks were refused; a build before 249304d has '
                                  'none for Spider-Man 2) - the hero stays hidden.', flush=True)
                        elif body['state'] == 'active' and not body['problem']:
                            print(f"Body: on, {body['joints']} joints.", flush=True)
                        elif body['problem']:
                            print(f"Body: {body['problem']} (rig {body['rig']}, {body['joints']} joints) - the hero "
                                  'stays hidden.', flush=True)
                    if body:
                        sample['body'] = {k: body[k] for k in ('state', 'problem', 'weight', 'hand_error_m',
                                                               'head_error_m', 'turn_last', 'grounded', 'scale',
                                                               'arm_scale', 'rig', 'rig_switches')}
                    landed = punch_snapshot(game, rays['SpidyPunchData'])
                    if landed:
                        # Thugs within reach of a fist, and each fist's speed relative to the head.
                        sample['punch'] = dict(bots=landed['bots'], punches=landed['punches'],
                                               speed=[h['speed'] for h in landed['hands']])
                        if any(landed[k] != (punch or {}).get(k) for k in ('punches', 'dropped', 'unnamed')):
                            punch_samples.append(dict(landed, seconds=round(time.monotonic()-started, 3)))
                        punch = landed
                    ropes = rope_snapshot(game, rays['SpidyRopeData']) or ropes
                    steam = steam_snapshot(game, xr['SpidySteamData']) or steam
                    fight = strike_snapshot(game, rays['SpidyStrikeData'])
                    if fight:
                        # One row per web strike opened or ended and per blow landed.
                        if any(fight[k] != (strikes or {}).get(k)
                               for k in ('opened', 'landed', 'missed', 'blows', 'calls', 'knockouts', 'locked',
                                         'passed_over')):
                            strike_samples.append(dict(fight, seconds=round(time.monotonic()-started, 3)))
                        strikes = fight
                    senses = sense_snapshot(game, rays['SpidySenseData'])
                    if senses and 'protocol_mismatch' in senses:
                        sense = senses
                    elif senses:
                        # Warnings standing now; one row per warning gone off and per counter.
                        sample['sense'] = dict(standing=len(senses['standing']), attacks=senses['attacks'])
                        if any(senses[k] != (sense or {}).get(k)
                               for k in ('warnings', 'thrown', 'landed', 'counter_blows', 'breaks', 'managers')):
                            sense_samples.append(dict(senses, seconds=round(time.monotonic()-started, 3)))
                        sense = senses
                    shots = shooter_snapshot(game, rays['SpidyShooterData'])
                    if shots:
                        sample['shooter'] = {k: shots[k] for k in ('status', 'fired', 'dropped', 'bots', 'collisions',
                                                                   'webbed')}
                        if any(shots[k] != (shooter or {}).get(k)
                               for k in ('requested', 'fired', 'dropped', 'error', 'collisions', 'webbed', 'pushed')):
                            shooter_samples.append(dict(shots, seconds=round(time.monotonic()-started, 3)))
                        shooter = shots
                    slow = slow_motion_snapshot(game, xr['SpidySlowMotionData'])
                    if slow:
                        sample['slow_motion'] = {k: slow[k] for k in ('active', 'focus', 'wanted', 'world_scale')}
                        if any(slow[k] != (slow_motion or {}).get(k) for k in SLOW_MOTION_COUNTS):
                            slow_motion_samples.append(dict(slow, seconds=round(time.monotonic()-started, 3)))
                        slow_motion = slow
                    # Body haptics: whether the Player and the devices are there, and what they played.
                    felt = haptics_snapshot(game, xr['SpidyHapticsData'])
                    if felt and felt['written']:
                        haptics_last = felt
                        sample['haptics'] = {k: felt[k] for k in ('status', 'devices', 'frames', 'hits', 'landings')}
                        if sample.get('vr_settings'):
                            sample['vr_settings']['body_haptics'] = felt['level']
                    # Update 20: the wrist hologram's gesture and the music player.
                    try:
                        seen_hud = wrist_hud_snapshot(game, xr['SpidyWristHudData'])
                    except RuntimeError as error:
                        seen_hud = None
                        if not wrist_hud_last:
                            print(f'WARNING: {error}: the wrist hologram is not in this report.', flush=True)
                            wrist_hud_last = {'error': str(error)}
                    if seen_hud and seen_hud['written']:
                        for line in wrist_hud_lines(seen_hud, wrist_hud_last if 'presses' in (wrist_hud_last or {})
                                                    else None):
                            print(line, flush=True)
                        wrist_hud_last = seen_hud
                        sample['wrist_hud'] = {k: seen_hud[k] for k in ('distance_m', 'presses', 'toggles',
                                                                        'f8_toggles', 'state')}
                    # What the game asked for, and the census of the calls its UI
                    # makes: kept for the report however the session ends.
                    prompt_last = prompt_snapshot(game, xr['SpidyPromptData']) or prompt_last
                    hud = hud_snapshot(game, xr['SpidyHudData'])
                    if hud:
                        hud_last = hud
                        sample['hud'] = {k: hud[k] for k in ('placed_frames', 'layer_frames', 'marker_projections',
                                                             'followed_frames', 'follow_angle', 'size',
                                                             'hidden_frames', 'hidden_draws')}
                    # Eye job copies the game dropped unrendered (reclaimed by age).
                    eye_jobs = frame_snapshot(game, xr['SpidyStereoFrames']) or eye_jobs
                    sample['eye_jobs_reclaimed'] = eye_jobs['reclaimed'] if eye_jobs else None
                    # Update 21: the eye history in force and its work, each sample (the October 23 report,
                    # written after the game was closed, had no eye_history at all: it was only read at a
                    # normal stop). eye_camera: the per-eye camera audit (probe_stereo.frame_snapshot).
                    history_last = history_snapshot(game, xr['SpidyEyeHistoryData']) or history_last
                    # Update 34.2: ENGINE TAA's record each sample, a console line when its state or mode changes.
                    if (taa := engine_taa_snapshot(game, xr['SpidyEngineTaaData'])):
                        said = (taa['state'], taa['reason'], taa['jitter_mode'], taa['jitter_scale'])
                        if said != engine_taa_said:
                            engine_taa_said = said
                            print(engine_taa_line(taa), flush=True)
                        engine_taa_last = taa
                        sample['engine_taa'] = {k: taa[k] for k in ('state', 'reason', 'jitter_mode', 'jitter_scale',
                                                                    'eye_jitter', 'game_jitter', 'on_frames')}
                    # 0.0.73 TAA SHAKE: the per-eye TAA record each sample, a console line when TAA JITTER changes.
                    if (eyes := engine_taa_eyes_snapshot(game, xr.get('SpidyEngineTaaEyes'))):
                        if eyes['jitter'] != taa_eyes_said:
                            taa_eyes_said = eyes['jitter']
                            print(engine_taa_eyes_line(eyes), flush=True)
                        taa_eyes_last = eyes
                        sample['engine_taa_eyes'] = {k: eyes[k] for k in ('jitter', 'render', 'left', 'right')}
                    # 0.0.74 test 2b TAA DIAGNOSTICS: the per-frame record each sample, a console line every 10 s.
                    if (diag := taa_diag_snapshot(game, xr.get('SpidyTaaDiag'))):
                        sample['taa_diag'] = diag
                        if time.monotonic() - taa_diag_said >= 10:
                            taa_diag_said = time.monotonic()
                            print(taa_diag_line(diag), flush=True)
                    # 0.0.74 test 2b TAA HOLD: always (a few counters), a console line every 10 s.
                    if (hold := taa_hold_snapshot(game, xr.get('SpidyTaaHoldData'))):
                        sample['taa_hold'] = hold
                        if time.monotonic() - taa_hold_said >= 10:
                            taa_hold_said = time.monotonic()
                            print(taa_hold_line(hold), flush=True)
                    if (link := taa_link_snapshot(game, xr.get('SpidyTaaLinkData'))):
                        sample['taa_link'] = link
                        if time.monotonic() - taa_link_said >= 10:
                            taa_link_said = time.monotonic()
                            print(taa_link_line(link), flush=True)
                    if history_last:
                        sample['eye_history'] = {k: history_last[k] for k in ('percent', 'blended_pairs',
                                                                              'restarted_pairs', 'fault')}
                    if eye_jobs and eye_jobs.get('eye_camera'):
                        sample['eye_camera'] = eye_jobs['eye_camera']
                    # Update 26: the eyes' shadow cascades vs the game view's, for whichever report is written.
                    shadows_last = shadow_snapshot(game, xr['SpidyShadowData']) or shadows_last
                    # Update 27 / 27.2: ADDRESS FINDER — partials every ~10 s; always on finish/fail.
                    finder = address_finder.snapshot(game, xr['SpidyAddressFinderData'])
                    if finder:
                        finder_last = finder
                        sample['address_finder'] = {
                            k: finder[k] for k in (
                                'status_name', 'candidates', 'fault', 'frames', 'lod_offset', 'generation')}
                        reports_dir = ROOT / 'reports'
                        report_addr = xr['SpidyAddressFinderReport']
                        if finder['status'] in (2, 3) and finder['generation'] != finder_written_gen:
                            rtext = address_finder.read_report(game, report_addr)
                            if not rtext.strip():
                                print(address_finder.missing_line(
                                    reports_dir, 'native report buffer empty at finish'), flush=True)
                            jpath, tpath = address_finder.write_files(
                                reports_dir, finder, rtext)
                            rlines = address_finder.parse_report_lines(rtext)
                            print(address_finder.done_line(
                                finder, jpath, address_finder.almost_count(rlines),
                                address_finder.shadow_count(rlines)), flush=True)
                            print(f'ADDRESS FINDER text: {tpath}', flush=True)
                            print(f'ADDRESS FINDER folder: {reports_dir} '
                                  f'(also address-finder-latest.json / .txt)', flush=True)
                            finder_written_gen = finder['generation']
                        elif (finder['status'] == 1 and finder['generation'] and
                              (finder_partial_gen != finder['generation'] or
                               time.monotonic() - finder_partial_at >= address_finder.PARTIAL_WRITE_S)):
                            rtext = address_finder.read_report(game, report_addr)
                            jpath, tpath = address_finder.write_files(
                                reports_dir, finder, rtext, partial=True)
                            print(address_finder.partial_line(finder, jpath), flush=True)
                            print(f'ADDRESS FINDER folder: {reports_dir} '
                                  f'(also address-finder-latest.json / .txt)', flush=True)
                            finder_partial_at = time.monotonic()
                            finder_partial_gen = finder['generation']
                    # Eyes with their own occlusion culling (0 with --no-eye-occlusion or before VR starts).
                    views = stereo_snapshot(game, xr['SpidyStereoData'])
                    sample['eye_occlusion'] = sum(e['occlusion'] for e in views['eyes'] if e['view']) if views else None
                    if views and views['eyes'][0]['view'] and views['eyes'][0]['render_width']:
                        eye_options['render_size'] = [views['eyes'][0]['render_width'],
                                                      views['eyes'][0]['render_height']]
                    # Frames whose render memory request did not fit lose a view's work.
                    if render_data:
                        render_memory = render_memory_snapshot(game, render_data) or render_memory
                    if render_memory:
                        sample['render_frame_mb'] = render_memory['last_frame_mb']
                        sample['render_overflow_frames'] = render_memory['overflow_frames']
                        if render_memory['frames'] > 2 and not printed_ring:
                            printed_ring = True
                            try:
                                if render_module.get('SpidyMipBiasData'):
                                    mip_bias_early = mip_bias_snapshot(game, render_module['SpidyMipBiasData'])
                                if mip_bias_early is not None:
                                    mip_bias_early['start_code'] = render_module.get('mip_bias_start')
                                    if mip_bias_early.get('status') == 'late':
                                        print('Texture mip bias: not applied (the game made its graphics device '
                                              'before Spidy started; start the game with this launcher).', flush=True)
                                    else:
                                        print(f"Texture mip bias now: {mip_bias_early.get('bias')} on "
                                              f"{mip_bias_early.get('changed_samplers')} samplers.", flush=True)
                            except Exception:  # only a report line: never stops the session
                                mip_bias_early = None
                            if render_memory['ring'] == 'spidy_ring':
                                print(f"Render memory: {render_memory['ring_mb']:.0f} MB for two frames "
                                      f"(the game's own: {render_memory['game_ring_mb']:.0f} MB).", flush=True)
                            else:
                                print("WARNING: the game is on its own 128 MB of render memory, which three "
                                      'views overflow in heavy scenes: eye frames drop and the game can '
                                      'crash. Close the game and start it with Launch Spidy VR.', flush=True)
                    # What the headset's loop spent on its newest frame (XR_STAGES, milliseconds), and the
                    # eye pairs handed to the graphics card that it had not finished: a loop that falls
                    # behind the headset shows here which step it waited in.
                    timing = timing_snapshot(game, xr['SpidyXrTimingData']) or timing
                    if timing:
                        sample['xr_ms'] = [round(timing[name]['last_ms'], 1) for name in XR_STAGES]
                    pair = gpu_snapshot(game, xr['SpidyGpuData'])
                    if pair and pair['completed'] != 0xffffffffffffffff:
                        sample['gpu_waiting'] = pair['fence']-pair['completed']
                    sample['free_commit_mb'] = free_commit_mb()
                    if sample['free_commit_mb'] is not None:
                        lowest_commit = min(lowest_commit or sample['free_commit_mb'], sample['free_commit_mb'])
                    samples.append(sample)
                    if pad_ignored(pad_watch, sample):
                        print(PAD_IGNORED, flush=True)
                    if sample['eye_width'] and not printed_dimensions:
                        print(rendering_line(sample['eye_width'], sample['eye_height'], a.render_scale, a.size,
                                             runtime['recommended_eye'], 100, False),  # SM2 port
                              flush=True)
                        if a.supersample != 100:
                            print(f'Supersampling: the eyes are rendered at {a.supersample}% of that per side and '
                                  "filtered down to it (no FXAA).", flush=True)
                        print(reflections_line, flush=True)
                        if game_view_line:
                            print(game_view_line, flush=True)
                        printed_dimensions=True
                    aim = ray_snapshot(game, rays['SpidyRayData'])
                    if aim and aim['serial'] and (not ray_samples or aim['serial'] != ray_samples[-1]['serial']):
                        ray_samples.append(aim)
                    swing = swing_snapshot(game,rays['SpidySwingData'])
                    if swing and swing['steps'] and (not swing_samples or swing['steps'] != swing_samples[-1]['steps']):
                        swing_samples.append(swing)
                    grab = grab_snapshot(game, rays['SpidyGrabData'])
                    if grab:
                        grab['seconds'] = round(time.monotonic()-started, 3)
                        key = [grab[k] for k in ('grabs', 'yanks', 'catches', 'throws', 'releases', 'lost', 'landed',
                                                 'flings', 'refused', 'drive_failures', 'error', 'launches',
                                                 'impacts', 'struck')]
                        key += [h['phase'] for h in grab['hands']]
                        last = grab_samples[-1] if grab_samples else None
                        if not last or last['key'] != key or grab['seconds']-last['seconds'] >= 1:
                            grab['key'] = key
                            grab_samples.append(grab)
                    movement = motion_snapshot(game, motion['SpidyMotionData'])
                    if movement and movement['steps'] and (not motion_samples or movement['steps'] != motion_samples[-1]['steps']):
                        motion_samples.append(movement)
                    # What the headset showed: the newest copy of the left eye, when there is a new one.
                    shot = eye_snapshot(game, xr['SpidyXrSnapshot'])
                    if shot and shot['pixels'] and shot['count'] != shot_count:
                        shot_count = shot['count']
                        try:
                            files = save_eye_snapshot(game, xr['SpidyXrSnapshot'], shot, eye_folder,
                                                      f"{shot['count']:04d}")
                        except OSError:
                            files = None  # a full disk must not end the session
                        if files:
                            eye_shots.append(dict(seconds=sample['seconds'], files=files,
                                                  **{k: shot[k] for k in EYE_SHOT_FIELDS}))
                            while len(eye_shots) > EYE_SHOTS:
                                remove_eye_files(eye_folder, eye_shots.popleft()['files'])
                    # Which layout the runtime uses for the controllers, once it says and when it changes.
                    layout = controllers_line(sample['controllers'], runtime.get('name'))
                    if layout and layout != printed_controllers:
                        print(layout, flush=True)
                        printed_controllers = layout
                    # And what the game asks for, each time it asks for something new: the moments that
                    # used to stop a player are the ones where this names a button VR play never had.
                    asked = sample.get('prompt_last_demand', 0)
                    if asked and asked != printed_prompt:
                        printed_prompt = asked
                        line = prompts_line(asked)
                        if line:
                            print(line, flush=True)
                    state = (sample['status'], sample['message'])
                    if state != previous:
                        print(sample['message'], flush=True)
                        previous = state
                    if sample['status'] == 5 or (sample['status'] == 4 and 'runtime closed' in sample['message']):
                        # Update 24.1: say plainly that VR has ended and why (the line above is the reason).
                        print(flat_fallback_line(sample['message']), flush=True)
                        # 0.0.5 hotfix 3: and what to do; the launcher offers to close the game (CLOSE_GAME_EXIT).
                        print(VR_LOST_HELP, flush=True)
                        vr_lost = True
                    if sample['status'] in (4, 5):
                        break
                time.sleep(.05)
        except (OSError, RuntimeError) as error:
            # A closing game fails remote calls while its process handle still reports it running.
            # The 12:05 session on October 5 ended that way and its report was never written.
            loop_error = error
        if closing.is_set():
            flush_address_finder_on_exit()
            write_report(safe_report(launcher_closed=True))
            flushed.set()
        if loop_error is not None:
            leaving = time.monotonic()+10
            while alive(game) and time.monotonic() < leaving:
                time.sleep(.1)
        if not alive(game):
            xr_active=bridge_active=False
            flush_address_finder_on_exit()
            write_report(safe_report(game_exited=True, error=str(loop_error) if loop_error else None))
            hand_over_settings(samples[-1] if samples else None)
            stalls = stream_stalls(list(samples))
            if stalls['rounds'] >= 5:
                print(f"The headset's picture stalled while you played: the VR runtime held {stalls['rounds']} "
                      f"frames for a tenth of a second each ({stalls['seconds']} s in all). That is the stream to "
                      'the headset, not the game' + (': this PC was on Wi-Fi.' if links.get('wifi') else '.'),
                      flush=True)
            tell_device_lost()
            print(f'Game closed. Session report: {a.output.resolve()}',flush=True)
            return 0
        if loop_error is not None:
            flush_address_finder_on_exit()
            write_report(safe_report(game_exited=False, error=str(loop_error)))
            hand_over_settings(samples[-1] if samples else None)
            tell_device_lost()
            print(f'Session report: {a.output.resolve()}',flush=True)
            raise loop_error
        xr_stop = call_remote(process, xr['SpidyXrStop'])
        xr_active = xr_stop != 0
        bridge_stop = call_remote(process, bridge['SpidyStop'])
        bridge_active = bridge_stop != 0
        final_render_memory = render_memory_snapshot(game, render_data) if render_data else None
        if (felt := haptics_snapshot(game, xr['SpidyHapticsData'])) and felt['written']:
            haptics_last = felt
        try:
            if (seen_hud := wrist_hud_snapshot(game, xr['SpidyWristHudData'])) and seen_hud['written']:
                wrist_hud_last = seen_hud
        except RuntimeError:
            pass
        mip_bias_seen = (mip_bias_snapshot(game, render_module['SpidyMipBiasData'])
                         if render_module.get('SpidyMipBiasData') else None)
        if mip_bias_seen is not None:
            mip_bias_seen['start_code'] = render_module.get('mip_bias_start')
        render_stop = call_remote(process, render_module['SpidyRenderMemoryStop']) if render_module else 0
        if trace_journal is not None:  # Update 37: the WORLD LOG's last events and census
            try:
                trace_journal.finish(update37_records.snapshot(game, xr, 'world_trace'))
            except (OSError, RuntimeError):
                pass
        session_fps_last = read_session_fps() or session_fps_last
        result = dict(session_fps=session_fps_last, version=public_version(), port_version=port_version(), launch_args=sys.argv[1:], pid=game.pid, module_base=hex(game.base),
            bridge_hash=bridge_hash,
            xr_hash=xr_hash,
            eye_size=a.size,render_scale=a.render_scale,supersample=a.supersample,eyes=eye_options,live_changes=live_changes,capture_images=a.capture_images,
            timing=timing_snapshot(game,xr['SpidyXrTimingData']),
            ray_hash=ray_hash, ray_samples=list(ray_samples), rays=ray_snapshot(game, rays['SpidyRayData']),
            motion_hash=motion_hash, swing_speed=a.swing_speed, vr_settings=settings_start, xr_runtime=runtime,
            game_settings_refused=startup.fields.get('game_settings_refused'),
            game_registry=startup.fields.get('game_registry'),
            swing_samples=list(swing_samples),
            grab_samples=list(grab_samples), grab=grab_snapshot(game, rays['SpidyGrabData']),
            body=body_snapshot(game, xr['SpidyBodyData']) or body,
            punch=punch_snapshot(game, rays['SpidyPunchData']) or punch, punch_samples=list(punch_samples),
            punch_note=('punch/strikes: the Spider-Man 1 damage path (game_punch.cpp, sm2::notPorted), never started '
                        'in SM2; the punches are update37.combat38 (melee_taps, melee_merged, melee_dropped).'),
            strikes=strike_snapshot(game, rays['SpidyStrikeData']) or strikes, strike_samples=list(strike_samples),
            sense=sense_snapshot(game, rays['SpidySenseData']) or sense, sense_samples=list(sense_samples),
            ropes=rope_snapshot(game, rays['SpidyRopeData']) or ropes,
            steam_guard=steam_snapshot(game, xr['SpidySteamData']) or steam,
            shooter=shooter_snapshot(game, rays['SpidyShooterData']) or shooter,
            shooter_samples=list(shooter_samples),
            slow_motion=slow_motion_snapshot(game, xr['SpidySlowMotionData']) or slow_motion,
            slow_motion_samples=list(slow_motion_samples), haptics=haptics_last, wrist_hud=wrist_hud_last,
            motion_samples=list(motion_samples), appearance=appearance_snapshot(game,xr['SpidyAppearanceData']),
            **eye_lod.report_fields(eye_lod_sample(game, xr) or eye_lod_last),
            eye_lod_timeline=list(eye_lod_timeline),
            **shadow_camera.report_fields(shadow_camera_sample(game, xr) or shadow_camera_last, shadow_camera_modes),
            **shadow_distance.report_fields(shadow_distance_sample(game, xr) or shadow_distance_last),
            **far_shadows.report_fields(far_shadows_sample(game, xr) or far_shadows_last),
            update37={k: v or update37_last.get(k) for k, v in update37_records.snapshots(game, xr).items()},
            eye_shadows=shadow_snapshot(game, xr['SpidyShadowData']) or shadows_last,
            hud=hud_snapshot(game, xr['SpidyHudData']),
            prompts=prompt_snapshot(game, xr['SpidyPromptData']),
            eye_jobs=frame_snapshot(game, xr['SpidyStereoFrames']) or eye_jobs,
            render_memory=final_render_memory or render_memory, render_stop=render_stop, mip_bias=mip_bias_seen or mip_bias_early,
            free_commit_mb=dict(start=start_commit, lowest=lowest_commit),
            swing=swing_snapshot(game,rays['SpidySwingData']),
            samples=list(samples), final=snapshot(game, xr['SpidyXrData']), gpu=gpu_snapshot(game, xr['SpidyGpuData']),
            eye_snapshots=list(eye_shots), eye_snapshot_folder=str(eye_folder) if eye_shots else None,
            bridge=bridge_snapshot(game, bridge['SpidyBridgeData']), xr_stop=xr_stop, bridge_stop=bridge_stop,
            eye_history=history_snapshot(game, xr['SpidyEyeHistoryData']) or history_last,
            engine_taa=engine_taa_snapshot(game, xr['SpidyEngineTaaData']) or engine_taa_last,
            engine_taa_eyes=engine_taa_eyes_snapshot(game, xr.get('SpidyEngineTaaEyes')) or taa_eyes_last)
        result['game_entries_restored'] = all(game.read(game.base+rva,16) == code
                                              for rva,code in original_entries.items())
        result['frame_rates'] = frame_rates(list(samples))
        a.output.parent.mkdir(parents=True, exist_ok=True)
        try:
            if a.capture_images:
                result.update(save_eye_images(game, result['gpu'], a.output))
        except (OSError, ValueError, RuntimeError) as error:
            result['image_capture_error'] = str(error)
        result['passed'] = accepted(result)
        flush_address_finder_on_exit()
        write_report(result)
        hand_over_settings(result['final'] or (samples[-1] if samples else None))
        final = result['final']
        print(json.dumps(dict(final=final, gpu=result['gpu'], appearance=result['appearance'],
                              xr_stop=xr_stop, bridge_stop=bridge_stop), indent=2))
        for line in update37_records.console_lines(result.get('update37')):
            print(line, flush=True)
        if result['timing']:
            print('Mean milliseconds after warmup: '+', '.join(f'{k}={v["mean_ms"]:.2f}' for k,v in result['timing'].items()
                                                                if 'mean_ms' in v),flush=True)
            if (smoothing := result['timing'].get('eye_aa')):
                print(eye_aa_line(smoothing, a.eye_aa), flush=True)
            if (sharp := result['timing'].get('sharpen')):
                print(sharpen_line(sharp), flush=True)
            # Update 37 performance pass: copy_gpu is a wait, mostly not work (update37_records.copy_gpu_split).
            copy = result['timing'].get('copy_gpu') or {}
            split = update37_records.copy_gpu_split(copy.get('mean_ms'), (sharp or {}).get('gpu_ms_mean'))
            if split:
                result['copy_gpu_split'] = split
                print(f"Update 37 copy_gpu {split['copy_gpu_ms']} ms = GPU work {split['work_ms']} ms (sharpen pass, "
                      f"else two plain copies) + {split['wait_ms']} ms waiting behind the game's frame on the queue "
                      f"(not extra GPU load).", flush=True)
        if result['frame_rates']:
            rates=result['frame_rates']
            print(f"New native scene pairs: {rates['new_scene_fps']:.1f}/s; "
                  f"XR submissions including reuse: {rates['submitted_fps']:.1f}/s; "
                  f"reused pairs: {rates['reused_fps']:.1f}/s.",flush=True)
        print(live_settings.session_fps_line(result.get('session_fps')), flush=True)
        if result['render_memory']:
            memory = result['render_memory']
            print(f"Render memory: two frames used at most {memory['worst_two_frames_mb']:.0f} of "
                  f"{memory['ring_mb']:.0f} MB; {memory['overflow_frames']} of {memory['frames']} frames "
                  'did not fit.', flush=True)
        # 0.0.74 test 3 HEAP GUARD: the game rebuilt its render heaps and aliases (its log's "[Mem12] ...
        # Recreating heap and aliases") while VR ran; the eye bridge let go of the eyes and took them again.
        if (rebuilds := (result.get('gpu') or {}).get('heap_rebuilds')):
            print(f"Heap guard: the game rebuilt its render memory {rebuilds} time(s) in VR; the eye bridge let go "
                  'of the eye images and took them again from the next frame.', flush=True)
        if (seen := result.get('mip_bias')):
            if seen['status'] == 'late':
                print('Texture mip bias: the game made its graphics device before Spidy started, so it was not '
                      'seen or changed; start the game with this launcher.', flush=True)
            elif seen['samplers'] or seen['static_samplers']:
                negative = seen['negative_samplers'] + seen['negative_static_samplers']
                print(f"Texture mip bias: the game's most negative was {seen['most_negative']:+.2f} "
                      f"({negative} of {seen['samplers'] + seen['static_samplers']} samplers below 0); "
                      + (f"{seen['changed_samplers']} samplers and {seen['changed_root_signatures']} root "
                         f"signatures got {seen['bias']:+.2f}." if seen['bias'] != 'game' else 'kept as the game made it.'),
                      flush=True)
                print(f"Texture filtering of those: {seen['anisotropic_samplers']} anisotropic (up to "
                      f"{seen['most_anisotropy']}x), {seen['trilinear_samplers']} trilinear; "
                      + (f"{seen['raised_samplers']} raised to {seen['anisotropy']}x anisotropic."
                         if seen['anisotropy'] != 'game' else "kept as the game made them."), flush=True)
        tell_device_lost()
        print(f"Test {'passed' if result['passed'] else 'failed'}; report: {a.output.resolve()}", flush=True)
        if vr_lost and alive(game):
            # 0.0.5 hotfix 3: never silently flat. The launcher asks to close the game and start VR again.
            return CLOSE_GAME_EXIT
        return 0 if result['passed'] else 1
    finally:
        if previous_signal is not None: signal.signal(signal.SIGINT,previous_signal)
        if safe_report and not report_written:
            # Stopping failed part-way. Keep what was sampled.
            try:
                flush_address_finder_on_exit()
                write_report(safe_report(incomplete=True))
            except (OSError, ValueError): pass
        flushed.set()
        if console_handler: c.windll.kernel32.SetConsoleCtrlHandler(console_handler, False)
        try:
            if xr_active and alive(game):
                call_remote(process, xr['SpidyXrStop'])
            if bridge_active and alive(game):
                call_remote(process, bridge['SpidyStop'])
            # Last: it only counts frames, and the other two drive the game.
            if render_module and process and alive(game):
                call_remote(process, render_module['SpidyRenderMemoryStop'])
        except OSError:
            pass  # the game is closing; its exit removes the hooks with it
        if process:
            close(process)
        settle_display(alive(game))
        settle_mods(alive(game))
        game.close()


# 0.0.62: why the last start failed, for launch-game-vr.ps1 to show in a window when an older launcher (0.0.6.1)
# would only reset its PLAY VR button. Removed at every start; written only when the session could not run.
START_ERROR = ROOT/'reports'/'last-vr-error.txt'


def note_start_error(text):
    try:
        START_ERROR.parent.mkdir(parents=True, exist_ok=True)
        START_ERROR.write_text(str(text).strip() + '\n', encoding='utf-8')
    except OSError:
        pass


if __name__ == '__main__':
    # 0.0.71 hotfix: a dead standard handle from the launcher (WinError 6 at "finding the headset").
    std_handles.repair()
    try:
        START_ERROR.unlink()
    except OSError:
        pass
    try:
        with LauncherLock(): sys.exit(main())
    except KeyboardInterrupt:
        print('VR launch cancelled.',file=sys.stderr)
        settle_display(game_running())
        settle_mods(game_running())
        sys.exit(130)
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f'Spidy VR unavailable: {error}', file=sys.stderr)
        note_start_error(f'Spidy VR unavailable: {error}')
        settle_display(game_running())
        settle_mods(game_running())
        sys.exit(exit_code(error))
    except Exception as error:  # 0.0.62: never a silent stop, whatever went wrong
        import traceback
        traceback.print_exc()
        note_start_error(f'Spidy VR stopped on an internal error: {type(error).__name__}: {error}')
        try:
            settle_display(game_running())
            settle_mods(game_running())
        except Exception:
            pass
        sys.exit(1)
