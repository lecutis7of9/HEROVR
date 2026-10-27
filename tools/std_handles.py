"""0.0.70.1 hotfix: the standard handles this process was given, made usable before anything starts a program.

A player's 0.0.7 session ended at "finding the headset" with "[WinError 6] The handle is invalid": the launcher
had stopped a session before (Stop VR attaches to that session's console and frees it again), which leaves its
standard handles naming a console that is gone, and every PowerShell it starts afterwards hands that dead
handle on as standard input. Python's subprocess duplicates the standard input of the program it starts
(stdin not given): with a dead handle that is WinError 6, raised before the headset probe ever ran.

repair() replaces a standard handle that is set but dead with one on NUL (a missing one, NULL, is left:
subprocess then makes a pipe of its own). The launcher (build 12) also keeps its own handles valid; this guard
covers every older launcher that starts these files. Elsewhere it does nothing."""
import os

STD = (('stdin', -10), ('stdout', -11), ('stderr', -12))


def _kernel32():
    import ctypes as c
    from ctypes import wintypes as w
    k = c.WinDLL('kernel32', use_last_error=True)
    k.GetStdHandle.argtypes = [w.DWORD]; k.GetStdHandle.restype = w.HANDLE
    k.SetStdHandle.argtypes = [w.DWORD, w.HANDLE]; k.SetStdHandle.restype = w.BOOL
    k.GetCurrentProcess.restype = w.HANDLE
    k.DuplicateHandle.argtypes = [w.HANDLE, w.HANDLE, w.HANDLE, c.POINTER(w.HANDLE), w.DWORD, w.BOOL, w.DWORD]
    k.DuplicateHandle.restype = w.BOOL
    k.CloseHandle.argtypes = [w.HANDLE]; k.CloseHandle.restype = w.BOOL
    k.CreateFileW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, c.c_void_p, w.DWORD, w.DWORD, w.HANDLE]
    k.CreateFileW.restype = w.HANDLE
    return k


def usable(k, handle):
    """True if subprocess could pass `handle` on: it duplicates it exactly like this (DUPLICATE_SAME_ACCESS)."""
    import ctypes as c
    from ctypes import wintypes as w
    if handle in (None, 0):
        return True                      # no handle: subprocess makes a pipe, which always works
    if handle in (-1, 2**64 - 1, 2**32 - 1):
        return False                     # INVALID_HANDLE_VALUE (would duplicate the process itself)
    me = k.GetCurrentProcess(); copy = w.HANDLE()
    if not k.DuplicateHandle(me, handle, me, c.byref(copy), 0, False, 2):
        return False
    k.CloseHandle(copy)
    return True


def repair(kernel32=None):
    """The names ('stdin', 'stdout', 'stderr') of the standard handles that were dead and now are NUL."""
    if os.name != 'nt' and kernel32 is None:
        return []
    try:
        k = kernel32 or _kernel32()
        fixed = []
        for name, number in STD:
            handle = k.GetStdHandle(number)
            if usable(k, handle):
                continue
            nul = k.CreateFileW('NUL', 0xC0000000, 3, None, 3, 0, None)   # read/write, shared, OPEN_EXISTING
            if nul in (None, 0, -1, 2**64 - 1, 2**32 - 1):
                nul = None                                              # NULL: subprocess makes a pipe
            if k.SetStdHandle(number, nul):
                fixed.append(name)
        return fixed
    except (OSError, AttributeError, ValueError):
        return []
