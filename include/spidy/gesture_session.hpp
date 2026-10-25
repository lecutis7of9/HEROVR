#pragma once
// Update 38.5 GESTURE REMAP on the hologram: the state behind its EXTRAS rows (REMAP MOVE, RECORD,
// the conflict popup's KEEP ANYWAY / TRY AGAIN / CANCEL, RESET MOVE, RESET ALL, GESTURE PROFILE).
// Pure (no game, no files): game_xr feeds the frames and the presses, shows view(), saves text()
// when dirty() and plays what update() fires - only while GESTURE MOVES is ON (experimental, OFF
// by default), never while the hologram is busy and never while a move is being recorded.
#include "gesture_everyday.hpp"
#include "gesture_remap.hpp"

namespace spidy::gesture_session {

using gesture_remap::points;
using gesture_remap::slots;

// What the hologram draws: the trail (front view, arm lengths: u right, v up), its colour, the step,
// why the last take did not match, and the popup.
struct View {
    uint32_t move{};          // REMAP MOVE: gestures385::Gesture
    uint32_t profile{};       // 0..profiles-1
    uint32_t customMask{};    // bit g: move g has the player's own motion (this profile)
    uint8_t trail{};          // gesture_remap::Trail
    uint8_t step{};           // gesture_remap::Step
    bool recording{}, popup{}, safety{}, resetAllArmed{};
    int trailPoints{};        // 0 or points, per hand
    float u[2][points]{}, v[2][points]{};
    const char* stepText = "";
    const char* reasonText = ""; // "" when the last take matched
    const char* conflictName = "";
};

class Session {
  public:
    explicit Session(const gesture_remap::Store& loaded = {}) : store_(loaded) {
        store_.active = std::clamp(store_.active, 0, gesture_remap::profiles - 1);
        dirty_ = loaded.migrated; // a version 1 file is written back as 2
    }
    // The presses.
    void nextMove() {
        if (learner_.recording() || learner_.popup())
            return;
        move_ = (move_ + 1) % slots;
        resetAllArmed_ = false;
        learner_ = {};
    }
    void record() {
        resetAllArmed_ = false;
        if (learner_.recording()) { // a second press stops it
            learner_.cancel();
            return;
        }
        if (learner_.popup())
            return;
        learner_.begin(move_);
        capture_.reset();
    }
    void keepAnyway() {
        if (!learner_.popup() || learner_.step() == gesture_remap::Step::blocked)
            return;
        learner_.keepAnyway();
        commit();
    }
    void tryAgain() {
        if (!learner_.popup())
            return;
        learner_.tryAgain();
        capture_.reset();
    }
    void cancel() {
        if (learner_.recording() || learner_.popup())
            learner_.cancel();
    }
    void resetMove() {
        if (learner_.recording() || learner_.popup())
            return;
        resetAllArmed_ = false;
        if (custom()[move_].valid) {
            custom()[move_] = {};
            dirty_ = true;
            detector_.reset();
        }
    }
    // A first press arms it (the row says PRESS AGAIN), a second resets every move of this profile.
    void resetAll() {
        if (learner_.recording() || learner_.popup())
            return;
        if (!resetAllArmed_) {
            resetAllArmed_ = true;
            return;
        }
        resetAllArmed_ = false;
        for (auto& m : custom())
            m = {};
        dirty_ = true;
        detector_.reset();
    }
    void nextProfile() {
        if (learner_.recording() || learner_.popup())
            return;
        store_.active = (store_.active + 1) % gesture_remap::profiles;
        resetAllArmed_ = false;
        dirty_ = true;
        detector_.reset();
    }
    // A frame: recording takes the motions; otherwise (playOn: GESTURE MOVES ON) the custom moves
    // are matched. Returns the move that fired, or -1.
    int update(const gesture_remap::Capture::Frame& f, float arm, bool holoBusy, bool playOn) {
        const auto& everyday = gesture_everyday::table();
        if (learner_.recording()) {
            // The hologram is open while recording (the trail shows there); its presses are short
            // trigger taps, under the capture's start speed and travel.
            if (capture_.push(f, arm)) {
                learner_.offer(capture_.shape(), custom(), gesture_everyday::count, everyday.shapes,
                               everyday.names);
                if (learner_.step() == gesture_remap::Step::saved)
                    commit();
            }
            return -1;
        }
        if (!playOn || learner_.popup())
            return -1;
        const auto fired = detector_.update(f, arm, holoBusy, custom(), gesture_everyday::count, everyday.shapes);
        if (fired.gesture >= 0)
            ++fires_;
        return fired.gesture;
    }
    View view() const {
        View out;
        out.move = move_;
        out.profile = static_cast<uint32_t>(store_.active);
        for (int g = 0; g < slots; ++g)
            if (store_.moves[store_.active][g].valid)
                out.customMask |= 1u << g;
        out.trail = static_cast<uint8_t>(learner_.trail());
        out.step = static_cast<uint8_t>(learner_.step());
        out.recording = learner_.recording();
        out.popup = learner_.popup();
        out.safety = learner_.step() == gesture_remap::Step::blocked;
        out.resetAllArmed = resetAllArmed_;
        out.stepText = gesture_remap::stepName(learner_.step());
        out.reasonText = learner_.reason() == gesture_remap::matched ? "" : gesture_remap::reasonText(learner_.reason());
        out.conflictName = learner_.popup() ? learner_.conflict().name : "";
        Vec3 path[points];
        for (int h = 0; h < 2; ++h) {
            int n = 0;
            if (learner_.recording())
                n = capture_.trail(path, h);
            else if (learner_.step() != gesture_remap::Step::idle && learner_.step() != gesture_remap::Step::cancelled &&
                     learner_.result().valid)
            {
                std::copy(std::begin(learner_.result().path[h]), std::end(learner_.result().path[h]), path);
                n = points;
            }
            if (n == points) {
                out.trailPoints = points;
                for (int i = 0; i < points; ++i) {
                    out.u[h][i] = std::isfinite(path[i].x) ? std::clamp(path[i].x, -1.5f, 1.5f) : 0.f;
                    out.v[h][i] = std::isfinite(path[i].y) ? std::clamp(path[i].y, -1.5f, 1.5f) : 0.f;
                }
            }
        }
        return out;
    }
    bool dirty() const {
        return dirty_;
    }
    // The file's text; clears dirty().
    std::string text() {
        dirty_ = false;
        return gesture_remap::save(store_);
    }
    const gesture_remap::Shape (&moves() const)[slots] {
        return store_.moves[store_.active];
    }
    bool hasCustom(uint32_t g) const {
        return g < static_cast<uint32_t>(slots) && moves()[g].valid;
    }
    bool busy() const {
        return learner_.recording() || learner_.popup();
    }
    uint64_t fires() const {
        return fires_;
    }
    const gesture_remap::Learner& learner() const {
        return learner_;
    }
    const gesture_remap::Store& store() const {
        return store_;
    }

