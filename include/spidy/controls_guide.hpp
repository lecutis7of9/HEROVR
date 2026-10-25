#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// Update 37: the hologram's CONTROLS page - every motion and gesture control in one
// short line, marked as the mod runs NOW: a line whose feature is switched off reads
// "OFF" and is greyed; one that cannot work in this game reads "N/A". The marks come
// from View::guideOff / View::guideNa (bits by Line), filled each frame from the live
// settings, so a live.txt change or a hologram press shows at once. Theme colours only
// (the Painter's), so every theme draws it, the 2000s Raimi one included.
#include <cstdint>
#include <initializer_list>

namespace spidy::controls_guide {
enum Line : uint32_t {
    holo, swing, zip, webShooter, webGrabber, sonicBurst, upshot, ricochet, handHits, lockOn, heal, wallCrawl,
    dodge, interact, pause, mirror, blood, lines
};
struct Text {
    const char* name;
    const char* how;
};
// Short enough for one line each at the default text size (the tests check).
inline constexpr Text texts[lines] = {
    {"HOLOGRAM", "other hand to this wrist + trigger"},
    {"WEB SWING", "grip + trigger, swing your arm"},
    {"ZIP", "right stick up, let go"},
    {"WEB BALL", "tap the trigger"},
    {"WEB GRABBER", "aim, hold the trigger, yank back"},
    {"SONIC BURST", "hands forward, close + both triggers"},
    {"UPSHOT", "aim low, hold, swing up"},
    {"RICOCHET", "grip + trigger, flick"},
    {"HAND HITS", "punch (the game's X)"},
    {"LOCK-ON", "aim your wrist at a foe; it sticks"},
    {"HEAL", "hold right stick down"},
    {"WALL CRAWL", "grab the wall, pull hand over hand"},
    {"DODGE", "B"},
    {"INTERACT", "Y"},
    {"PAUSE", "both thumbsticks in"},
    {"MIRROR", "MIRROR on VR SETTINGS"},
    {"BLOOD", "punch near a foe (BLOOD SPLATTER)"},
};
enum State { on, off, na };
inline State stateOf(Line l, uint32_t offBits, uint32_t naBits) {
    return naBits >> l & 1 ? na : offBits >> l & 1 ? off : on;
}
inline const char* mark(State s) { return s == na ? "N/A" : s == off ? "OFF" : ""; }
// What the live settings say, for game_xr to fill the two masks.
struct Live {
    bool combat = true, grabberSlot = true, sonicSlot = true, upshotSlot = true, ricochetSlot = true;
    bool punch = true, steer = true, heal = true, climb = true, gameCrawl = true, mirror = false, blood = false;
    bool worldLog = true; // BLOOD's plan B needs the WORLD LOG's enemy distance
    bool pad = true; // the virtual controller is up (every game button needs it)
};
inline void masks(const Live& v, uint32_t& off, uint32_t& naBits) {
    off = naBits = 0;
    const auto set = [&](Line l, bool isOff) { off |= isOff ? 1u << l : 0u; };
    set(webShooter, !v.combat);
    set(webGrabber, !v.combat || !v.grabberSlot);
    set(sonicBurst, !v.combat || !v.sonicSlot);
    set(upshot, !v.combat || !v.upshotSlot);
    set(ricochet, !v.combat || !v.ricochetSlot);
    set(handHits, !v.combat || !v.punch);
    set(lockOn, !v.combat || !v.steer);
    set(heal, !v.heal);
    set(wallCrawl, !v.climb || !v.gameCrawl);
    set(mirror, !v.mirror);
    set(blood, !v.blood);
    if (v.blood && !v.worldLog)
        naBits |= 1u << blood;
    if (!v.pad) // no virtual controller: nothing that reaches the game works
        for (Line l : {zip, webShooter, webGrabber, sonicBurst, upshot, ricochet, handHits, lockOn, heal, dodge,
                       interact, pause, wallCrawl})
            naBits |= 1u << l;
}
} // namespace spidy::controls_guide
