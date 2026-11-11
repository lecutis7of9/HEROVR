// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
// ULTIMATE ALLIANCE EXPANSION CHARACTERS (ua_cast.hpp).
#include "spidy/ua_cast.hpp"

#include "spidy/overlay_text.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace spidy::ua_cast {
namespace {

bool fin(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

// The first `n` words of `s` (split on spaces) and what follows them (one space dropped), or false.
bool words(const std::string& s, size_t n, std::vector<std::string>& out, std::string& rest) {
    out.clear();
    size_t i = 0;
    while (out.size() < n) {
        while (i < s.size() && s[i] == ' ')
            ++i;
        if (i >= s.size())
            return false;
        size_t j = i;
        while (j < s.size() && s[j] != ' ')
            ++j;
        out.push_back(s.substr(i, j - i));
        i = j;
    }
    rest = i < s.size() ? s.substr(i + 1) : std::string();
    while (!rest.empty() && (rest.back() == '\r' || rest.back() == ' '))
        rest.pop_back();
    return true;
}

bool toInt(const std::string& s, int& v) {
    if (s.empty())
        return false;
    char* e = nullptr;
    const long r = std::strtol(s.c_str(), &e, 10);
    if (*e || r < -1000000 || r > 1000000)
        return false;
    v = static_cast<int>(r);
    return true;
}
bool toFloat(const std::string& s, float& v) {
    if (s.empty())
        return false;
    char* e = nullptr;
    v = std::strtof(s.c_str(), &e);
    return !*e && std::isfinite(v) && std::abs(v) < 1e6f;
}

std::vector<std::string> lines(const std::string& text) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i <= text.size()) {
        size_t j = text.find('\n', i);
        if (j == std::string::npos)
            j = text.size();
        std::string l = text.substr(i, j - i);
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        out.push_back(l);
        i = j + 1;
    }
    return out;
}

std::string upper(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

const Vec3 light = normalized(Vec3{.3f, 1.f, .25f});

// "always" / "never" / "hero:a,b" / "not:a,b" into its kind and list; false: none of these.
bool condOf(const std::string& c, Cond& kind, std::vector<std::string>& heroes) {
    heroes.clear();
    if (c == "always")
        return kind = Cond::always, true;
    if (c == "never")
        return kind = Cond::never, true;
    const bool is = c.rfind("hero:", 0) == 0;
    if (!is && c.rfind("not:", 0) != 0)
        return false;
    kind = is ? Cond::heroIs : Cond::heroNot;
    const std::string list = c.substr(is ? 5 : 4);
    size_t i = 0;
    while (i <= list.size()) {
        size_t j = list.find(',', i);
        if (j == std::string::npos)
            j = list.size();
        if (j > i)
            heroes.push_back(list.substr(i, j - i));
        i = j + 1;
    }
    return true;
}
bool passes(Cond kind, const std::vector<std::string>& heroes, const std::string& name) {
    const bool listed = std::find(heroes.begin(), heroes.end(), name) != heroes.end();
    return kind == Cond::always || (kind == Cond::heroIs && listed) || (kind == Cond::heroNot && !listed);
}

} // namespace

