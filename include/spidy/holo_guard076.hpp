// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
#pragma once
// 0.0.76 PUNISHER DUAL-WIELD HOLO GUARD (owner: "as Punisher while dual wielding the wrist holo must not open
// when the hands come close"). Two pistols held together (or a pistol hand passing the other forearm) put the
// pointing hand's trigger on the hologram forearm - the FOREARM / TAP / CLICK open gesture - so every shot
// fired with the hands close opened the one-page hologram. Now, as the PUNISHER, the hands-close gesture
// cannot OPEN the hologram while either hand holds a gun (pistol or shotgun) or for combatHold seconds after
// his last shot; the trigger is the gun's (nothing swallowed). Otherwise the gesture is unchanged: other
// heroes, the PUNISHER with empty hands and no shot for combatHold, an already open hologram (it still
// closes the same way), and F8 (not a hands gesture) all work as before.
#include "spidy/hero_modes.hpp"
#include <cmath>
#include <cstdint>
namespace spidy::holo_guard076 {
inline constexpr float combatHold = 2.0f; // seconds after the last shot the gesture stays shut
struct Guard {
    uint64_t shotsSeen{};
    float sinceShot = 1e9f;
    bool primed{}, armed{};
    uint64_t suppressedFrames{};
    // hero: heroNow(); gunInHand: each hand holds a gun; shots: the guns' running shot count (any hero's).
    // 0.0.76 fix 2 capGrapple: CAPTAIN AMERICA holds his drawn grapple gun (the chest draw): its trigger at the
    // chest is the gun's, never the hologram's open (the e808401 test's right-hand grabs opened it instead).
    bool step(float seconds, uint32_t hero, bool gunLeft, bool gunRight, uint64_t shots, bool capGrapple = false) {
        const float dt = std::isfinite(seconds) && seconds > 0 ? (seconds < .1f ? seconds : .1f) : 0.f;
        sinceShot = sinceShot < 1e8f ? sinceShot + dt : sinceShot;
        if (!primed) { // the first frame: a count carried in from before is not a shot now
            shotsSeen = shots;
            primed = true;
        } else if (shots != shotsSeen) {
            shotsSeen = shots;
            sinceShot = 0;
        }
        armed = hero == hero_modes::punisher && (gunLeft || gunRight || sinceShot < combatHold);
        armed = armed || (hero == hero_modes::captainAmerica && capGrapple);
        return armed;
    }
    // The hologram's gesture plays (may open it) this frame: never while armed and closed.
    bool gesturePlay(bool hudPlay, bool holoOn) {
        const bool shut = hudPlay && armed && !holoOn;
        if (shut)
            ++suppressedFrames;
        return hudPlay && !shut;
    }
};
} // namespace spidy::holo_guard076
