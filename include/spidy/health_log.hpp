#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 37: the health the ring's RED half shows, logged so a session confirms it.
// The reading (game_player.cpp): the hero's health component found by its vtable 0x541edd0
// (the one whose RTTI names .?AVHeroHealth@@, static, October 24), hit points at +0xd0,
// their most at +0xa0. CANDIDATE: not yet seen live in Spider-Man 2. This record says which:
//   status 0  no component found (the ring shows N/A)
//   status 1  found, but the values are not health (outside 0..most, or most not > 0): N/A
//   status 2  valid, never changed yet (take a hit or heal to confirm)
//   status 3  CONFIRMED live: valid, and it fell on a hit or rose on a heal
// It samples once a second, and at every change, every heal press and 1 s after one, into a
// 32-entry ring the launcher prints (tools/update37_records.py).
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::health_log {
enum Event : uint32_t { tick = 0, fell = 1, rose = 2, healPress = 3, afterHeal = 4, invalid = 5 };
inline constexpr const char* eventNames[] = {"tick", "hit (fell)", "rose", "heal pressed", "1 s after heal",
                                             "invalid"};
inline bool valid(float hp, float most) {
    return std::isfinite(hp) && std::isfinite(most) && most > 0 && most < 1e6f && hp >= 0 && hp <= most * 1.001f;
}
struct Sample {
    float seconds{}, hp{}, most{};
    uint32_t event{};
};
inline constexpr uint32_t samples = 32;
struct Data {
    uint32_t magic = 0x48544c48, version = 1, bytes = 616, status{}; // HLTH
    uint64_t reads{}, found{}, invalidReads{}, falls{}, rises{}, healPresses{}, healsThatRose{};
    float hp{}, most{}, share{}, seconds{};
    uint32_t next{}, pad{}; // Update 38: pad is this frame's status (status_now)
    Sample ring[samples]{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 616);
// One frame's reading. found: a component was found; hp/most NaN where unread.
class Log {
  public:
    void update(Data& d, float dt, bool found, float hp, float most, bool healPressed) {
        if (!std::isfinite(dt) || dt < 0)
            dt = 0;
        clock_ += std::min(dt, 1.f);
        d.seconds = clock_;
        ++d.reads;
        d.found += found;
        const bool ok = found && valid(hp, most);
        if (found && !ok)
            ++d.invalidReads;
        d.hp = hp;
        d.most = most;
        d.share = ok ? std::min(hp / most, 1.f) : std::nanf("");
        if (healPressed) {
            ++d.healPresses;
            push(d, hp, most, healPress);
            afterHeal_ = 1.f;
            healHp_ = ok ? hp : -1.f;
        }
        if (ok && last_ >= 0) {
            const float step = .002f * most;
            if (hp < last_ - step) {
                ++d.falls;
                push(d, hp, most, fell);
                changed_ = true;
            } else if (hp > last_ + step) {
                ++d.rises;
                push(d, hp, most, rose);
                changed_ = true;
            }
        }
        if (afterHeal_ > 0 && (afterHeal_ -= dt) <= 0) {
            push(d, hp, most, afterHeal);
            if (ok && healHp_ >= 0 && hp > healHp_ + .002f * most)
                ++d.healsThatRose;
        }
        if (found && !ok && wasOk_)
            push(d, hp, most, invalid);
        wasOk_ = ok;
        last_ = ok ? hp : -1.f;
        if ((tickLeft_ -= dt) <= 0) {
            tickLeft_ = 1.f;
            push(d, hp, most, tick);
        }
        // Update 38: the status is the best seen this session (run372 ended "no component"
        // after 33 falls: the last frame, in a menu or the death cam, had none); this
        // frame's own is in `pad` (status_now).
        const uint32_t now = !found ? 0u : !ok ? 1u : changed_ ? 3u : 2u;
        best_ = std::max(best_, now);
        d.status = best_;
        d.pad = now;
    }

  private:
    void push(Data& d, float hp, float most, uint32_t e) {
        d.ring[d.next % samples] = {clock_, hp, most, e};
        d.next = (d.next + 1) % samples;
    }
    float clock_{}, last_ = -1.f, tickLeft_{}, afterHeal_{}, healHp_ = -1.f;
    bool changed_{}, wasOk_{};
    uint32_t best_{}; // Update 38
};
} // namespace spidy::health_log
