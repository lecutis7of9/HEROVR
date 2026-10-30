"""0.0.74 test 3 CLEARER ERRORS: what to tell a player whose game died while starting, or lost its device.

The October 29 0.0.73 player's console said only "The game closed while starting." His game's own log said
`[Crash] Unhandled exception at address 0x000000006aa9251e (code: 0xc0000005): "Access violation"` four
seconds after it started, with a winmm.dll mod loader in its folder. This reads that line, finds which
library in the game the address is in (the module list Spidy kept while the game started), and names the
loaders. Nothing here guesses a game address: it only compares the logged one with the loaded modules.
Made by Agent Mulder.
"""
import re

CRASH_LINE = re.compile(r'\[Crash\] Unhandled exception at address (0x[0-9A-Fa-f]+)'
                        r'(?: \(code: (0x[0-9A-Fa-f]+)\))?(?:: "([^"\r\n]*)")?')
MINIDUMP_LINE = re.compile(r'\[Crash\] Created minidump at "([^"\r\n]+)"')
CODES = {'0xC0000005': 'access violation', '0xC0000409': 'stack buffer overrun', '0xC000001D': 'illegal instruction',
         '0xC00000FD': 'stack overflow', '0xC0000374': 'heap corruption', '0x80000003': 'breakpoint'}
# DXGI's "device lost" family: the game logs "Device removed detected (0x...)".
DEVICE_LOST = {'0x887A0005', '0x887A0006', '0x887A0007', '0x887A0020', '0x887A002B'}


def crash(log):
    """The game log's last crash: dict(address, code, description, minidump), or None."""
    found = list(CRASH_LINE.finditer(log or ''))
    if not found:
        return None
    last = found[-1]
    dump = MINIDUMP_LINE.findall(log or '')
    code = ('0x' + last.group(2)[2:].upper().rjust(8, '0')) if last.group(2) else None
    return dict(address=int(last.group(1), 16), code=code, description=last.group(3),
                minidump=dump[-1] if dump else None)


def module_at(address, modules):
    """(name, offset, path) of the module in `modules` (dicts with name, base, size, path) holding `address`,
    or None."""
    for module in modules or ():
        try:
            base, size = int(module['base']), int(module['size'])
        except (KeyError, TypeError, ValueError):
            continue
        if base <= address < base + size:
            return module.get('name') or '?', address - base, module.get('path')
    return None


def crash_message(found, modules=None, loaders=(), game_name='Spider-Man2.exe', game_folder=None):
    """The plain words for a game that crashed while loading (`found`: crash()'s dict; `loaders`: the mod loader
    files in the game's folder; `game_folder` tells a loader there from Windows' own library of that name)."""
    what = CODES.get(found.get('code') or '', found.get('description') or 'a crash')
    where = module_at(found['address'], modules)
    if where is None:
        place = (f"at 0x{found['address']:x}, which is not inside any library the game had loaded"
                 if modules else f"at 0x{found['address']:x}")
    elif where[0].lower() == game_name.lower():
        place = f"in the game's own code ({where[0]}+0x{where[1]:x})"
    else:
        place = f'in {where[0]} (+0x{where[1]:x})'
    text = f'The game crashed while loading ({what} {place}).'
    in_folder = where is not None and (not where[2] or not game_folder or
                                       str(where[2]).lower().replace('/', '\\').rsplit('\\', 1)[0]
                                       == str(game_folder).lower().replace('/', '\\').rstrip('\\'))
    crashed_in_loader = in_folder and any(where[0].lower() == name.lower() for name in loaders)
    windows = where is not None and where[2] and '\\windows\\' in str(where[2]).lower().replace('/', '\\')
    if crashed_in_loader:
        text += (f' {where[0]} is not part of the game or Spidy: it is a mod loader in the game\'s folder. Turn the '
                 'MOD CHECK on (OPTIONS) and say yes when it offers to move it aside, or remove that mod.')
    elif loaders:
        text += (f" Likely cause: {', '.join(loaders)} in the game's folder, a loader for another mod. Turn the MOD "
                 'CHECK on (OPTIONS) and say yes when it offers to move it aside, or turn that mod off in your mod '
                 'manager (Overstrike, Vortex...).')
    elif where is not None and not windows and where[0].lower() != game_name.lower() \
            and not where[0].lower().startswith('spidy'):
        text += f' {where[0]} is not part of the game: an overlay or another tool may be the cause.'
    text += ' Press START VR again; if it happens again, press Reports and send the zip (it has the crash dump).'
    return text


def device_lost_steps(reason, name, when):
    """The console's words for a lost graphics device (DXGI_ERROR_ACCESS_DENIED and the rest of its family)."""
    return (f'The game lost its graphics device {when} ({reason}, {name}): its "problem has occurred with your '
            'display driver" message. To fix it, do these and press START VR after each: 1) plug the monitor into '
            "the graphics card, not the motherboard (the processor's graphics); 2) Windows Settings > System > "
            'Display > Graphics: set Spider-Man2.exe, vrserver.exe and vrcompositor.exe to High performance (the '
            'TWO-GPU CHECK does this for you); 3) install the newest driver for your graphics card; 4) OPTIONS > '
            'Normal camera on the monitor: On. If it happens '
            'again, press Reports and send the zip in #spidy-bugs on Discord.')
