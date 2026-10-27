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
bool Notice::laser(unsigned i, Vec3& from, Vec3& to) const {
    if (i >= 2 || !laserOn_[i] || state_ != State::shown)
        return false;
    from = laserFrom_[i];
    to = laserTo_[i];
    return true;
}

void Notice::update(const Input& in) {
    const double dt = std::clamp(in.seconds - last_, 0.0, .25);
    last_ = in.seconds;
    hover_ = false;
    laserOn_[0] = laserOn_[1] = false;
    if (aLatch_ && !in.hands[0].a && !in.hands[1].a)
        aLatch_ = false; // the closing A let go: play has A again
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
            aReleased_[i] = !in.hands[i].a; // nor an A already held
        }
        return;
    }
    case State::shown: {
        fade_ = std::min(1.f, fade_ + static_cast<float>(dt / fadeSeconds));
        const bool armed = in.seconds - shownAt_ >= armSeconds;
        Ended why = Ended::no;
        for (unsigned i = 0; i < 2; ++i) {
            const Hand& h = in.hands[i];
            // Build 6: A (tracked or not) closes it, a fresh press once it is armed.
            if (!h.a)
                aReleased_[i] = true;
            else if (aReleased_[i]) {
                aReleased_[i] = false;
                if (armed && why == Ended::no)
                    why = Ended::pressedA;
            }
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
            const Vec3 way = h.aim.orientation.rotate({0, 0, -1});
            const bool onPlane = hitPlane(panel_, h.aim.position, way, hit);
            const bool aimed = onPlane && overButton(hit, .05f);
            hover_ |= aimed;
            // Build 6: the laser, while the aim ray is on the panel (or a little round it).
            if (onPlane && std::abs(hit.x) <= width / 2 + .15f && std::abs(hit.y) <= height / 2 + .15f) {
                laserOn_[i] = true;
                laserFrom_[i] = h.aim.position;
                laserTo_[i] = panel_.position + panel_.orientation.rotate({hit.x, hit.y, .006f});
            }
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
            if (why == Ended::pressedA)
                aLatch_ = true; // its A stays the notice's until let go
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
    for (unsigned i = 0; i < 2; ++i) {
        Vec3 a{}, b{};
        if (v.opacity > 0 && n.laser(i, a, b)) {
            v.laserFrom[i] = worldFromTracking.position + worldFromTracking.orientation.rotate(a);
            v.laserTo[i] = worldFromTracking.position + worldFromTracking.orientation.rotate(b);
            v.laser[i] = finite(v.laserFrom[i]) && finite(v.laserTo[i]);
        }
    }
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
    // the OK button (build 6: under a hover brighter, a touch bigger, outlined in white)
    const Vec3 face = v.hover ? Vec3{1.f, .86f, .38f} : gold * .8f;
    const float grow = v.hover ? .012f : 0.f, bx = buttonWidth / 2 + grow, by0 = buttonY - buttonHeight / 2 - grow,
                by1 = buttonY + buttonHeight / 2 + grow;
    if (v.hover) {
        const Vec3 white{1, 1, 1};
        const float e = .007f;
        quad(-bx - e, by1, bx + e, by1 + e, .0025f, white, a);
        quad(-bx - e, by0 - e, bx + e, by0, .0025f, white, a);
        quad(-bx - e, by0, -bx, by1, .0025f, white, a);
        quad(bx, by0, bx + e, by1, .0025f, white, a);
    }
    quad(-bx, by0, bx, by1, .002f, face, .97f * a);
    TextStyle bs = ts;
    bs.height = .045f;
    bs.color = {.02f, .02f, .03f};
    bs.weight = .16f;
    const float bw = textWidth(okLabel, bs.height);
    const size_t mark = out.size();
    appendText(out, okLabel, P(-bw / 2, buttonY - bs.height / 2, .004f), r, u, bs);
    for (size_t i = mark; i < out.size(); ++i)
        out[i].alpha *= a;
    // Build 6: "Press A to close" beside OK.
    {
        TextStyle hs = ts;
        hs.height = closeHintHeight;
        hs.color = {.85f, .88f, .95f};
        const float x = buttonWidth / 2 + closeHintGap;
        const size_t hm = out.size();
        appendText(out, closeHint, P(x, buttonY - hs.height / 2, .003f), r, u, hs);
        for (size_t i = hm; i < out.size(); ++i)
            out[i].alpha *= a;
    }
    // Build 6: the lasers, a bright beam (two crossed strips) from the hand to the panel and a dot there.
    for (unsigned k = 0; k < 2; ++k) {
        if (!v.laser[k])
            continue;
        const Vec3 s0 = v.laserFrom[k], s1 = v.laserTo[k], d = s1 - s0;
        const float len = length(d);
        if (!(len > .02f && len < 20.f))
            continue;
        const Vec3 dir = d * (1.f / len);
        Vec3 side = cross(dir, u);
        if (!(length(side) > .1f))
            side = cross(dir, r);
        side = normalized(side);
        const Vec3 up2 = normalized(cross(side, dir));
        const Vec3 beam{.55f, .9f, 1.f};
        for (const Vec3& across : {side, up2}) {
            const Vec3 h0 = across * (laserWidth / 2);
            for (const Vec3& p : {s0 - h0, s0 + h0, s1 + h0, s0 - h0, s1 + h0, s1 - h0})
                out.push_back(Vertex{p, beam, .85f * a});
        }
        const Vec3 c = v.panel.orientation.conjugate().rotate(s1 - o);
        quad(c.x - laserDot / 2, c.y - laserDot / 2, c.x + laserDot / 2, c.y + laserDot / 2, .007f, Vec3{1, 1, 1}, a);
    }
    for (size_t i = from; i < out.size(); ++i)
        if (!finite(out[i].position) || !std::isfinite(out[i].alpha)) {
            out.resize(from);
            return;
        }
}
} // namespace spidy::test_notice
