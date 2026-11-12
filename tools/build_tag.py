"""79.5 BUILD TAG: which build a gameplay DLL is, read from the DLL file itself.

79.4's session ran the 79.3 DLL (the report's xr_hash was ee300e76..., not 938a84c8...) and nothing in the folder said so:
every test package has the same artifact name, ZIP name and folder name, and the reports' version is the same
("0.0.79", "Update 39.12") for all of them. The DLL carries the string SpidyBuildTag ("HEROVR-BUILD-TAG <version>
<test number> <commit>", game_xr.cpp, from CMake's HERO_TEST_VERSION / HERO_TEST_NUMBER and the git commit), so what
runs can be named from the file about to be injected, with no help from any other file in the folder.
"""
import hashlib
import pathlib
import re

PATTERN = re.compile(rb'HEROVR-BUILD-TAG ([0-9][0-9.]*) ([0-9]+) ([0-9A-Za-z._-]{1,40})')


def dll_tag(path):
    """('0.0.79.5', '5', 'abc1234') from a DLL file, or None (an older build, or no such file)."""
    try:
        data = pathlib.Path(path).read_bytes()
    except OSError:
        return None
    found = PATTERN.search(data)
    return tuple(part.decode('ascii') for part in found.groups()) if found else None


def tag_line(path):
    """The line the launcher prints and the report keeps: what the DLL at `path` is."""
    path = pathlib.Path(path)
    tag = dll_tag(path)
    try:
        digest = hashlib.sha256(path.read_bytes()).hexdigest()[:12]
    except OSError:
        digest = 'unreadable'
    if tag is None:
        return (f'BUILD: this gameplay DLL carries NO build tag (sha256 {digest}...): it is an older build than 79.5 '
                f'- if you meant to run a newer test, this folder was not updated (unzip the new ZIP into a NEW empty folder)')
    return f'BUILD: HERO VR {tag[0]} TEST {tag[1]} commit {tag[2]} (gameplay DLL sha256 {digest}...)'


def record(path):
    tag = dll_tag(path)
    return dict(version=tag[0], test=tag[1], commit=tag[2]) if tag else None