// ------------------------------------------------------------------------------------------- cast.txt
bool parseCast(const std::string& text, Cast& out, std::string& error, uint32_t triangles) {
    out = {};
    error.clear();
    const auto ls = lines(text);
    if (ls.empty() || ls[0] != castHeader)
        return error = std::string("not a cast file (no ") + castHeader + ")", false;
    std::vector<std::string> w;
    std::string rest;
    for (size_t n = 1; n < ls.size(); ++n) {
        const std::string& l = ls[n];
        if (l.empty() || l[0] == '#')
            continue;
        const std::string at = "line " + std::to_string(n + 1) + ": ";
        if (l.rfind("look ", 0) == 0) {
            int first, per, frames, turns;
            float loop;
            if (!words(l, 7, w, rest) || !toInt(w[2], first) || !toInt(w[3], per) || !toInt(w[4], frames) || !toFloat(w[5], loop) ||
                !toInt(w[6], turns) || first < 0 || per <= 0 || frames <= 0 || frames > static_cast<int>(mostFrames) || !(loop > 0))
                return out = {}, error = at + "a bad look", false;
            const uint64_t end = uint64_t(first) + uint64_t(per) * uint64_t(frames);
            if (triangles && end > triangles)
                return out = {}, error = at + "look " + w[1] + " past the model's triangles", false;
            if (out.looks.size() >= mostLooks)
                return out = {}, error = at + "too many looks", false;
            out.looks.push_back({w[1], uint32_t(first), uint32_t(per), uint32_t(frames), loop, turns != 0});
        } else if (l.rfind("npc ", 0) == 0) {
            Npc c;
            float x, y, z, yaw;
            if (!words(l, 9, w, rest) || !toFloat(w[3], x) || !toFloat(w[4], y) || !toFloat(w[5], z) || !toFloat(w[6], yaw))
                return out = {}, error = at + "a bad npc", false;
            const auto look = std::find_if(out.looks.begin(), out.looks.end(), [&](const Look& k) { return k.name == w[2]; });
            if (look == out.looks.end())
                return out = {}, error = at + "npc " + w[1] + ": no look " + w[2], false;
            if (out.npcs.size() >= mostNpcs)
                return out = {}, error = at + "too many characters", false;
            c.id = w[1];
            c.look = static_cast<uint32_t>(look - out.looks.begin());
            c.at = {x, y, z};
            c.yawDeg = yaw;
            c.group = w[7] == "-" ? "" : w[7];
            c.talk = w[8] == "-" ? "" : w[8];
            c.label = rest;
            if (!c.group.empty() && std::find(out.pairs.begin(), out.pairs.end(), c.group) == out.pairs.end())
                out.pairs.push_back(c.group);
            out.npcs.push_back(std::move(c));
        } else {
            return out = {}, error = at + "unknown line", false;
        }
    }
    std::sort(out.pairs.begin(), out.pairs.end());
    if (out.npcs.empty())
        return out = {}, error = "no characters", false;
    return true;
}

// ------------------------------------------------------------------------------------------- talk.txt
int Talk::find(const std::string& name) const {
    for (size_t i = 0; i < convs.size(); ++i)
        if (convs[i].name == name)
            return static_cast<int>(i);
    return -1;
}
std::string Talk::nameOf(const std::string& token) const {
    for (const auto& s : speakers)
        if (s.first == token)
            return s.second;
    return token;
}

bool parseTalk(const std::string& text, Talk& out, std::string& error) {
    out = {};
    error.clear();
    const auto ls = lines(text);
    if (ls.empty() || ls[0] != talkHeader)
        return error = std::string("not a talk file (no ") + talkHeader + ")", false;
    std::vector<std::string> w;
    std::string rest;
    Conv* cur = nullptr;
    size_t lineCount = 0;
    for (size_t n = 1; n < ls.size(); ++n) {
        const std::string& l = ls[n];
        if (l.empty() || l[0] == '#')
            continue;
        const std::string at = "line " + std::to_string(n + 1) + ": ";
        if (l.rfind("speaker ", 0) == 0) {
            if (!words(l, 2, w, rest))
                return out = {}, error = at + "a bad speaker", false;
            out.speakers.push_back({w[1], rest});
        } else if (l.rfind("conv ", 0) == 0) {
            if (!words(l, 2, w, rest) || out.convs.size() >= mostConvs)
                return out = {}, error = at + "a bad conv", false;
            out.convs.push_back({});
            cur = &out.convs.back();
            cur->name = w[1];
        } else if (!cur) {
            return out = {}, error = at + "before any conv", false;
        } else if (l.rfind("start ", 0) == 0) {
            int line, once;
            if (!words(l, 4, w, rest) || !toInt(w[1], line) || !toInt(w[2], once) || line < 0)
                return out = {}, error = at + "a bad start", false;
            Start s;
            s.line = line;
            s.once = once != 0;
            if (!condOf(w[3], s.cond, s.heroes))
                return out = {}, error = at + "a bad condition", false;
            cur->starts.push_back(std::move(s));
        } else if (l.rfind("line ", 0) == 0) {
            int i, voice;
            float secs;
            if (!words(l, 5, w, rest) || !toInt(w[1], i) || !toInt(w[3], voice) || !toFloat(w[4], secs) || secs < 0 ||
                i != static_cast<int>(cur->lines.size()) || voice < -1 || ++lineCount > mostLines)
                return out = {}, error = at + "a bad line", false;
            cur->lines.push_back({w[2], voice, std::min(secs, 30.f), rest});
        } else if (l.rfind("resp ", 0) == 0) {
            int line, to, end, needs;
            if (!words(l, 7, w, rest) || !toInt(w[1], line) || !toInt(w[2], to) || !toInt(w[3], end) || !toInt(w[5], needs) ||
                line < 0 || to < -1 || needs < 0)
                return out = {}, error = at + "a bad resp", false;
            Resp r;
            r.line = line;
            r.to = to;
            r.end = end != 0;
            r.needs = needs;
            if (!condOf(w[6], r.cond, r.heroes))
                return out = {}, error = at + "a bad response condition", false;
            r.text = rest;
            const std::string& a = w[4];
            if (a == "none")
                r.act = Act::none;
            else if (a == "missions")
                r.act = Act::missions;
            else if (a.rfind("flag:", 0) == 0 && toInt(a.substr(5), r.flag))
                r.act = Act::flag;
            else
                return out = {}, error = at + "a bad action", false;
            cur->resps.push_back(std::move(r));
        } else {
            return out = {}, error = at + "unknown line", false;
        }
    }
    for (const Conv& c : out.convs) {
        const int n = static_cast<int>(c.lines.size());
        std::string bad;
        for (const Start& s : c.starts)
            if (s.line >= n)
                bad = c.name + ": a start past its lines";
        for (const Resp& r : c.resps)
            if (r.line >= n || r.to >= n)
                bad = c.name + ": a response past its lines";
        if (!bad.empty()) { // the message made before `out` (and `c` with it) is cleared
            error = bad;
            out = {};
            return false;
        }
    }
    if (out.convs.empty())
        return out = {}, error = "no conversations", false;
    return true;
}

