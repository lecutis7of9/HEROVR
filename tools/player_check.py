# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""Update 24: the checks that make the shareable package unzip-and-play, in plain words for a player.

    python player_check.py                 everything below, for this folder (exit 1 when VR cannot start)
    python player_check.py folder DIR      only the folder checks (package.ps1's self-test)
    python player_check.py runtime DIR     the modules in DIR that still need Microsoft's Visual C++
                                           Redistributable (exit 3 when any; package.ps1's self-test)

* The folder: Spidy must write its reports and settings there. Program Files (Windows guards it), a folder
  OneDrive syncs (it can lock or remove files while the game holds them) and a very long path are warned about.
* Microsoft's C++ runtime: since Update 24 every Spidy module has it built in (/MT, CMakeLists.txt). A build
  whose modules still import it (vcruntime140.dll, msvcp140.dll) needs the Redistributable 14.40 or newer.
* The game: Steam's copy from every Steam library (libraryfolders.vdf), else the Epic Games Store's, and
  whether its version is the one Spidy supports.
* The OpenXR runtime Windows has active (Virtual Desktop, SteamVR, Meta Quest Link); xr_runtime.detect finds
  the headset itself, this only names it or says that none is set.
Standard library only; the PE reader and the text parsers run anywhere (tests on Linux).
"""
import hashlib
import os
import pathlib
import re
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
STEAM_APP_ID = '2651280'
GAME_EXE = 'Spider-Man2.exe'
GAME_FOLDER = "Marvel's Spider-Man 2"
# The longest path Spidy itself makes inside its folder: reports\stereo-modules\<sha-256>\spidy_stereo_probe.dll.
DEEPEST = len(r'\reports\stereo-modules\\') + 64 + len(r'\spidy_stereo_probe.dll')
LONG_PATH = 259  # MAX_PATH less the terminator: older Windows parts and the game's loader stop there
# Microsoft's Visual C++ Redistributable (not the Universal CRT, part of Windows 10 and 11).
VC_REDIST = re.compile(r'^(vcruntime140|msvcp140|concrt140|vcomp140)[^\\/]*\.dll$', re.I)  # as pe_imports.hpp


# --- Microsoft's C++ runtime ----------------------------------------------------------------------------------

def pe_imports(data):
    """The DLL names a PE file (bytes) imports, lower case, in order; [] for a file that is not one."""
    try:
        if data[:2] != b'MZ':
            return []
        pe = struct.unpack_from('<I', data, 0x3C)[0]
        if data[pe:pe + 4] != b'PE\0\0':
            return []
        sections = struct.unpack_from('<H', data, pe + 6)[0]
        optional_size = struct.unpack_from('<H', data, pe + 20)[0]
        optional = pe + 24
        magic = struct.unpack_from('<H', data, optional)[0]
        directories = optional + (112 if magic == 0x20B else 96)
        count = struct.unpack_from('<I', data, directories - 4)[0]
        if count < 2:
            return []
        rva = struct.unpack_from('<I', data, directories + 8)[0]
        if not rva:
            return []
        table = []
        start = optional + optional_size
        for index in range(sections):
            header = start + 40 * index
            size, address, raw_size, raw = struct.unpack_from('<IIII', data, header + 8)
            table.append((address, max(size, raw_size), raw))

        def offset(address):
            for begin, size, raw in table:
                if begin <= address < begin + size:
                    return raw + address - begin
            raise ValueError('address outside the sections')

        names, at = [], offset(rva)
        for _ in range(4096):
            entry = struct.unpack_from('<5I', data, at)
            if not any(entry):
                break
            name = offset(entry[3])
            end = data.index(b'\0', name)
            names.append(data[name:end].decode('ascii', 'replace').lower())
            at += 20
        return names
    except (struct.error, ValueError, IndexError):
        return []


def needs_vc_redist(data):
    """The Redistributable's DLLs a PE file (bytes) imports."""
    return [name for name in pe_imports(data) if VC_REDIST.match(name)]


def runtime_users(folder, names=None):
    """{module: [Redistributable DLLs it imports]} for Spidy's own modules (spidy_*.dll/exe) in `folder`.
    bHaptics' library is not Spidy's: it uses vcruntime140.dll, which the game itself loads."""
    folder = pathlib.Path(folder)
    found = {}
    for path in sorted(folder.glob('spidy_*.*')) if names is None else (folder / n for n in names):
        if path.suffix.lower() not in ('.dll', '.exe') or not path.is_file():
            continue
        dlls = needs_vc_redist(path.read_bytes())
        if dlls:
            found[path.name] = dlls
    return found


def system_runtime_version(system=None):
    """msvcp140.dll's version in System32 as a tuple, None when the Redistributable is not installed."""
    system = pathlib.Path(system or os.path.join(os.environ.get('SystemRoot', r'C:\Windows'), 'System32'))
    if not all((system / n).is_file() for n in ('vcruntime140.dll', 'vcruntime140_1.dll', 'msvcp140.dll')):
        return None
    try:
        from inspect_game import file_version
        text = file_version(system / 'msvcp140.dll') or '0'
    except (ImportError, OSError, AttributeError):
        return (0,)
    return tuple(int(part) for part in re.findall(r'\d+', text)) or (0,)


def runtime_problem(folder, version=system_runtime_version):
    """None, or what to do when this build's modules still need a Redistributable the PC lacks."""
    users = runtime_users(folder)
    if not users:
        return None
    have = version()
    if have and have >= (14, 40):
        return None
    return ("Microsoft's Visual C++ runtime 14.40 or newer is needed by " + ', '.join(sorted(users)) +
            (f' (this PC has {".".join(map(str, have))})' if have else '') +
            ". Install the x64 'Visual C++ Redistributable' from https://aka.ms/vs/17/release/vc_redist.x64.exe, "
            'or build again (Update 24 builds have it built in), then start again.')


# --- The folder -----------------------------------------------------------------------------------------------

def folder_problems(root, environ=None, writable=None):
    """(errors, warnings) about the folder Spidy runs from. `writable(path)` defaults to really writing a file."""
    environ = os.environ if environ is None else environ
    root = pathlib.PureWindowsPath(str(root))
    text = str(root)
    lowered = text.lower().rstrip('\\') + '\\'
    errors, warnings = [], []
    if writable is None:
        writable = _can_write
    if not writable(root):
        errors.append(f"Spidy can't write in {text}. Move the whole folder somewhere you own, such as Documents or "
                      r'C:\Games, then start again.')
    guarded = [environ.get(n) for n in ('ProgramFiles', 'ProgramFiles(x86)', 'ProgramW6432')]
    guarded += [r'C:\Program Files', r'C:\Program Files (x86)']
    if any(g and lowered.startswith(g.lower().rstrip('\\') + '\\') for g in guarded):
        warnings.append(r'Spidy is in Program Files, which Windows guards: settings and reports may not save. '
                        r'Move the folder to Documents or C:\Games.')
    synced = [environ.get(n) for n in ('OneDrive', 'OneDriveConsumer', 'OneDriveCommercial')]
    if any(s and lowered.startswith(s.lower().rstrip('\\') + '\\') for s in synced) or \
            any(part.lower().startswith('onedrive') for part in root.parts[1:]):
        warnings.append(r'Spidy is in a folder OneDrive syncs, which can lock or remove its files while you play. '
                        r'Move the folder to C:\Games (or another folder OneDrive does not sync).')
    if len(text) + DEEPEST > LONG_PATH:
        warnings.append(f'The folder path is long ({len(text)} characters): files Spidy makes inside it may pass '
                        f"Windows' 260-character limit. Move the folder somewhere shorter, such as C:\\Games.")
    return errors, warnings


def _can_write(root):
    probe = pathlib.Path(str(root)) / 'reports' / f'.write-test-{os.getpid()}'
    try:
        probe.parent.mkdir(parents=True, exist_ok=True)
        probe.write_bytes(b'ok')
        probe.unlink()
        return True
    except OSError:
        return False


# --- The game -------------------------------------------------------------------------------------------------

def vdf_values(text, key):
    """Every value of a quoted key in Valve's VDF text (libraryfolders.vdf, appmanifest_*.acf)."""
    return [v.replace('\\\\', '\\') for v in re.findall(r'"%s"\s+"([^"]*)"' % re.escape(key), text, re.I)]


def _read(path):
    try:
        return pathlib.Path(path).read_text(encoding='utf-8', errors='replace')
    except OSError:
        return ''


def steam_libraries(roots):
    """Steam's folder(s), then every library their libraryfolders.vdf lists, once each."""
    found, seen = [], set()
    for root in roots:
        root = pathlib.Path(root)
        for library in [str(root)] + [p for vdf in ('steamapps/libraryfolders.vdf', 'config/libraryfolders.vdf')
                                      for p in vdf_values(_read(root / vdf), 'path')]:
            key = os.path.normcase(library.rstrip('\\/'))
            if library and key not in seen:
                seen.add(key)
                found.append(pathlib.Path(library))
    return found


def steam_game(libraries):
    """Spider-Man2.exe in the Steam library whose app manifest has the game; None when none has."""
    for library in libraries:
        acf = library / 'steamapps' / f'appmanifest_{STEAM_APP_ID}.acf'
        if not acf.is_file():
            continue
        folder = (vdf_values(_read(acf), 'installdir') or [GAME_FOLDER])[0]
        exe = library / 'steamapps' / 'common' / folder / GAME_EXE
        if exe.is_file():
            return exe
    return None


def steam_roots():
    roots = []
    try:
        import winreg
    except ImportError:
        return roots
    for hive, key, name in ((winreg.HKEY_CURRENT_USER, r'Software\Valve\Steam', 'SteamPath'),
                            (winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\WOW6432Node\Valve\Steam', 'InstallPath'),
                            (winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\Valve\Steam', 'InstallPath')):
        try:
            with winreg.OpenKey(hive, key) as opened:
                value = winreg.QueryValueEx(opened, name)[0]
        except OSError:
            continue
        if value and (pathlib.Path(value) / 'steam.exe').is_file():
            roots.append(pathlib.Path(value.replace('/', '\\')))
    default = pathlib.Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Steam'
    if (default / 'steam.exe').is_file():
        roots.append(default)
    return roots


def epic_game(manifests=None):
    import json
    folder = pathlib.Path(manifests or pathlib.Path(os.environ.get('ProgramData', r'C:\ProgramData')) /
                          'Epic/EpicGamesLauncher/Data/Manifests')
    for item in sorted(folder.glob('*.item')) if folder.is_dir() else ():
        try:
            manifest = json.loads(item.read_text(encoding='utf-8', errors='replace'))
        except (OSError, ValueError):
            continue
        if isinstance(manifest, dict) and 'Spider-Man 2' in str(manifest.get('DisplayName', '')):
            exe = pathlib.Path(str(manifest.get('InstallLocation', ''))) / GAME_EXE
            if exe.is_file():
                return exe
    return None


def game_verdict(digest, version, expected_digest, expected_version):
    """('steam'|'other_copy'|'unsupported', message or None) for a game file's SHA-256 and version."""
    if digest == expected_digest:
        return 'steam', None
    if version == expected_version or not version:
        if not version:  # the version could not be read: the running game is checked instead, never blocked here
            return 'other_copy', f'Spidy could not read the version of this {GAME_EXE}; it checks the running game.'
        return 'other_copy', (f'This {GAME_EXE} is version {expected_version} but not Steam\'s file (an Epic Games '
                              'Store copy?). Spidy tries it.')
    return 'unsupported', (f'Your Spider-Man 2 is version {version or "unknown"}; this Spidy supports '
                           f'{expected_version} (Steam). A game update changed it: VR would not work. Wait for a '
                           'Spidy update for this game version.')


def find_game(roots=None, epic=epic_game):
    """(Spider-Man2.exe or None, the libraries looked in)."""
    libraries = steam_libraries(steam_roots() if roots is None else roots)
    return (steam_game(libraries) or epic()), libraries


def game_problem(exe, libraries):
    """(error or None, note or None) for the game the launcher would start. Only a known other version is an
    error: a game not found is a note (Steam may still start it by its app id)."""
    from inspect_game import EXPECTED_SHA256, EXPECTED_VERSION, file_version
    if exe is None:
        where = ', '.join(str(lib) for lib in libraries) or 'no Steam folder found'
        return None, (f'Spider-Man 2 was not found (Steam libraries looked in: {where}; the Epic Games Store has '
                      'none). Spidy asks Steam to start it; if nothing starts, install it, or start it yourself and '
                      'then start Spidy VR again.')
    try:
        digest = hashlib.sha256(pathlib.Path(exe).read_bytes()).hexdigest()
        version = file_version(exe)
    except OSError as error:
        return None, f'Spidy could not read {exe} ({error}); it checks the running game instead.'
    kind, message = game_verdict(digest, version, EXPECTED_SHA256, EXPECTED_VERSION)
    return (message, None) if kind == 'unsupported' else (None, message)


# --- The OpenXR runtime ---------------------------------------------------------------------------------------

def runtime_line(active, installed):
    """What the console says about Windows' OpenXR runtime before the headset is looked for."""
    if active:
        try:
            import xr_runtime
            label = xr_runtime.name(active)
        except ImportError:
            label = pathlib.Path(active).stem
        return f"Windows' active OpenXR runtime: {label}."
    if installed:
        return ("Windows has no active OpenXR runtime set; Spidy looks in Virtual Desktop, SteamVR and Meta Quest "
                'Link itself. If it finds no headset, start your VR app and connect the headset first.')
    return ('No OpenXR runtime is installed. Install and start Virtual Desktop (the tested way to play), SteamVR or '
            'the Meta Quest Link app, connect the headset, then start Spidy VR again.')


# --- Before a session -----------------------------------------------------------------------------------------

def before_session(root=ROOT, launching=True, say=print):
    """Run by run_game_vr.py before it writes anything. Raises RuntimeError with what to do when VR cannot
    start; prints warnings and notes."""
    errors, warnings = folder_problems(root)
    problem = runtime_problem(pathlib.Path(root) / 'build' / 'windows-ninja')
    if problem:
        errors.append(problem)
    if launching:
        exe, libraries = find_game()
        error, note = game_problem(exe, libraries)
        if error:
            errors.append(error)
        elif exe:
            say(f'Game: {exe}.')
        if note:
            say(note)
    try:
        import xr_runtime
        active, available = xr_runtime.registered()
        say(runtime_line(active, xr_runtime.runtimes()))
    except Exception:  # only a line of information: never a reason not to start
        pass
    for warning in warnings:
        say('WARNING: ' + warning)
    if errors:
        raise RuntimeError(' '.join(errors))


def main(argv):
    if len(argv) >= 2 and argv[0] == 'runtime':
        users = runtime_users(argv[1])
        for name, dlls in users.items():
            print(f"{name} needs Microsoft's C++ runtime: {', '.join(dlls)}")
        print('ok' if not users else 'needs the Visual C++ Redistributable')
        return 3 if users else 0
    if len(argv) >= 2 and argv[0] == 'folder':
        errors, warnings = folder_problems(argv[1])
        for line in errors + ['WARNING: ' + w for w in warnings]:
            print(line)
        print('ok' if not errors else 'the folder cannot be used')
        return 1 if errors else 0
    try:
        before_session(ROOT, launching=True)
    except RuntimeError as error:
        print(error)
        return 1
    print('ok')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
