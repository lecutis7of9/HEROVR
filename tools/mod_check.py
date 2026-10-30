"""0.0.74 test 3 MOD CHECK: libraries in the game's folder that load other mods into the game.

A program loads a library from its own folder before Windows' own, so a file named like one of Windows'
libraries beside Spider-Man2.exe is a loader for another mod, an overlay or a trainer (an ASI loader is
usually dinput8.dll, version.dll or winmm.dll). The October 29 0.0.73 player's game crashed four seconds
after it started (access violation at 0x6aa9251e) with a winmm.dll loader there that came back every time
he removed it: a mod manager redeployed it at each launch.

With the player's yes (the launcher asks), a session moves them into <game folder>\\_hero-vr-moved before
the game starts and puts them back when the game closes. Nothing here raises: a file that cannot be moved
is named, and VR starts as before.
Made by Agent Mulder.
"""
import hashlib
import json
import os
import pathlib
import time

MOVED_FOLDER = '_hero-vr-moved'
MANIFEST = 'moved.json'
# Windows' own libraries, by name, that mods use as loaders (proxy DLLs, ASI loaders, ReShade, injectors).
# The game's folder holds none of them. Not dbghelp.dll or the Bink/Oodle libraries: games ship those.
LOADERS = ('dxgi.dll', 'd3d12.dll', 'd3d11.dll', 'd3d10.dll', 'd3d9.dll', 'd3d8.dll', 'ddraw.dll',
           'dinput8.dll', 'dinput.dll', 'version.dll', 'winmm.dll', 'winhttp.dll', 'wininet.dll',
           'xinput1_3.dll', 'xinput1_4.dll', 'xinput9_1_0.dll', 'dsound.dll', 'opengl32.dll', 'msacm32.dll',
           'msvfw32.dll')


def found(folder, names=LOADERS):
    """The loader files in `folder`, as they are named there ([] when it cannot be read)."""
    try:
        present = {entry.name.lower(): entry.name for entry in pathlib.Path(folder).iterdir() if entry.is_file()}
    except OSError:
        return []
    return [present[name] for name in names if name in present]


def _digest(path):
    try:
        return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()
    except OSError:
        return None


def _pointer(reports):
    return pathlib.Path(reports)/'mod-check-moved.json'


def move_aside(folder, reports, names=None, log=print):
    """Move the loaders out of the game's `folder` for this session. Returns the names moved.

    The manifest (in the moved folder) and a pointer in `reports` let a later session, or `--restore`, put
    them back however this one ends."""
    folder = pathlib.Path(folder)
    names = found(folder) if names is None else names
    if not names:
        return []
    target = folder/MOVED_FOLDER
    manifest = target/MANIFEST
    try:
        target.mkdir(exist_ok=True)
        entries = json.loads(manifest.read_text(encoding='utf-8')).get('files', []) if manifest.exists() else []
    except (OSError, ValueError):
        entries = []
    moved, failed = [], []
    for name in names:
        source, kept = folder/name, target/name
        if kept.exists():
            # Still here from a session that could not put it back: keep that one, drop the duplicate.
            if _digest(kept) == _digest(source):
                try:
                    source.unlink()
                    moved.append(name)
                except OSError as error:
                    failed.append(f'{name} ({error.strerror or error})')
            else:
                failed.append(f'{name} (a different {name} is already in {MOVED_FOLDER})')
            continue
        try:
            os.replace(source, kept)
        except OSError as error:
            failed.append(f'{name} ({error.strerror or error})')
            continue
        moved.append(name)
        entries = [e for e in entries if e.get('name', '').lower() != name.lower()]
        entries.append(dict(name=name, sha256=_digest(kept)))
    if entries:
        record = dict(game_folder=str(folder), moved_at=time.strftime('%Y-%m-%d %H:%M:%S'),
                      token=f'{time.time():.6f}', files=entries)
        try:
            manifest.write_text(json.dumps(record, indent=2)+'\n', encoding='utf-8')
            pathlib.Path(reports).mkdir(parents=True, exist_ok=True)
            _pointer(reports).write_text(json.dumps(dict(game_folder=str(folder)))+'\n', encoding='utf-8')
        except OSError:
            pass
    if moved:
        log(f"MOD CHECK: moved {', '.join(moved)} out of the game's folder into {target} for this VR session; "
            'it goes back when the game closes.')
    if failed:
        log(f"MOD CHECK: could not move {', '.join(failed)}; the game starts with "
            f"{'it' if len(failed) == 1 else 'them'} (close any mod manager and press START VR again).")
    return moved


def came_back(folder, moved):
    """Of the files moved aside, those back in the game's `folder` (a mod manager redeployed them)."""
    present = {name.lower() for name in found(folder)}
    return [name for name in moved if name.lower() in present]


