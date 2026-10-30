// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR (Ilya Mezerowsky).
// Update 19: the MUSIC page's player (music_audio.hpp): Media Foundation's
// source reader decodes, WASAPI (shared, event driven) plays, on a thread
// of its own; another looks through the Music folder.
#include "spidy/music_audio.hpp"
#include "spidy/phone_music.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <audioclient.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mmdeviceapi.h>
#include <objbase.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>

// Update 22: both Media Foundation libraries the player calls, from System32 (kept loaded: the delay-load
// helper then finds them). False on Windows N/KN without the Media Feature Pack.
static bool mediaFoundationPresent() {
    for (const wchar_t* name : {L"mfplat.dll", L"mfreadwrite.dll"})
        if (!LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32))
            return false;
    return true;
}

namespace spidy::music {
namespace {
template <class T> struct Com {
    T* p{};
    Com() = default;
    Com(const Com&) = delete;
    Com& operator=(const Com&) = delete;
    ~Com() {
        reset();
    }
    void reset() {
        if (p)
            p->Release();
        p = nullptr;
    }
    T** put() {
        reset();
        return &p;
    }
    T* operator->() const {
        return p;
    }
    explicit operator bool() const {
        return p != nullptr;
    }
};
std::string utf8(const std::wstring& w) {
    const int size = static_cast<int>(w.size());
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), size, nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(std::max(bytes, 0)), '\0');
    if (bytes > 0)
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), size, s.data(), bytes, nullptr, nullptr);
    return s;
}
std::wstring wide(const std::string& s) {
    const int size = static_cast<int>(s.size());
    const int chars = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), size, nullptr, 0);
    std::wstring w(static_cast<size_t>(std::max(chars, 0)), L'\0');
    if (chars > 0)
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), size, w.data(), chars);
    return w;
}
enum class Kind : uint8_t { rescan, play, toggle, stop, previous, next, seekBy, seekTo, select };
struct Command {
    Kind kind{};
    float value{};
};
// KSDATAFORMAT_SUBTYPE_IEEE_FLOAT (ksmedia.h), spelled out: no INITGUID games.
constexpr GUID floatSamples{0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
constexpr REFERENCE_TIME bufferHns = 600000; // 60 ms: the picture at most that far ahead of the sound
constexpr DWORD tickMs = 12;                 // the spectrum's pace with nothing to play, too
} // namespace

struct Player::Impl {
    // Shared with the caller.
    mutable std::mutex statusLock;
    Status shown;
    TripleBuffer<Spectrum> spectra;
    std::atomic<float> gain{gainOf(defaultVolume)};
    std::array<Command, 64> ring{};
    std::atomic<uint32_t> head{}, tail{}; // the caller writes head, the player tail
    std::atomic<bool> quit{}, faulted{};
    HANDLE wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    HANDLE scanWake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    // The scanner's hand-over.
    std::mutex listLock;
    std::vector<Track> listed;
    bool listFresh{};
    std::wstring folder;
    std::thread player, scanner;

    // The player thread's own.
    std::vector<Track> list;
    int track = -1;
    std::string file; // the song open (UTF-8), so a rescan keeps it
    enum class State : uint8_t { stopped, playing, paused } state{};
    Com<IMFSourceReader> reader;
    Com<IMMDeviceEnumerator> devices;
    Com<IAudioClient> client;
    Com<IAudioRenderClient> render;
    HANDLE audioEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    UINT32 bufferFrames{};
    int rate = 48000, channels = 2, clientRate{}, clientChannels{};
    std::vector<float> pending;
    size_t pendingAt{};
    uint64_t written{}; // frames sent since seekBase
    double seekBase{}, duration{};
    bool ended{};
    std::string note;
    ULONGLONG noteUntil{}, deviceRetry{};
    Analyzer analyzer;
    ULONGLONG lastSpectrum{};
    std::atomic<uint32_t> serial{};
    bool dirty = true;