bool isBlank(const std::string& raw) { return raw.empty() || raw == "%BLANK%"; }
bool isPlayerSaid(const std::string& raw) { return raw.rfind("%PLAYER%:", 0) == 0; }
std::string shownText(const std::string& raw) {
    if (isBlank(raw))
        return "";
    std::string s = raw;
    if (isPlayerSaid(s))
        s = s.substr(9);
    else if (s.size() > 2 && s.front() == '%' && s.back() == '%')
        s = s.substr(1, s.size() - 2); // %YES% -> YES
    const size_t a = s.find_first_not_of(' ');
    return a == std::string::npos ? "" : s.substr(a);
}

std::vector<std::string> wrap(const std::string& text, float height, float width, size_t most) {
    std::vector<std::string> out;
    std::string row;
    size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && text[i] == ' ')
            ++i;
        size_t j = i;
        while (j < text.size() && text[j] != ' ')
            ++j;
        if (j == i)
            break;
        const std::string word = text.substr(i, j - i);
        const std::string trial = row.empty() ? word : row + " " + word;
        if (!row.empty() && textWidth(trial, height) > width) {
            out.push_back(row);
            row = word;
        } else
            row = trial;
        i = j;
    }
    if (!row.empty())
        out.push_back(row);
    if (most && out.size() > most) {
        out.resize(most);
        std::string& last = out.back();
        while (!last.empty() && textWidth(last + "...", height) > width) {
            const size_t cut = last.find_last_of(' ');
            last.resize(cut == std::string::npos ? 0 : cut);
        }
        last += "...";
    }
    return out;
}

// ------------------------------------------------------------------------------------------- who is there
std::vector<uint32_t> shownFor(const Cast& cast, uint32_t pair) {
    std::vector<uint32_t> out;
    const std::string want = pair < cast.pairs.size() ? cast.pairs[pair] : std::string("\x01");
    for (uint32_t i = 0; i < cast.npcs.size(); ++i)
        if (cast.npcs[i].group.empty() || cast.npcs[i].group == want)
            out.push_back(i);
    return out;
}

