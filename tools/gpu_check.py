"""0.0.74 test 3 TWO-GPU CHECK: a PC with a graphics chip in its processor and a graphics card.

The October 29 0.0.73 player (RTX 5090 beside an AMD processor's graphics, SteamVR, Quest) lost the game's
graphics device as VR began (DXGI_ERROR_ACCESS_DENIED) once another mod was out of the way. With two graphics
adapters, Windows may give the game, SteamVR's server or its compositor a different one, and a VR picture
then has to cross between them. Windows' own per-app setting (Settings > System > Display > Graphics,
stored in HKCU\\Software\\Microsoft\\DirectX\\UserGpuPreferences) pins each to the graphics card.

A session sets GpuPreference=2 (high performance) for Spider-Man2.exe, vrserver.exe and vrcompositor.exe,
keeping anything else in each value, after writing the old values to reports\\gpu-preference-backup.json.
`restore` puts the old values back (switching the check off, the launcher's revert, or `--restore`). Nothing
here raises: a PC it cannot read is left as it is.
Made by Agent Mulder.
"""
import json
import pathlib
import re

KEY = r'Software\Microsoft\DirectX\UserGpuPreferences'
BACKUP = 'gpu-preference-backup.json'
HIGH_PERFORMANCE = 2
INTEL, AMD, NVIDIA, QUALCOMM, MICROSOFT = 0x8086, 0x1002, 0x10DE, 0x5143, 0x1414
# A graphics chip in the processor has little or no memory of its own (BIOS carve-out: 0.5 to 2 GB, at most 4).
INTEGRATED_MEMORY_MB = 4096
INTEGRATED_NAMES = re.compile(r'(UHD|Iris|HD) Graphics|Radeon\(TM\) Graphics|Radeon Graphics$|Radeon \d{3}M|'
                              r'Vega \d+ Graphics|Adreno', re.I)


def integrated(adapter):
    """Whether this adapter (dict with name, vendor, dedicated_mb) is a processor's graphics chip."""
    vendor, name = adapter.get('vendor'), adapter.get('name') or ''
    if vendor in (NVIDIA, MICROSOFT) or adapter.get('software'):
        return False
    if vendor == QUALCOMM or INTEGRATED_NAMES.search(name):
        return True
    return vendor in (INTEL, AMD) and (adapter.get('dedicated_mb') or 0) < 1024


def classify(adapters):
    """{'hybrid', 'integrated', 'discrete', 'monitor_on_integrated'} for a list of adapters (dicts: name, vendor,
    dedicated_mb, software, outputs). A graphics card listed twice counts once."""
    seen, integ, disc = set(), [], []
    for adapter in adapters or ():
        if adapter.get('software') or adapter.get('vendor') == MICROSOFT:
            continue  # Microsoft Basic Render Driver
        key = (adapter.get('vendor'), adapter.get('device'), adapter.get('name'))
        if key in seen:
            continue
        seen.add(key)
        (integ if integrated(adapter) else disc).append(adapter)
    hybrid = bool(integ and disc)
    on_integrated = hybrid and any(a.get('outputs') for a in integ) and not any(a.get('outputs') for a in disc)
    return dict(hybrid=hybrid, integrated=[a.get('name') for a in integ], discrete=[a.get('name') for a in disc],
                monitor_on_integrated=on_integrated)


