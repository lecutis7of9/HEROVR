#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.73 test 3 REP PAGE: the hologram's REP tab - the rep meter (level, title and step, how far to
// the next level) and a log of what changed rep: what happened, when (the session and its time, and the
// date), which hero, and the change (+/-). The log is kept across sessions in the mod's save folder
// (%APPDATA%\Spidy\hero_rep_log.txt, beside hero072_progress.txt) as a ring of the last `most` entries,
// written whole through a temporary file and a replace (game_xr's writeWhole); a file that does not read
// back whole (its header, a line, or its checksum) is ignored and a fresh log is started, never a crash.
//
// Only events that ALREADY change rep are logged (hero072_play.hpp's Play::step, the same adds as
// Reputation::step - nothing here changes how much rep anything gives):
//   downed        CONFIRMED  an enemy's health read at 0 (world_trace::enemyVitals, the BotHealth record at
//                            the CONFIRMED HeroHealth layout), +repDown each, at most 8 an update
//   crimeStopped  CONFIRMED  (mod logic over the CONFIRMED downs) a fight's standing enemies all down
//   crimeFailed   CONFIRMED  (mod logic over the same reads) a fight walked away from with enemies standing
// Never logged, because nothing feeds them yet: fires put out, civilians saved, cars wrecked and the
// player's own defeat (RepEvents' fires, saved, cars, defeated - CANDIDATE, unwired; they give no rep today).
#include "spidy/hero072.hpp"
#include "spidy/hero_modes.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