int pick(const Cast& cast, const std::vector<uint32_t>& shown, Vec3 feet, Vec3 eye, Vec3 look, bool handOn, Vec3 hand, Vec3 handDir) {
    if (!fin(feet) || !fin(eye) || !fin(look))
        return -1;
    const float cosLimit = std::cos(aimDegrees * 3.14159265f / 180.f);
    int best = -1;
    float bestCos = cosLimit;
    auto aim = [&](Vec3 from, Vec3 dir, Vec3 chest) {
        const float dl = length(dir);
        const Vec3 d = chest - from;
        const float l = length(d);
        if (!(dl > 1e-4f) || !(l > 1e-3f))
            return -2.f;
        return dot(d, dir) / (l * dl);
    };
    for (const uint32_t i : shown) {
        if (i >= cast.npcs.size() || cast.npcs[i].talk.empty())
            continue;
        const Npc& c = cast.npcs[i];
        const float dx = c.at.x - feet.x, dz = c.at.z - feet.z;
        if (std::sqrt(dx * dx + dz * dz) > talkReach || std::abs(c.at.y - feet.y) > 1.5f)
            continue;
        const bool seated = c.look < cast.looks.size() && !cast.looks[c.look].turns;
        const Vec3 chest = c.at + Vec3{0, seated ? .9f : chestHeight, 0};
        float k = aim(eye, look, chest);
        if (handOn && fin(hand) && fin(handDir))
            k = std::max(k, aim(hand, handDir, chest));
        if (k > bestCos)
            bestCos = k, best = static_cast<int>(i);
    }
    return best;
}

// ------------------------------------------------------------------------------------------- a conversation
bool Chat::begin(const Talk& talk, int conv, const std::string& heroName) {
    conv_ = -1;
    if (conv < 0 || static_cast<size_t>(conv) >= talk.convs.size())
        return false;
    const Conv& c = talk.convs[static_cast<size_t>(conv)];
    for (size_t s = 0; s < c.starts.size(); ++s) {
        const Start& st = c.starts[s];
        const std::pair<int, int> key{conv, static_cast<int>(s)};
        if (!passes(st.cond, st.heroes, heroName) || (st.once && std::find(used_.begin(), used_.end(), key) != used_.end()))
            continue;
        hero_ = heroName;
        if (st.once)
            used_.push_back(key);
        talk_ = &talk;
        conv_ = conv;
        ChatOut ignored;
        enter(st.line, ignored);
        return conv_ >= 0;
    }
    return false;
}

bool Chat::flagSet(int flag) const { return std::find(flags_.begin(), flags_.end(), std::pair<int, int>{conv_, flag}) != flags_.end(); }

void Chat::enter(int line, ChatOut& out) {
    const Conv& c = talk_->convs[static_cast<size_t>(conv_)];
    if (line < 0 || static_cast<size_t>(line) >= c.lines.size()) {
        out.ended = true;
        out.stopVoice = true;
        conv_ = -1;
        return;
    }
    line_ = line;
    clock_ = 0;
    hot_ = 0;
    visible_.clear();
    for (size_t r = 0; r < c.resps.size(); ++r)
        if (c.resps[r].line == line && (c.resps[r].needs == 0 || flagSet(c.resps[r].needs)) &&
            passes(c.resps[r].cond, c.resps[r].heroes, hero_))
            visible_.push_back(static_cast<int>(r));
    const Line& l = c.lines[static_cast<size_t>(line)];
    seconds_ = l.seconds;
    phase_ = l.text.empty() ? Phase::choosing : Phase::saying;
    if (phase_ == Phase::choosing && choices().empty()) // a header with nothing to choose: go on (or end)
        phase_ = Phase::saying, seconds_ = 0;
    if (phase_ == Phase::saying && l.voice >= 0)
        out.voice = l.voice;
}

void Chat::follow(int r, ChatOut& out) {
    const Resp& resp = talk_->convs[static_cast<size_t>(conv_)].resps[static_cast<size_t>(r)];
    if (resp.act == Act::flag && !flagSet(resp.flag))
        flags_.push_back({conv_, resp.flag});
    if (resp.act == Act::missions) {
        out.missions = true;
        out.ended = true;
        out.stopVoice = true;
        conv_ = -1;
        return;
    }
    if (resp.end || resp.to < 0) {
        out.ended = true;
        out.stopVoice = true;
        conv_ = -1;
        return;
    }
    enter(resp.to, out);
}

