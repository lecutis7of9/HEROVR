#include "spidy/test_notice.hpp"
#include "spidy/overlay_text.hpp"

#include <algorithm>
#include <cmath>

namespace spidy::test_notice {
namespace {
bool okPose(const Pose& p) {
    const Quat& q = p.orientation;
    return finite(p.position) && std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w) &&
           std::abs(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w - 1.f) < .05f;
}
Vec3 level(Vec3 v) {
    return {v.x, 0, v.z};
}
// Where it appears: 1.3 m ahead of the head, level, a little under the eyes, facing the player.
Pose placeFor(Pose head) {
    Vec3 ahead = normalized(level(head.orientation.rotate({0, 0, -1})));
    if (!(length(ahead) > .5f))
        ahead = {0, 0, -1};
    return {head.position + ahead * 1.3f + Vec3{0, -.10f, 0}, Quat::yaw(std::atan2(-ahead.x, -ahead.z))};
}
// The point where the ray hits the panel's plane, in the panel's frame (false: misses it, or behind).
bool hitPlane(Pose panel, Vec3 from, Vec3 way, Vec3& local) {
    const Vec3 n = panel.orientation.rotate({0, 0, 1});
    const float d = dot(way, n);
    if (!(std::abs(d) > 1e-4f))
        return false;
    const float t = dot(panel.position - from, n) / d;
    if (!(t > 0 && t < 20.f))
        return false;
    local = panel.orientation.conjugate().rotate(from + way * t - panel.position);
    return finite(local);
}
bool overButton(Vec3 local, float slack) {
    return std::abs(local.x) <= buttonWidth / 2 + slack && std::abs(local.y - buttonY) <= buttonHeight / 2 + slack;
}
} // namespace

std::vector<std::string_view> lines(std::string_view s, float h, float most) {
    std::vector<std::string_view> out;
    size_t start = 0;
    while (start < s.size()) {
        while (start < s.size() && s[start] == ' ')
            ++start;
        if (start >= s.size())
            break;
        size_t end = start, best = std::string_view::npos;
        while (end <= s.size()) {
            const size_t next = s.find(' ', end);
            const size_t stop = next == std::string_view::npos ? s.size() : next;
            if (textWidth(s.substr(start, stop - start), h) > most && best != std::string_view::npos)
                break;
            best = stop;
            if (stop >= s.size())
                break;
            end = stop + 1;
        }
        out.push_back(s.substr(start, best - start));
        start = best;
    }
    return out;
}

Vec3 Notice::buttonCentre() const {
    return panel_.position + panel_.orientation.rotate({0, buttonY, 0});
}
Vec3 Notice::facing() const {
    return panel_.orientation.rotate({0, 0, 1});
}

void Notice::update(const Input& in) {
    const double dt = std::clamp(in.seconds - last_, 0.0, .25);
    last_ = in.seconds;
    hover_ = false;
    if (!in.enabled) {
        if (state_ == State::shown)
            ended_ = Ended::off;
        state_ = state_ == State::waiting ? State::waiting : State::gone;
        fade_ = 0;
        return;
    }
    switch (state_) {
    case State::waiting: {
        const bool ready = in.immersive && !in.calibrating && !in.calibrationDue && okPose(in.head);
        if (!ready) {
            settledSince_ = -1;
            return;
        }
        if (settledSince_ < 0)
            settledSince_ = in.seconds;
        if (in.seconds - settledSince_ < settleSeconds)
            return;
        panel_ = placeFor(in.head);
        if (!okPose(panel_))
            return;
        state_ = State::shown;
        shownAt_ = in.seconds;
        fade_ = 0;
        for (unsigned i = 0; i < 2; ++i) {
            released_[i] = in.hands[i].trigger < triggerOff; // a trigger held from the calibration does not count
            wasNear_[i] = false;
        }
        return;
    }
    case State::shown: {
        fade_ = std::min(1.f, fade_ + static_cast<float>(dt / fadeSeconds));
        const bool armed = in.seconds - shownAt_ >= armSeconds;
        Ended why = Ended::no;
        for (unsigned i = 0; i < 2; ++i) {
            const Hand& h = in.hands[i];
            if (!h.tracked || !okPose(h.grip) || !okPose(h.aim)) {
                released_[i] = false;
                wasNear_[i] = false;
                continue;
            }
            // Poke: the hand comes within pokeDepth of the face over the button.
            const Vec3 local = panel_.orientation.conjugate().rotate(h.grip.position - panel_.position);
            const bool near = finite(local) && std::abs(local.z) <= pokeDepth && overButton(local, pokeSlack);
            hover_ |= near;
            if (armed && near && !wasNear_[i])
                why = Ended::poked;
            wasNear_[i] = near;
            // Aim and pull: the aim ray over the button (with some slack) and a fresh pull.
            Vec3 hit{};
            const bool aimed = hitPlane(panel_, h.aim.position, h.aim.orientation.rotate({0, 0, -1}), hit) && overButton(hit, .05f);
            hover_ |= aimed;
            if (h.trigger < triggerOff)
                released_[i] = true;
            else if (h.trigger > triggerOn && released_[i]) {
                released_[i] = false;
                if (armed && aimed && why == Ended::no)
                    why = Ended::triggered;
            }
        }
        if (why == Ended::no && in.seconds - shownAt_ >= autoHideSeconds)
            why = Ended::timedOut;
        if (why != Ended::no) {
            ended_ = why;
            state_ = State::gone;
            goneAt_ = in.seconds;
        }
        return;
    }
    case State::gone:
        fade_ = std::max(0.f, fade_ - static_cast<float>(dt / fadeSeconds));
        return;
    }
}

