#pragma once
// 0.0.74 test 3 CAVE QUIET (owner: "mute the city sound and the car's engine in the Batcave / safehouse, back on
// RETURN TO CITY"). Made by Agent Mulder.
// In a hideout (hideout074::Visit active) the car's engine loop stops, and the game's own sound (the city ambience
// and traffic, all of the game's mix: the game process's default Windows audio session, CANDIDATE that the game's
// audio plays in it - it is the session any WASAPI stream without its own session GUID joins) is ducked to
// `level`. Our car music / Batcomputer MUSIC plays in its own session (music_audio_win.cpp), so it is not ducked.
// On leaving (RETURN TO CITY, the hero changing, VR stopping) the game's volume goes back to what it was.
// Windows remembers an app's session volume, so a duck left by a crash is put back at the next VR start
// (stale()). ON by default, live; OFF: test 2b (engine and city on in the hideouts).
#include <cmath>
#include <cstdint>

namespace spidy::cave_quiet {
inline constexpr float level = .12f;       // the game's sound in a hideout (fraction of what it was)
inline constexpr float staleTolerance = .002f;
// 0 ON (the default), 1 OFF.
inline bool on(uint32_t value) { return value != 1; }
// What the engine loop and the duck want this frame.
inline bool quiet(bool option, bool inHideout) { return option && inHideout; }
inline bool engineSounds(bool driving, bool option, bool inHideout) { return driving && !quiet(option, inHideout); }
// The volume to give the game: ducked from what it was (never louder than it was).
inline float ducked(float was) { return std::isfinite(was) && was > 0 ? was * level : 0.f; }
// At a VR start: a session volume left at a duck (ducked from full) by a crash is put back to full.
inline bool stale(float now) { return std::isfinite(now) && std::fabs(now - level) <= staleTolerance; }

// The state machine the Windows side runs (pure: unit-tested).
struct Duck {
    bool ducked{};
    float was{1.f};
    // want: quiet this frame; now: the session's volume now. Returns the volume to set, or a negative for none.
    float step(bool want, float now) {
        if (want && !ducked) {
            was = std::isfinite(now) && now > 0 ? now : 1.f;
            ducked = true;
            return cave_quiet::ducked(was);
        }
        if (!want && ducked) {
            ducked = false;
            return was;
        }
        return -1.f;
    }
};
} // namespace spidy::cave_quiet

namespace spidy {
// game_duck_win.cpp (Windows): the game process's default audio session. false: no session (nothing changed).
bool gameDuck(bool quiet);
bool gameDuckRecover(); // at VR start: a duck a crash left behind is put back
} // namespace spidy