ChatOut Chat::step(const ChatIn& in) {
    ChatOut out;
    if (conv_ < 0 || !talk_)
        return out;
    if (in.quit) {
        out.ended = out.stopVoice = true;
        conv_ = -1;
        return out;
    }
    const float dt = std::isfinite(in.dt) && in.dt > 0 && in.dt < 1 ? in.dt : 0.f;
    clock_ += dt;
    const Conv& c = talk_->convs[static_cast<size_t>(conv_)];
    // the shown choices (non-blank responses) and the first blank one (go on)
    std::vector<int> shown;
    int blank = -1;
    for (const int r : visible_)
        if (isBlank(c.resps[static_cast<size_t>(r)].text)) {
            if (blank < 0)
                blank = r;
        } else
            shown.push_back(r);
    switch (phase_) {
    case Phase::saying:
        if (in.next || clock_ >= seconds_ + .35f) {
            if (in.next && clock_ < seconds_)
                out.stopVoice = true;
            if (!shown.empty()) {
                phase_ = Phase::choosing;
                hot_ = 0;
            } else if (blank >= 0)
                follow(blank, out);
            else {
                out.ended = out.stopVoice = true;
                conv_ = -1;
            }
        }
        break;
    case Phase::choosing: {
        const int n = static_cast<int>(shown.size());
        if (n == 0) {
            out.ended = true;
            conv_ = -1;
            break;
        }
        if (in.point >= 0 && in.point < n)
            hot_ = in.point;
        else if (in.move)
            hot_ = ((hot_ + in.move) % n + n) % n;
        hot_ = std::clamp(hot_, 0, n - 1);
        if (in.next) {
            const int r = shown[static_cast<size_t>(hot_)];
            const std::string& raw = c.resps[static_cast<size_t>(r)].text;
            if (isPlayerSaid(raw)) { // Spider-Man says it first
                reply_ = shownText(raw);
                after_ = r;
                phase_ = Phase::replying;
                clock_ = 0;
                seconds_ = std::min(9.f, 1.2f + .055f * static_cast<float>(reply_.size()));
            } else
                follow(r, out);
        }
        break;
    }
    case Phase::replying:
        if (in.next || clock_ >= seconds_)
            follow(after_, out);
        break;
    }
    return out;
}

std::string Chat::speaker() const {
    if (conv_ < 0)
        return "";
    if (phase_ == Phase::replying)
        return talk_->nameOf("PLAYER");
    if (phase_ == Phase::choosing)
        return "";
    const Line& l = talk_->convs[static_cast<size_t>(conv_)].lines[static_cast<size_t>(line_)];
    return l.speaker == "MENU" ? "" : talk_->nameOf(l.speaker);
}
std::string Chat::text() const {
    if (conv_ < 0)
        return "";
    if (phase_ == Phase::replying)
        return reply_;
    return talk_->convs[static_cast<size_t>(conv_)].lines[static_cast<size_t>(line_)].text;
}
int Chat::voiceNow() const {
    if (conv_ < 0 || phase_ != Phase::saying)
        return -1;
    return talk_->convs[static_cast<size_t>(conv_)].lines[static_cast<size_t>(line_)].voice;
}
std::vector<std::string> Chat::choices() const {
    std::vector<std::string> out;
    if (conv_ < 0)
        return out;
    const Conv& c = talk_->convs[static_cast<size_t>(conv_)];
    for (const int r : visible_)
        if (!isBlank(c.resps[static_cast<size_t>(r)].text))
            out.push_back(shownText(c.resps[static_cast<size_t>(r)].text));
    return out;
}

// ------------------------------------------------------------------------------------------- turning
float turnToward(float yawDeg, Vec3 at, Vec3 target, float dt) {
    const float dx = target.x - at.x, dz = target.z - at.z;
    if (!std::isfinite(yawDeg) || !std::isfinite(dt) || !(dx * dx + dz * dz > 1e-6f) || !std::isfinite(dx + dz))
        return yawDeg;
    const float want = std::atan2(-dz, dx) * 180.f / 3.14159265f; // Quat::yaw(want) takes +x toward the target
    float d = std::fmod(want - yawDeg + 540.f, 360.f) - 180.f;
    const float most = turnDegreesPerSecond * std::clamp(dt, 0.f, .1f);
    d = std::clamp(d, -most, most);
    float y = yawDeg + d;
    if (y > 180.f)
        y -= 360.f;
    if (y < -180.f)
        y += 360.f;
    return y;
}

