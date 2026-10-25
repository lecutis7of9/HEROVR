"""Update 27.1: MSVC C2712 — __try must not share a function with C++ unwinding.

MSVC /EHsc refuses `__try` in any function that also needs object unwinding
(std::string, vector, lock_guard, temporaries from sigAtRva(), …). The ADDRESS
FINDER path hit C2712 at onBarrier. Helpers must take/return only POD / raw
pointers; callers build C++ objects outside SEH.

This heuristic scans function bodies. It must fail on the pre-27.1 pattern.
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

# Types / helpers that imply non-trivial destruction in the same function.
NON_TRIVIAL = re.compile(
    r'\b(?:std::(?:string|wstring|vector|list|map|set|unordered_map|unordered_set|'
    r'unique_ptr|shared_ptr|optional|lock_guard|unique_lock|scoped_lock|function|'
    r'thread|array|deque|queue|stack|mutex|recursive_mutex)|sigAtRva)\b'
)

# Update 27 surface plus every translation unit that uses SEH near that work.
SCAN_FILES = [
    ROOT / 'src' / 'native_address_finder.cpp',
    ROOT / 'src' / 'native_eye_gpu.cpp',
    ROOT / 'src' / 'stereo_probe.cpp',
    ROOT / 'src' / 'game_xr.cpp',
    ROOT / 'src' / 'wrist_hud.cpp',
    ROOT / 'src' / 'native_shadow_distance.cpp',  # Update 33
]

# Pre-27.1 onBarrier shape (std::string temporary via sigAtRva + __try).
OLD_ON_BARRIER = r'''
void onBarrier(ID3D12GraphicsCommandList* list, UINT count, const D3D12_RESOURCE_BARRIER* bs) {
    if (!armed.load() || failed.load())
        return;
    maybeInstallGpuHooks(list);
    tickFrame();
    if (!bs || !count)
        return;
    for (UINT i = 0; i < count; ++i) {
        if (bs[i].Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION || !bs[i].Transition.pResource)
            continue;
        D3D12_RESOURCE_DESC desc{};
        __try {
            desc = bs[i].Transition.pResource->GetDesc();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            continue;
        }
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
            continue;
        const UINT w = static_cast<UINT>(desc.Width), h = desc.Height;
        if (w < 64 || h < 64)
            continue;
        if (desc.Format == DXGI_FORMAT_R16G16_FLOAT || desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
            const uint32_t rva = returnRva();
            char detail[128];
            std::snprintf(detail, sizeof detail, "tex %ux%u fmt=%u", w, h, static_cast<unsigned>(desc.Format));
            addCandidate(desc.Format == DXGI_FORMAT_R16G16_FLOAT ? "mvec_target" : "history_color", detail, rva,
                         functionRvaFor(rva), sigAtRva(functionRvaFor(rva)).c_str());
        }
    }
}
'''


def strip_comments(text: str) -> str:
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    text = re.sub(r'//.*?$', '', text, flags=re.M)
    return text


def functions(text: str):
    """Yield (name, body) for definitions whose body is a `{ ... }` block."""
    text = strip_comments(text)
    i, n = 0, len(text)
    while i < n:
        m = re.search(
            r'([A-Za-z_][\w:]*)\s*\([^;{}]*\)\s*(?:const\s*)?(?:override\s*)?\{',
            text[i:],
            re.S,
        )
        if not m:
            break
        name = m.group(1)
        if name in ('if', 'for', 'while', 'switch', 'catch', 'else'):
            i += m.start() + 1
            continue
        body_start = i + m.end() - 1
        depth = 0
        j = body_start
        while j < n:
            c = text[j]
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    yield name, text[body_start : j + 1]
                    i = j + 1
                    break
            j += 1
        else:
            break


def violations(text: str):
    """Return [(function_name, matched_tokens), ...] for C2712-risk bodies."""
    out = []
    for name, body in functions(text):
        if '__try' not in body:
            continue
        hits = NON_TRIVIAL.findall(body)
        if hits:
            out.append((name, hits))
    return out


class SehSafety(unittest.TestCase):
    def test_heuristic_fails_on_old_on_barrier(self):
        bad = violations(OLD_ON_BARRIER)
        self.assertTrue(bad, 'old onBarrier must be flagged')
        names = {n for n, _ in bad}
        self.assertIn('onBarrier', names)
        self.assertIn('sigAtRva', bad[0][1])

    def test_update_27_sources_are_clean(self):
        problems = []
        for path in SCAN_FILES:
            self.assertTrue(path.is_file(), f'missing {path}')
            for name, hits in violations(path.read_text(encoding='utf-8')):
                problems.append(f'{path.relative_to(ROOT)}::{name} uses {hits} with __try')
        self.assertEqual(problems, [], '\n'.join(problems))

    def test_address_finder_seh_helpers_are_pod_only(self):
        text = (ROOT / 'src' / 'native_address_finder.cpp').read_text(encoding='utf-8')
        seh = [name for name, body in functions(text) if '__try' in body]
        self.assertEqual(
            seh,
            ['safeRead', 'sehVtable', 'sehMap', 'sehUnmap', 'sehGpuVa', 'sehGetResourceDesc',
             'sehHeapInfo', 'sehDeviceOf', 'sehRelease'],  # Update 31: descriptor-table trace
        )
        # No leftover __try in callers (onBarrier / detourCopyBuffer / maybeInstallGpuHooks).
        for caller in ('onBarrier', 'detourCopyBuffer', 'maybeInstallGpuHooks', 'maybeInstallTableHooks',
                       'traceTableBind', 'detourSetHeaps', 'detourCreateCbv', 'detourCreateSrv'):
            bodies = [b for n, b in functions(text) if n == caller]
            self.assertTrue(bodies, caller)
            self.assertNotIn('__try', bodies[0], caller)


if __name__ == '__main__':
    unittest.main()
