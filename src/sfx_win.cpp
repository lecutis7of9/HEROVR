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
    IXAudio2SourceVoice* looping[sounds]{}; // Version 0.0.72 SOUND LOOPS: the voice repeating each loop
    uint64_t droppedHere{};
    // Version 0.0.73 PUNISHER VAN ENGINE: its voice (made for van_engine's format, which van_start shares),
    // fading out since fadeFrom (ms; 0: not fading), and the pitch and volume last set.
    IXAudio2SourceVoice* engineVoice{};
    uint64_t fadeFrom{};
    float pitchSet = -1.f, volumeSet = -1.f;
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
        v->SetVolume(id == vanDoorSlam ? slamGain : 1.f); // 0.0.73: the van's recording is quiet (kept as it is)
        v->Start(0);
        played.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    ++p.droppedHere;
}

// Version 0.0.72 SOUND LOOPS: start one voice repeating the sound (an idle one; none idle: the first is
// cut short for it), or stop it - its volume eased to 0 for a few ms first, so it ends without a click.
void loopOne(Player& p, Id id, bool on) {
    if (!on) {
        if (auto* v = p.looping[id]) {
            v->SetVolume(0.f);
            Sleep(12);
            v->Stop(0);
            v->FlushSourceBuffers();
            v->SetVolume(1.f);
            p.looping[id] = nullptr;
        }
        return;
    }
    if (p.looping[id])
        return;
    const Pcm& pcm = sound(id);
    IXAudio2SourceVoice* pick{};
    for (auto* v : p.voices[id]) {
        if (!v)
            continue;
        XAUDIO2_VOICE_STATE st{};
        v->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (!st.BuffersQueued) {
            pick = v;
            break;
        }
    }
    if (!pick && (pick = p.voices[id][0]) != nullptr) {
        pick->Stop(0);
        pick->FlushSourceBuffers();
    }
    if (!pick)
        return;
    XAUDIO2_BUFFER b{};
    b.Flags = XAUDIO2_END_OF_STREAM;
    b.AudioBytes = pcm.bytes;
    b.pAudioData = pcm.data;
    b.LoopBegin = 0;
    b.LoopLength = 0; // the whole buffer
    b.LoopCount = XAUDIO2_LOOP_INFINITE;
    if (FAILED(pick->SubmitSourceBuffer(&b))) {
        ++p.droppedHere;
        return;
    }
    pick->Start(0);
    p.looping[id] = pick;
    played.fetch_add(1, std::memory_order_relaxed);
}

