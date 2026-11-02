// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Version 0.0.5 SOUND EFFECTS (sfx.hpp): the player. Its own thread (COM multithreaded) loads
// XAudio2 at run time (XAudio2_9.dll, Windows 10 and 11; else XAudio2_8.dll), makes a mastering
// voice on the default device and a few source voices per sound, then waits for requests. The VR
// thread only adds to sfx::requests() and signals; nothing here ever blocks or throws into it.
// Any failure leaves the player silent with its state and code for the log; a fault inside the
// audio calls (SEH) ends the player, never the game.
#include "spidy/sfx.hpp"
#include "spidy/sfx_diag.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <objbase.h>
#include <xaudio2.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <thread>

// 0.0.76 SFX DIAG (sfx_diag.hpp; read-only, by the launcher's tools/sfx_diag_record.py).
extern "C" __declspec(dllexport) spidy::sfx_diag::Data SpidySfxDiagData;
spidy::sfx_diag::Data SpidySfxDiagData;

namespace spidy::sfx {
namespace {
using CreateFn = HRESULT(WINAPI*)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR);

std::atomic<uint32_t> state{notStarted}, failCode{};
std::atomic<uint64_t> asked{}, played{}, dropped{}, muted{};
std::atomic<float> gainNow{1.f};
std::atomic<bool> started{};
HANDLE wake{};
// 0.0.76 SFX DIAG: vr_sfx_layer (0 on, the default; 1 off: the mod's XAudio2 engine stopped, nothing of ours is
// mixed or sent to the device; the game's audio untouched), and the callbacks' counters (atomics only: XAudio2's
// own thread runs the callbacks).
std::atomic<bool> layerWanted{true};
std::atomic<uint32_t> completedNow{}, voiceErrorsNow{}, criticalNow{}, criticalHr{}, passesNow{}, gapSumNow{},
    gapMaxNow{}, latePassUs{};
std::atomic<int64_t> lastPass{}, latePassAt{};
int64_t qpcFrequency() {
    static const int64_t f = [] {
        LARGE_INTEGER q{};
        QueryPerformanceFrequency(&q);
        return q.QuadPart ? q.QuadPart : 1;
    }();
    return f;
}
int64_t qpcTicks() {
    LARGE_INTEGER q{};
    QueryPerformanceCounter(&q);
    return q.QuadPart;
}
uint64_t ticksUs(int64_t t) { return static_cast<uint64_t>(static_cast<double>(t) * 1e6 / static_cast<double>(qpcFrequency())); }
void keepMax(std::atomic<uint32_t>& a, uint32_t v) {
    uint32_t was = a.load(std::memory_order_relaxed);
    while (v > was && !a.compare_exchange_weak(was, v, std::memory_order_relaxed)) {
    }
}
struct VoiceCounts final : IXAudio2VoiceCallback {
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnStreamEnd() override {}
    void STDMETHODCALLTYPE OnBufferStart(void*) override {}
    void STDMETHODCALLTYPE OnBufferEnd(void*) override { completedNow.fetch_add(1, std::memory_order_relaxed); }
    void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
    void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT) override { voiceErrorsNow.fetch_add(1, std::memory_order_relaxed); }
} voiceCounts;
struct EngineCounts final : IXAudio2EngineCallback {
    void STDMETHODCALLTYPE OnProcessingPassStart() override {
        const int64_t now = qpcTicks();
        const int64_t was = lastPass.exchange(now, std::memory_order_relaxed);
        passesNow.fetch_add(1, std::memory_order_relaxed);
        if (!was || now <= was)
            return;
        const uint64_t gap = ticksUs(now - was);
        const uint32_t us = static_cast<uint32_t>(std::min<uint64_t>(gap, 0xFFFFFFFFu));
        gapSumNow.fetch_add(us, std::memory_order_relaxed);
        keepMax(gapMaxNow, us);
        if (us >= spidy::sfx_diag::lateMs * 1000) {
            latePassUs.store(us, std::memory_order_relaxed);
            latePassAt.store(now, std::memory_order_relaxed);
        }
    }
    void STDMETHODCALLTYPE OnProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnCriticalError(HRESULT hr) override {
        criticalHr.store(static_cast<uint32_t>(hr), std::memory_order_relaxed);
        criticalNow.fetch_add(1, std::memory_order_relaxed);
    }
} engineCounts;

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
    // 0.0.76 SFX DIAG
    bool layerOn = true;
    uint32_t glitchesSeen{};
    uint64_t windowFrom{};
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
        ++SpidySfxDiagData.current.submitted; // 0.0.76 SFX DIAG
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
    ++SpidySfxDiagData.current.submitted; // 0.0.76 SFX DIAG
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
        SpidySfxDiagData.current.submitted += withStart ? 2 : 1; // 0.0.76 SFX DIAG
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

