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

// ---- SEATED DRIVING (combat38 SEATED DRIVING: SIT BUTTON, AUTO SIT, OFF) ----
enum SeatMode : uint8_t { seatButton = 0, seatAuto = 1, seatOff = 2 };
inline constexpr const char* seatHelp = "SIT on the van dash puts your eye at the driver's seat, no restart needed.";
// The driver's eye in the van frame (vehicle_view's seat: x left, y up, z forward; metres).
inline constexpr Vec3 seatEye{-.45f, 1.60f, -.55f};
inline constexpr float liftLow = -.9f, liftHigh = .6f; // the most it moves the eye down / up
inline constexpr float autoSitDelay = 1.f;            // seconds in the seat before AUTO SIT

// The lift (world y, metres) that puts the eye now at `headY` at the seat eye's `seatY`, on top of `current`.
inline float seatLift(float current, float headY, float seatY) {
    if (!(headY == headY) || !(seatY == seatY) || !(current == current))
        return 0.f;
    float l = current + (seatY - headY);
    return l < liftLow ? liftLow : l > liftHigh ? liftHigh : l;
}

// The seat's state: sat (a lift held), cleared on getting out, OFF, or a hideout.
class Seat {
public:
    // One frame: mode, in the driver's seat, the eye's y, the seat eye's world y. Returns the lift to hold.
    float step(uint8_t mode, bool driving, bool inHideout, float dt, float headY, float seatY) {
        if (mode == seatOff || !driving) {
            sat_ = false;
            lift_ = 0;
            wasSat_ = false;
            boarded_ = 0;
            autoDone_ = false;
            return 0;
        }
        if (inHideout) { // standing in the room; sat again on the way back
            if (sat_)
                wasSat_ = true;
            sat_ = false;
            lift_ = 0;
            return 0;
        }
        if (wasSat_) {
            wasSat_ = false;
            want_ = true;
        }
        boarded_ += dt > 0 && dt < 1 ? dt : 0;
        if (mode == seatAuto && !autoDone_ && boarded_ >= autoSitDelay) {
            autoDone_ = true;
            want_ = !sat_;
        }
        if (want_) {
            want_ = false;
            lift_ = seatLift(lift_, headY, seatY);
            sat_ = true;
        }
        return lift_;
    }
    void sit() { want_ = true; }   // the dash's SIT (again: re-levels)
    void stand() {                 // the dash's STAND
        sat_ = false;
        want_ = false;
        lift_ = 0;
    }
    bool sat() const { return sat_; }
    float lift() const { return lift_; }

private:
    bool sat_{}, want_{}, wasSat_{}, autoDone_{};
    float lift_{}, boarded_{};
};

} // namespace spidy::vehicle074
