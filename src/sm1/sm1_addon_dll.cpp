// HERO VR 0.0.76 SM1 (hotfix 0.0.75.2): herovr_sm1_addon.dll - Mulder's Heroes add-on for iilyn's Spider-Man VR mod (Spidy by
// Ilya Mezerowsky, GPL-3.0, https://github.com/IlyaMez/SpidyVR). The HERO VR launcher loads it into Spider-Man.exe
// after iilyn's VR module (spidy_stereo_probe.dll) is in. See include/spidy/sm1_addon.hpp.
//
// This DLL is the add-on's REPORT only: no hook, no game memory, no D3D or OpenXR call. One thread checks the host
// (exe, version, iilyn's modules, read only), reads %APPDATA%\HERO VR SM1\addon.ini, notes whether the add-on's
// OpenXR layer (herovr_sm1_layer.dll, which does the wrist menu, heroes and flight) is in the game, and writes it all
// to %APPDATA%\HERO VR SM1\addon.log, then ends. It never touches %APPDATA%\Spidy or his files.
#include "spidy/sm1_addon.hpp"

#include <windows.h>

#include <cstdio>
#include <string>

namespace {
std::wstring addonFolder() {
    wchar_t buf[MAX_PATH]{};
    const DWORD n = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return {};
    std::wstring dir = std::wstring(buf) + L"\\HERO VR SM1";
    CreateDirectoryW(dir.c_str(), nullptr); // fine when it exists
    return dir;
}

std::string narrow(const std::wstring& w) {
    if (w.empty())
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0)
        return {};
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::string fileVersion(const std::wstring& path) {
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (size == 0)
        return {};
    std::string data(size, '\0');
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data()))
        return {};
    VS_FIXEDFILEINFO* info = nullptr;
    UINT len = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &len) || !info || len < sizeof(VS_FIXEDFILEINFO))
        return {};
    char text[64]{};
    std::snprintf(text, sizeof text, "%lu.%lu.%lu.%lu", static_cast<unsigned long>(HIWORD(info->dwFileVersionMS)),
                  static_cast<unsigned long>(LOWORD(info->dwFileVersionMS)), static_cast<unsigned long>(HIWORD(info->dwFileVersionLS)),
                  static_cast<unsigned long>(LOWORD(info->dwFileVersionLS)));
    return text;
}

std::string readSmallFile(const std::wstring& path) {
    std::string out;
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f)
        return out;
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0 && out.size() < 65536)
        out.append(buf, n);
    std::fclose(f);
    return out;
}

void writeLog(const std::wstring& dir, const std::string& text) {
    if (dir.empty())
        return;
    FILE* f = _wfopen((dir + L"\\addon.log").c_str(), L"wb");
    if (!f)
        return;
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
}

std::wstring widen(const char* s) {
    std::wstring w;
    for (; s && *s; ++s)
        w += static_cast<wchar_t>(static_cast<unsigned char>(*s));
    return w;
}

DWORD WINAPI addonThread(void*) {
    using namespace spidy::sm1;
    std::string log;
    log += std::string(addonName) + " " + addonVersion + " (HERO VR) for iilyn's Spider-Man VR mod\n";
    log += "iilyn's mod: Spidy by Ilya Mezerowsky, GPL-3.0, https://github.com/IlyaMez/SpidyVR (unchanged)\n";
    HostInfo host;
    wchar_t exe[MAX_PATH]{};
    const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring exePath = (n > 0 && n < MAX_PATH) ? std::wstring(exe, n) : std::wstring();
    const size_t slash = exePath.find_last_of(L"\\/");
    host.exeName = narrow(slash == std::wstring::npos ? exePath : exePath.substr(slash + 1));
    host.version = exePath.empty() ? std::string() : fileVersion(exePath);
    for (int i = 0; i < spidyModuleCount; ++i)
        host.spidy[i] = GetModuleHandleW(widen(spidyModules[i]).c_str()) != nullptr;
    host.vr = GetModuleHandleW(widen(spidyVrModule).c_str()) != nullptr;
    const Verdict verdict = judge(host);
    log += "game: " + host.exeName + " " + (host.version.empty() ? std::string("(version unreadable)") : host.version) +
           " (expected " + hostExe + " " + hostVersion + ")\n";
    for (int i = 0; i < spidyModuleCount; ++i)
        log += std::string("  ") + spidyModules[i] + (host.spidy[i] ? ": loaded\n" : ": not loaded\n");
    log += std::string("  ") + spidyVrModule + (host.vr ? ": loaded\n" : ": not loaded\n");
    log += std::string("check: ") + verdictText(verdict) + "\n";
    const std::wstring dir = addonFolder();
    const Config config = parseConfig(readSmallFile(dir + L"\\addon.ini"));
    log += std::string("settings: heroes=") + (config.heroes ? "on" : "off") + " wrist_menu=" + (config.wristMenu ? "on" : "off") +
           " flight=" + (config.flight ? "on" : "off") + " hero=" + heroName(config.hero) + "\n";
    log += "features:\n";
    for (const Feature& f : features)
        log += std::string("  ") + f.name + ": " + evidenceText(f.evidence) + ", " + stateText(f.state) + " - " + f.note + "\n";
    // The layer is loaded by the OpenXR loader when his mod creates its OpenXR instance; give it a moment.
    bool layer = false;
    for (int i = 0; i < 120 && !layer; ++i) {
        layer = GetModuleHandleW(widen(layerDll).c_str()) != nullptr;
        if (!layer)
            Sleep(500);
    }
    log += std::string("layer ") + layerDll + (layer ? ": loaded (wrist menu, heroes and flight run there; see layer.log)\n"
                                                     : ": NOT loaded after 60 s (no wrist menu, heroes or flight this run; "
                                                       "the launcher registers it while Spider-Man 1 plays)\n");
    log += "this DLL installs no hook and touches no game memory; this thread ends now\n";
    writeLog(dir, log);
    return 0;
}
} // namespace

extern "C" __declspec(dllexport) DWORD WINAPI HeroVrSm1AddonVersion(void*) {
    return 76; // 0.0.76 (hotfix 0.0.75.2)
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        // Never work under the loader lock: one short thread does it all, and the add-on is never unloaded.
        HANDLE thread = CreateThread(nullptr, 0, addonThread, nullptr, 0, nullptr);
        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}
