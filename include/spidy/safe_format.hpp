#pragma once
// HERO VR 0.0.79.1 (t2158): sprintf_s / swprintf_s that CUT a text too long for its buffer instead of the CRT's
// invalid-parameter fail-fast (0xC0000409 for the whole game; a long folder path in a console line was enough).
// A cut is written to the crash breadcrumb (export_guard::formatCut) with the caller's module offset.
// Include it AFTER every other header of the .cpp: it redefines the two names for that file only.
#ifdef _WIN32
#include <cstddef>
#include <cstdio>
#include <cwchar>
#include <intrin.h>
#include "spidy/export_guard.hpp"

namespace spidy::safe_format {
inline int finish(int wanted, size_t size, void* caller) noexcept {
    if (wanted < 0 || static_cast<size_t>(wanted) >= size) {
        export_guard::formatCut(caller, size, wanted);
        return size ? static_cast<int>(size - 1) : 0;
    }
    return wanted;
}
template <size_t N, class... A>
__declspec(noinline) int sprintfCut(char (&buffer)[N], const char* format, A... a) noexcept {
    return finish(std::snprintf(buffer, N, format, a...), N, _ReturnAddress());
}
template <class... A>
__declspec(noinline) int sprintfCut(char* buffer, size_t size, const char* format, A... a) noexcept {
    if (!buffer || !size)
        return 0;
    return finish(std::snprintf(buffer, size, format, a...), size, _ReturnAddress());
}
template <size_t N, class... A>
__declspec(noinline) int swprintfCut(wchar_t (&buffer)[N], const wchar_t* format, A... a) noexcept {
    const int r = std::swprintf(buffer, N, format, a...);
    buffer[N - 1] = 0;
    return finish(r, N, _ReturnAddress());
}
} // namespace spidy::safe_format
#define sprintf_s ::spidy::safe_format::sprintfCut
#define swprintf_s ::spidy::safe_format::swprintfCut
#endif
