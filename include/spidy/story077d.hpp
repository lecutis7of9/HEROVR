#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 PROMPT CARDS (CANDIDATE), after the player's test of b8b33ce (pt1773u: "you put an entire 2d
// flatscreen projection of the game in VR"). The PROMPT WINDOW copied the game's whole presented frame; it is
// gone by default (live.txt prompt_window is now off / debug_full, off by default).
// Instead the mod draws its own small VR cards - never a picture of the game - from what the game's object table
// holds (world_trace's scan, read-only, the same guarded reads): objects of the prompt classes named below
// (their decorated RTTI names are CONFIRMED in Spider-Man2.exe; that they stand for the prompt on screen is
// CANDIDATE). The SM2 HUD is cohtml (Coherent), not Scaleform: its prompt texts and icons are not read (no hook).
//   - object cards: over a throwable / web strike target the player is near (the throw icon on debris, the Y
//     web strike), world-anchored;
//   - screen cards: a QTE / mash object that appeared since VR play began, a small card a bit below the centre,
//     for at most its kind's seconds.
// Each card names the game's button and the VR control that sends it (the mod's own mapping).
// Pure and allocation-free; nothing here reads or writes the game.
#include "math.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>

namespace spidy::story077d {
// marker (HERO VR 0.0.77 INTERACT CARDS AND WAYPOINT, story077g.hpp): never a card - a candidate place of the
// current objective for the yellow waypoint, and logged.
// cine (CUT5): a scene's own "press Y" offer (CineQTEBeginAction). CineApproachPrompt was also listed here as
// CANDIDATE; 79.10 remaps it to an LB+RB pull (Prowler stash doors open with L1+R1, not Y: 79.9 report
// game-vr-20261112-042721 sent Y 1654 frames with no effect).
enum Where : uint32_t { screen = 0, object = 1, watchOnly = 2, marker = 3, cine = 4 };
struct Kind {
    const char* cls;  // decorated RTTI name (exact)
    const char* game; // the game's button(s), as its prompt shows them
    const char* vr;   // the VR control that sends it (the mod's mapping)
    uint32_t where;
    float seconds; // screen cards: shown at most this long after the object appeared
    float reach;   // object cards: shown within this many metres of the hero
};
// The game's buttons as the VR controls send them (story077b / game_pad, the mod's mapping).
inline const char* vrControl(const char* gameButtons) {
    struct Map {
        const char* game;
        const char* vr;
    };
    static constexpr Map maps[] = {
        {"LB+RB", "HOLD RIGHT STICK CLICK / TWO-HAND YANK"},     {"LB+X", "HOLD X + PUNCH"},        {"LB+Y", "HOLD X + Y"},
        {"LB+B", "HOLD X + B"},                {"LB+A", "HOLD X + A"},            {"L3+R3", "HOLD X + RIGHT STICK CLICK"},
        {"X", "TAP X / PUNCH"},                {"MASH X", "TAP X FAST / PUNCH FAST"},          {"Y", "Y (WEB STRIKE)"},
        {"B", "B / STEP YOUR HEAD (DODGE)"},   {"A", "A (JUMP)"},                 {"Y+B", "Y + B"},
        {"LEFT STICK", "LEFT STICK"},          {"AIM", "LOOK AT IT"},             {"RT", "RIGHT TRIGGER"},
    };
    if (!gameButtons)
        return nullptr;
    for (const auto& m : maps)
        if (!std::strcmp(m.game, gameButtons))
            return m.vr;
    return nullptr;
}
inline constexpr Kind kinds[] = {
    // Object prompts (world-anchored).
    {".?AVWebStrikePrompt@@", "Y", "Y (WEB STRIKE)", object, 0, 30},
    // HERO VR 0.0.78 cut15 BOSS Y CARD (CANDIDATE): the public 0.0.77 report never saw a WebStrikePrompt object (0 of
    // its 44 sessions' "first seen" notes, the Sandman one too), so the Y card never had a source. The game's own
    // web strike component (.?AVWebStrikeTarget@@, its RTTI name in the same report's world trace) is the
    // candidate: on the boss himself it is the Y zip to him (bossCardMetres), elsewhere the usual 30 m.
    {".?AVWebStrikeTarget@@", "Y", "Y (WEB STRIKE)", object, 0, 30},
    {".?AVGrabAndThrowable@@", "LB+RB", "HOLD RIGHT STICK CLICK / TWO-HAND YANK", object, 0, 12},
    {".?AVThrowablePickupPoint@@", "LB+RB", "HOLD RIGHT STICK CLICK / TWO-HAND YANK", object, 0, 12},
    {".?AVWebSnatchSpecialThrowable@@", "LB+RB", "HOLD RIGHT STICK CLICK / TWO-HAND YANK", object, 0, 15},
    {".?AVLiftableInteract@@", "LB+RB", "HOLD RIGHT STICK CLICK / TWO-HAND YANK", object, 0, 6},
    // Screen prompts (QTE / mash), below the centre.
    {".?AVButtonMashInteract@@", "MASH X", "TAP X FAST / PUNCH FAST", screen, 8, 0},
    {".?AVCineQTEShotControllerButtonMashAction@@", "MASH X", "TAP X FAST / PUNCH FAST", screen, 8, 0},
    {".?AVCineQTEShotControllerPowerMeterAction@@", "MASH X", "TAP X FAST / PUNCH FAST", screen, 8, 0},
    {".?AVCineQTEShotControllerButtonPressAction@@", "QTE", "PRESS IT NOW: X = PUNCH, Y, B, A", screen, 4, 0},
    {".?AVCineQTEShotControllerStickMotionAction@@", "LEFT STICK", "LEFT STICK", screen, 5, 0},
    {".?AVCineQTEShotControllerThrowArcAction@@", "AIM", "LOOK AT IT + RIGHT STICK CLICK", screen, 6, 0},
    {".?AVCineQTEShotControllerAimAtPointsAction@@", "AIM", "LOOK AT IT", screen, 6, 0},
    {".?AVCineQTEShotControllerTriggerSweetSpotAction@@", "RT", "RIGHT TRIGGER", screen, 6, 0},
    {".?AVCombatQTEOverlayTrigger@@", "QTE", "PRESS IT NOW: X = PUNCH, Y, B, A", screen, 4, 0},
    {".?AVJobEntryCombatQTE@@", "QTE", "PRESS IT NOW: X = PUNCH, Y, B, A", screen, 4, 0},
    {".?AVQTEPromptAction@@", "QTE", "PRESS IT NOW: X = PUNCH, Y, B, A", screen, 4, 0},
    // Watched only (diagnostics: what a prompt of theirs asks is not known).
    {".?AVCineQTEBeginAction@@", "Y", "PRESS Y (THE SCENE ASKS)", cine, 0, 4},
    {".?AVCineQTEButtonPromptLocation@@", "", "", watchOnly, 0, 0},
    {".?AVSimplePrompt@@", "", "", watchOnly, 0, 0},
    {".?AVTutorialContextualPrompt@@", "", "", watchOnly, 0, 0},
    {".?AVStateContextPromptAction@@", "", "", watchOnly, 0, 0},
    {".?AVCineApproachPrompt@@", "LB+RB", "HOLD RIGHT STICK CLICK / TWO-HAND YANK", object, 0, 10},
    {".?AVSandFragmentOffer@@", "", "", watchOnly, 0, 0},
    {".?AVSandFragment@@", "", "", watchOnly, 0, 0},
    // HERO VR 0.0.77 INTERACT CARDS (CANDIDATE kinds; the class names and their bases are CONFIRMED in
    // Spider-Man2.exe's RTTI: VehiclePushable : VehicleBase : Component, WaterPipe : Component,
    // EnviroInteractStandard/Drag/Pillar/Faller/Bot : EnviroInteractBase, InteractLocation*Offer :
    // InteractLocationOffer : HoldOffer : ProximityOffer : Offer, PickupCarryableOffer : HoldOffer, DoorOffer /
    // VentOffer / InWorldInteractOffer / BotInteractOffer / ... : ProximityOffer). A class's most derived kind wins.
    // The truck / tanker to move (the game: "LB + RB (hold)", then the left stick toward you).
    {".?AVVehiclePushable@@", "LB+RB HOLD + LEFT STICK BACK", "HOLD RIGHT STICK CLICK + LEFT STICK BACK", object, 0, 20},
    // The water pipe to tear off (the game: LB + RB).
    {".?AVWaterPipe@@", "LB+RB HOLD", "HOLD RIGHT STICK CLICK", object, 0, 20},
    // Things in the world to yank and throw (the game: LB + RB).
    {".?AVEnviroInteractBase@@", "LB+RB", "HOLD RIGHT STICK CLICK / TWO-HAND YANK", object, 0, 12},
    // Hold-to-use offers: the stand-here interactions, pickups and carries (the game: hold its interact).
    {".?AVInteractLocationOffer@@", "HOLD Y", "HOLD Y", object, 0, 10},
    {".?AVPickupCarryableOffer@@", "HOLD Y", "HOLD Y", object, 0, 10},
    {".?AVHoldOffer@@", "HOLD Y", "HOLD Y", object, 0, 8},
    // Every other offer the game makes near the hero (a launcher, a door, a vent, a person to help): its interact.
    {".?AVInWorldInteractOffer@@", "Y", "Y (HOLD Y: WALKS YOU TO IT)", object, 0, 10},
    {".?AVProximityOffer@@", "Y", "Y (HOLD Y: WALKS YOU TO IT)", object, 0, 6},
    // WAYPOINT candidates (story077g.hpp): never a card.
    // cut3: the game's own POI components (the HUD's markers); story077g reads their POI type.
    {".?AVPointOfInterestPrius@@", "", "", marker, 0, 0},
    {".?AVPointOfInterest@@", "", "", marker, 0, 0},
    {".?AVPOIGamePrius@@", "", "", marker, 0, 0},
    {".?AVPOIPrius@@", "", "", marker, 0, 0},
    {".?AVPOIPriusBase@@", "", "", marker, 0, 0},
    {".?AVSyncedPOIObject@@", "", "", marker, 0, 0},
    {".?AVPOIGame@GUI2@@", "", "", marker, 0, 0}, // the HUD's own POI (GUI2, CONFIRMED in the exe's RTTI)
    {".?AVPOI@GUI2@@", "", "", marker, 0, 0},
    {".?AVWaypoint@@", "", "", watchOnly, 0, 0},
    {".?AVInWorldInteract@@", "", "", watchOnly, 0, 0},
};
inline constexpr uint32_t kindCount = sizeof(kinds) / sizeof(kinds[0]);
static_assert(kindCount <= 64); // promptKindsNoted is a 64-bit mask
// The kind of an exact decorated class name; -1 none.
inline int kindOf(const char* name) {
    if (!name || std::strncmp(name, ".?AV", 4))
        return -1;
    for (uint32_t i = 0; i < kindCount; ++i)
        if (!std::strcmp(kinds[i].cls, name))
            return static_cast<int>(i);
    return -1;
}
inline const Kind* kind(uint32_t i) { return i < kindCount ? &kinds[i] : nullptr; }
// HERO VR 0.0.77 INTERACT CARDS: the interactables' kinds (the truck, the pipe, yanks, offers). One right on the hero
// (an offer the hero entity carries) is never a card: the screen card is for a web strike / throw prompt only.
inline bool interactKind(uint32_t i) {
    const Kind* k = kind(i);
    if (!k || k->where != object)
        return false;
    for (const char* c : {".?AVVehiclePushable@@", ".?AVWaterPipe@@", ".?AVEnviroInteractBase@@",
                          ".?AVInteractLocationOffer@@", ".?AVPickupCarryableOffer@@", ".?AVHoldOffer@@",
                          ".?AVInWorldInteractOffer@@", ".?AVProximityOffer@@", ".?AVCineApproachPrompt@@"})
        if (!std::strcmp(k->cls, c))
            return true;
    return false;
}
// The catch-all kinds (any offer, a path waypoint, an in-world interact): many in a city; capped first.
inline bool catchAllKind(uint32_t i) {
    const Kind* k = kind(i);
    return k && (!std::strcmp(k->cls, ".?AVProximityOffer@@") || !std::strcmp(k->cls, ".?AVWaypoint@@") ||
                 !std::strcmp(k->cls, ".?AVInWorldInteract@@"));
}
// A kind whose game button is LB + RB (the pull / yank); the rest of the interact kinds are the game's Y.
inline bool pullKind(uint32_t i) {
    const Kind* k = kind(i);
    return k && k->where == object && !std::strncmp(k->game, "LB+RB", 5);
}

// One object of a prompt class from the last scan (world_trace::prompts).
struct Seen {
    uint64_t id{};
    Vec3 world{};       // its record's position (y up); placed false: none read
    float age{};        // seconds since it appeared in the scan
    uint32_t kind{};
    bool placed{}, before{}; // before: already there when the cards' scan began (never a screen card)
    bool onBoss{};           // cut15: a web strike kind on the boss (his record, or within bossAnchorMetres of him)
};
// HERO VR 0.0.78 cut15 BOSS Y CARD: the web strike kinds (the Y on a target).
inline bool webStrikeKind(uint32_t i) {
    const Kind* k = kind(i);
    return k && (!std::strcmp(k->cls, ".?AVWebStrikeTarget@@") || !std::strcmp(k->cls, ".?AVWebStrikePrompt@@"));
}
// Sandman's record read 84-130 m from the hero all fight (public 0.0.77 report), past every card reach (30 m) and
// the scan's 60 m pick: a web strike object on the boss is carded out to this, ahead of the other cards.
inline constexpr float bossCardMetres = 200.f;
inline constexpr float bossAnchorMetres = 6.f; // a web strike object this close to the boss's place is on him
inline constexpr float bossCardLift = 2.5f;    // over the boss's place, metres (CANDIDATE: his record is his feet)
// BOSS Y CARD (live.txt boss_y_card): 0 on (the default), 1 off.
enum BossYCard : uint32_t { bossYOn = 0, bossYOff = 1 };
inline constexpr uint32_t maxCards = 3;
struct Card {
    uint32_t kind{};
    bool onObject{};    // world-anchored at `world` (y up, game world); else the screen card
    Vec3 world{};
    uint64_t id{};
};
// CUT5: a cine kind (Where cine) within its reach.
inline bool cineKind(uint32_t i) {
    const Kind* k = kind(i);
    return k && k->where == cine;
}
inline constexpr float cineCardSeconds = 12.f; // the cine card's time on screen after it came within reach
inline constexpr float nearHeroMetres = 2.f; // an object prompt on the hero himself: a screen card instead
inline constexpr float cardLift = .6f;       // over the object, metres
// The cards for this frame: at most two object cards (the nearest within reach) and one screen card (the newest
// QTE / mash object that appeared since the scan began, within its seconds). Nothing when nothing qualifies.
inline uint32_t choose(const Seen* seen, uint32_t n, Vec3 hero, bool heroKnown, Card (&out)[maxCards]) {
    uint32_t count = 0;
    int bestScreen = -1, bestCine = -1;
    float bestScreenAge = 1e9f, bestCineMetres = 1e9f;
    int objects[2] = {-1, -1};
    float objectMetres[2] = {1e9f, 1e9f};
    for (uint32_t i = 0; i < n; ++i) {
        const Seen& s = seen[i];
        const Kind* k = kind(s.kind);
        if (!k || k->where == watchOnly || k->where == marker || !std::isfinite(s.age) || s.age < 0)
            continue;
        bool asScreen = k->where == screen;
        if (k->where == cine) {
            // CUT5: new or old (it is live from the save's load), on the hero himself too: within its reach.
            if (!s.placed || !heroKnown || !finite(s.world))
                continue;
            const float m = length(s.world - hero);
            if (std::isfinite(m) && m <= k->reach && m < bestCineMetres) {
                bestCineMetres = m;
                bestCine = static_cast<int>(i);
            }
            continue;
        }
        if (k->where == object) {
            if (!s.placed || !heroKnown || !finite(s.world))
                continue;
            const float m = length(s.world - hero);
            const bool boss = s.onBoss && webStrikeKind(s.kind); // cut15 BOSS Y CARD
            if (!std::isfinite(m) || m > (boss ? bossCardMetres : k->reach))
                continue;
            if (boss) { // the Y on the boss: the first object card
                objects[1] = objects[0];
                objectMetres[1] = objectMetres[0];
                objects[0] = static_cast<int>(i);
                objectMetres[0] = -1.f;
                continue;
            }
            if (m < nearHeroMetres && interactKind(s.kind))
                continue; // an offer on the hero himself: none
            if (m < nearHeroMetres) {
                // on the hero himself (a web strike's prompt object): only while new, as a screen card
                if (s.before || s.age > 6.f)
                    continue;
                asScreen = true;
            } else {
                if (m < objectMetres[0]) {
                    objects[1] = objects[0];
                    objectMetres[1] = objectMetres[0];
                    objects[0] = static_cast<int>(i);
                    objectMetres[0] = m;
                } else if (m < objectMetres[1]) {
                    objects[1] = static_cast<int>(i);
                    objectMetres[1] = m;
                }
                continue;
            }
        }
        if (asScreen) {
            const float most = k->where == screen ? k->seconds : 6.f;
            if (s.before || s.age > most)
                continue;
            if (s.age < bestScreenAge) {
                bestScreenAge = s.age;
                bestScreen = static_cast<int>(i);
            }
        }
    }
    for (int o : objects)
        if (o >= 0 && count < maxCards) {
            const Seen& s = seen[o];
            out[count++] = Card{s.kind, true, s.world + Vec3{0, s.onBoss ? bossCardLift : cardLift, 0}, s.id};
        }
    // One screen card: a QTE / mash first, else the scene's Y (CUT5).
    if (bestScreen >= 0 && count < maxCards)
        out[count++] = Card{seen[bestScreen].kind, false, {}, seen[bestScreen].id};
    else if (bestCine >= 0 && count < maxCards)
        out[count++] = Card{seen[bestCine].kind, false, seen[bestCine].world, seen[bestCine].id};
    return count;
}
// The card's two lines: the game's button, and the VR control.
inline const char* gameLine(uint32_t k) { return kind(k) ? kind(k)->game : ""; }
inline const char* vrLine(uint32_t k) { return kind(k) ? kind(k)->vr : ""; }

// PROMPT WINDOW (live.txt prompt_window), now a debug view only: 0 off (the default), 1 debug_full (the whole
// presented frame in a window, b8b33ce's view - never on unless asked for).
enum PromptWindow : uint32_t { windowOff = 0, windowDebugFull = 1 };
// PROMPT CARDS (live.txt prompt_cards): 0 on (the default), 1 off.
enum PromptCards : uint32_t { cardsOn = 0, cardsOff = 1 };
// Screen card placement: metres ahead of the head, and below the look (about 11 degrees down).
inline constexpr float screenAhead = 1.5f, screenDown = .3f, screenHeight = .045f;
// Object card text height per metre away (keeps about the same size in view), clamped.
inline constexpr float objectHeightPerMetre = .028f, objectHeightMin = .06f, objectHeightMax = .5f;
inline float objectHeight(float metres) {
    if (!std::isfinite(metres))
        return objectHeightMin;
    const float h = metres * objectHeightPerMetre;
    return h < objectHeightMin ? objectHeightMin : h > objectHeightMax ? objectHeightMax : h;
}
} // namespace spidy::story077d
