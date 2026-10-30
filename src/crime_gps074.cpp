// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
// 0.0.74 test 2b CRIME GPS (crime_gps074.hpp).
#include "spidy/crime_gps074.hpp"

#include <algorithm>
#include <cmath>

namespace spidy::crime_gps074 {
namespace {
constexpr float pi = 3.14159265358979f;
bool fine(Vec3 v) { return finite(v); }
float flat(Vec3 a, Vec3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
} // namespace

bool bearingTo(Vec3 from, Vec3 to, float& degrees, float& metres) {
    degrees = 0;
    metres = 0;
    if (!fine(from) || !fine(to))
        return false;
    const float dx = to.x - from.x, dz = to.z - from.z;
    metres = std::hypot(dx, dz);
    float d = std::atan2(dx, -dz) * 180.f / pi; // clockwise from -z
    if (d < 0)
        d += 360.f;
    degrees = std::isfinite(d) ? std::fmod(d, 360.f) : 0.f;
    return std::isfinite(metres);
}

float relative(float bearingDegrees, float headingRadians) {
    if (!std::isfinite(bearingDegrees) || !std::isfinite(headingRadians))
        return 0.f;
    float r = bearingDegrees - headingRadians * 180.f / pi;
    r = std::fmod(r, 360.f);
    if (r > 180.f)
        r -= 360.f;
    if (r < -180.f)
        r += 360.f;
    return r;
}

const char* compass(float degrees) {
    static const char* names[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    if (!std::isfinite(degrees))
        return "?";
    const int i = static_cast<int>(std::floor((degrees + 22.5f) / 45.f)) & 7;
    return names[i];
}

void Tracker::reset() { *this = Tracker{}; }

uint32_t Tracker::liveCount() const {
    uint32_t n = 0;
    for (const Crime& c : live_)
        n += c.id != 0;
    return n;
}

const Crime* Tracker::find(uint32_t id) const {
    if (!id)
        return nullptr;
    for (const Crime& c : live_)
        if (c.id == id)
            return &c;
    return nullptr;
}

const Past& Tracker::past(uint32_t k) const {
    static const Past none{};
    if (k >= pastN_)
        return none;
    return past_[(pastHead_ + mostPast - 1 - k) % mostPast];
}

const LogLine& Tracker::log(uint32_t k) const {
    static const LogLine none{};
    if (k >= logN_)
        return none;
    return log_[(logHead_ + mostLog - 1 - k) % mostLog];
}

void Tracker::addPast(const Crime& c, float now, Outcome o) {
    past_[pastHead_] = Past{c.id, c.at, c.most, now, std::max(0.f, c.last - c.first), o};
    pastHead_ = (pastHead_ + 1) % mostPast;
    pastN_ = std::min(pastN_ + 1, mostPast);
}

void Tracker::addLog(LogKind k, const Crime& c, float now) {
    LogLine l;
    l.kind = k;
    l.id = c.id;
    l.at = now;
    l.thugs = c.most;
    bearingTo(player_, c.at, l.bearing, l.metres);
    log_[logHead_] = l;
    logHead_ = (logHead_ + 1) % mostLog;
    logN_ = std::min(logN_ + 1, mostLog);
}

float Tracker::popupLeft(float now) const {
    if (!popup_ || !std::isfinite(now))
        return 0.f;
    return std::max(0.f, popupSeconds - (now - popupAt_));
}

bool Tracker::accept(float now) {
    const uint32_t id = popup_;
    popup_ = 0;
    return id && track(id, now);
}

bool Tracker::track(uint32_t id, float now) {
    if (!id) {
        tracked_ = 0;
        return true;
    }
    const Crime* c = find(id);
    if (!c)
        return false;
    tracked_ = id;
    addLog(LogKind::tracked, *c, now);
    return true;
}

Event Tracker::step(float now, const Vec3* thugs, uint32_t count, Vec3 player, bool on) {
    Event e;
    if (!std::isfinite(now))
        return e;
    if (fine(player))
        player_ = player;
    if (!on) {
        live_ = {};
        popup_ = tracked_ = 0;
        return e;
    }
    // this scan's groups (greedy: each thug joins the first group whose centre is near)
    struct Group {
        Vec3 sum{};
        uint32_t n{};
        Vec3 centre() const { return sum * (1.f / static_cast<float>(n)); }
    };
    std::array<Group, mostThugs> groups{};
    uint32_t gn = 0;
    count = thugs ? std::min(count, mostThugs) : 0;
    for (uint32_t i = 0; i < count; ++i) {
        const Vec3 p = thugs[i];
        if (!fine(p))
            continue;
        uint32_t g = 0;
        for (; g < gn; ++g)
            if (flat(groups[g].centre(), p) <= groupMetres)
                break;
        if (g == gn) {
            if (gn >= groups.size())
                continue;
            ++gn;
        }
        groups[g].sum += p;
        ++groups[g].n;
    }
    // match each group (big enough) to a live crime, else a new one
    std::array<bool, mostCrimes> seen{};
    for (uint32_t g = 0; g < gn; ++g) {
        if (groups[g].n < groupLeast)
            continue;
        const Vec3 c = groups[g].centre();
        int best = -1;
        float bestD = matchMetres;
        for (uint32_t k = 0; k < mostCrimes; ++k)
            if (live_[k].id && !seen[k]) {
                const float d = flat(live_[k].at, c);
                if (d <= bestD)
                    bestD = d, best = static_cast<int>(k);
            }
        if (best < 0) {
            for (uint32_t k = 0; k < mostCrimes; ++k)
                if (!live_[k].id) {
                    best = static_cast<int>(k);
                    break;
                }
            if (best < 0)
                continue; // full: the far ones wait
            Crime& n = live_[static_cast<size_t>(best)];
            n = Crime{nextId_++, c, groups[g].n, groups[g].n, now, now, false};
            addLog(LogKind::spotted, n, now);
            if (!e.appeared)
                e.appeared = n.id;
            if (!popup_ && !tracked_) {
                popup_ = n.id;
                popupAt_ = now;
            }
        }
        Crime& m = live_[static_cast<size_t>(best)];
        seen[static_cast<size_t>(best)] = true;
        m.at = c;
        m.thugs = groups[g].n;
        m.most = std::max(m.most, groups[g].n);
        m.last = now;
        m.nearSeen = fine(player) && flat(player, c) <= clearedMetres;
    }
    // the ones unseen long enough are over
    for (Crime& c : live_) {
        if (!c.id || now - c.last < goneSeconds)
            continue;
        const Outcome o = c.nearSeen ? Outcome::cleared : Outcome::lost;
        addPast(c, now, o);
        addLog(o == Outcome::cleared ? LogKind::cleared : LogKind::lost, c, now);
        if (!e.ended) {
            e.ended = c.id;
            e.outcome = o;
        }
        if (tracked_ == c.id)
            tracked_ = 0;
        if (popup_ == c.id)
            popup_ = 0;
        c = Crime{};
    }
    if (popup_ && now - popupAt_ > popupSeconds)
        popup_ = 0;
    return e;
}

} // namespace spidy::crime_gps074