    explicit Impl(std::wstring own = {}) {
        try {
            folder = own.empty() ? phoneMusicFolder() : std::move(own); // 0.0.74: the vehicle player's own folder
        } catch (...) {
        }
        shown.folder = utf8(folder);
        try {
            if (wake && scanWake && audioEvent) {
                scanner = std::thread([this] { guarded(&Impl::scanLoop, "MUSIC OFF: THE FOLDER COULD NOT BE READ"); });
                player = std::thread([this] { guarded(&Impl::playLoop, "MUSIC OFF: THE PLAYER FAILED"); });
            } else
                fail("MUSIC OFF: NO EVENTS");
        } catch (...) {
            quit = true;
            fail("MUSIC OFF: NO THREAD");
        }
    }
    ~Impl() {
        quit = true;
        if (wake)
            SetEvent(wake);
        if (scanWake)
            SetEvent(scanWake);
        if (player.joinable())
            player.join();
        if (scanner.joinable())
            scanner.join();
        for (HANDLE h : {wake, scanWake, audioEvent})
            if (h)
                CloseHandle(h);
    }
    void fail(const char* why) {
        faulted = true;
        std::lock_guard guard(statusLock);
        shown.available = false;
        shown.playing = shown.paused = false;
        shown.note = why;
        shown.serial = ++serial;
    }
    // An access violation or a C++ exception in a decoder (or here) turns
    // the music off, never the game.
    static bool sehCall(Impl* self, void (Impl::*loop)()) {
        __try {
            (self->*loop)();
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }
    void guarded(void (Impl::*loop)(), const char* why) {
        bool ok = false;
        try {
            ok = sehCall(this, loop);
        } catch (...) {
            ok = false;
        }
        if (!ok)
            fail(why);
    }
    void send(Kind kind, float value = 0) {
        if (faulted)
            return;
        const uint32_t h = head.load(std::memory_order_relaxed);
        if (h - tail.load(std::memory_order_acquire) >= ring.size())
            return; // full: the press is dropped (the player is busy)
        ring[h % ring.size()] = {kind, value};
        head.store(h + 1, std::memory_order_release);
        SetEvent(wake);
    }

    // ---- The scanner thread.
    void scanLoop() {
        std::vector<std::pair<std::string, uint64_t>> last;
        bool first = true;
        while (!quit) {
            std::vector<std::pair<std::string, uint64_t>> files;
            if (!folder.empty()) {
                CreateDirectoryW(folder.c_str(), nullptr);
                WIN32_FIND_DATAW found{};
                const HANDLE find = FindFirstFileW((folder + L"\\*").c_str(), &found);
                if (find != INVALID_HANDLE_VALUE) {
                    do {
                        if (found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN))
                            continue;
                        const uint64_t bytes = (static_cast<uint64_t>(found.nFileSizeHigh) << 32) | found.nFileSizeLow;
                        files.emplace_back(utf8(found.cFileName), bytes);
                    } while (!quit && FindNextFileW(find, &found) && files.size() < 5000);
                    FindClose(find);
                }
            }
            std::sort(files.begin(), files.end());
            if (first || files != last) {
                auto songs = songList(files);
                {
                    std::lock_guard guard(listLock);
                    listed = std::move(songs);
                    listFresh = true;
                }
                SetEvent(wake);
                last = std::move(files);
                first = false;
            }
            WaitForSingleObject(scanWake, 3000);
        }
    }

