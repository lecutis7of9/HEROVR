#pragma once
// HERO VR 0.0.79.1 (t2155): the launcher calls spidy_stereo_probe.dll's exports through SpidyGuardedCall, so a
// C++ exception never leaves a remote thread (an uncaught one is std::terminate -> abort -> 0xC0000409 for the whole game), and a
// crash breadcrumb in reports\xr-breadcrumb.txt, a FILE-BACKED mapping (Windows writes its pages to disk even
// when the game is killed by a fail-fast), says the last export entered, the VR worker's frame phase and any
// CRT/terminate/exception note. Plain memcpy per mark; never throws.
#ifdef _WIN32
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <intrin.h>

namespace spidy::export_guard {
inline constexpr DWORD exportThrew = 0xE0790002u; // an export's body threw; caught, nothing changed after it
inline constexpr size_t lineBytes = 160, lines = 6, fileBytes = lineBytes * lines;
enum Line : size_t { header = 0, exportLine = 1, phaseLine = 2, noteLine = 3, crtLine = 4, workerLine = 5 };

inline void anchor() noexcept {}

inline char* view() noexcept {
    static char* const mapped = []() noexcept -> char* {
        HMODULE self{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCWSTR>(&anchor), &self))
            return nullptr;
        wchar_t path[1024]{};
        const DWORD n = GetModuleFileNameW(self, path, 1000);
        if (!n || n >= 1000)
            return nullptr;
        // ...\reports\stereo-modules\<sha256>\spidy_stereo_probe.dll -> ...\reports\xr-breadcrumb.txt;
        // anywhere else: beside the DLL.
        wchar_t file[1100]{};
        wchar_t* const dllSlash = wcsrchr(path, L'\\');
        if (!dllSlash)
            return nullptr;
        *dllSlash = 0; // path: the DLL's folder
        bool staged = false;
        if (wchar_t* const shaSlash = wcsrchr(path, L'\\')) {
            *shaSlash = 0;
            if (wchar_t* const kindSlash = wcsrchr(path, L'\\')) {
                if (_wcsicmp(kindSlash + 1, L"stereo-modules") == 0) {
                    *kindSlash = 0; // path: ...\reports
                    staged = true;
                } 
            }
            if (!staged)
                *shaSlash = L'\\';
        }
        swprintf_s(file, L"%s\\xr-breadcrumb.txt", path);
        const HANDLE f = CreateFileW(file, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                     nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE)
            return nullptr;
        const HANDLE m = CreateFileMappingW(f, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(fileBytes), nullptr);
        CloseHandle(f);
        if (!m)
            return nullptr;
        auto* v = static_cast<char*>(MapViewOfFile(m, FILE_MAP_WRITE, 0, 0, fileBytes));
        CloseHandle(m);
        if (!v)
            return nullptr;
        std::memset(v, ' ', fileBytes);
        for (size_t i = 0; i < lines; ++i)
            v[i * lineBytes + lineBytes - 1] = '\n';
        char head[lineBytes];
        const int k = std::snprintf(head, sizeof head, "HERO VR xr-breadcrumb pid %lu (the last of each line below is what ran last)",
                                    GetCurrentProcessId());
        if (k > 0)
            std::memcpy(v, head, static_cast<size_t>(k) < lineBytes - 1 ? static_cast<size_t>(k) : lineBytes - 1);
        return v;
    }();
    return mapped;
}

inline void put(Line line, const char* text) noexcept {
    char* v = view();
    if (!v || !text)
        return;
    char buf[lineBytes];
    std::memset(buf, ' ', sizeof buf);
    const size_t n = strnlen(text, lineBytes - 1);
    std::memcpy(buf, text, n);
    buf[lineBytes - 1] = '\n';
    std::memcpy(v + line * lineBytes, buf, lineBytes);
}

inline void mark(Line line, const char* what, const char* name, unsigned long long n = 0) noexcept {
    char text[lineBytes];
    std::snprintf(text, sizeof text, "%s %s #%llu tick %llu tid %lu", what, name ? name : "?", n,
                  static_cast<unsigned long long>(GetTickCount64()), GetCurrentThreadId());
    put(line, text);
}

inline void crtHandlers() noexcept;

inline void enter(const char* name) noexcept {
    static std::atomic<unsigned long long> calls{0};
    crtHandlers();
    mark(exportLine, "export entered:", name, ++calls);
}

inline void left(const char* name, DWORD r) noexcept {
    char text[lineBytes];
    std::snprintf(text, sizeof text, "export returned: %s -> %lu (0x%08lX) tick %llu", name ? name : "?", r, r,
                  static_cast<unsigned long long>(GetTickCount64()));
    put(noteLine, text);
}

inline DWORD failed(const char* name) noexcept {
    mark(noteLine, "export THREW (caught, returned 0xE0790002):", name);
    return exportThrew;
}

inline void phase(const char* name) noexcept {
    crtHandlers();
    static std::atomic<unsigned long long> frames{0};
    mark(phaseLine, "VR worker phase:", name, ++frames);
}

inline void worker(const char* text) noexcept { mark(workerLine, "VR worker:", text); }

// This DLL's own CRT (/MT): terminate, invalid parameter (STL hardening, *_s overflow) and pure calls in OUR code
// note themselves first, then end the game as before.
inline void crtHandlers() noexcept {
    // set_terminate is per thread in the UCRT: each thread that enters an export or marks a phase sets it.
    thread_local bool terminateSet = false;
    if (!terminateSet) {
        terminateSet = true;
        std::set_terminate([] {
            put(crtLine, "CRT: std::terminate in spidy_stereo_probe.dll (an uncaught C++ exception or noexcept "
                         "violation) - the game ends with 0xC0000409");
            std::abort();
        });
    }
    static const bool once = []() noexcept {
        _set_invalid_parameter_handler([](const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t) {
            put(crtLine, "CRT: invalid parameter in spidy_stereo_probe.dll (STL bounds check, *_s overflow, bad "
                         "handle) - the game ends with 0xC0000409");
            __fastfail(FAST_FAIL_INVALID_ARG);
        });
        _set_purecall_handler([] {
            put(crtLine, "CRT: pure virtual call in spidy_stereo_probe.dll");
            std::abort();
        });
        return true;
    }();
    (void)once;
}
} // namespace spidy::export_guard

#endif