  private:
    gesture_remap::Shape (&custom())[slots] {
        return store_.moves[store_.active];
    }
    void commit() {
        if (learner_.step() != gesture_remap::Step::saved || !learner_.result().valid)
            return;
        custom()[learner_.target()] = learner_.result();
        dirty_ = true;
        detector_.reset();
    }
    gesture_remap::Store store_{};
    gesture_remap::Learner learner_{};
    gesture_remap::Capture capture_{};
    gesture_remap::Detector detector_{};
    uint32_t move_{};
    bool resetAllArmed_{}, dirty_{};
    uint64_t fires_{};
};

// Help lines (one sentence each, at most 80 characters: the SUBTEXT GATE).
inline constexpr const char* helpMove = "The move to give your own motion; CUSTOM means yours is saved for it.";
inline constexpr const char* helpRecord = "Press, then do your move three times: red, blue, then green when saved.";
inline constexpr const char* helpKeep = "Your move is close to the one named above; keep it anyway, it may misfire.";
inline constexpr const char* helpAgain = "Record the move again from take one, a little different this time.";
inline constexpr const char* helpCancel = "Stop recording; the move keeps the motion it had before.";
inline constexpr const char* helpResetMove = "Put the picked move back to its built-in motion; buttons always work.";
inline constexpr const char* helpResetAll = "Press twice to put every move in this profile back to built-in motion.";
inline constexpr const char* helpProfile = "Four sets of your own moves, one per player, kept through updates.";

// The custom move's pad input: the move's own (GESTURE MOVES' table), the gadgets their RB combos.
inline uint8_t inputOf(uint32_t g, const uint8_t (&input)[gestures385::count]) {
    switch (g) {
    case gestures385::ricochet: return 15; // RB+X
    case gestures385::upshot: return 13;   // RB+A
    case gestures385::grabber: return 16;  // RB+Y
    case gestures385::sonic: return 14;    // RB+B
    default: return g < gestures385::count ? input[g] : 0;
    }
}

} // namespace spidy::gesture_session
