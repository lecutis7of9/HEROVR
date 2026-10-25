"""Update 24.1: a shader mistake must never take the player out of VR.

Update 24 named an HLSL variable `line`, a word HLSL keeps for geometry-shader
inputs. D3DCompile refused the overlay shader at the first drawing in gameplay
("Compile vertex shader HRESULT=2147500037") and the session ended in flat
screen. These checks keep such words out of every built-in shader and keep the
overlay's failures from ending VR.
"""
import os
import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SHADER_FILES = sorted((ROOT/'src').glob('*.cpp'))
# Words HLSL (FXC and DXC) reserves or keeps as modifiers: none may name anything.
RESERVED = {
    'line', 'lineadj', 'point', 'triangle', 'triangleadj', 'sample', 'precise', 'centroid', 'linear',
    'nointerpolation', 'noperspective', 'snorm', 'unorm', 'shared', 'groupshared', 'uniform', 'volatile',
    'packoffset', 'register', 'compile', 'pass', 'technique', 'technique10', 'technique11', 'string',
    'texture', 'sampler', 'vector', 'matrix', 'in', 'out', 'inout', 'export', 'extern', 'static', 'const',
    'row_major', 'column_major', 'struct', 'cbuffer', 'tbuffer', 'return', 'discard', 'break', 'continue',
    'while', 'for', 'do', 'if', 'else', 'switch', 'case', 'default', 'true', 'false', 'void', 'inline',
    'interface', 'class', 'namespace', 'typedef', 'template', 'this', 'auto', 'catch', 'char', 'const_cast',
    'delete', 'dynamic_cast', 'enum', 'explicit', 'friend', 'goto', 'long', 'mutable', 'new', 'operator',
    'private', 'protected', 'public', 'reinterpret_cast', 'short', 'signed', 'sizeof', 'static_cast',
    'throw', 'try', 'typename', 'union', 'unsigned', 'using', 'virtual', 'asm', 'half', 'fixed',
}
TYPE = (r'(?:float|half|double|int|uint|bool|dword|min16float|min16int|min16uint)'
        r'(?:[1-4](?:x[1-4])?)?|Texture2D|Texture2DArray|SamplerState|RWTexture2D|Buffer|StructuredBuffer')
DECLARED = re.compile(r'\b(?:' + TYPE + r')(?:\s*<[^>]*>)?\s+(\w+)')


def shaders():
    """Every HLSL source built into the port: raw strings that hold an SV_ semantic."""
    for path in SHADER_FILES:
        text = path.read_text(encoding='utf-8')
        for match in re.finditer(r'R"\((.*?)\)"', text, re.S):
            if 'SV_' in match.group(1):
                yield path.name, text.count('\n', 0, match.start()) + 1, match.group(1)


def strip_comments(source):
    return re.sub(r'//[^\n]*', '', re.sub(r'/\*.*?\*/', '', source, flags=re.S))


class ShaderSafetyTests(unittest.TestCase):
    def test_shaders_are_found(self):
        names = {name for name, _, _ in shaders()}
        self.assertIn('d3d12_renderer.cpp', names)
        self.assertIn('native_eye_gpu.cpp', names)

    def test_no_shader_names_anything_with_an_hlsl_reserved_word(self):
        for name, line, source in shaders():
            for declared in DECLARED.finditer(strip_comments(source)):
                with self.subTest(file=name, shader_line=line, name=declared.group(1)):
                    self.assertFalse(declared.group(1) in RESERVED,
                                     f'{name}:{line}: "{declared.group(1)}" is an HLSL reserved word')

    def test_the_update_24_mistake_is_caught(self):
        source = 'float4 ps(float4 c:COLOR):SV_TARGET { const float line = c.x; return line; }'
        self.assertIn('line', {m.group(1) for m in DECLARED.finditer(source)} & RESERVED)

    def test_overlay_drawing_failure_keeps_vr(self):
        game = (ROOT/'src/game_xr.cpp').read_text(encoding='utf-8')
        self.assertRegex(game, r'try \{\s*overlay\.renderViews\(')
        self.assertIn('overlayFault = true', game)
        self.assertIn('VR continues', game)
        renderer = (ROOT/'src/d3d12_renderer.cpp').read_text(encoding='utf-8')
        self.assertIn('plainOverlayShader', renderer)
        self.assertIn('compilerText(error.Get())', renderer)

    def test_a_flat_fallback_names_its_reason_on_the_console(self):
        import ast
        source = (ROOT/'tools/run_game_vr.py').read_text(encoding='utf-8')
        tree = ast.parse(source)
        found = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'flat_fallback_line')
        space = {}
        exec(compile(ast.Module([found], []), 'run_game_vr.py', 'exec'), space)
        line = space['flat_fallback_line']('Compile vertex shader HRESULT=2147500037')
        self.assertIn('flat screen', line)
        self.assertTrue(line.endswith('Reason: Compile vertex shader HRESULT=2147500037'))
        self.assertIn('no reason given', space['flat_fallback_line'](''))
        self.assertIn("if sample['status'] == 5", source)

    @unittest.skipUnless(shutil.which('dxc') or os.path.exists('/tmp/dxc/bin/dxc'), 'DXC is not installed')
    def test_every_shader_compiles_with_dxc(self):
        dxc = shutil.which('dxc') or '/tmp/dxc/bin/dxc'
        env = dict(os.environ)
        if dxc.startswith('/tmp/dxc'):
            env['LD_LIBRARY_PATH'] = '/tmp/dxc/lib'
        with tempfile.TemporaryDirectory() as folder:
            for index, (name, line, source) in enumerate(shaders()):
                for entry, profile in (('vs', 'vs_6_0'), ('ps', 'ps_6_0'), ('cs', 'cs_6_0')):
                    if not re.search(r'\b' + entry + r'\s*\(', source):
                        continue
                    path = pathlib.Path(folder)/f'{index}.hlsl'
                    path.write_text(source, encoding='utf-8')
                    with self.subTest(file=name, shader_line=line, entry=entry):
                        run = subprocess.run([dxc, '-HV', '2018', '-T', profile, '-E', entry, str(path),
                                              '-Fo', str(path.with_suffix('.bin'))],
                                             capture_output=True, text=True, env=env, timeout=120)
                        self.assertEqual(run.returncode, 0, f'{name}:{line} {entry}: {run.stderr[-1500:]}')


if __name__ == '__main__':
    unittest.main()
