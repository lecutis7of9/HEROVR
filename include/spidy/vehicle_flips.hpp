#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 test 10 VAN NO FLIPS (owner pt1380u: "the van should auto toggle realistic flips off i had to manually
// do it when i got in bc it kept flipping me when id take a sharp turn").
//
// What flipped him: no car physics of ours or the game's rolls the van (van072 has no roll at all). It is the
// mod's own REALISTIC FLIPS (RealisticFlip, a view tilt): while he drives, the carry moves his body as the van
// goes, so the swing's flight the flip reads sees a hard sideways change of velocity on a sharp turn and turns the
// view over with it. The Batmobiles are the same van072::Driver and the same carry, so they flip just the same.
//
// The fix: while he drives (the van or any Batmobile: van072::driving) and VAN NO FLIPS is ON (the default), the
// rig is given REALISTIC FLIPS off and FLIPS (A / the left stick in the air) off, frame by frame. The player's own
// settings (vr_settings::Values) are never written: out of the car the next frame gives the rig his choice again.
// OFF: his REALISTIC FLIPS / FLIPS choice in the car too, as test 9. No game address is read or written
// (everything here is the mod's own state: CONFIRMED by construction, no CANDIDATE).
#include "spidy/realistic_flip.hpp"

#include <cstdint>

namespace spidy::vehicle_flips {

// live.txt van_no_flips: 0 ON (the default), 1 OFF (SpidyVanNoFlips 5540).
inline constexpr const char* help =
    "VAN NO FLIPS ON: realistic flips turn off by themselves while you drive the van or a Batmobile so sharp turns "
    "don't roll you; OFF keeps your REALISTIC FLIPS choice; live, no restart.";
inline bool on(uint32_t value) {
    return value != 1;
}

// Whether the in-car override holds this frame.
inline bool active(bool driving, bool noFlipsOn) {
    return driving && noFlipsOn;
}
// The REALISTIC FLIPS settings the rig gets this frame: the player's own, or off while the override holds.
inline RealisticFlipSettings realistic(RealisticFlipSettings saved, bool driving, bool noFlipsOn) {
    if (active(driving, noFlipsOn))
        saved.enabled = false;
    return saved;
}
// FLIPS (FlipMotion: A and the left stick in the air) the rig gets this frame.
inline bool stickFlips(bool saved, bool driving, bool noFlipsOn) {
    return saved && !active(driving, noFlipsOn);
}

// Counts for the log: how many times the override began / ended this session.
struct Watch {
    bool was{};
    uint32_t began{}, ended{};
    // Returns +1 the frame it begins, -1 the frame it ends, 0 otherwise.
    int update(bool now) {
        if (now == was)
            return 0;
        was = now;
        if (now) {
            ++began;
            return 1;
        }
        ++ended;
        return -1;
    }
};

} // namespace spidy::vehicle_flips