// 0.0.76 SFX DIAG: the default render device's shared-mode mix format and name (what the headset's audio stream
// takes in), the mastering voice's and our sounds' formats, once; a console line through the debug log.
void deviceMix(spidy::sfx_diag::Format& f) {
    IMMDeviceEnumerator* list{};
    IMMDevice* device{};
    IAudioClient* client{};
    IPropertyStore* props{};
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&list));
    if (SUCCEEDED(hr))
        hr = list->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (SUCCEEDED(hr))
        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client));
    WAVEFORMATEX* mix{};
    if (SUCCEEDED(hr))
        hr = client->GetMixFormat(&mix);
    if (SUCCEEDED(hr) && mix) {
        f.mixRate = mix->nSamplesPerSec;
        f.mixChannels = mix->nChannels;
        f.mixBits = mix->wBitsPerSample;
        CoTaskMemFree(mix);
    }
    f.mixHr = SUCCEEDED(hr) ? 0u : static_cast<uint32_t>(hr);
    // PKEY_Device_FriendlyName (functiondiscoverykeys_devpkey.h), spelled out: no INITGUID needed.
    static const PROPERTYKEY friendlyName{{0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};
    if (device && SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
        PROPVARIANT v;
        PropVariantInit(&v);
        if (SUCCEEDED(props->GetValue(friendlyName, &v)) && v.vt == VT_LPWSTR && v.pwszVal)
            WideCharToMultiByte(CP_UTF8, 0, v.pwszVal, -1, f.device, sizeof f.device - 1, nullptr, nullptr);
        PropVariantClear(&v);
        props->Release();
    }
    if (client)
        client->Release();
    if (device)
        device->Release();
    if (list)
        list->Release();
}
void noteFormat(Player& p) {
    auto& f = SpidySfxDiagData.format;
    XAUDIO2_VOICE_DETAILS d{};
    p.master->GetVoiceDetails(&d);
    f.masterRate = d.InputSampleRate;
    f.masterChannels = d.InputChannels;
    DWORD mask{};
    if (SUCCEEDED(p.master->GetChannelMask(&mask)))
        f.masterMask = mask;
    f.sourceRateMin = f.sourceChannelsMin = 0xFFFFFFFFu;
    for (uint32_t id = 0; id < sounds; ++id) {
        const Pcm& pcm = sound(static_cast<Id>(id));
        f.sourceRateMin = std::min(f.sourceRateMin, pcm.rate);
        f.sourceRateMax = std::max(f.sourceRateMax, pcm.rate);
        f.sourceChannelsMin = std::min(f.sourceChannelsMin, pcm.channels);
        f.sourceChannelsMax = std::max(f.sourceChannelsMax, pcm.channels);
        f.resampled += pcm.rate != d.InputSampleRate;
        for (uint32_t k = 0; k < voicesFor(id); ++k)
            f.sourceVoices += p.voices[id][k] != nullptr;
    }
    f.sourceVoices += p.engineVoice != nullptr;
    f.sourceBits = sound(static_cast<Id>(0)).bits;
    deviceMix(f);
    f.made = 1;
    char line[320];
    std::snprintf(line, sizeof line,
                  "SpidyVR: 0.0.76 SFX DIAG format: our %u source voices %u-%u Hz %u-%u ch %u-bit (%u sounds "
                  "resampled), mastering voice %u Hz %u ch (mask 0x%X), device \"%s\" mix %u Hz %u ch %u-bit "
                  "(query 0x%08X), XAudio2_%u\n",
                  f.sourceVoices, f.sourceRateMin, f.sourceRateMax, f.sourceChannelsMin, f.sourceChannelsMax,
                  f.sourceBits, f.resampled, f.masterRate, f.masterChannels, f.masterMask, f.device, f.mixRate,
                  f.mixChannels, f.mixBits, f.mixHr, f.xaudio);
    OutputDebugStringA(line);
}
// 0.0.76 SFX DIAG: each serve, the queue depth and XAudio2's glitches, critical errors and late passes; every 10 s
// the window closed (the callbacks' counters taken).
void serveDiag(Player& p, uint64_t now) {
    auto& d = SpidySfxDiagData;
    auto& w = d.current;
    const uint64_t at = ticksUs(qpcTicks());
    uint32_t queued{};
    for (uint32_t id = 0; id < sounds; ++id)
        for (auto* v : p.voices[id])
            if (v) {
                XAUDIO2_VOICE_STATE st{};
                v->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
                queued += st.BuffersQueued;
            }
    if (p.engineVoice) {
        XAUDIO2_VOICE_STATE st{};
        p.engineVoice->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        queued += st.BuffersQueued;
    }
    w.queuedLast = queued;
    w.queuedMax = std::max(w.queuedMax, queued);
    XAUDIO2_PERFORMANCE_DATA perf{};
    p.engine->GetPerformanceData(&perf);
    if (perf.GlitchesSinceEngineStarted < p.glitchesSeen) // the engine was started again
        p.glitchesSeen = 0;
    if (perf.GlitchesSinceEngineStarted > p.glitchesSeen) {
        w.glitches += perf.GlitchesSinceEngineStarted - p.glitchesSeen;
        p.glitchesSeen = perf.GlitchesSinceEngineStarted;
        sfx_diag::note(d, at, sfx_diag::glitch, p.glitchesSeen);
    }
    w.activeVoices = perf.ActiveSourceVoiceCount;
    if (const uint32_t c = criticalNow.exchange(0, std::memory_order_relaxed)) {
        w.criticalErrors += c;
        sfx_diag::note(d, at, sfx_diag::criticalError, criticalHr.load(std::memory_order_relaxed));
    }
    if (const int64_t late = latePassAt.exchange(0, std::memory_order_relaxed))
        sfx_diag::note(d, ticksUs(late), sfx_diag::latePass, latePassUs.load(std::memory_order_relaxed));
    if (!p.windowFrom)
        p.windowFrom = now;
    if (now - p.windowFrom < sfx_diag::windowMs)
        return;
    w.ms = static_cast<uint32_t>(now - p.windowFrom);
    p.windowFrom = now;
    w.completed = completedNow.exchange(0, std::memory_order_relaxed);
    w.voiceErrors = voiceErrorsNow.exchange(0, std::memory_order_relaxed);
    w.passes = passesNow.exchange(0, std::memory_order_relaxed);
    w.passGapUsSum = gapSumNow.exchange(0, std::memory_order_relaxed);
    w.passGapUsMax = gapMaxNow.exchange(0, std::memory_order_relaxed);
    w.layer = p.layerOn;
    sfx_diag::close(d);
}
// vr_sfx_layer: off, every loop of ours stopped and the engine stopped (nothing of ours reaches the device); on,
// started again (the loops come back on their next request).
void serveLayer(Player& p) {
    const bool want = layerWanted.load(std::memory_order_relaxed);
    if (want == p.layerOn)
        return;
    const uint64_t at = ticksUs(qpcTicks());
    if (!want) {
        for (uint32_t id = 0; id < sounds; ++id)
            if (p.looping[id] && !engineLoop(id))
                loopOne(p, static_cast<Id>(id), false);
        if (p.engineVoice && p.looping[vanEngine]) {
            p.engineVoice->Stop(0);
            p.engineVoice->FlushSourceBuffers();
            p.looping[vanEngine] = nullptr;
            p.fadeFrom = 0;
            p.volumeSet = p.pitchSet = -1.f;
        }
        p.engine->StopEngine();
        lastPass.store(0, std::memory_order_relaxed);
        OutputDebugStringA("SpidyVR: 0.0.76 SFX DIAG: the mod's sound layer OFF (vr_sfx_layer 1; XAudio2 engine stopped)\n");
    } else {
        const HRESULT hr = p.engine->StartEngine();
        char line[128];
        std::snprintf(line, sizeof line, "SpidyVR: 0.0.76 SFX DIAG: the mod's sound layer ON (vr_sfx_layer 0; start 0x%08X)\n",
                      static_cast<uint32_t>(hr));
        OutputDebugStringA(line);
    }
    p.layerOn = want;
    SpidySfxDiagData.layer = want;
    sfx_diag::note(SpidySfxDiagData, at, want ? sfx_diag::layerOn : sfx_diag::layerOff, 0);
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
    p.engine->RegisterForCallbacks(&engineCounts); // 0.0.76 SFX DIAG (counts only)
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
            hr = p.engine->CreateSourceVoice(&v, &wf, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &voiceCounts);
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
        if (FAILED(p.engine->CreateSourceVoice(&p.engineVoice, &wf, 0, engineFullPitch + .1f, &voiceCounts)))
            p.engineVoice = nullptr;
    }
    noteFormat(p);
    state.store(playing, std::memory_order_relaxed);
    return true;
}
bool serve(Player& p) {
    serveLayer(p);
    serveDiag(p, nowMs());
    if (!p.layerOn)
        return true; // 0.0.76 SFX DIAG: vr_sfx_layer off, nothing of ours plays
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
    SpidySfxDiagData.format.xaudio = dll ? 9 : 8;
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
        const DWORD waitMs = p.looping[vanEngine] ? 20 : 100;
        const int64_t before = qpcTicks();
        if (WaitForSingleObject(wake, waitMs) == WAIT_TIMEOUT) { // 0.0.76 SFX DIAG: how late the wake-up came
            const uint64_t took = ticksUs(qpcTicks() - before), due = waitMs * 1000ull;
            const uint32_t late = static_cast<uint32_t>(std::min<uint64_t>(took > due ? took - due : 0, 0xFFFFFFFFu));
            auto& w = SpidySfxDiagData.current;
            ++w.wakes;
            w.wakeLateUsSum += late;
            w.wakeLateUsMax = std::max(w.wakeLateUsMax, late);
            if (late >= sfx_diag::lateMs * 1000)
                sfx_diag::note(SpidySfxDiagData, ticksUs(qpcTicks()), sfx_diag::lateWake, late);
        }
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
    if (gainNow.load(std::memory_order_relaxed) <= 0.f || state.load(std::memory_order_relaxed) != playing ||
        !layerWanted.load(std::memory_order_relaxed)) {
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
void setLayer(bool on) { layerWanted.store(on, std::memory_order_relaxed); } // 0.0.76 SFX DIAG: vr_sfx_layer
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
