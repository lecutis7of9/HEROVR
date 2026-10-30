// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
// 0.0.74 test 3 CAVE QUIET (cave_quiet074.hpp): the game's default audio session ducked in the hideouts.
#include "spidy/cave_quiet074.hpp"

#include <windows.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <objbase.h>
#include <mutex>

namespace spidy {
namespace {
std::mutex duckMutex;
cave_quiet::Duck duckState;

// The default render device's session for this process (GUID_NULL: the one the game's streams join).
ISimpleAudioVolume* openSession() {
    IMMDeviceEnumerator* devices{};
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(&devices))) || !devices)
        return nullptr;
    IMMDevice* device{};
    IAudioSessionManager* manager{};
    ISimpleAudioVolume* volume{};
    if (SUCCEEDED(devices->GetDefaultAudioEndpoint(eRender, eConsole, &device)) && device &&
        SUCCEEDED(device->Activate(__uuidof(IAudioSessionManager), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&manager))) &&
        manager)
        if (FAILED(manager->GetSimpleAudioVolume(nullptr, FALSE, &volume)))
            volume = nullptr;
    if (manager)
        manager->Release();
    if (device)
        device->Release();
    devices->Release();
    return volume;
}
template <class F> bool withSession(F&& f) {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool done = false;
    if (ISimpleAudioVolume* v = openSession()) {
        done = f(v);
        v->Release();
    }
    if (SUCCEEDED(co))
        CoUninitialize();
    return done;
}
} // namespace

bool gameDuck(bool quiet) {
    std::lock_guard lock(duckMutex);
    if (quiet == duckState.ducked)
        return true;
    return withSession([&](ISimpleAudioVolume* v) {
        float now = 1.f;
        if (FAILED(v->GetMasterVolume(&now)))
            now = 1.f;
        const float set = duckState.step(quiet, now);
        if (set >= 0)
            v->SetMasterVolume(set, nullptr);
        return true;
    });
}

bool gameDuckRecover() {
    std::lock_guard lock(duckMutex);
    if (duckState.ducked)
        return false;
    return withSession([](ISimpleAudioVolume* v) {
        float now = 1.f;
        if (SUCCEEDED(v->GetMasterVolume(&now)) && cave_quiet::stale(now)) {
            v->SetMasterVolume(1.f, nullptr);
            return true;
        }
        return false;
    });
}
} // namespace spidy