View view(const Notice& n, Pose worldFromTracking) {
    View v;
    if (!n.visible() || !okPose(worldFromTracking))
        return v;
    v.panel = compose(worldFromTracking, n.panel());
    v.opacity = okPose(v.panel) ? n.opacity() : 0.f;
    v.hover = n.hover();
    return v;
}

void append(std::vector<Vertex>& out, const View& v) {
    if (!(v.opacity > 0) || !okPose(v.panel))
        return;
    const size_t from = out.size();
    const Vec3 r = v.panel.orientation.rotate({1, 0, 0}), u = v.panel.orientation.rotate({0, 1, 0}),
               f = v.panel.orientation.rotate({0, 0, 1});
    const Vec3 o = v.panel.position;
    const float a = std::clamp(v.opacity, 0.f, 1.f);
    const auto P = [&](float x, float y, float z = 0) { return o + r * x + u * y + f * z; };
    const auto quad = [&](float x0, float y0, float x1, float y1, float z, Vec3 c, float al) {
        const Vec3 p00 = P(x0, y0, z), p10 = P(x1, y0, z), p11 = P(x1, y1, z), p01 = P(x0, y1, z);
        for (const Vec3& p : {p00, p10, p11, p00, p11, p01})
            out.push_back(Vertex{p, c, al});
    };
    const Vec3 gold{1.f, .62f, .08f}, plate{.012f, .016f, .03f}, ink{.92f, .92f, .95f};
    const float w = width / 2, h = height / 2, rim = .008f;
    quad(-w, -h, w, h, 0, plate, .93f * a);
    quad(-w - rim, h, w + rim, h + rim, .001f, gold, .95f * a); // the rim, four strips round the plate
    quad(-w - rim, -h - rim, w + rim, -h, .001f, gold, .95f * a);
    quad(-w - rim, -h, -w, h, .001f, gold, .95f * a);
    quad(w, -h, w + rim, h, .001f, gold, .95f * a);
    // the lines, centred, from the top
    const auto ls = lines(text, textHeight, width - 2 * margin);
    TextStyle ts;
    ts.height = textHeight;
    ts.color = ink;
    ts.soft = .0015f;
    float y = h - margin - textHeight;
    for (const auto& line : ls) {
        const float lw = textWidth(line, textHeight);
        const size_t mark = out.size();
        appendText(out, line, P(-lw / 2, y, .003f), r, u, ts);
        for (size_t i = mark; i < out.size(); ++i)
            out[i].alpha *= a;
        y -= textHeight + lineGap;
    }
    // the OK button
    const Vec3 face = v.hover ? Vec3{1.f, .75f, .2f} : gold * .8f;
    quad(-buttonWidth / 2, buttonY - buttonHeight / 2, buttonWidth / 2, buttonY + buttonHeight / 2, .002f, face, .97f * a);
    TextStyle bs = ts;
    bs.height = .045f;
    bs.color = {.02f, .02f, .03f};
    bs.weight = .16f;
    const float bw = textWidth(okLabel, bs.height);
    const size_t mark = out.size();
    appendText(out, okLabel, P(-bw / 2, buttonY - bs.height / 2, .004f), r, u, bs);
    for (size_t i = mark; i < out.size(); ++i)
        out[i].alpha *= a;
    for (size_t i = from; i < out.size(); ++i)
        if (!finite(out[i].position) || !std::isfinite(out[i].alpha)) {
            out.resize(from);
            return;
        }
}
} // namespace spidy::test_notice
