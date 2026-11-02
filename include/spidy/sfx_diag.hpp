#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// 0.0.76 SFX DIAG (read-only; tools/sfx_diag_record.py): what the mod's own XAudio2 sound layer (src/sfx_win.cpp)
// does, to tell it apart from the game's audio and the headset's audio stream. Made once: the source voices'
// format and the device's mix format; every 10 s: buffers submitted and completed, buffers queued (the last serve
// and the window's high water), XAudio2 engine glitches (its audio passes that came too late: an underrun), voice
// and critical errors, the gap between XAudio2's audio passes and the player thread's wake-up lateness (mean and max);
// and a ring of moments (QPC microseconds, the clock the launcher's time.perf_counter reads) where a glitch, a
// critical error or a late pass or wake-up happened. Counted with atomics in the callbacks; nothing is written to
// disk from here. The sound is the same with or without it.
#include <cstdint>

namespace spidy::sfx_diag {

inline constexpr uint32_t magic = 0x47445853; // 'SXDG'
inline constexpr uint32_t version = 1;
inline constexpr uint32_t windowMs = 10000;
inline constexpr uint32_t windows = 90;
inline constexpr uint32_t moments = 64;
inline constexpr uint32_t lateMs = 50; // a pass or wake-up this late: a moment of its own

// Made once, when the player is set up.
struct Format {
    uint32_t made{};                         // 1 once filled
    uint32_t masterRate{}, masterChannels{}; // the mastering voice (XAudio2 2.9: the device's own mix format)
    uint32_t masterMask{};                   // its speaker mask
    uint32_t mixHr{};                        // the default device's mix-format query: 0 fine, else its HRESULT
    uint32_t mixRate{}, mixChannels{}, mixBits{}; // the default render device's shared-mode mix format
    uint32_t sourceRateMin{}, sourceRateMax{};
    uint32_t sourceChannelsMin{}, sourceChannelsMax{};
    uint32_t sourceBits{};    // of the first sound
    uint32_t sourceVoices{};  // made
    uint32_t resampled{};     // sounds whose rate is not the mastering voice's (XAudio2 resamples them)
    uint32_t xaudio{};        // 9: XAudio2_9.dll, 8: XAudio2_8.dll
    char device[64]{};        // the default render device's name (UTF-8, cut short)
};
static_assert(sizeof(Format) == 4 * 16 + 64);

enum MomentKind : uint32_t { glitch = 1, criticalError = 2, latePass = 3, lateWake = 4, layerOff = 5, layerOn = 6 };

struct Window {
    uint32_t ms{};
    uint32_t submitted{}, completed{};   // buffers handed to and finished by the source voices
    uint32_t queuedLast{}, queuedMax{};  // buffers queued over all source voices (each serve)
    uint32_t glitches{};                 // XAudio2 GlitchesSinceEngineStarted, this window's share
    uint32_t voiceErrors{}, criticalErrors{};
    uint32_t passes{};                   // XAudio2 audio passes
    uint32_t passGapUsSum{}, passGapUsMax{};
    uint32_t wakes{};                    // the player thread's timed-out waits
    uint32_t wakeLateUsSum{}, wakeLateUsMax{};
    uint32_t layer{};                    // 1 the layer was on at the window's end (vr_sfx_layer 0), 0 off
    uint32_t activeVoices{};             // XAudio2 performance data: ActiveSourceVoiceCount at the window's end
};
static_assert(sizeof(Window) == 4 * 16);

struct Moment {
    uint64_t qpcUs{};
    uint32_t kind{}, value{}; // value: the glitch total, the error HRESULT, or the lateness in microseconds
};
static_assert(sizeof(Moment) == 16);

struct Data {
    uint32_t magic = sfx_diag::magic, version = sfx_diag::version, bytes = sizeof(Data), windowMs = sfx_diag::windowMs;
    uint32_t closed{}, momentsNoted{}, layer = 1, spare{};
    Format format{};
    Window current{};
    Window ring[windows]{};
    Moment moment[moments]{};
};
static_assert(sizeof(Data) == 32 + 128 + 64 * 91 + 16 * 64);

// One finished window into the ring.
inline void close(Data& d) {
    d.ring[d.closed % windows] = d.current;
    ++d.closed;
    d.current = {};
}
inline void note(Data& d, uint64_t qpcUs, uint32_t kind, uint32_t value) {
    d.moment[d.momentsNoted % moments] = {qpcUs, kind, value};
    ++d.momentsNoted;
}

} // namespace spidy::sfx_diag
