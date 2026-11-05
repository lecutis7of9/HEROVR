// HERO VR 0.0.77 PROMPT CARDS (include/spidy/story077d.hpp, src/prompt_cards.cpp), pure.
#include "spidy/prompt_cards.hpp"
#include "spidy/story077d.hpp"
#include "spidy/world_trace.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace spidy;
using namespace spidy::story077d;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
int kindNamed(const char* cls) { return kindOf(cls); }
} // namespace

int main() {
    // The kinds and the VR controls the player named.
    const int strike = kindNamed(".?AVWebStrikePrompt@@"), throwable = kindNamed(".?AVGrabAndThrowable@@"),
              mash = kindNamed(".?AVButtonMashInteract@@"), simple = kindNamed(".?AVSimplePrompt@@");
    check(strike >= 0 && throwable >= 0 && mash >= 0 && simple >= 0, "kinds found");
    check(kindOf(".?AVNothing@@") < 0 && kindOf(nullptr) < 0 && kindOf("WebStrikePrompt") < 0, "no kind");
    check(!std::strcmp(vrControl("LB+RB"), "HOLD RIGHT STICK CLICK / TWO-HAND YANK"), "LB+RB yank");
    check(!std::strcmp(vrControl("LB+X"), "HOLD X + PUNCH"), "LB+X hold x punch");
    check(!vrControl("ZZ") && !vrControl(nullptr), "unknown button");
    check(!std::strcmp(vrLine(throwable), "HOLD RIGHT STICK CLICK / TWO-HAND YANK") && !std::strcmp(gameLine(throwable), "LB+RB"), "throw card");
    check(!std::strcmp(vrLine(strike), vrControl("Y")), "web strike card");
    check(!std::strcmp(vrLine(mash), vrControl("MASH X")), "mash card");
    for (uint32_t k = 0; k < kindCount; ++k)
        if (kinds[k].where == object || kinds[k].where == screen) {
            check(kinds[k].game[0] && kinds[k].vr[0], "every card has both lines");
            for (const char* c = kinds[k].vr; *c; ++c)
                check(!(*c >= 'a' && *c <= 'z'), "capitals only (the stroke font)");
        }

    Card out[maxCards];
    const Vec3 hero{100, 0, 100};
    // Nothing: no cards.
    check(choose(nullptr, 0, hero, true, out) == 0, "none: no card");
    // Debris 8 m away: an object card over it; 40 m away: none.
    Seen s[6]{};
    s[0] = Seen{1, {108, 0, 100}, 3.f, static_cast<uint32_t>(throwable), true, true};
    check(choose(s, 1, hero, true, out) == 1 && out[0].onObject && std::fabs(out[0].world.y - cardLift) < 1e-4f,
          "debris near: an object card above it");
    s[0].world = {140, 0, 100};
    check(choose(s, 1, hero, true, out) == 0, "debris far: none");
    check(choose(s, 1, hero, false, out) == 0, "hero unknown: none");
    s[0].world = {108, 0, 100};
    s[0].placed = false;
    check(choose(s, 1, hero, true, out) == 0, "no position: none");
    // A mash that was there before the scan: never; a new one: a screen card for its seconds only.
    s[1] = Seen{2, {}, 1.f, static_cast<uint32_t>(mash), false, true};
    check(choose(s + 1, 1, hero, true, out) == 0, "a preexisting QTE object: none");
    s[1].before = false;
    check(choose(s + 1, 1, hero, true, out) == 1 && !out[0].onObject, "a new QTE: a screen card");
    s[1].age = 30.f;
    check(choose(s + 1, 1, hero, true, out) == 0, "an old QTE: none");
    s[1].age = NAN;
    check(choose(s + 1, 1, hero, true, out) == 0, "NaN age: none");
    // A watched-only class: never a card.
    s[2] = Seen{3, {101, 0, 100}, .1f, static_cast<uint32_t>(simple), true, false};
    check(choose(s + 2, 1, hero, true, out) == 0, "watched only: none");
    // The web strike prompt on the hero himself: a screen card while new, none when old or preexisting.
    s[3] = Seen{4, {100.5f, 0, 100}, 1.f, static_cast<uint32_t>(strike), true, false};
    check(choose(s + 3, 1, hero, true, out) == 1 && !out[0].onObject, "web strike on the hero: a screen card");
    s[3].before = true;
    check(choose(s + 3, 1, hero, true, out) == 0, "a preexisting one on the hero: none");
    // At most two object cards (the nearest) and one screen card.
    Seen many[5]{};
    for (int i = 0; i < 4; ++i)
        many[i] = Seen{static_cast<uint64_t>(10 + i), {100.f + 3 + i, 0, 100}, 2.f, static_cast<uint32_t>(throwable), true, true};
    many[4] = Seen{20, {}, .5f, static_cast<uint32_t>(mash), false, false};
    const uint32_t n = choose(many, 5, hero, true, out);
    check(n == 3 && out[0].id == 10 && out[1].id == 11 && out[2].id == 20, "two nearest objects + the screen card");
    // Bad kinds / NaN positions: no crash, none.
    Seen bad[2]{};
    bad[0] = Seen{1, {NAN, 0, 0}, 1.f, static_cast<uint32_t>(throwable), true, false};
    bad[1] = Seen{2, {}, 1.f, 999u, true, false};
    check(choose(bad, 2, hero, true, out) == 0, "bad: none");
    check(objectHeight(NAN) == objectHeightMin && objectHeight(1000.f) == objectHeightMax, "heights clamped");
    // The card's vertices: some for a card; none for nonsense.
    std::vector<Vertex> v;
    check(prompt_cards::append(v, "GAME: LB+RB", "VR: BOTH TRIGGERS / YANK", {0, 0, -1.5f}, {1, 0, 0}, {0, 1, 0}, .045f, 1.f) > 30,
          "a card draws");
    bool sane = true;
    for (const auto& x : v)
        sane &= finite(x.position) && std::fabs(x.position.x) < .8f && std::fabs(x.position.y) < .3f;
    check(sane, "a card is small (not a screen)");
    v.clear();
    check(prompt_cards::append(v, "A", "B", {NAN, 0, 0}, {1, 0, 0}, {0, 1, 0}, .045f, 1.f) == 0 && v.empty(), "NaN: none");
    check(prompt_cards::append(v, "", "", {0, 0, -1}, {1, 0, 0}, {0, 1, 0}, .045f, 1.f) == 0, "empty: none");
    check(prompt_cards::append(v, "A", "B", {0, 0, -1}, {1, 0, 0}, {0, 1, 0}, 0.f, 1.f) == 0, "no height: none");
    // HERO VR 0.0.77 INTERACT CARDS: the truck, the pipe, a yank, the offers.
    const int truck = kindNamed(".?AVVehiclePushable@@"), pipe = kindNamed(".?AVWaterPipe@@"),
              enviro = kindNamed(".?AVEnviroInteractBase@@"), hold = kindNamed(".?AVHoldOffer@@"),
              carry = kindNamed(".?AVPickupCarryableOffer@@"), offer = kindNamed(".?AVProximityOffer@@"),
              poiK = kindNamed(".?AVPointOfInterestPrius@@"), mission = kindNamed(".?AVPOIGamePrius@@");
    check(truck >= 0 && pipe >= 0 && enviro >= 0 && hold >= 0 && carry >= 0 && offer >= 0 && poiK >= 0 && mission >= 0,
          "interact kinds found");
    check(pullKind(truck) && pullKind(pipe) && pullKind(enviro) && !pullKind(hold) && !pullKind(offer), "pull kinds");
    check(interactKind(truck) && interactKind(carry) && interactKind(offer) && !interactKind(throwable) &&
              !interactKind(poiK),
          "interact kinds");
    check(!std::strcmp(vrLine(truck), "HOLD RIGHT STICK CLICK + LEFT STICK BACK"), "truck card");
    check(!std::strcmp(vrLine(pipe), "HOLD RIGHT STICK CLICK") && !std::strcmp(vrLine(hold), "HOLD Y"), "pipe / hold");
    check(kinds[poiK].where == marker && kinds[mission].where == marker, "markers");
    Seen t[3]{};
    t[0] = Seen{31, {115, 0, 100}, 30.f, static_cast<uint32_t>(truck), true, true}; // 15 m, there before
    check(choose(t, 1, hero, true, out) == 1 && out[0].onObject && out[0].id == 31, "truck 15 m: an object card");
    t[0].world = {130, 0, 100};
    check(choose(t, 1, hero, true, out) == 0, "truck 30 m: none");
    t[1] = Seen{32, {100.5f, 0, 100}, .5f, static_cast<uint32_t>(offer), true, false}; // on the hero, new
    check(choose(t + 1, 1, hero, true, out) == 0, "an offer on the hero: no screen card");
    t[2] = Seen{33, {100, 0, 100}, 1.f, static_cast<uint32_t>(poiK), true, false};
    check(choose(t + 2, 1, hero, true, out) == 0, "a marker: never a card");
    for (uint32_t k = 0; k < kindCount; ++k)
        check(std::strlen(kinds[k].game) + 6 < 96 && std::strlen(kinds[k].vr) + 4 < 96, "lines fit the buffers");
    check(kindCount <= 64, "kinds fit the 64-bit noted mask");
    // HERO VR 0.0.78 cut15 BOSS Y CARD: the web strike target on the boss is carded out to 200 m, first.
    const int target = kindOf(".?AVWebStrikeTarget@@");
    check(target >= 0 && kinds[target].where == object && !std::strcmp(vrLine(target), vrControl("Y")) &&
              webStrikeKind(static_cast<uint32_t>(target)) && webStrikeKind(static_cast<uint32_t>(strike)) &&
              !webStrikeKind(static_cast<uint32_t>(truck)),
          "web strike target kind");
    Seen b[2]{};
    b[0] = Seen{41, {200, 0, 100}, 3.f, static_cast<uint32_t>(target), true, true}; // 100 m (Sandman: 84-130 m)
    check(choose(b, 1, hero, true, out) == 0, "web strike target 100 m, not on the boss: none");
    b[0].onBoss = true;
    b[1] = Seen{42, {110, 0, 100}, 3.f, static_cast<uint32_t>(truck), true, true}; // 10 m
    check(choose(b, 2, hero, true, out) == 2 && out[0].id == 41 && out[0].onObject &&
              std::fabs(out[0].world.y - bossCardLift) < 1e-4f && out[1].id == 42,
          "on the boss 100 m: the first card, lifted");
    b[0].world = {320, 0, 100}; // 220 m
    check(choose(b, 1, hero, true, out) == 0, "on the boss past 200 m: none");
    check(bossYOn == 0 && bossCardMetres >= 200.f, "boss Y card: on by default, 200 m");
    // The settings: the full frame off by default, the cards on.
    check(windowOff == 0 && cardsOn == 0, "defaults: no full frame, cards on");
    if (failures)
        std::printf("%d failure(s)\n", failures);
    else
        std::printf("story077d: all passed\n");
    return failures ? 1 : 0;
}
