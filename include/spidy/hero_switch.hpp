#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// Update 38.5 HERO SWITCH PROBE: read-only. The player asked for a PETER / MILES switch in the
// mod. What is known (static, the exe of October 24; nothing here is called or hooked):
//   - The game has its own switch: RTTI names HeroCharacterSwitchManager@Hero (vtable RVA
//     0x54294f8, set at 0xf849d2), HeroCharacterSwitchAction (0x5429a90), the events
//     HeroCharacterSwitchStartedEvent (0x5a371a0) / ...FinishedEvent (0x5a372e0), the phone's
//     UIFNSMButtonSwitchCharacterHold{Start,Complete,Released}Event (HoldComplete 0x5a13398) and
//     PauseMenuScreenPageHeroSwapData (0x57a9318). All CANDIDATE: found from RTTI only, none seen
//     at run time, so the mod does not call them (a wrong call could crash or break the save).
//   - The player starts a switch in the game's own phone (Requests app, hold the switch button);
//     the game then replaces the hero's actor (game_player.hpp), and the port already re-attaches
//     its view, body, webs, swing and suit look to the new hero (game_xr.cpp attach()).
// This probe measures that re-attach on a real switch: how long there was no hero, whether VR's
// view and the body came back on the new hero, and whether the game screen took over. Its record
// (HSWP) is in update37.json; the console line says what happened.
#include <cstdint>

namespace spidy::hero_switch {
#pragma pack(push, 1)
struct Data {
    uint32_t magic = 0x50575348, version = 1, bytes = 112; // "HSWP"
    uint32_t swaps{};      // the hero's actor replaced by another (not the first found)
    uint32_t viewBack{};   // swaps after which VR's gameplay view came back within `backSeconds`
    uint32_t bodyBack{};   // swaps after which the body solved on the new hero within `backSeconds`
    uint32_t screenAfter{}; // swaps after which the game screen (not VR) showed at `backSeconds`
    uint32_t pending{};    // 1 while a swap's re-attach is still being timed
    uint32_t pad[2]{};
    uint64_t hero{}, record{};         // now
    uint64_t lastHero{}, lastRecord{}; // before the latest swap
    float lastGapMs{};  // no hero between the two
    float lastViewMs{}; // the new hero found -> VR's gameplay view (-1 not yet / never)
    float lastBodyMs{}; // the new hero found -> the body solved on him (-1 not yet / never)
    float worstViewMs{}, worstBodyMs{};
    float pad2{};
    uint64_t gapsWithoutHero{}; // times the hero was gone (a load, a switch's fade)
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 112);
#pragma pack(pop)

inline constexpr double backSeconds = 10;

// Pure bookkeeping (tested on its own): fed each frame.
class Watch {
  public:
    // The hero now (0, 0: none), the time, whether VR shows gameplay, whether the body solved on
    // the hero this frame, whether the game's screen (menus, loading) is up.
    void frame(uint64_t hero, uint64_t record, double nowMs, bool vrGameplay, bool bodySolved, bool gameScreen) {
        if (hero != d.hero) {
            if (!hero) {
                goneAt = nowMs;
                ++d.gapsWithoutHero;
            } else {
                if (seen) {
                    ++d.swaps;
                    d.lastHero = lastSeen;
                    d.lastRecord = lastSeenRecord;
                    d.lastGapMs = d.hero ? 0.f : static_cast<float>(nowMs - goneAt);
                    d.lastViewMs = d.lastBodyMs = -1;
                    d.pending = 1;
                    foundAt = nowMs;
                    viewDone = bodyDone = false;
                }
                seen = true;
                lastSeen = hero;
                lastSeenRecord = record;
            }
            d.hero = hero;
            d.record = record;
        }
        if (!d.pending || !hero)
            return;
        const double since = nowMs - foundAt;
        if (!viewDone && vrGameplay) {
            viewDone = true;
            d.lastViewMs = static_cast<float>(since);
            d.worstViewMs = d.lastViewMs > d.worstViewMs ? d.lastViewMs : d.worstViewMs;
            if (since <= backSeconds * 1000)
                ++d.viewBack;
        }
        if (!bodyDone && bodySolved) {
            bodyDone = true;
            d.lastBodyMs = static_cast<float>(since);
            d.worstBodyMs = d.lastBodyMs > d.worstBodyMs ? d.lastBodyMs : d.worstBodyMs;
            if (since <= backSeconds * 1000)
                ++d.bodyBack;
        }
        if (since >= backSeconds * 1000) {
            d.screenAfter += gameScreen ? 1 : 0;
            d.pending = 0;
        }
    }
    const Data& data() const {
        return d;
    }
    // Just counted a swap (the frame it was found): for the console line.
    bool swapStarted(uint32_t& last) const {
        const bool started = d.swaps != last;
        last = d.swaps;
        return started;
    }

  private:
    Data d{};
    bool seen{}, viewDone{}, bodyDone{};
    uint64_t lastSeen{}, lastSeenRecord{};
    double goneAt{}, foundAt{};
};
} // namespace spidy::hero_switch