namespace spidy::rep_log {
enum Kind : uint8_t { downed = 0, crimeStopped, crimeFailed, kinds };
inline constexpr const char* kindKeys[kinds] = {"downed", "stopped", "failed"};
// How sure each hook is (the header's list; the tests keep the two in step).
inline constexpr const char* kindStatus[kinds] = {"CONFIRMED", "CONFIRMED", "CONFIRMED"};
inline constexpr uint32_t most = 200, version = 1;
inline constexpr const char* fileName = "hero_rep_log.txt";

struct Entry {
    uint8_t kind{};      // Kind
    uint8_t hero{};      // hero_modes::Hero
    uint16_t count = 1;  // downed: how many in that update
    int32_t delta{};     // the rep it actually changed (after the 0..repMost clamp)
    int32_t after{};     // the hero's rep after it
    uint32_t session{};  // the session it happened in (1 the first)
    uint32_t seconds{};  // seconds into that session
    int64_t unixTime{};  // when, seconds since 1970 UTC (0 unknown)
};
// "3 BAD GUYS DOWN", "CRIME STOPPED", "CRIME FAILED".
inline std::string what(const Entry& e) {
    if (e.kind == downed)
        return e.count > 1 ? std::to_string(e.count) + " BAD GUYS DOWN" : std::string("BAD GUY DOWN");
    return e.kind == crimeStopped ? "CRIME STOPPED" : "CRIME FAILED";
}
// "+24", "-60", "0".
inline std::string deltaText(int32_t d) {
    return d > 0 ? "+" + std::to_string(d) : std::to_string(d);
}
// When: the date and time where it is known ("OCT 28 14:05", local time), else the session and its clock
// ("S3 12:04").
inline std::string when(const Entry& e) {
    char t[32];
    if (e.unixTime > 0) {
        const std::time_t tt = static_cast<std::time_t>(e.unixTime);
        std::tm lt{};
#ifdef _WIN32
        const bool ok = localtime_s(&lt, &tt) == 0;
#else
        const bool ok = localtime_r(&tt, &lt) != nullptr;
#endif
        static constexpr const char* months[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                                   "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
        if (ok && lt.tm_mon >= 0 && lt.tm_mon < 12) {
            std::snprintf(t, sizeof t, "%s %d %02d:%02d", months[lt.tm_mon], lt.tm_mday, lt.tm_hour, lt.tm_min);
            return t;
        }
    }
    std::snprintf(t, sizeof t, "S%u %u:%02u", e.session, e.seconds / 60, e.seconds % 60);
    return t;
}

// ---- the meter: the hero's level and how far to the next (hero072's rep tuning, never other numbers) ----
struct Meter {
    int32_t points{}, level{}, floor{}, next{}; // level's first point, the next level's (== floor at the top)
    float progress{};                           // 0..1 of the way from floor to next (1 at max)
    uint32_t title{}, step{};                   // repTitleNames index, 1..4 (0 at level 0)
    bool max{};
};
inline Meter meterOf(int32_t points) {
    using namespace hero072;
    Meter m;
    m.points = std::clamp(points, 0, repMost);
    m.level = repLevel(m.points);
    m.floor = m.level * repTuning.levelEvery;
    m.max = m.points >= repMost;
    m.next = m.max ? m.floor : std::min(repMost, m.floor + repTuning.levelEvery);
    m.progress = m.max ? 1.f : std::clamp(static_cast<float>(m.points - m.floor) / static_cast<float>(m.next - m.floor), 0.f, 1.f);
    m.title = repTitleIndex(m.points);
    m.step = repStep(m.points);
    return m;
}
// "HERO 2" ("ROOKIE" at level 0, "LEGEND 4" at the top).
inline std::string rankText(int32_t points) {
    const Meter m = meterOf(points);
    std::string s = hero072::repTitleNames[m.title];
    if (m.step)
        s += " " + std::to_string(m.step);
    return s;
}
// "LV 10   120 TO HERO 3" / "LV 20   MAX".
inline std::string nextText(int32_t points) {
    const Meter m = meterOf(points);
    if (m.max)
        return "LV " + std::to_string(m.level) + "   MAX REP";
    return "LV " + std::to_string(m.level) + "   " + std::to_string(m.next - m.points) + " TO " + rankText(m.next);
}

// ---- each hero's counts, kept with the log (counted from 0.0.73 test 3 on) ----
struct Counts {
    uint32_t downed{}, stopped{}, failed{};
    int64_t gained{}, lost{}; // rep, both positive
};

class Log {
  public:
    // A new session (once a start, after load): entries are stamped with it.
    void begin() {
        ++session_;
        dirty_ = true;
    }
    uint32_t session() const { return session_; }
    void add(Entry e) {
        if (e.kind >= kinds || e.hero >= hero_modes::heroes)
            return;
        e.session = session_;
        ring_[head_] = e;
        head_ = (head_ + 1) % most;
        size_ = std::min(size_ + 1, most);
        ++total_;
        Counts& c = counts_[e.hero];
        if (e.kind == downed)
            c.downed += e.count;
        else if (e.kind == crimeStopped)
            ++c.stopped;
        else
            ++c.failed;
        if (e.delta > 0)
            c.gained += e.delta;
        else
            c.lost -= e.delta;
        dirty_ = true;
    }
    uint32_t size() const { return size_; }
    uint64_t total() const { return total_; } // every add ever (the ring keeps the last `most`)
    // i = 0 the newest.
    const Entry& newest(uint32_t i) const {
        return ring_[(head_ + most * 2 - 1 - std::min(i, most - 1)) % most];
    }
    // How many entries are `hero`'s (hero >= heroes: all of them), and the i-th newest of those.
    uint32_t countFor(uint32_t hero) const {
        if (hero >= hero_modes::heroes)
            return size_;
        uint32_t n = 0;
        for (uint32_t i = 0; i < size_; ++i)
            n += newest(i).hero == hero;
        return n;
    }
    const Entry* nthFor(uint32_t hero, uint32_t k) const {
        for (uint32_t i = 0; i < size_; ++i)
            if (hero >= hero_modes::heroes || newest(i).hero == hero) {
                if (k == 0)
                    return &newest(i);
                --k;
            }
        return nullptr;
    }
    const Counts& counts(uint32_t hero) const { return counts_[std::min(hero, hero_modes::heroes - 1)]; }
    bool dirty() const { return dirty_; }
    void clean() { dirty_ = false; }

    // The file: a header, the session, each hero's counts, the entries oldest first, and an end line with
    // how many and an FNV-1a checksum of everything above it.
    std::string save() const {
        std::string s = "HERO VR REP LOG " + std::to_string(version) + "\n";
        char t[160];
        std::snprintf(t, sizeof t, "session=%u\ntotal=%llu\n", session_, static_cast<unsigned long long>(total_));
        s += t;
        for (uint32_t h = 0; h < hero_modes::heroes; ++h) {
            const Counts& c = counts_[h];
            std::snprintf(t, sizeof t, "counts=%u,%u,%u,%u,%lld,%lld\n", h, c.downed, c.stopped, c.failed,
                          static_cast<long long>(c.gained), static_cast<long long>(c.lost));
            s += t;
        }
        for (uint32_t i = size_; i-- > 0;) {
            const Entry& e = newest(i);
            std::snprintf(t, sizeof t, "e=%u,%u,%u,%d,%d,%u,%u,%lld\n", e.kind, e.hero, e.count, e.delta, e.after, e.session,
                          e.seconds, static_cast<long long>(e.unixTime));
            s += t;
        }
        std::snprintf(t, sizeof t, "end=%u,%08x\n", size_, checksum(s));
        return s + t;
    }
    // Reads a saved log. False (and the log left empty) for anything that does not read back whole; it
    // never throws.
    bool load(const std::string& text) noexcept {
        Log fresh;
        *this = fresh;
        try {
            Log l;
            if (!l.parse(text))
                return false;
            *this = l;
            dirty_ = false;
            return true;
        } catch (...) {
            *this = fresh;
            return false;
        }
    }
    static uint32_t checksum(const std::string& s) {
        uint32_t h = 2166136261u;
        for (const unsigned char c : s)
            h = (h ^ c) * 16777619u;
        return h;
    }

  private:
    bool parse(const std::string& text) {
        const std::string head = "HERO VR REP LOG " + std::to_string(version) + "\n";
        if (text.size() > 256 * 1024 || text.compare(0, head.size(), head) != 0)
            return false;
        const size_t endAt = text.rfind("end=");
        if (endAt == std::string::npos || endAt == 0 || text[endAt - 1] != '\n')
            return false;
        unsigned n = 0, sum = 0;
        if (std::sscanf(text.c_str() + endAt, "end=%u,%x", &n, &sum) != 2 || sum != checksum(text.substr(0, endAt)) || n > most)
            return false;
        size_t at = head.size();
        bool sawSession = false;
        while (at < endAt) {
            const size_t nl = text.find('\n', at);
            if (nl == std::string::npos || nl > endAt)
                return false;
            const std::string line = text.substr(at, nl - at);
            at = nl + 1;
            unsigned a = 0, b = 0, c = 0, d = 0, f = 0, g = 0;
            int x = 0, y = 0;
            long long p = 0, q = 0;
            unsigned long long tot = 0;
            if (std::sscanf(line.c_str(), "session=%u", &a) == 1) {
                session_ = a;
                sawSession = true;
            } else if (std::sscanf(line.c_str(), "total=%llu", &tot) == 1) {
                total_ = tot;
            } else if (std::sscanf(line.c_str(), "counts=%u,%u,%u,%u,%lld,%lld", &a, &b, &c, &d, &p, &q) == 6) {
                if (a >= hero_modes::heroes || p < 0 || q < 0)
                    return false;
                counts_[a] = Counts{b, c, d, p, q};
            } else if (std::sscanf(line.c_str(), "e=%u,%u,%u,%d,%d,%u,%u,%lld", &a, &b, &c, &x, &y, &f, &g, &p) == 8) {
                if (a >= kinds || b >= hero_modes::heroes || c > 0xffff || y < 0 || y > hero072::repMost || size_ >= most)
                    return false;
                Entry e;
                e.kind = static_cast<uint8_t>(a);
                e.hero = static_cast<uint8_t>(b);
                e.count = static_cast<uint16_t>(c);
                e.delta = x;
                e.after = y;
                e.session = f;
                e.seconds = g;
                e.unixTime = p;
                ring_[head_] = e;
                head_ = (head_ + 1) % most;
                ++size_;
            } else {
                return false;
            }
        }
        return sawSession && size_ == n && total_ >= size_;
    }
    Entry ring_[most]{};
    uint32_t head_{}, size_{}, session_{};
    uint64_t total_{};
    Counts counts_[hero_modes::heroes]{};
    bool dirty_{};
};
} // namespace spidy::rep_log