def adapters():
    """The PC's graphics adapters from DXGI ([] when they cannot be read, or not on Windows)."""
    try:
        import ctypes as c
        from ctypes import wintypes
    except ImportError:
        return []
    try:
        class Desc1(c.Structure):
            _fields_ = [('Description', c.c_wchar*128), ('VendorId', wintypes.UINT), ('DeviceId', wintypes.UINT),
                        ('SubSysId', wintypes.UINT), ('Revision', wintypes.UINT),
                        ('DedicatedVideoMemory', c.c_size_t), ('DedicatedSystemMemory', c.c_size_t),
                        ('SharedSystemMemory', c.c_size_t), ('LuidLow', wintypes.DWORD), ('LuidHigh', wintypes.LONG),
                        ('Flags', wintypes.UINT)]
        dxgi = c.WinDLL('dxgi')
        iid = (c.c_ubyte*16).from_buffer_copy(
            __import__('uuid').UUID('770aae78-f26f-4dba-a829-253c83d1b387').bytes_le)  # IDXGIFactory1
        factory = c.c_void_p()
        dxgi.CreateDXGIFactory1.argtypes = [c.c_void_p, c.POINTER(c.c_void_p)]
        if dxgi.CreateDXGIFactory1(iid, c.byref(factory)) or not factory.value:
            return []

        def method(obj, index, *argtypes):
            table = c.cast(c.cast(obj, c.POINTER(c.c_void_p))[0], c.POINTER(c.c_void_p))
            return c.WINFUNCTYPE(c.c_long, c.c_void_p, *argtypes)(table[index])

        def release(obj):
            if obj:
                c.WINFUNCTYPE(c.c_ulong, c.c_void_p)(
                    c.cast(c.cast(obj, c.POINTER(c.c_void_p))[0], c.POINTER(c.c_void_p))[2])(obj)

        found = []
        try:
            for index in range(16):
                adapter = c.c_void_p()
                # IDXGIFactory1::EnumAdapters1 (12); DXGI_ERROR_NOT_FOUND ends the list
                if method(factory, 12, wintypes.UINT, c.POINTER(c.c_void_p))(factory, index, c.byref(adapter)):
                    break
                try:
                    desc = Desc1()
                    if method(adapter, 10, c.POINTER(Desc1))(adapter, c.byref(desc)):  # GetDesc1
                        continue
                    outputs = 0
                    for out_index in range(16):
                        output = c.c_void_p()
                        # IDXGIAdapter::EnumOutputs (7)
                        if method(adapter, 7, wintypes.UINT, c.POINTER(c.c_void_p))(adapter, out_index,
                                                                                   c.byref(output)):
                            break
                        outputs += 1
                        release(output)
                    found.append(dict(name=desc.Description, vendor=desc.VendorId, device=desc.DeviceId,
                                      dedicated_mb=desc.DedicatedVideoMemory >> 20,
                                      shared_mb=desc.SharedSystemMemory >> 20,
                                      software=bool(desc.Flags & 2), outputs=outputs))
                finally:
                    release(adapter)
        finally:
            release(factory)
        return found
    except Exception:  # noqa: BLE001 - never stops a session
        return []


def steamvr_folders(manifest=None, local_appdata=None):
    """Where SteamVR may be: beside its OpenXR runtime manifest, then openvrpaths.vrpath's runtimes."""
    folders = []
    if manifest:
        parent = pathlib.Path(manifest).parent
        if (parent/'bin'/'win64').is_dir():
            folders.append(parent)
    if local_appdata:
        try:
            paths = json.loads((pathlib.Path(local_appdata)/'openvr'/'openvrpaths.vrpath').read_text(
                encoding='utf-8', errors='replace'))
            folders += [pathlib.Path(p) for p in paths.get('runtime') or [] if isinstance(p, str)]
        except (OSError, ValueError, AttributeError):
            pass
    unique = []
    for folder in folders:
        if folder not in unique:
            unique.append(folder)
    return unique


def programs(game_exe, steamvr):
    """The full paths the preference is set for (Windows keys the setting by the program's full path)."""
    paths = [str(game_exe)] if game_exe else []
    for folder in steamvr:
        for name in ('vrserver.exe', 'vrcompositor.exe'):
            candidate = pathlib.Path(folder)/'bin'/'win64'/name
            if candidate.is_file():
                paths.append(str(candidate))
    return paths


def with_preference(old, preference=HIGH_PERFORMANCE):
    """The value data with GpuPreference set, keeping the rest ('SwapEffectUpgradeEnable=1;' and the like)."""
    parts = [p for p in (old or '').split(';') if p.strip() and not p.strip().lower().startswith('gpupreference=')]
    return ''.join(f'{p};' for p in [f'GpuPreference={preference}', *parts])


def _read(reg, name):
    try:
        with reg.OpenKey(reg.HKEY_CURRENT_USER, KEY, 0, reg.KEY_READ) as key:
            return reg.QueryValueEx(key, name)[0]
    except OSError:
        return None


