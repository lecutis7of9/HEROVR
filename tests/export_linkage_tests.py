# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.74 test 2b: every export the launcher looks up by name is unmangled (C linkage).

Test 2b ended VR with exit code 2 "Missing export: SpidyTaaHoldData": the record was defined
`__declspec(dllexport)` outside extern "C", so the DLL exported a C++-mangled name and GetProcAddress
found nothing. A dllexport definition has C linkage when it carries extern "C" itself, sits inside an
extern "C" { } block, or its translation unit (the .cpp or a header it includes) declared the name
extern "C" before it. This test parses the sources and fails on any dllexport without that.
"""
import ast
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings  # noqa: E402
import update37_records  # noqa: E402

_TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|R"(?P<d>[^(\s]*)\(.*?\)(?P=d)"|"(?:\\.|[^"\\\n])*"'
                    r"|'(?:\\.|[^'\\\n])*'", re.S)
_DLLEXPORT = re.compile(r'__declspec\s*\(\s*dllexport\s*\)')


def strip(text):
    """Comments and literals blanked (newlines kept, so line numbers hold); the "C" of extern "C" is kept."""
    return _TOKEN.sub(lambda m: m.group(0) if m.group(0) == '"C"' else re.sub(r'[^\n]', ' ', m.group(0)), text)


def c_blocks(code):
    """(open, close) offsets of every extern "C" { } block, by brace depth."""
    starts = {m.end() - 1 for m in re.finditer(r'extern\s+"C"\s*\{', code)}
    stack, blocks = [], []
    for i, ch in enumerate(code):
        if ch == '{':
            stack.append((i, i in starts))
        elif ch == '}' and stack:
            start, is_c = stack.pop()
            if is_c:
                blocks.append((start, i))
    return blocks


def c_declared(code):
    """Spidy names declared extern "C" in `code` (the one-line form or inside a block)."""
    names = {m.group(1) for m in re.finditer(r'extern\s+"C"\s+[^;{]*?\b(Spidy\w+)\s*[;(\[{=]', code)}
    for a, b in c_blocks(code):
        names.update(re.findall(r'\b(Spidy\w+)\b', code[a:b]))
    return names


def scan(root=ROOT, sources=None):
    """[(file, line, name, c_linkage)] for every dllexport definition in root/src/*.cpp (or `sources`)."""
    found, headers = [], {}
    for cpp in sources or sorted((root / 'src').glob('*.cpp')):
        raw = cpp.read_text(encoding='utf-8', errors='replace')
        code = strip(raw)
        blocks = c_blocks(code)
        from_headers = set()
        for inc in re.findall(r'#\s*include\s+"([^"]+)"', raw):
            header = next((h for h in (root / 'include' / inc, cpp.parent / inc) if h.exists()), None)
            if header:
                if header not in headers:
                    headers[header] = c_declared(strip(header.read_text(encoding='utf-8', errors='replace')))
                from_headers |= headers[header]
        for m in _DLLEXPORT.finditer(code):
            named = re.match(r'[^;{(=\[]*?\b(Spidy\w+)\s*[;{(=\[]', code[m.end():])
            if not named:
                continue
            name = named.group(1)
            lead = max(code.rfind(';', 0, m.start()), code.rfind('{', 0, m.start()), code.rfind('}', 0, m.start())) + 1
            is_c = (bool(re.search(r'extern\s+"C"', code[lead:m.start()]))
                    or any(a < m.start() < b for a, b in blocks)
                    or name in c_declared(code[:lead]) or name in from_headers)
            found.append((cpp.relative_to(root).as_posix() if root in cpp.parents else cpp.name,
                          code.count('\n', 0, m.start()) + 1, name, is_c))
    return found


def required_exports():
    """Every name tools/run_game_vr.py passes to prepare(): a missing one ends VR with exit code 2."""
    tree = ast.parse((ROOT / 'tools/run_game_vr.py').read_text(encoding='utf-8'))
    scope = {'live_settings': live_settings, 'update37_records': update37_records}
    names, calls = set(), 0
    for node in ast.walk(tree):
        if not (isinstance(node, ast.Call) and getattr(node.func, 'id', None) == 'prepare'):
            continue
        arg = node.args[4] if len(node.args) >= 5 else next((k.value for k in node.keywords if k.arg == 'names'), None)
        if arg is None:
            continue
        calls += 1
        for el in arg.elts:
            if isinstance(el, ast.Constant):
                names.add(el.value)
            elif isinstance(el, ast.Starred):
                names.update(eval(compile(ast.Expression(el.value), 'run_game_vr.py', 'eval'), scope))
            else:
                raise AssertionError(f'prepare() export list has an entry this test cannot read: {ast.dump(el)}')
    return names, calls


class ExportLinkage(unittest.TestCase):
    def setUp(self):
        self.defs = {}
        for f, line, name, is_c in scan():
            self.defs.setdefault(name, []).append((f, line, is_c))

    def test_required_exports_are_unmangled(self):
        names, calls = required_exports()
        self.assertEqual(calls, 4)  # bridge, rays, motion, stereo probe
        self.assertIn('SpidyTaaHoldData', names)
        self.assertGreater(len(names), 300)
        mangled = [f'{f}:{line} {n}' for n in sorted(names) for f, line, is_c in self.defs.get(n, ()) if not is_c]
        self.assertEqual(mangled, [], 'dllexport without extern "C" (mangled, so "Missing export" at START VR)')
        self.assertEqual(sorted(n for n in names if n not in self.defs), [],
                         'run_game_vr.py needs exports no src/*.cpp defines with dllexport')

    def test_every_dllexport_is_unmangled(self):
        mangled = [f'{f}:{line} {n}' for n, where in sorted(self.defs.items()) for f, line, is_c in where if not is_c]
        self.assertEqual(mangled, [])

    def test_taa_hold_record_in_extern_c_block(self):
        self.assertEqual([is_c for *_, is_c in self.defs['SpidyTaaHoldData']], [True])

    def test_scanner_catches_the_test2b_bug(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            src = pathlib.Path(d) / 'src'
            src.mkdir()
            (src / 'a.cpp').write_text(
                '// extern "C" {\n'
                'namespace spidy { struct Data { int x; }; }\n'
                '__declspec(dllexport) spidy::Data SpidyBadData;\n'
                'extern "C" {\n__declspec(dllexport) spidy::Data SpidyGoodData;\n'
                'namespace { int f() { return 1; } }\n}\n'
                'extern "C" __declspec(dllexport) unsigned long __stdcall SpidyGoodCall(void*) { return 0; }\n'
                'extern "C" spidy::Data SpidyLaterData;\n'
                '__declspec(dllexport) spidy::Data SpidyLaterData{};\n'
                'namespace n { __declspec(dllexport) unsigned long __stdcall SpidyBadCall(void*) { return 0; } }\n'
                'const char* s = "extern \\"C\\" {";\n__declspec(dllexport) int SpidyBadAfterString;\n',
                encoding='utf-8')
            got = {name: is_c for _, _, name, is_c in scan(pathlib.Path(d))}
        self.assertEqual(got, {'SpidyBadData': False, 'SpidyGoodData': True, 'SpidyGoodCall': True,
                               'SpidyLaterData': True, 'SpidyBadCall': False, 'SpidyBadAfterString': False})


if __name__ == '__main__':
    unittest.main()