def came_back_text(names):
    """The console's plain words when a moved loader is back as the game starts."""
    one = len(names) == 1
    listed = ', '.join(names)
    return (f"MOD CHECK: {listed} came back into the game's folder while the game was starting, so "
            f"{'it is' if one else 'they are'} loaded in the game again. Something puts "
            f"{'it' if one else 'them'} back at every launch - usually a mod manager such as Overstrike "
            '(its script or ASI loader support) or Vortex redeploying its mods. Turn that mod (or the loader) off '
            'in the mod manager, then press START VR.')


def _token(folder):
    try:
        return json.loads((pathlib.Path(folder)/MOVED_FOLDER/MANIFEST).read_text(encoding='utf-8')).get('token')
    except (OSError, ValueError, AttributeError):
        return None


def restore(reports, game_running=False, log=print, only_token=None):
    """Put back what an earlier session moved aside. Not while the game runs (it has them open, or will
    look again at its next start): the next session or `--restore` does it then. `only_token`: only the files
    one session moved (a waiter must not undo the next session's move). Returns the names put back."""
    pointer = _pointer(reports)
    try:
        folder = pathlib.Path(json.loads(pointer.read_text(encoding='utf-8'))['game_folder'])
    except (OSError, ValueError, KeyError, TypeError):
        return []
    if game_running or (only_token is not None and _token(folder) != only_token):
        return []
    target = folder/MOVED_FOLDER
    try:
        entries = json.loads((target/MANIFEST).read_text(encoding='utf-8')).get('files', [])
    except (OSError, ValueError):
        entries = []
    back, kept, left = [], [], []
    for entry in entries:
        name = entry.get('name')
        if not name:
            continue
        source, home = target/name, folder/name
        if not source.exists():
            continue
        if home.exists():
            if _digest(home) == _digest(source):
                try:
                    source.unlink()  # a mod manager put the same file back already
                    back.append(name)
                except OSError:
                    left.append(entry)
            else:
                kept.append(name)
                left.append(entry)
            continue
        try:
            os.replace(source, home)
            back.append(name)
        except OSError:
            left.append(entry)
    try:
        if left:
            record = dict(game_folder=str(folder), token=_token(folder), files=left)
            (target/MANIFEST).write_text(json.dumps(record, indent=2)+'\n', encoding='utf-8')
        else:
            (target/MANIFEST).unlink(missing_ok=True)
            try:
                target.rmdir()
            except OSError:
                pass  # something else is in there: it stays
            pointer.unlink(missing_ok=True)
    except OSError:
        pass
    if back:
        log(f"MOD CHECK: put {', '.join(back)} back in the game's folder.")
    if kept:
        log(f"MOD CHECK: kept {', '.join(kept)} in {target}: a different file of that name is in the game's "
            'folder now (a mod manager put a new one there). Delete whichever you do not want.')
    return back


def pending(reports):
    """Whether files moved aside are waiting to go back."""
    return _pointer(reports).exists()


def restore_when_closed(pid, reports):
    """Start a small hidden waiter that puts this session's files back when the game (`pid`) closes."""
    import subprocess
    import sys
    try:
        folder = json.loads(_pointer(reports).read_text(encoding='utf-8'))['game_folder']
    except (OSError, ValueError, KeyError, TypeError):
        return
    token = _token(folder)
    flags = 0x00000008 | 0x08000000  # DETACHED_PROCESS | CREATE_NO_WINDOW
    subprocess.Popen([sys.executable, str(pathlib.Path(__file__).resolve()), '--restore-after', str(int(pid)), str(token)],
                     stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                     creationflags=flags, close_fds=True)


def wait_for_exit(pid):
    """Block until process `pid` has ended (at most a day)."""
    import ctypes
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.restype = ctypes.c_void_p
    handle = kernel.OpenProcess(0x00100000, False, int(pid))  # SYNCHRONIZE
    if not handle:
        return  # gone already
    try:
        kernel.WaitForSingleObject(ctypes.c_void_p(handle), 24*3600*1000)
    finally:
        kernel.CloseHandle(ctypes.c_void_p(handle))


def _running(image):
    """Whether a process of that image name runs (a new game started meanwhile keeps its files)."""
    import subprocess
    try:
        out = subprocess.run(['tasklist', '/FI', f'IMAGENAME eq {image}', '/NH'], stdin=subprocess.DEVNULL,
                             capture_output=True, text=True, timeout=20, creationflags=0x08000000).stdout
    except (OSError, subprocess.SubprocessError):
        return True  # unknown: leave them for the next START VR
    return image.lower() in out.lower()


if __name__ == '__main__':
    import sys
    root = pathlib.Path(__file__).resolve().parents[1]
    args = sys.argv[1:]
    if args[:1] == ['--restore-after'] and len(args) > 1:
        try:
            wait_for_exit(args[1])
            time.sleep(2)  # Windows lets go of the game's files
        except Exception:  # noqa: BLE001
            pass
        if not _running('Spider-Man2.exe'):
            restore(root/'reports', log=lambda line: None, only_token=args[2] if len(args) > 2 else None)
    elif '--restore' in args:
        if not restore(root/'reports'):
            print('MOD CHECK: nothing to put back.')
    else:
        print('Usage: mod_check.py --restore   (puts back the files a VR session moved aside)')
