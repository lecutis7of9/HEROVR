// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 PROMPT CARDS: one card (include/spidy/prompt_cards.hpp).
#include "spidy/prompt_cards.hpp"
#include "spidy/overlay_text.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::prompt_cards {
size_t append(std::vector<Vertex>& out, std::string_view game, std::string_view vr, Vec3 centre, Vec3 right, Vec3 up,
              float height, float alpha) {
    if (!finite(centre) || !finite(right) || !finite(up) || !std::isfinite(height) || !(height > 0) ||
        !std::isfinite(alpha) || !(alpha > 0) || (game.empty() && vr.empty()))
        return 0;
    const float a = std::min(alpha, 1.f);
    const Vec3 toward = cross(right, up); // out of the card, toward the viewer
    if (!finite(toward))
        return 0;
    const size_t from = out.size();
    const float top = height, low = height * .8f, gap = height * .45f, margin = height * .6f;
    const float most = height * 26; // the widest a line may be
    const float topH = fittedHeight(game, top, most), lowH = fittedHeight(vr, low, most);
    const float wide = std::max(textWidth(game, topH), textWidth(vr, lowH));
    const float w = wide / 2 + margin, h = (topH + lowH + gap) / 2 + margin;
    const auto P = [&](float x, float y, float z) { return centre + right * x + up * y + toward * z; };
    const auto quad = [&](float x0, float y0, float x1, float y1, float z, Vec3 c, float al) {
        const Vec3 p00 = P(x0, y0, z), p10 = P(x1, y0, z), p11 = P(x1, y1, z), p01 = P(x0, y1, z);
        for (const Vec3& p : {p00, p10, p11, p00, p11, p01})
            out.push_back(Vertex{p, c, al});
    };
    const Vec3 plate{.01f, .012f, .02f}, rimColor{.85f, .06f, .05f}, gold{1.f, .78f, .2f}, ink{.95f, .95f, .97f};
    const float rim = height * .18f, z = height * .02f;
    quad(-w, -h, w, h, 0, plate, .82f * a);
    quad(-w - rim, h, w + rim, h + rim, z, rimColor, .95f * a);
    quad(-w - rim, -h - rim, w + rim, -h, z, rimColor, .95f * a);
    quad(-w - rim, -h, -w, h, z, rimColor, .95f * a);
    quad(w, -h, w + rim, h, z, rimColor, .95f * a);
    TextStyle ts;
    ts.round = true;
    ts.soft = height * .04f;
    const auto line = [&](std::string_view text, float th, float y, Vec3 color) {
        if (text.empty())
            return;
        ts.height = th;
        ts.color = color;
        const size_t mark = out.size();
        appendText(out, text, P(-textWidth(text, th) / 2, y, 2 * z), right, up, ts);
        for (size_t i = mark; i < out.size(); ++i)
            out[i].alpha *= a;
    };
    line(game, topH, h - margin - topH, gold);
    line(vr, lowH, -h + margin, ink);
    return out.size() - from;
}
} // namespace spidy::prompt_cards
