// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.73 MP SOLO TEST: the remote player's drawing (mp_ghost.hpp).
#include "spidy/mp_ghost.hpp"

#include "spidy/hero072.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/overlay_text.hpp"
#include "spidy/soft_shapes.hpp"

#include <algorithm>
#include <cmath>

namespace spidy::mp {
namespace {
bool finiteV(Vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
} // namespace

uint8_t suitIdOfSlot(uint32_t slot) {
    if (!mesh_model::slotIsSuit(slot))
        return noSuit;
    return static_cast<uint8_t>(slot - mesh_model::slotSuitFirst);
}
uint32_t suitHashOf(uint8_t suit) {
    return suit < mesh_model::suitSlots ? fnv1a(mesh_model::suitFolders[suit]) : 0u;
}
uint32_t slotOfState(const PoseState& s) {
    if (s.suit == noSuit)
        return 0;
    if (s.suitHash)
        for (uint32_t k = 0; k < mesh_model::suitSlots; ++k)
            if (fnv1a(mesh_model::suitFolders[k]) == s.suitHash)
                return mesh_model::slotSuitFirst + k;
    if (s.suit < mesh_model::suitSlots && (!s.suitHash || suitHashOf(s.suit) == s.suitHash))
        return mesh_model::slotSuitFirst + s.suit;
    return 0;
}

suit_wear::Tracked toTracked(const Ghost& g, Vec3 origin, Quat turn) {
    suit_wear::Tracked t;
    const auto at = [&](Vec3 p) { return origin + turn.rotate(p); };
    const auto dir = [&](Vec3 d) { return normalized(turn.rotate(d)); };
    t.up = dir({0, 1, 0});
    t.feet = at(g.feet);
    t.eye = at(g.eye);
    t.eyeForward = dir(g.eyeForward);
    t.eyeUp = dir(g.eyeUp);
    for (int i = 0; i < 2; ++i) {
        t.hand[i] = g.hand[i];
        t.grip[i] = at(g.grip[i]);
        t.gripForward[i] = dir(g.gripForward[i]);
        t.gripX[i] = dir(g.gripX[i]);
        t.gripUp[i] = dir(g.gripUp[i]);
        t.handleTiltDeg[i] = 30.f;
        t.curl[i] = std::clamp(std::isfinite(g.curl[i]) ? g.curl[i] : 0.f, 0.f, 1.f);
    }
    t.standingEye = g.standingEye;
    return t;
}

Vec3 heroColor(uint8_t hero) {
    const auto& b = hero072::bio(std::min<uint32_t>(hero, hero072::rosterCount - 1));
    return {b.color[0], b.color[1], b.color[2]};
}

size_t appendFigure(std::vector<Vertex>& out, const suit_wear::Tracked& t, Vec3 viewer, Vec3 color, float alpha) {
    const size_t before = out.size();
    if (!finiteV(t.feet) || !finiteV(t.eye) || !(alpha > 0))
        return 0;
    const Vec3 up = t.up;
    const float eyeH = std::clamp(t.standingEye > .8f ? t.standingEye : dot(t.eye - t.feet, up), 1.0f, 2.4f);
    const float s = eyeH / 1.65f;
    Vec3 fwd = t.eyeForward - up * dot(t.eyeForward, up);
    fwd = length(fwd) > 1e-3f ? normalized(fwd) : Vec3{0, 0, -1};
    const Vec3 side = normalized(cross(fwd, up)); // the figure's right
    const Vec3 headC = t.eye - fwd * (.06f * s) + up * (.04f * s);
    const Vec3 base = t.feet + (headC - t.feet - up * dot(headC - t.feet, up)) * .6f; // under the head, mostly
    const Vec3 neck = headC - up * (.16f * s);
    const Vec3 hip = base + up * (.95f * s);
    const Vec3 shoulder[2] = {neck - side * (.19f * s) - up * (.05f * s), neck + side * (.19f * s) - up * (.05f * s)};
    const Vec3 hipSide[2] = {hip - side * (.1f * s), hip + side * (.1f * s)};
    const Vec3 foot[2] = {base - side * (.13f * s), base + side * (.13f * s)};
    const auto view = [&](Vec3 p) { return finiteV(viewer) && length(viewer - p) > 1e-3f ? normalized(viewer - p) : fwd; };
    const float w = .035f * s, soft = .012f * s;
    const auto bar = [&](Vec3 a, Vec3 b, float hw) {
        if (finiteV(a) && finiteV(b) && length(b - a) > 1e-4f)
            soft::softBar(out, a, b, view((a + b) * .5f), hw, color, alpha, soft);
    };
    // Head: a ring of short bars.
    const Vec3 hv = view(headC);
    const Vec3 hx = normalized(cross(up, hv)), hy = normalized(cross(hv, hx));
    const float r = .11f * s;
    for (int k = 0; k < 12; ++k) {
        const float a0 = k * 6.2831853f / 12, a1 = (k + 1) * 6.2831853f / 12;
        bar(headC + (hx * std::cos(a0) + hy * std::sin(a0)) * r, headC + (hx * std::cos(a1) + hy * std::sin(a1)) * r, w * .6f);
    }
    // The face's direction: a short nose bar, so which way it looks reads at a glance.
    bar(headC + fwd * (r * .6f), headC + fwd * (r * 1.25f), w * .45f);
    bar(neck, hip, w * 1.6f);
    bar(shoulder[0], shoulder[1], w);
    bar(hipSide[0], hipSide[1], w);
    for (int i = 0; i < 2; ++i) {
        Vec3 hand = t.grip[i];
        if (!t.hand[i] || !finiteV(hand) || length(hand - shoulder[i]) > .9f * s)
            hand = shoulder[i] - up * (.6f * s) + side * ((i ? .06f : -.06f) * s);
        // An elbow out and down from the line (two bars).
        const Vec3 mid = (shoulder[i] + hand) * .5f - up * (.07f * s) + side * ((i ? .07f : -.07f) * s);
        bar(shoulder[i], mid, w * .9f);
        bar(mid, hand, w * .8f);
        bar(hand - up * (.03f * s), hand + up * (.03f * s), w * 1.3f); // the fist
        const Vec3 knee = (hipSide[i] + foot[i]) * .5f + fwd * (.05f * s);
        bar(hipSide[i], knee, w * 1.1f);
        bar(knee, foot[i], w);
        bar(foot[i], foot[i] + fwd * (.14f * s), w * .7f);
    }
    return out.size() - before;
}

std::string nameplate(bool buddy, const std::string& buddyName, int pingMs) {
    std::string who = buddy ? (buddyName.empty() ? std::string("TEST BUDDY") : buddyName) : std::string("YOU, LOOPBACK");
    std::string s = "PLAYER 2 (" + who + ")  PING ";
    s += pingMs >= 0 ? std::to_string(std::min(pingMs, 9999)) + " MS" : std::string("--");
    return s;
}

size_t appendNameplate(std::vector<Vertex>& out, const std::string& text, Vec3 above, Vec3 up, Vec3 viewer, float alpha) {
    const size_t before = out.size();
    if (!finiteV(above) || !finiteV(viewer) || !(alpha > 0) || text.empty())
        return 0;
    Vec3 to = viewer - above;
    to = to - up * dot(to, up);
    if (length(to) < 1e-3f)
        return 0;
    to = normalized(to);
    const Vec3 right = normalized(cross(up, to));
    const float dist = std::max(1.f, length(viewer - above));
    TextStyle st;
    st.height = std::clamp(.022f * dist, .03f, .12f); // readable at its distance (about a degree tall)
    st.weight = .14f;
    st.color = {.55f, 1.f, .75f};
    st.round = true;
    st.soft = st.height * .05f;
    const float w = textWidth(text, st.height);
    // A dark backing a little behind the letters.
    const Vec3 c = above - to * .01f;
    const float hw = w * .5f + st.height * .5f, hh = st.height * 1.0f;
    const Vertex a{c - right * hw - up * (hh * .55f), {0.01f, .02f, .03f}, .55f * alpha};
    const Vertex b{c + right * hw - up * (hh * .55f), {0.01f, .02f, .03f}, .55f * alpha};
    const Vertex d{c + right * hw + up * (hh * 1.25f), {0.01f, .02f, .03f}, .55f * alpha};
    const Vertex e{c - right * hw + up * (hh * 1.25f), {0.01f, .02f, .03f}, .55f * alpha};
    soft::quad(out, a, b, d, e);
    const size_t textFrom = out.size();
    appendText(out, text, above - right * (w * .5f), right, up, st);
    for (size_t i = textFrom; i < out.size(); ++i)
        out[i].alpha *= alpha;
    return out.size() - before;
}
} // namespace spidy::mp
