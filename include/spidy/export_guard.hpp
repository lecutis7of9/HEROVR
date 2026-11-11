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
inline constexpr size_t lineBytes = 160, lines = 16, fileBytes = lineBytes * lines;
// t2158: crtLine says what the C runtime caught; stackLine.. (8 lines) its return addresses as module+offset
// (resolve with the CI artifact's spidy_stereo_probe.map / .pdb); formatLine: the last sprintf_s cut short.
enum Line : size_t {
    header = 0, exportLine = 1, phaseLine = 2, noteLine = 3, crtLine = 4, workerLine = 5, formatLine = 6,
    stackLine = 7, stackLines = 8, countLine = 15
};

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

// t2158: up to 24 return addresses of the calling thread, as module+0xOFFSET, onto the stack lines.
inline void stack(void* first = nullptr) noexcept {
    void* frames[40]{};
    const USHORT n = CaptureStackBackTrace(1, 40, frames, nullptr);
    char text[lineBytes];
    size_t used = 0;
    size_t line = stackLine;
    auto flush = [&] {
        if (line < stackLine + stackLines)
            put(static_cast<Line>(line++), text);
        used = 0;
        text[0] = 0;
    };
    text[0] = 0;
    used = static_cast<size_t>(std::snprintf(text, sizeof text, "stack (tid %lu):", GetCurrentThreadId()));
    auto add = [&](void* at) {
        HMODULE module{};
        char name[MAX_PATH]{};
        uintptr_t base = 0;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               static_cast<LPCSTR>(at), &module) &&
            GetModuleFileNameA(module, name, MAX_PATH)) {
            base = reinterpret_cast<uintptr_t>(module);
        } else {
            std::snprintf(name, sizeof name, "?");
        }
        const char* leaf = std::strrchr(name, '\\');
        leaf = leaf ? leaf + 1 : name;
        char one[96];
        const int k = std::snprintf(one, sizeof one, " %.40s+0x%llX", leaf,
                                    static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(at) - base));
        if (k <= 0)
            return;
        if (used + static_cast<size_t>(k) >= lineBytes - 1)
            flush();
        std::memcpy(text + used, one, static_cast<size_t>(k) + 1);
        used += static_cast<size_t>(k);
    };
    if (first)
        add(first);
    for (USHORT i = 0; i < n; ++i)
        add(frames[i]);
    if (used)
        flush();
}

// This DLL's own CRT (/MT): terminate, invalid parameter (*_s overflow, a bad handle or stream) and pure calls in
// OUR code (and what is linked into it) note themselves, with the thread's stack, first. t2158: the invalid
// parameter handler only records (it never ends the game itself): the CRT function then returns its error as
// usual, and only the CRT's own no-return checks still end the game - after this note.
inline void crtHandlers() noexcept {
    // set_terminate is per thread in the UCRT: each thread that enters an export or marks a phase sets it.
    thread_local bool terminateSet = false;
    if (!terminateSet) {
        terminateSet = true;
        std::set_terminate([] {
            mark(crtLine, "CRT: std::terminate (uncaught C++ exception / noexcept) - game ends 0xC0000409;", "in our DLL");
            stack(_ReturnAddress());
            std::abort();
        });
    }
    static const bool once = []() noexcept {
        _set_invalid_parameter_handler([](const wchar_t* expression, const wchar_t* function, const wchar_t* file,
                                          unsigned line, uintptr_t) {
            static std::atomic<unsigned long long> count{0};
            const unsigned long long n = ++count;
            char text[lineBytes];
            std::snprintf(text, sizeof text, "CRT: invalid parameter #%llu tick %llu tid %lu: %ls %ls %ls:%u", n,
                          static_cast<unsigned long long>(GetTickCount64()), GetCurrentThreadId(),
                          expression ? expression : L"(release: no expression)", function ? function : L"",
                          file ? file : L"", line);
            put(crtLine, text);
            stack(_ReturnAddress());
            std::snprintf(text, sizeof text, "CRT invalid parameters this session: %llu (the last one is above)", n);
            put(countLine, text);
        });
        _set_purecall_handler([] {
            mark(crtLine, "CRT: pure virtual call", "in our DLL");
            stack(_ReturnAddress());
            std::abort();
        });
        return true;
    }();
    (void)once;
}

// t2158: a sprintf_s / swprintf_s whose text did not fit (safe_format.hpp cuts it instead of ending the game).
inline void formatCut(void* caller, size_t size, int wanted) noexcept {
    HMODULE module{};
    uintptr_t base = 0;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCSTR>(caller), &module))
        base = reinterpret_cast<uintptr_t>(module);
    char text[lineBytes];
    std::snprintf(text, sizeof text, "sprintf_s cut short (would have ended the game): +0x%llX buffer %zu wanted %d tick %llu tid %lu",
                  static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(caller) - base), size, wanted,
                  static_cast<unsigned long long>(GetTickCount64()), GetCurrentThreadId());
    put(formatLine, text);
}
} // namespace spidy::export_guard

#endif