// Version 0.0.73 PUNISHER VAN ENGINE: on, the start-up then the loop queued on one voice (they join at the
// recording's own sample A); off, faded over engineFadeMs (never blocking: served every 20 ms while it runs),
// then stopped. Running, the throttle pitches and swells the loop - never the start-up, played as recorded.
bool sameFormat(const Pcm& a, const Pcm& b) { return a.ok && b.ok && a.rate == b.rate && a.channels == b.channels && a.bits == b.bits; }
void engineOne(Player& p, bool on, uint64_t now) {
    if (!p.engineVoice)
        return;
    if (on) {
        if (p.fadeFrom) { // asked again while fading: back up to full, the loop goes on
            p.fadeFrom = 0;
            p.volumeSet = -1.f;
        }
        if (p.looping[vanEngine])
            return;
        const Pcm &start = sound(vanStart), &run = sound(vanEngine);
        XAUDIO2_BUFFER a{};
        a.AudioBytes = start.bytes;
        a.pAudioData = start.data;
        XAUDIO2_BUFFER b{};
        b.Flags = XAUDIO2_END_OF_STREAM;
        b.AudioBytes = run.bytes;
        b.pAudioData = run.data;
        b.LoopBegin = 0;
        b.LoopLength = 0;
        b.LoopCount = XAUDIO2_LOOP_INFINITE;
        p.engineVoice->Stop(0);
        p.engineVoice->FlushSourceBuffers();
        p.engineVoice->SetFrequencyRatio(1.f);
        p.engineVoice->SetVolume(engineIdleGain);
        p.pitchSet = 1.f;
        p.volumeSet = engineIdleGain;
        const bool withStart = sameFormat(start, run);
        if ((withStart && FAILED(p.engineVoice->SubmitSourceBuffer(&a))) || FAILED(p.engineVoice->SubmitSourceBuffer(&b))) {
            p.engineVoice->FlushSourceBuffers();
            ++p.droppedHere;
            return;
        }
        p.engineVoice->Start(0);
        p.looping[vanEngine] = p.engineVoice;
        played.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    if (!p.looping[vanEngine])
        return;
    if (!p.fadeFrom)
        p.fadeFrom = now;
}
// Each serve while the engine runs: its fade, or its throttle.
void engineServe(Player& p, uint64_t now) {
    if (!p.engineVoice || !p.looping[vanEngine])
        return;
    XAUDIO2_VOICE_STATE st{};
    p.engineVoice->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    const bool inStartUp = st.BuffersQueued >= 2; // van_start not yet done
    const float rpm = inStartUp ? 0.f : requests().engineRpm();
    float volume = engineGain(rpm);
    if (p.fadeFrom) {
        const uint64_t gone = now > p.fadeFrom ? now - p.fadeFrom : 0;
        if (gone >= engineFadeMs) {
            p.engineVoice->SetVolume(0.f);
            p.engineVoice->Stop(0);
            p.engineVoice->FlushSourceBuffers();
            p.looping[vanEngine] = nullptr;
            p.fadeFrom = 0;
            p.volumeSet = p.pitchSet = -1.f;
            return;
        }
        volume *= 1.f - static_cast<float>(gone) / static_cast<float>(engineFadeMs);
    }
    const float pitch = inStartUp ? 1.f : enginePitch(rpm);
    if (pitch != p.pitchSet) {
        p.engineVoice->SetFrequencyRatio(pitch);
        p.pitchSet = pitch;
    }
    if (volume != p.volumeSet) {
        p.engineVoice->SetVolume(volume);
        p.volumeSet = volume;
    }
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
    { // Version 0.0.73 PUNISHER VAN ENGINE: its own voice (pitch up to engineFullPitch; a failure: no engine
      // sound, everything else plays)
        const Pcm& pcm = sound(vanEngine);
        WAVEFORMATEX wf{};
        wf.wFormatTag = WAVE_FORMAT_PCM;
        wf.nChannels = static_cast<WORD>(pcm.channels);
        wf.nSamplesPerSec = pcm.rate;
        wf.wBitsPerSample = static_cast<WORD>(pcm.bits);
        wf.nBlockAlign = static_cast<WORD>(pcm.channels * pcm.bits / 8);
        wf.nAvgBytesPerSec = pcm.rate * wf.nBlockAlign;
        if (FAILED(p.engine->CreateSourceVoice(&p.engineVoice, &wf, 0, engineFullPitch + .1f)))
            p.engineVoice = nullptr;
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
    const uint64_t now = nowMs();
    { // Version 0.0.73 PUNISHER VAN ENGINE
        const bool live = g > 0.f && loopLive(requests().loopAt(vanEngine), now);
        const bool running = p.looping[vanEngine] != nullptr && !p.fadeFrom;
        if (live != running)
            engineOne(p, live, now);
        engineServe(p, now);
    }
    for (uint32_t id = 0; id < sounds; ++id)
        if (loopable(id) && !engineLoop(id)) {
            const bool live = g > 0.f && loopLive(requests().loopAt(static_cast<Id>(id)), now);
            if (live != (p.looping[id] != nullptr))
                loopOne(p, static_cast<Id>(id), live);
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
        WaitForSingleObject(wake, p.looping[vanEngine] ? 20 : 100); // 0.0.73: the engine's throttle and fade
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
uint64_t nowMs() { return GetTickCount64() + 1; }
void loop(Id id, bool on) {
    if (id >= sounds || !loopable(id))
        return;
    const uint64_t was = requests().loop(id, nowMs(), on);
    if ((was != 0) != on && wake) // started or stopped: the player acts now (refreshes need no wake)
        SetEvent(wake);
}
void engineRpm(float rpm) { requests().engineRpm(rpm); } // Version 0.0.73 PUNISHER VAN ENGINE (read by the player)
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