    // ---- The player thread.
    void say(const std::string& text) {
        note = text;
        noteUntil = GetTickCount64() + 6000;
        dirty = true;
    }
    std::string nameOf(int t) const {
        return t >= 0 && t < static_cast<int>(list.size()) ? list[static_cast<size_t>(t)].name : std::string();
    }
    void closeClient() {
        if (client)
            client->Stop();
        render.reset();
        client.reset();
        clientRate = clientChannels = 0;
        bufferFrames = 0;
    }
    bool openClient() {
        if (client && clientRate == rate && clientChannels == channels)
            return true;
        closeClient();
        if (!devices && FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                                __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(devices.put()))))
            return false;
        Com<IMMDevice> device;
        if (FAILED(devices->GetDefaultAudioEndpoint(eRender, eConsole, device.put())))
            return false;
        if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(client.put()))))
            return false;
        WAVEFORMATEXTENSIBLE f{};
        f.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        f.Format.nChannels = static_cast<WORD>(channels);
        f.Format.nSamplesPerSec = static_cast<DWORD>(rate);
        f.Format.wBitsPerSample = 32;
        f.Format.nBlockAlign = static_cast<WORD>(4 * channels);
        f.Format.nAvgBytesPerSec = f.Format.nSamplesPerSec * f.Format.nBlockAlign;
        f.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        f.Samples.wValidBitsPerSample = 32;
        f.dwChannelMask = channels == 1 ? SPEAKER_FRONT_CENTER : channels == 2 ? (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT) : 0;
        f.SubFormat = floatSamples;
        // Shared: the game's own sound plays on beside it. AUTOCONVERTPCM:
        // Windows resamples the song's rate to the device's.
        const DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                            AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
        // 0.0.74 test 3 CAVE QUIET: the songs in their own session, so ducking the game's (game_duck_win.cpp) in a
        // hideout leaves the car music and the Batcomputer's MUSIC as they are.
        static const GUID musicSession = {0x6d1f0c1e, 0x74a3, 0x4c37, {0x9b, 0x2e, 0x41, 0x6d, 0x75, 0x6c, 0x64, 0x72}};
        if (FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, bufferHns, 0, &f.Format, &musicSession)) ||
            FAILED(client->SetEventHandle(audioEvent)) || FAILED(client->GetBufferSize(&bufferFrames)) ||
            FAILED(client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(render.put())))) {
            closeClient();
            return false;
        }
        clientRate = rate;
        clientChannels = channels;
        return true;
    }
    void closeSong() {
        reader.reset();
        pending.clear();
        pendingAt = 0;
        written = 0;
        seekBase = duration = 0;
        file.clear();
    }
    // Opens song t for decoding to float; false if it will not.
    bool openSong(int t) {
        closeSong();
        if (t < 0 || t >= static_cast<int>(list.size()))
            return false;
        const std::wstring path = folder + L"\\" + wide(list[static_cast<size_t>(t)].file);
        if (FAILED(MFCreateSourceReaderFromURL(path.c_str(), nullptr, reader.put())))
            return false;
        reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
        if (FAILED(reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), TRUE)))
            return closeSong(), false;
        Com<IMFMediaType> want;
        if (FAILED(MFCreateMediaType(want.put())) || FAILED(want->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
            FAILED(want->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float)) ||
            FAILED(reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, want.p)))
            return closeSong(), false;
        Com<IMFMediaType> got;
        if (FAILED(reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), got.put())))
            return closeSong(), false;
        UINT32 r = 0, c = 0;
        got->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &r);
        got->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &c);
        if (r < 8000 || r > 384000 || c < 1 || c > 8)
            return closeSong(), false;
        rate = static_cast<int>(r);
        channels = static_cast<int>(c);
        PROPVARIANT length;
        PropVariantInit(&length);
        if (SUCCEEDED(reader->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION,
                                                       &length)) &&
            length.vt == VT_UI8)
            duration = static_cast<double>(length.uhVal.QuadPart) / 1e7;
        PropVariantClear(&length);
        file = list[static_cast<size_t>(t)].file;
        track = t;
        ended = false;
        return true;
    }
    // Plays from song `from`, or the first after it (`step`-wise) that opens;
    // the ones that will not are named in the note.
    void start(int from, int step) {
        const int count = static_cast<int>(list.size());
        if (!count) {
            stopAll();
            return;
        }
        from = ((from % count) + count) % count;
        for (int k = 0; k < count && !quit; ++k) {
            const int t = ((from + step * k) % count + count) % count;
            if (openSong(t)) {
                if (k)
                    say("SKIPPED: " + nameOf(from) + (k > 1 ? " AND MORE" : ""));
                state = State::playing;
                restartOutput();
                dirty = true;
                return;
            }
        }
        say("NO SONG WOULD PLAY: TRY MP3 OR WAV");
        track = from;
        closeSong();
        state = State::stopped;
        dirty = true;
    }
    void restartOutput() {
        if (!openClient()) {
            if (!client)
                say("NO AUDIO DEVICE: WAITING FOR ONE");
            deviceRetry = GetTickCount64() + 2000;
            return;
        }
        client->Stop();
        client->Reset();
        if (state == State::playing)
            client->Start();
    }
    void stopAll() {
        if (client) {
            client->Stop();
            client->Reset();
        }
        if (reader)
            seekTo(0, false);
        state = State::stopped;
        dirty = true;
    }
    double elapsed() const {
        UINT32 padding = 0;
        if (client)
            client->GetCurrentPadding(&padding);
        const double sent = static_cast<double>(written) - std::min<double>(padding, static_cast<double>(written));
        return seekBase + sent / std::max(rate, 1);
    }
    void seekTo(double seconds, bool keepPlaying = true) {
        if (!reader)
            return;
        if (duration > 0)
            seconds = std::min(seconds, std::max(0., duration - .25));
        seconds = std::max(0., seconds);
        PROPVARIANT at;
        PropVariantInit(&at);
        at.vt = VT_I8;
        at.hVal.QuadPart = static_cast<LONGLONG>(seconds * 1e7);
        const bool ok = SUCCEEDED(reader->SetCurrentPosition(GUID_NULL, at));
        PropVariantClear(&at);
        if (!ok)
            return;
        if (client) {
            client->Stop();
            client->Reset();
        }
        pending.clear();
        pendingAt = 0;
        written = 0;
        seekBase = seconds;
        ended = false;
        if (keepPlaying && state == State::playing && client)
            client->Start();
        dirty = true;
    }
    void command(const Command& c) {
        const int count = static_cast<int>(list.size());
        switch (c.kind) {
        case Kind::rescan:
            SetEvent(scanWake);
            break;
        case Kind::play:
            start(static_cast<int>(c.value), 1);
            break;
        case Kind::select:
            if (state == State::stopped && count) {
                track = std::clamp(static_cast<int>(c.value), 0, count - 1);
                closeSong();
                dirty = true;
            }
            break;
        case Kind::toggle:
            if (state == State::playing) {
                state = State::paused;
                if (client)
                    client->Stop();
            } else if (state == State::paused && reader) {
                state = State::playing;
                if (client)
                    client->Start();
                else
                    restartOutput();
            } else if (reader) {
                state = State::playing;
                restartOutput();
            } else
                start(track < 0 ? 0 : track, 1);
            dirty = true;
            break;
        case Kind::stop:
            stopAll();
            break;
        case Kind::previous: {
            const int t = previousTrack(track, count, reader ? static_cast<float>(elapsed()) : 0.f);
            if (t < 0)
                break;
            if (t == track && reader)
                seekTo(0);
            else if (state == State::stopped) {
                track = t;
                closeSong();
                dirty = true;
            } else
                start(t, -1);
            break;
        }
        case Kind::next: {
            const int t = nextTrack(track, count);
            if (t < 0)
                break;
            if (state == State::stopped) {
                track = t;
                closeSong();
                dirty = true;
            } else
                start(t, 1);
            break;
        }
        case Kind::seekBy:
            if (std::isfinite(c.value))
                seekTo(elapsed() + c.value);
            break;
        case Kind::seekTo:
            if (std::isfinite(c.value) && duration > 0)
                seekTo(std::clamp(static_cast<double>(c.value), 0., 1.) * duration);
            break;
        }
    }
    // Takes a new song list from the scanner: the song open keeps playing
    // (found again by its file name).
    void takeList() {
        std::lock_guard guard(listLock);
        if (!listFresh)
            return;
        list = std::move(listed);
        listed.clear();
        listFresh = false;
        if (!file.empty())
            track = indexOf(list, file); // -1 if it went: it plays on, then the list's first
        else if (track >= static_cast<int>(list.size()))
            track = list.empty() ? -1 : 0;
        dirty = true;
    }
    // Sends what the device has room for; a song's end plays the next.
    void fill() {
        if (state != State::playing)
            return;
        if (!client) {
            if (GetTickCount64() >= deviceRetry) {
                const double at = seekBase;
                restartOutput();
                if (client && reader)
                    seekTo(at);
            }
            return;
        }
        UINT32 padding = 0;
        HRESULT hr = client->GetCurrentPadding(&padding);
        if (FAILED(hr)) {
            const double at = elapsed();
            closeClient();
            devices.reset();
            say("NO AUDIO DEVICE: WAITING FOR ONE");
            deviceRetry = GetTickCount64() + 2000;
            seekBase = at;
            written = 0;
            return;
        }
        UINT32 room = bufferFrames > padding ? bufferFrames - padding : 0;
        const float g = gain.load(std::memory_order_relaxed);
        int reads = 0;
        while (room > 0 && !quit) {
            const size_t have = (pending.size() - pendingAt) / static_cast<size_t>(channels);
            if (!have) {
                if (ended || !reader || ++reads > 8)
                    break;
                pending.clear();
                pendingAt = 0;
                DWORD stream = 0, flags = 0;
                LONGLONG stamp = 0;
                Com<IMFSample> sample;
                hr = reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, &stream, &flags,
                                        &stamp, sample.put());
                if (FAILED(hr) || (flags & MF_SOURCE_READERF_ERROR)) {
                    say("SKIPPED (BROKEN): " + nameOf(track));
                    ended = true;
                    break;
                }
                if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
                    ended = true;
                if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
                    say("SKIPPED (FORMAT CHANGED): " + nameOf(track));
                    ended = true;
                    break;
                }
                if (!sample)
                    continue;
                Com<IMFMediaBuffer> buffer;
                if (FAILED(sample->ConvertToContiguousBuffer(buffer.put())))
                    continue;
                BYTE* data = nullptr;
                DWORD bytes = 0;
                if (SUCCEEDED(buffer->Lock(&data, nullptr, &bytes)) && data) {
                    const size_t n = bytes / sizeof(float);
                    pending.resize(n - n % static_cast<size_t>(channels));
                    std::memcpy(pending.data(), data, pending.size() * sizeof(float));
                    buffer->Unlock();
                }
                continue;
            }
            const UINT32 n = static_cast<UINT32>(std::min<size_t>(have, room));
            BYTE* out = nullptr;
            if (FAILED(render->GetBuffer(n, &out)) || !out)
                break;
            const float* in = pending.data() + pendingAt;
            analyzer.push(in, n, channels, rate);
            float* o = reinterpret_cast<float*>(out);
            const size_t samples = static_cast<size_t>(n) * static_cast<size_t>(channels);
            for (size_t i = 0; i < samples; ++i) {
                const float v = in[i] * g;
                o[i] = std::isfinite(v) ? std::clamp(v, -1.f, 1.f) : 0.f;
            }
            render->ReleaseBuffer(n, 0);
            pendingAt += samples;
            written += n;
            room -= n;
        }
        // The song is over once the device has played what was sent.
        if (ended && pendingAt >= pending.size() && padding == 0) {
            const int count = static_cast<int>(list.size());
            if (count)
                start(nextTrack(track < 0 ? 0 : track, count), 1);
            else
                stopAll();
        }
    }
    void publish(bool force) {
        if (!force && !dirty)
            return;
        if (faulted)
            return; // MUSIC OFF stays
        Status s;
        s.available = true;
        s.tracks = static_cast<int>(list.size());
        s.track = track;
        s.playing = state == State::playing;
        s.paused = state == State::paused;
        s.elapsed = reader ? static_cast<float>(elapsed()) : 0.f;
        s.total = static_cast<float>(duration);
        s.song = nameOf(track);
        s.note = GetTickCount64() < noteUntil ? note : std::string();
        s.folder = utf8(folder);
        s.names.reserve(list.size());
        for (const auto& t : list)
            s.names.push_back(t.name);
        s.serial = ++serial;
        std::lock_guard guard(statusLock);
        shown = std::move(s);
        dirty = false;
    }
    void playLoop() {
        const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        // Update 22: Media Foundation is delay-loaded (CMakeLists.txt), so a Windows N/KN PC without the Media
        // Feature Pack still loads the stereo module and plays VR; only the music stays off. Nothing of mfplat or
        // mfreadwrite is called unless both are in System32.
        const bool mf = mediaFoundationPresent() && SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE));
        if (!mf) {
            if (SUCCEEDED(com))
                CoUninitialize();
            fail("MUSIC OFF: WINDOWS MEDIA FEATURES MISSING");
            return;
        }
        ULONGLONG lastPublish = 0;
        while (!quit) {
            const HANDLE waits[2]{wake, audioEvent};
            WaitForMultipleObjects(2, waits, FALSE, tickMs);
            takeList();
            for (uint32_t t = tail.load(std::memory_order_relaxed); t != head.load(std::memory_order_acquire);) {
                const Command c = ring[t % ring.size()];
                tail.store(++t, std::memory_order_release);
                command(c);
            }
            fill();
            const ULONGLONG now = GetTickCount64();
            if (now - lastSpectrum >= 11) {
                const float dt = static_cast<float>(std::min<ULONGLONG>(now - lastSpectrum, 250)) / 1000.f;
                lastSpectrum = now;
                spectra.write(state == State::playing && client ? analyzer.update(dt) : analyzer.quiet(dt));
            }
            if (now - lastPublish >= 100 || dirty) {
                publish(true);
                lastPublish = now;
            }
        }
        closeSong();
        closeClient();
        devices.reset();
        MFShutdown();
        if (SUCCEEDED(com))
            CoUninitialize();
    }
};

