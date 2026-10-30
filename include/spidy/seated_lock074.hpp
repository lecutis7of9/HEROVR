#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 test 2b SEATED TOOLS split (owner pt1208u: "you broke all driving cars. you cant exit any vehicle now").
// CONFIRMED by reading test 2 (2d0db7a): the SEATED TOOLS lock zeroed the hand's grip in the combat frame
// (cf.hands[i].grip), and the van / Batmobile read that same grip for the wheel's latch, the door handles
// and (through the latch) the wheel-hand pedals - so in every car no wheel, no throttle and no way out.
// Now each hand's raw trigger and grip are split once: the car's copy is never touched; the hero tools'
// copy is emptied while the lock is on (and the trigger is the pedal while driving, as 0.0.72).
// Pure: no game reads.
namespace spidy::seated_lock074 {

struct Hand {
    float trigger{}, grip{};
};
struct Split {
    Hand car;    // the wheel, the pedals, the door handles, the dash
    Hand tools;  // punch, web ball, gadget, zip, grapple, claws
    bool busy{}; // the hand is the car's: no hero tool answers it
};
// 0.0.74 test 2b (owner, 8:38 PM ET): the hero tools are blocked for BATMAN in the Batmobiles only; the PUNISHER keeps
// his guns in the van (a free hand aims and fires while the other drives) - no grapple in any car.
struct Lock {
    bool tools{};   // every hero tool and ability (the Batmobile)
    bool grapple{}; // the grapple / grapple gun / cape launch (every car)
};
inline Lock lockFor(bool settingBlocked, bool inCar, bool batmobile) {
    Lock l;
    l.grapple = settingBlocked && inCar;
    l.tools = l.grapple && batmobile;
    return l;
}
inline Split split(Hand raw, bool locked, bool driving) {
    Split s;
    s.car = raw;
    s.tools = raw;
    if (driving)
        s.tools.trigger = 0; // Version 0.0.72: driving, the trigger is the pedal and nothing else
    if (locked) {
        s.tools = {};
        s.busy = true;
    }
    return s;
}

} // namespace spidy::seated_lock074
