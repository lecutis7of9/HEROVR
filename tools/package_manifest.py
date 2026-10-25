# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""What the shareable package (tools/package.ps1) must carry for a VR session, worked out from the code.

    python package_manifest.py list [--tools DIR] [--entry run_game_vr ...]
        JSON: the tools (modules in DIR) the entry scripts import, directly or not, the other modules they import
        (the standard library: the embeddable Python must have them), and the build\\windows-ninja binaries
        those tools and tools\\launch-game-vr.ps1 name.
    python package_manifest.py check [--tools DIR] [--entry ...]
        Run with the packaged Python on the packaged tools folder: every tool must be there, every other import
        must resolve, and importing the entry scripts must work. Prints "ok" or what is missing; exit 1 then.

Imports are read with ast, so ones inside functions and try blocks count too. An import guarded by
`except ImportError` (or ModuleNotFoundError) is optional: "check" reports it but does not fail.
Standard library only: it runs in the embeddable Python as it comes.
"""
import argparse
import ast
import importlib
import importlib.util
import json
import pathlib
import re
import sys

ENTRIES = ('run_game_vr', 'xr_runtime')   # what launch-game-vr.ps1 and Spidy Launcher.exe run
BINARY = re.compile(r"\b(spidy_[a-z0-9_]+\.(?:dll|exe)|bhaptics_library\.dll)\b", re.I)


def _optional_lines(tree):
    """Line spans of try bodies whose handlers catch ImportError/ModuleNotFoundError (or everything)."""
    spans = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Try):
            for handler in node.handlers:
                names = []
                if handler.type is None:
                    names = ['*']
                elif isinstance(handler.type, ast.Name):
                    names = [handler.type.id]
                elif isinstance(handler.type, ast.Tuple):
                    names = [e.id for e in handler.type.elts if isinstance(e, ast.Name)]
                if {'*', 'ImportError', 'ModuleNotFoundError', 'Exception'} & set(names):
                    spans.append((node.body[0].lineno, node.body[-1].end_lineno))
                    break
    return spans


def imports_of(path):
    """(module, optional) for each absolute import in one file."""
    tree = ast.parse(pathlib.Path(path).read_text(encoding='utf-8'), filename=str(path))
    spans = _optional_lines(tree)
    found = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            names = [a.name for a in node.names]
        elif isinstance(node, ast.ImportFrom) and node.level == 0 and node.module:
            names = [node.module]
        else:
            continue
        optional = any(a <= node.lineno <= b for a, b in spans)
        found += [(name, optional) for name in names]
    return found


def manifest(tools, entries=ENTRIES):
    tools = pathlib.Path(tools)
    local = {p.stem for p in tools.glob('*.py')}
    seen, todo, other, missing = [], list(entries), {}, []
    while todo:
        name = todo.pop(0)
        if name in seen or name in missing:
            continue
        source = tools/f'{name}.py'
        if not source.is_file():
            missing.append(name)
            continue
        seen.append(name)
        for module, optional in imports_of(source):
            top = module.split('.')[0]
            if top in local:
                todo.append(top)
            elif top in entries and top not in local:
                missing.append(top)
            else:
                other[module] = other.get(module, True) and optional
    binaries = set()
    for name in seen:
        binaries |= {b.lower() for b in BINARY.findall((tools/f'{name}.py').read_text(encoding='utf-8'))}
    launcher = tools/'launch-game-vr.ps1'
    if launcher.is_file():
        binaries |= {b.lower() for b in BINARY.findall(launcher.read_text(encoding='utf-8'))}
    return {'tools': sorted(seen), 'missing_tools': sorted(set(missing)),
            'stdlib': sorted(m for m, opt in other.items() if not opt),
            'optional': sorted(m for m, opt in other.items() if opt),
            'binaries': sorted(binaries)}


def check(tools, entries=ENTRIES, import_entries=True):
    """Problems (empty when the folder is complete) and notes about optional imports."""
    found = manifest(tools, entries)
    problems = [f'missing tool: {t}.py (imported, not in {tools})' for t in found['missing_tools']]
    notes = []
    for module in found['stdlib'] + found['optional']:
        try:
            spec = importlib.util.find_spec(module)
        except (ImportError, ValueError):
            spec = None
        if spec is None:
            if module in found['optional']:
                notes.append(f'optional import not available: {module}')
            else:
                problems.append(f'missing module: {module} (this Python cannot import it)')
    if import_entries and not problems:
        sys.path.insert(0, str(pathlib.Path(tools).resolve()))
        for entry in entries:
            try:
                importlib.import_module(entry)
            except Exception as error:  # what the player would see at the start of a session
                problems.append(f'import {entry} failed: {type(error).__name__}: {error}')
    return problems, notes


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('mode', choices=('list', 'check'))
    p.add_argument('--tools', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parent)
    p.add_argument('--entry', action='append', help='entry script names (default: run_game_vr, xr_runtime)')
    p.add_argument('--no-import', action='store_true', help='check: resolve imports only, do not run the entries')
    a = p.parse_args(argv)
    entries = tuple(a.entry) if a.entry else ENTRIES
    if a.mode == 'list':
        print(json.dumps(manifest(a.tools, entries), indent=1))
        return 0 if not manifest(a.tools, entries)['missing_tools'] else 1
    problems, notes = check(a.tools, entries, not a.no_import)
    for line in notes:
        print('note: ' + line)
    for line in problems:
        print('MISSING: ' + line)
    if not problems:
        print('ok')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