Player::Player() {
    try {
        impl_ = std::make_unique<Impl>();
    } catch (...) {
        impl_.reset();
    }
}
Player::Player(std::wstring folder) {
    try {
        impl_ = std::make_unique<Impl>(std::move(folder));
    } catch (...) {
        impl_.reset();
    }
}
Player::~Player() = default;
bool Player::status(Status& out) const {
    if (!impl_) {
        out.available = false;
        out.note = "MUSIC OFF: NO MEMORY";
        return true;
    }
    std::unique_lock guard(impl_->statusLock, std::try_to_lock);
    if (!guard.owns_lock())
        return false;
    if (out.serial == impl_->shown.serial && out.serial)
        return true; // nothing new: no copy
    try {
        out = impl_->shown;
    } catch (...) {
        return false;
    }
    return true;
}
bool Player::spectrum(Spectrum& out) {
    return impl_ && impl_->spectra.read(out);
}
void Player::rescan() {
    if (impl_)
        impl_->send(Kind::rescan);
}
void Player::play(int track) {
    if (impl_)
        impl_->send(Kind::play, static_cast<float>(track));
}
void Player::toggle() {
    if (impl_)
        impl_->send(Kind::toggle);
}
void Player::stop() {
    if (impl_)
        impl_->send(Kind::stop);
}
void Player::previous() {
    if (impl_)
        impl_->send(Kind::previous);
}
void Player::next() {
    if (impl_)
        impl_->send(Kind::next);
}
void Player::seekBy(float seconds) {
    if (impl_)
        impl_->send(Kind::seekBy, seconds);
}
void Player::seekTo(float fraction) {
    if (impl_)
        impl_->send(Kind::seekTo, fraction);
}
void Player::setVolume(float percent) {
    if (impl_)
        impl_->gain.store(gainOf(percent), std::memory_order_relaxed);
}
void Player::select(int track) {
    if (impl_)
        impl_->send(Kind::select, static_cast<float>(track));
}
} // namespace spidy::music