def apply(paths, reports, reg=None, log=print):
    """Set the high-performance preference for `paths`; the first old value of each is kept in the backup
    (later sessions do not overwrite it). Returns the paths changed."""
    if reg is None:
        try:
            import winreg as reg
        except ImportError:
            return []
    backup_file = pathlib.Path(reports)/BACKUP
    try:
        backup = json.loads(backup_file.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        backup = {}
    changed = []
    try:
        key = reg.CreateKeyEx(reg.HKEY_CURRENT_USER, KEY, 0, reg.KEY_READ | reg.KEY_WRITE)
    except OSError as error:
        log(f"TWO-GPU CHECK: could not open Windows' graphics settings ({error}); nothing changed.")
        return []
    with key:
        for path in paths:
            old = _read(reg, path)
            new = with_preference(old)
            if old == new:
                continue
            if path not in backup:
                backup[path] = dict(old=old, set=new)
            else:
                backup[path]['set'] = new
            try:
                backup_file.parent.mkdir(parents=True, exist_ok=True)
                backup_file.write_text(json.dumps(backup, indent=2)+'\n', encoding='utf-8')
                reg.SetValueEx(key, path, 0, reg.REG_SZ, new)
                changed.append(path)
            except OSError as error:
                log(f'TWO-GPU CHECK: could not set {path} ({error}).')
    return changed


def restore(reports, reg=None, log=print):
    """Put back the values `apply` changed, where they are still what it set (a value changed since is the
    player's and stays). Returns the paths put back."""
    backup_file = pathlib.Path(reports)/BACKUP
    try:
        backup = json.loads(backup_file.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return []
    if reg is None:
        try:
            import winreg as reg
        except ImportError:
            return []
    back = []
    try:
        key = reg.CreateKeyEx(reg.HKEY_CURRENT_USER, KEY, 0, reg.KEY_READ | reg.KEY_WRITE)
    except OSError:
        return []
    with key:
        for path, entry in list(backup.items()):
            try:
                if _read(reg, path) == entry.get('set'):
                    if entry.get('old') is None:
                        reg.DeleteValue(key, path)
                    else:
                        reg.SetValueEx(key, path, 0, reg.REG_SZ, entry['old'])
                    back.append(path)
                backup.pop(path)
            except (OSError, AttributeError):
                pass
    try:
        if backup:
            backup_file.write_text(json.dumps(backup, indent=2)+'\n', encoding='utf-8')
        else:
            backup_file.unlink(missing_ok=True)
    except OSError:
        pass
    if back:
        log("TWO-GPU CHECK: Windows' graphics preference put back for "
            + ', '.join(pathlib.Path(p).name for p in back) + '.')
    return back


def check(game_exe, manifest, reports, local_appdata=None, found=None, reg=None, log=print):
    """The session's check: on a PC with two graphics adapters, pin the game and SteamVR to the graphics card
    and say where the monitor is. Returns the report's record."""
    found = adapters() if found is None else found
    kind = classify(found)
    record = dict(adapters=found, **kind, set_for=[])
    if not kind['hybrid']:
        return record
    paths = programs(game_exe, steamvr_folders(manifest, local_appdata))
    record['set_for'] = apply(paths, reports, reg=reg, log=log)
    card = kind['discrete'][0] if kind['discrete'] else 'the graphics card'
    log(f"TWO-GPU CHECK: this PC has two graphics adapters ({', '.join(kind['integrated'])} in the processor, "
        f"{', '.join(kind['discrete'])}). Windows is set to run "
        + (', '.join(pathlib.Path(p).name for p in paths) or 'the game') +
        f" on {card} (high performance); SteamVR picks it up the next time SteamVR starts. Your old settings are "
        'kept and come back when this check is turned off.')
    if kind['monitor_on_integrated']:
        log(f'TWO-GPU CHECK WARNING: your monitor is plugged into the processor\'s graphics '
            f"({', '.join(kind['integrated'])}), not into {card}. Each picture then crosses between the two and VR "
            f'can lose the graphics device. Plug the monitor cable into {card} (the ports lower on the back of '
            'a desktop PC). On a laptop this is normal.')
    return record


if __name__ == '__main__':
    import sys
    root = pathlib.Path(__file__).resolve().parents[1]
    if '--restore' in sys.argv[1:]:
        if not restore(root/'reports'):
            print('TWO-GPU CHECK: nothing to put back.')
    else:
        print(json.dumps(classify(adapters()), indent=2))
