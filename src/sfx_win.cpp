// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Version 0.0.5 SOUND EFFECTS (sfx.hpp): the player. Its own thread (COM multithreaded) loads
// XAudio2 at run time (XAudio2_9.dll, Windows 10 and 11; else XAudio2_8.dll), makes a mastering
// voice on the default device and a few source voices per sound, then waits for requests. The VR
// thread only adds to sfx::requests() and signals; nothing here ever blocks or throws into it.
// Any failure leaves the player silent with its state and code for the log; a fault inside the
// audio calls (SEH) ends the player, never the game.
#include "spidy/sfx.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <xaudio2.h>
#include <atomic>
#include <thread>

namespace spidy::sfx {
namespace {
using CreateFn = HRESULT(WINAPI*)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR);

std::atomic<uint32_t> state{notStarted}, failCode{};
std::atomic<uint64_t> asked{}, played{}, dropped{}, muted{};
std::atomic<float> gainNow{1.f};
std::atomic<bool> started{};
HANDLE wake{};

struct Player {
    IXAudio2* engine{};
    IXAudio2MasteringVoice* master{};
    IXAudio2SourceVoice* voices[sounds][mostVoices]{}; // voicesFor(id) of each made
    float gainSet = -1.f;
    uint64_t droppedHere{};
};

void fail(uint32_t s, uint32_t code) {
    failCode.store(code, std::memory_order_relaxed);
    state.store(s, std::memory_order_relaxed);
}

// One request: an idle voice of that sound (none: dropped).
void playOne(Player& p, Id id) {
    const Pcm& pcm = sound(id);
    for (auto* v : p.voices[id]) {
        if (!v)
            continue;
        XAUDIO2_VOICE_STATE st{};
        v->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (st.BuffersQueued)
            continue;
        XAUDIO2_BUFFER b{};
        b.Flags = XAUDIO2_END_OF_STREAM;
        b.AudioBytes = pcm.bytes;
        b.pAudioData = pcm.data;
        if (FAILED(v->SubmitSourceBuffer(&b))) {
            ++p.droppedHere;
            return;
        }
        v->Start(0);
        played.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    ++p.droppedHere;
}

// The audio calls; SEH-guarded by the caller. Returns false to stop the player.
bool setUp(Player& p, HMODULE dll) {
    const auto create = reinterpret_cast<CreateFn>(reinterpret_cast<void*>(GetProcAddress(dll, "XAudio2Create")));
    if (!create) {
        fail(noXAudio2, GetLastError());
        return false;
    }
    HRESULT hr = create(&p.engine, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr) || !p.engine) {
        fail(engineFailed, static_cast<uint32_t>(hr));
        return false;
    }
    hr = p.engine->CreateMasteringVoice(&p.master);
    if (FAILED(hr) || !p.master) {
        fail(masterFailed, static_cast<uint32_t>(hr));
        return false;
    }
    for (uint32_t id = 0; id < sounds; ++id) {
        const Pcm& pcm = sound(static_cast<Id>(id));
        if (!pcm.ok) {
            fail(badSound, id);
            return false;
        }
        WAVEFORMATEX wf{};
        wf.wFormatTag = WAVE_FORMAT_PCM;
        wf.nChannels = static_cast<WORD>(pcm.channels);
        wf.nSamplesPerSec = pcm.rate;
        wf.wBitsPerSample = static_cast<WORD>(pcm.bits);
        wf.nBlockAlign = static_cast<WORD>(pcm.channels * pcm.bits / 8);
        wf.nAvgBytesPerSec = pcm.rate * wf.nBlockAlign;
        for (uint32_t k = 0; k < voicesFor(id); ++k) {
            auto& v = p.voices[id][k];
            hr = p.engine->CreateSourceVoice(&v, &wf);
            if (FAILED(hr) || !v) {
                v = nullptr;
                fail(voiceFailed, static_cast<uint32_t>(hr));
                return false;
            }
        }
    }
    state.store(playing, std::memory_order_relaxed);
    return true;
}
bool serve(Player& p) {
    const float g = gainNow.load(std::memory_order_relaxed);
    if (g != p.gainSet) {
        p.master->SetVolume(g);
        p.gainSet = g;
    }
    for (uint32_t id = 0; id < sounds; ++id) {
        const uint32_t n = requests().take(static_cast<Id>(id), p.droppedHere);
        for (uint32_t k = 0; k < n; ++k)
            playOne(p, static_cast<Id>(id));
    }
    if (p.droppedHere) {
        dropped.fetch_add(p.droppedHere, std::memory_order_relaxed);
        p.droppedHere = 0;
    }
    return true;
}
bool sehSetUp(Player& p, HMODULE dll) {
    __try {
        return setUp(p, dll);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool sehServe(Player& p) {
    __try {
        return serve(p);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void run() {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) {
        fail(comFailed, static_cast<uint32_t>(com));
        return;
    }
    HMODULE dll = LoadLibraryExW(L"XAudio2_9.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!dll)
        dll = LoadLibraryExW(L"XAudio2_8.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!dll) {
        fail(noXAudio2, GetLastError());
        return;
    }
    // The engine and voices are never released: they live to the game's exit (a release
    // racing the process ending is the one way this could hurt it).
    static Player p;
    if (!sehSetUp(p, dll)) {
        if (state.load() == starting)
            fail(engineFailed, 0xC0000005u); // a fault inside XAudio2's set-up
        return;
    }
    for (;;) {
        WaitForSingleObject(wake, 100);
        if (!sehServe(p)) {
            fail(voiceFailed, 0xC0000005u);
            return;
        }
    }
}
} // namespace

bool start() {
    bool expected = false;
    if (!started.compare_exchange_strong(expected, true))
        return state.load() == playing || state.load() == starting;
    state.store(starting);
    wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!wake) {
        fail(threadFailed, GetLastError());
        return false;
    }
    try {
        std::thread(run).detach();
    } catch (...) {
        fail(threadFailed, 0);
        return false;
    }
    return true;
}
void play(Id id) {
    if (id >= sounds)
        return;
    asked.fetch_add(1, std::memory_order_relaxed);
    if (gainNow.load(std::memory_order_relaxed) <= 0.f || state.load(std::memory_order_relaxed) != playing) {
        muted.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    requests().play(id);
    if (wake)
        SetEvent(wake);
}
void setGain(float gain) {
    gainNow.store(gain >= 0.f && gain <= 1.f ? gain : (gain > 1.f ? 1.f : 0.f), std::memory_order_relaxed);
}
Stats stats() {
    Stats s;
    s.state = state.load();
    s.code = failCode.load();
    s.asked = asked.load();
    s.played = played.load();
    s.dropped = dropped.load();
    s.muted = muted.load();
    s.gain = gainNow.load();
    return s;
}
} // namespace spidy::sfx
