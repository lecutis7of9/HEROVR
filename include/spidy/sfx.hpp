#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.5 (Update 38.9) SOUND EFFECTS: the mod's own original sounds (tools/sfx/make_sfx.py
// makes assets/sfx/*.wav: procedural, 48 kHz mono 16-bit; compiled in as bytes). The VR thread only
// asks (play: an atomic add, never a lock, a wait or a call into audio); src/sfx_win.cpp's own
// thread plays them on XAudio2 (loaded at run time; missing or failing: silence, a code in the
// log, the game and VR untouched). SFX VOLUME (combat38::sfxVolume) is the gain.
// The game's own audio engine is not used (a hook into it stays CANDIDATE, not shipped).
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace spidy::sfx {

// Version 0.0.6 (Update 39.0.2) REPULSOR: repulsor_fire is the chosen take's blast (CMakeLists.txt
// SPIDY_REPULSOR_TAKE, assets/sfx/repulsor_mcu_<take>_fire.wav) and repulsor_charge its whine, played on
// the press so it fills the 0.12 s chord wait (repulsor.hpp chordWindow); back to back they are the take.
// Version 0.0.7 HERO PROFILES (tools/sfx/make_sfx_007.py): the guns, reloads, holsters, claws, heat and frost;
// CAPTAIN AMERICA: the shield's throw, ricochet, catch and clang (bash).
enum Id : uint32_t { sonicBoom, repulsorFire, chestBeam, beamReady, batonWhoosh, batonHit, repulsorCharge,
                     pistolShot, shotgunBlast, dryClick, magDrop, magSeat, holsterTick, clawSnikt, heatVision, freezeBreath,
                     shieldThrow, shieldRicochet, shieldCatch, shieldClang, sounds };
inline constexpr const char* names[sounds] = {"sonic_boom", "repulsor_fire", "chest_beam",
                                              "beam_ready", "baton_whoosh", "baton_hit", "repulsor_charge",
                                              "pistol_shot", "shotgun_blast", "dry_click", "mag_drop", "mag_seat", "holster", "claw_snikt", "heat_vision", "freeze_breath",
                                              "shield_throw", "shield_ricochet", "shield_catch", "shield_clang"};
// Voices playing one sound at most at once (a fast run of blasts layers up to this).
inline constexpr uint32_t voicesPerSound = 4;

// A WAV's PCM (a view into the embedded bytes).
struct Pcm {
    bool ok{};
    uint32_t rate{}, channels{}, bits{};
    const uint8_t* data{};
    uint32_t bytes{};
    float seconds() const { return ok && rate && channels && bits ? static_cast<float>(bytes) / static_cast<float>(rate * channels * (bits / 8)) : 0.f; }
};
// RIFF/WAVE, PCM (format 1) only; anything else or a short or broken file is !ok.
Pcm parseWav(const uint8_t* file, size_t size);
// The embedded sound (parsed once; !ok if the bytes are broken).
const Pcm& sound(Id id);

// The VR thread's requests: one counter per sound (any thread may add; the player takes them).
class Requests {
public:
    void play(Id id) {
        if (id < sounds)
            pending_[id].fetch_add(1, std::memory_order_relaxed);
    }
    // The player: how many of this sound were asked since the last take (capped at voicesPerSound;
    // the rest counted as dropped).
    uint32_t take(Id id, uint64_t& dropped) {
        if (id >= sounds)
            return 0;
        const uint32_t n = pending_[id].exchange(0, std::memory_order_relaxed);
        if (n > voicesPerSound)
            dropped += n - voicesPerSound;
        return n > voicesPerSound ? voicesPerSound : n;
    }
    bool any() const {
        for (const auto& p : pending_)
            if (p.load(std::memory_order_relaxed))
                return true;
        return false;
    }

private:
    std::array<std::atomic<uint32_t>, sounds> pending_{};
};

// The player's state, for the log and STATUS.
enum State : uint32_t { notStarted, starting, playing, noXAudio2, comFailed, engineFailed, masterFailed, voiceFailed,
                        threadFailed, badSound, states };
inline constexpr const char* stateNames[states] = {
    "NOT STARTED", "STARTING", "PLAYING", "NO XAUDIO2 (XAudio2_9/2_8.dll missing)", "COM FAILED",
    "XAUDIO2 ENGINE FAILED", "NO AUDIO DEVICE", "VOICE FAILED", "THREAD FAILED", "A SOUND FILE IS BROKEN"};
struct Stats {
    uint32_t state{}, code{}; // code: the failing HRESULT or Win32 error (0 none)
    uint64_t asked{}, played{}, dropped{}, muted{};
    float gain = 1.f;
};

// The runtime (src/sfx_win.cpp on Windows; elsewhere a quiet stand-in that counts).
// start: once, from any thread; never throws; false if the player cannot run.
bool start();
void play(Id id);           // never blocks
void setGain(float gain);   // 0..1 (SFX VOLUME)
Stats stats();
Requests& requests();        // the shared queue (tests)

} // namespace spidy::sfx