// ------------------------------------------------------------------------------------------- the draw
void frameAt(const Look& look, float clock, uint32_t& a, uint32_t& b, float& t) {
    a = b = 0;
    t = 0;
    if (look.frames <= 1 || !(look.loopSeconds > 0) || !std::isfinite(clock))
        return;
    float p = std::fmod(clock, look.loopSeconds);
    if (p < 0)
        p += look.loopSeconds;
    const float f = p / look.loopSeconds * static_cast<float>(look.frames);
    a = std::min(static_cast<uint32_t>(f), look.frames - 1);
    b = (a + 1) % look.frames;
    t = std::clamp(f - static_cast<float>(a), 0.f, 1.f);
}

bool append(std::vector<Vertex>& sink, const mesh_model::Mesh& model, const Cast& cast, const std::vector<Pose>& poses,
            const hideout074::Place& place, size_t budget) {
    if (!model.ok() || !fin(place.at) || !std::isfinite(place.yaw))
        return false;
    size_t need = 0;
    for (const Pose& p : poses) {
        if (p.npc >= cast.npcs.size() || cast.npcs[p.npc].look >= cast.looks.size())
            return false;
        const Look& k = cast.looks[cast.npcs[p.npc].look];
        if (uint64_t(k.first) + uint64_t(k.perFrame) * k.frames > model.triangles())
            return false;
        need += size_t{k.perFrame} * 3;
    }
    if (sink.size() + need > budget)
        return false;
    const size_t from = sink.size();
    try {
        sink.reserve(from + need);
        const bool uvs = model.hasUvs();
        for (const Pose& p : poses) {
            const Npc& c = cast.npcs[p.npc];
            const Look& k = cast.looks[c.look];
            uint32_t fa, fb;
            float t;
            frameAt(k, p.clock, fa, fb, t);
            const float yaw = place.yaw + (std::isfinite(p.yawDeg) ? p.yawDeg : c.yawDeg) * 3.14159265f / 180.f;
            const Quat q = Quat::yaw(yaw);
            const Vec3 ex = q.rotate({1, 0, 0}), ey = q.rotate({0, 1, 0}), ez = q.rotate({0, 0, 1}), o = place.world(c.at);
            const uint32_t ta = k.first + fa * k.perFrame, tb = k.first + fb * k.perFrame;
            for (uint32_t i = 0; i < k.perFrame; ++i) {
                const uint32_t A = ta + i, B = tb + i;
                Vec3 p3[3];
                for (uint32_t v = 0; v < 3; ++v)
                    p3[v] = model.corners[A * 3 + v] * (1 - t) + model.corners[B * 3 + v] * t;
                const int16_t im = uvs && A < model.image.size() ? model.image[A] : int16_t{-1};
                const int32_t layerAt = im >= 0 && static_cast<size_t>(im) < model.images.size() ? model.images[static_cast<size_t>(im)].layer : -1;
                const float layer = layerAt >= 0 ? static_cast<float>(layerAt) : -1.f;
                const Vec3 n = cross(p3[1] - p3[0], p3[2] - p3[0]);
                const float nl = length(n);
                const float shade = nl > 1e-9f ? .78f + .22f * std::abs(dot(n * (1.f / nl), light)) : .9f;
                const Vec3 base = layerAt >= 0 ? (A < model.tints.size() ? model.tints[A] : Vec3{1, 1, 1}) : model.colors[A];
                const Vec3 colour = base * shade;
                for (uint32_t v = 0; v < 3; ++v) {
                    const Vec3 w = o + ex * p3[v].x + ey * p3[v].y + ez * p3[v].z;
                    const float u = uvs ? model.uvs[size_t{A} * 6 + v * 2] : 0.f, vv = uvs ? model.uvs[size_t{A} * 6 + v * 2 + 1] : 0.f;
                    sink.push_back(Vertex{w, colour, 1.f, {u, vv, layer, 1.f}});
                }
            }
        }
        for (size_t i = from; i < sink.size(); ++i)
            if (!fin(sink[i].position) || !fin(sink[i].color)) {
                sink.resize(from);
                return false;
            }
        return true;
    } catch (...) {
        sink.resize(from);
        return false;
    }
}

} // namespace spidy::ua_cast
