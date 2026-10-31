#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.74 test 1, the vehicle's own: the CAR MUSIC player (the van dash's MUSIC page) and SEATED DRIVING
// (the HOME page's SIT: the eye put at the driver seat's eye height; getting out stands him back up). Pure: the
// game_xr glue keeps the player and the rig.
#include "spidy/math.hpp"

#include <cstdint>

namespace spidy::vehicle074 {

// ---- CAR MUSIC (combat38 CAR MUSIC: IN VEHICLES, ALWAYS, OFF) ----
enum MusicMode : uint8_t { musicInVehicles = 0, musicAlways = 1, musicOff = 2 };
inline constexpr const char* musicHelp = "The van dash's MUSIC page plays your songs, no restart needed.";
// Its own folder: Documents\HERO VR\Music (made on the first run, a readme in it; no songs come with the mod).
// 0.0.74 test 10 MUSIC KEPT: the one music folder for every player (the phone, the wrist's MUSIC page and the van):
// it is outside the install, so a new version, an update, a revert or a fresh unzip never leaves it behind.
// The install's own Music folder (where songs went up to 0.0.73) is still read, after it.
inline constexpr const wchar_t* musicFolderUnderDocuments = L"HERO VR\\Music";
inline constexpr const char* musicReadmeName = "README.txt";
inline constexpr const char* musicReadme =
    "HERO VR - your music\r\n"
    "\r\n"
    "Put your own songs here (mp3, ogg, wav; flac / m4a / aac / wma also play where Windows can).\r\n"
    "Sub-folders are read too (up to 8 deep). No music comes with the mod.\r\n"
    "\r\n"
    "This folder is in your Documents, outside the mod's install folder, so updating HERO VR,\r\n"
    "unzipping a new version or reverting never loses it. Songs from an older install's Music\r\n"
    "folder are copied here once (never moved or deleted); that folder is still read too.\r\n"
    "\r\n"
    "The phone, the wrist's MUSIC page and the van's dash all play the songs here.\r\n"
    "In the van: the dash's MUSIC page (X: next page, or tap the tabs) has PREV, PLAY / PAUSE, NEXT,\r\n"
    "VOL- and VOL+. It plays only in the van unless the wrist menu's CAR MUSIC is set to ALWAYS;\r\n"
    "it ducks a little under the engine. CAR MUSIC OFF turns it off.\r\n"
    "\r\n"
    "Made by Agent Mulder.\r\n";

// Whether car music may sound now.
inline bool musicMaySound(uint8_t mode, bool inVehicle, bool inHideout) {
    if (mode == musicOff)
        return false;
    if (mode == musicAlways)
        return true;
    return inVehicle && !inHideout;
}
// The volume share under the engine: rpm 0..1 (the sfx engine's), a third down at the red line.
inline float duck(float rpm) {
    if (!(rpm == rpm))
        return 1.f;
    const float r = rpm < 0 ? 0.f : rpm > 1 ? 1.f : rpm;
    return 1.f - .35f * r;
}
inline constexpr float volumeStep = 10.f; // VOL- / VOL+ (0-100)
inline float stepVolume(float v, int dir) {
    const float n = (v == v ? v : 70.f) + volumeStep * static_cast<float>(dir < 0 ? -1 : dir > 0 ? 1 : 0);
    return n < 0 ? 0.f : n > 100 ? 100.f : n;
}

// ---- SEATED DRIVING (combat38 SEATED DRIVING: AUTO SIT, SIT BUTTON, OFF, HEIGHT ONLY) ----
// 0.0.74 test 10 (owner pt1357u: "youre not automatically seated ... it puts you too far back and doesnt recenter
// you on the chair"): AUTO SIT is first and the default (it was SIT BUTTON = 0, so a saved 0 is AUTO SIT now; OFF
// stays 2), and a sit puts the eye at the seat's eye in all three axes (clamped). HEIGHT ONLY: 0.0.74 test 1's
// SIT button - the eye's height only.
enum SeatMode : uint8_t { seatAuto = 0, seatButton = 1, seatOff = 2, seatHeight = 3 };
inline constexpr const char* seatHelp = "AUTO SIT centres you in the driver's seat as you get in; no restart needed.";
// The driver's eye in the van frame (vehicle_view's seat: x left, y up, z forward; metres).
inline constexpr Vec3 seatEye{-.45f, 1.60f, -.55f};
inline constexpr float liftLow = -.9f, liftHigh = .6f; // the most it moves the eye down / up
inline constexpr float seatReach = .5f;               // 0.0.74 test 10: the most it moves the eye level (any way)
inline constexpr float autoSitDelay = .3f;            // seconds after boarding ends before AUTO SIT (test 1: 1 s)
inline bool seatCentres(uint8_t mode) { return mode != seatHeight; }

// The lift (world y, metres) that puts the eye now at `headY` at the seat eye's `seatY`, on top of `current`.
inline float seatLift(float current, float headY, float seatY) {
    if (!(headY == headY) || !(seatY == seatY) || !(current == current))
        return 0.f;
    float l = current + (seatY - headY);
    return l < liftLow ? liftLow : l > liftHigh ? liftHigh : l;
}

// 0.0.74 test 10 SEAT CENTRE: the world offset that puts the eye now at `head` at the seat eye `seat`, on top of
// `current`: y as seatLift, the level part at most seatReach long (its way kept). Any NaN: none.
inline Vec3 seatOffset(Vec3 current, Vec3 head, Vec3 seat) {
    const auto bad = [](Vec3 v) { return !(v.x == v.x) || !(v.y == v.y) || !(v.z == v.z); };
    if (bad(current) || bad(head) || bad(seat))
        return {};
    Vec3 o{current.x + (seat.x - head.x), seatLift(current.y, head.y, seat.y), current.z + (seat.z - head.z)};
    const float n = std::sqrt(o.x * o.x + o.z * o.z);
    if (!(n == n) || n > 1e6f)
        return {};
    if (n > seatReach) {
        o.x *= seatReach / n;
        o.z *= seatReach / n;
    }
    return o;
}

// The seat's state: sat (an offset held), cleared on getting out, OFF, or a hideout.
class Seat {
public:
    // One frame: mode, in the driver's seat, in a hideout, the eye now and the seat eye (world), still boarding
    // (the carry into the seat), a hand latched on the wheel. Returns the offset to hold (world, metres).
    // 0.0.74 test 10: AUTO SIT autoSitDelay after the boarding ends; a sit (AUTO or SIT) waits while a hand holds
    // the wheel (the eye - and the hands - jumping under a latched hand would steer), so grip and steering always
    // work.
    Vec3 step(uint8_t mode, bool driving, bool inHideout, float dt, Vec3 head, Vec3 seat, bool boarding = false,
              bool wheelHeld = false) {
        if (mode == seatOff || !driving) {
            sat_ = false;
            want_ = false;
            offset_ = {};
            wasSat_ = false;
            boarded_ = 0;
            autoDone_ = false;
            return {};
        }
        if (inHideout) { // standing in the room; sat again on the way back
            if (sat_)
                wasSat_ = true;
            sat_ = false;
            offset_ = {};
            return {};
        }
        if (wasSat_) {
            wasSat_ = false;
            want_ = true;
        }
        if (!boarding)
            boarded_ += dt > 0 && dt < 1 ? dt : 0;
        if (mode == seatAuto && !autoDone_ && !boarding && boarded_ >= autoSitDelay) {
            autoDone_ = true;
            want_ = want_ || !sat_;
        }
        if (want_ && !wheelHeld && !boarding) {
            want_ = false;
            if (seatCentres(mode))
                offset_ = seatOffset(offset_, head, seat);
            else
                offset_ = {0, seatLift(offset_.y, head.y, seat.y), 0};
            sat_ = true;
        }
        return offset_;
    }
    // 0.0.74 test 1's: the eye's y only (the lift), as before.
    float step(uint8_t mode, bool driving, bool inHideout, float dt, float headY, float seatY) {
        return step(mode, driving, inHideout, dt, Vec3{0, headY, 0}, Vec3{0, seatY, 0}).y;
    }
    void sit() { want_ = true; }   // the dash's SIT (again: re-centres)
    void stand() {                 // the dash's STAND
        sat_ = false;
        want_ = false;
        offset_ = {};
    }
    bool sat() const { return sat_; }
    bool waiting() const { return want_; } // a sit asked, held back (a hand on the wheel, still boarding)
    float lift() const { return offset_.y; }
    Vec3 offset() const { return offset_; }

private:
    bool sat_{}, want_{}, wasSat_{}, autoDone_{};
    Vec3 offset_{};
    float boarded_{};
};

} // namespace spidy::vehicle074
