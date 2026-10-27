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
// CAPTAIN AMERICA: the shield's throw, ricochet, catch and clang (bash). Version 0.0.72: pistol_reload.
enum Id : uint32_t { sonicBoom, repulsorFire, chestBeam, beamReady, batonWhoosh, batonHit, repulsorCharge,
                     pistolShot, shotgunBlast, dryClick, magDrop, magSeat, holsterTick, clawSnikt, heatVision, freezeBreath,
                     shieldThrow, shieldRicochet, shieldCatch, shieldClang, pistolReload, thorLightning1, thorLightning2, sounds };
inline constexpr const char* names[sounds] = {"sonic_boom", "repulsor_fire", "chest_beam",
                                              "beam_ready", "baton_whoosh", "baton_hit", "repulsor_charge",
                                              "pistol_shot", "shotgun_blast", "dry_click", "mag_drop", "mag_seat", "holster", "claw_snikt", "heat_vision", "freeze_breath",
                                              "shield_throw", "shield_ricochet", "shield_catch", "shield_clang", "pistol_reload", "thor_lightning_1", "thor_lightning_2"};
// Voices playing one sound at most at once (a fast run of blasts layers up to this).
inline constexpr uint32_t voicesPerSound = 4;
// Version 0.0.72: the Punisher's guns fire the player's own sounds (tools/sfx/user_guns.py): the sawed-off
// 3.5 s, the pistols 2.5 s, the pistols' reload 1.75 s. Every shot (and every pistol reload) plays its sound
// once, whole: each has voices enough that one is always idle at the fastest rate (both pistols at
// pistolSpec.cooldown, the sawed-off at sawedOffSpec.cooldown; tests check it), so none is dropped or cut.
inline constexpr uint32_t mostVoices = 28;
inline constexpr uint32_t voicesFor(uint32_t id) {
    return id == pistolShot ? 28u : id == shotgunBlast ? 12u : id == pistolReload ? 12u
         : id == thorLightning1 || id == thorLightning2 ? 24u // THOR: a strike each stormEvery (.35 s) over 6.7 s
                                                        : voicesPerSound;
}

// Version 0.0.72 SOUND LOOPS (SUPERMAN's heat vision and cold breath: the 0.0.71 videos' stutter was a
// 0.6 s one-shot re-fired every 0.55 s). loop(id, true) each frame while the beam is held keeps one voice
// playing that sound on repeat (its WAV is a seamless 1 s loop, tools/sfx/make_loops_0072.py); loop(id,
// false) stops it. A loop not refreshed for loopHoldMs stops by itself, so a frame path that stops asking
// (a menu, a hero change, a fault) never leaves it humming.
inline constexpr uint64_t loopHoldMs = 250;
inline constexpr bool loopLive(uint64_t refreshedMs, uint64_t nowMs) {
    return refreshedMs != 0 && nowMs >= refreshedMs && nowMs - refreshedMs <= loopHoldMs;
}
inline constexpr bool loopable(uint32_t id) { return id == heatVision || id == freezeBreath; }

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
    // The player: how many of this sound were asked since the last take (capped at voicesFor(id);
    // the rest counted as dropped).
    uint32_t take(Id id, uint64_t& dropped) {
        if (id >= sounds)
            return 0;
        const uint32_t n = pending_[id].exchange(0, std::memory_order_relaxed);
        const uint32_t most = voicesFor(id);
        if (n > most)
            dropped += n - most;
        return n > most ? most : n;
    }
    // SOUND LOOPS: when each loop was last asked for (milliseconds; 0: stopped). Returns the previous.
    uint64_t loop(Id id, uint64_t nowMs, bool on) {
        if (id >= sounds || !loopable(id))
            return 0;
        return loopAt_[id].exchange(on ? (nowMs ? nowMs : 1) : 0, std::memory_order_relaxed);
    }
    uint64_t loopAt(Id id) const { return id < sounds ? loopAt_[id].load(std::memory_order_relaxed) : 0; }
    bool any() const {
        for (const auto& p : pending_)
            if (p.load(std::memory_order_relaxed))
                return true;
        return false;
    }

private:
    std::array<std::atomic<uint32_t>, sounds> pending_{};
    std::array<std::atomic<uint64_t>, sounds> loopAt_{};
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
void loop(Id id, bool on);  // never blocks (SOUND LOOPS; only loopable ids)
uint64_t nowMs();           // the loops' clock (milliseconds, monotonic)
void setGain(float gain);   // 0..1 (SFX VOLUME)
Stats stats();
Requests& requests();        // the shared queue (tests)

} // namespace spidy::sfx
