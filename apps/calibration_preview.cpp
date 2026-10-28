// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 36: draws the T-pose calibration panel (calibration_panel.hpp)
// without a headset, at about a headset's pixel density (18 a degree),
// from the player's eyes as the calibration places it, over a plain day
// sky: one image a state, written as <prefix>-<state>.bmp.
//   tracking  a controller not seen: no step met
//   stand     sitting (PLEASE STAND UP), step 1 pulsing
//   arms      standing, only the left arm out: step 1 met, step 2 pulsing, the left arm lit
//   look      both arms out but the head turned: step 1 undone again, step 2 met
//   triggers  steps 1 and 2 met, step 3 pulsing
//   holding   the hold at 60 percent (HOLD STILL)
//   done      CALIBRATED with the measurements
//   fallback  the previous panel, which shows if the art can't be used
//   holo-*    Version 0.0.73 test 2 HOLO CALIBRATION (holo_calibration.hpp): stand, tpose, belt, back,
//             holsters, confirm, done - the blue and white checklist with the TRUE T-pose figure
//
//   spidy_calibration_preview <prefix>
#include "spidy/body_calibration.hpp"
#include "spidy/calibration_art.hpp"
#include "spidy/calibration_panel.hpp"
#include "spidy/holo_calibration.hpp"
#include "preview_image.hpp"
#include <cstdio>
#include <string>
using namespace spidy;
using namespace spidy::preview;
namespace bc = body_calibration;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: spidy_calibration_preview <prefix>\n");
        return 2;
    }
    calibration_art::Image atlas;
    if (!calibration_art::load(atlas)) {
        std::fprintf(stderr, "the embedded atlas did not decode\n");
        return 1;
    }
    const auto texture = Texture::from(calibration_art::mips(atlas, 5));
    const int W = 1300, H = 700;
    // 18 pixels a degree near the middle, the panel (1.4 m ahead, .12 m
    // below the eyes) in the middle.
    const Camera camera{18 * 57.2958f, W / 2.f, H / 2.f - 18 * 57.2958f * .12f / 1.4f};
    bc::View base;
    base.panel = bc::panelPose({{0, 0, 0}, {}});
    base.handTracked = {true, true};
    struct State {
        const char* name;
        bc::Phase phase;
        bc::Hint hint;
        bool left, right;
        float progress;
    };
    const State states[] = {
        {"tracking", bc::Phase::waiting, bc::Hint::tracking, false, false, 0},
        {"stand", bc::Phase::waiting, bc::Hint::standUp, false, false, 0},
        {"arms", bc::Phase::waiting, bc::Hint::armsOut, true, false, 0},
        {"look", bc::Phase::waiting, bc::Hint::lookAhead, true, true, 0},
        {"triggers", bc::Phase::waiting, bc::Hint::triggers, true, true, 0},
        {"holding", bc::Phase::holding, bc::Hint::none, true, true, .6f},
        {"done", bc::Phase::done, bc::Hint::none, true, true, 1},
        {"fallback", bc::Phase::waiting, bc::Hint::armsOut, true, false, 0},
    };
    int failed = 0;
    for (const auto& s : states) {
        Image image(W, H);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                const float v = static_cast<float>(y) / H;
                float* o = image.at(x, y);
                const bool roof = v > .8f;
                o[0] = decode(roof ? .45f : .42f + .3f * v);
                o[1] = decode(roof ? .44f : .58f + .22f * v);
                o[2] = decode(roof ? .42f : .86f + .08f * v);
            }
        auto view = base;
        view.phase = s.phase;
        view.hint = s.hint;
        view.ready = {s.left, s.right};
        view.progress = s.progress;
        view.result.eyeHeight = 1.71f;
        view.result.armLength = .62f;
        std::vector<Vertex> art;
        if (std::string(s.name) == "fallback") {
            std::vector<Vertex> depthWriting;
            bc::appendView(depthWriting, view, {0, 0, 0});
            std::vector<float> depth(static_cast<size_t>(W) * H, 1e30f);
            draw(image, depthWriting, camera, 0, 0, 0, 0, W, H, &depth);
        } else {
            // The pulse at its brightest.
            calibration_panel::append(art, view, .1923f);
            drawArt(image, art, texture, camera, 0, 0, 0, 0, W, H);
        }
        if (!save(image, std::string(argv[1]) + "-" + s.name + ".bmp"))
            ++failed;
    }
    // Version 0.0.73 test 2: the holo checklist, one image a step.
    namespace hc = holo_calibration;
    struct HoloState {
        const char* name;
        bc::Phase phase;
        bc::Hint hint;
        bool left, right;
        float progress;
        uint32_t step, done, touch;
    };
    const HoloState holos[] = {
        {"holo-stand", bc::Phase::waiting, bc::Hint::standUp, false, false, 0, hc::stand, 0, 0},
        {"holo-tpose", bc::Phase::holding, bc::Hint::none, true, true, .5f, hc::tpose, 1, 0},
        {"holo-belt", bc::Phase::done, bc::Hint::none, true, true, 1, hc::belt, 3, hc::touchSqueeze},
        {"holo-back", bc::Phase::done, bc::Hint::none, true, true, 1, hc::back, 7, hc::touchBehind},
        {"holo-holsters", bc::Phase::done, bc::Hint::none, true, true, 1, hc::holsters, 15, hc::touchSides},
        {"holo-confirm", bc::Phase::done, bc::Hint::none, true, true, 1, hc::confirm, 31, hc::touchConfirm},
        {"holo-done", bc::Phase::done, bc::Hint::none, true, true, 1, hc::finished, 63, 0},
    };
    for (const auto& s : holos) {
        Image image(W, H);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                const float v = static_cast<float>(y) / H;
                float* o = image.at(x, y);
                o[0] = decode(.42f + .3f * v);
                o[1] = decode(.58f + .22f * v);
                o[2] = decode(.86f + .08f * v);
            }
        auto view = base;
        view.phase = s.phase;
        view.hint = s.hint;
        view.ready = {s.left, s.right};
        view.progress = s.progress;
        view.result.eyeHeight = 1.71f;
        view.result.armLength = .62f;
        view.holo = true;
        view.holoStep = s.step;
        view.holoDone = s.done;
        view.holoHint = s.touch;
        std::vector<Vertex> art;
        hc::append(art, view, .1923f);
        drawArt(image, art, texture, camera, 0, 0, 0, 0, W, H);
        if (!save(image, std::string(argv[1]) + "-" + s.name + ".bmp"))
            ++failed;
    }
    return failed ? 1 : 0;
}
