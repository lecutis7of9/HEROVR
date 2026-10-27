#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.72 TEST BUILD NOTICE: on a test / experimental build only (SPIDY_TEST_BUILD, CMake
// option HERO_TEST_BUILD), a panel right after the body calibration closes (measured or skipped; or,
// with no calibration due this session, once play has shown for a moment) with the notice below, word
// for word (the overlay font draws it in capitals), and an OK button: poke it, or aim at it and pull a
// trigger. Once a session. It never takes an input from play (the poke and the trigger still do what
// they do), never blocks anything, and goes by itself after autoHideSeconds. Portable; core_tests.
#include "spidy/math.hpp"
#include "spidy/vertex.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace spidy::test_notice {
// The user's text, exactly (nothing in it corrected).
inline constexpr std::string_view text =
    "You are on the latest test version, please be aware that some features do not work or are unfinished, "
    "revert to 0.0.63 if unplayable and report issues in my discord server linked on the launcher";
inline constexpr std::string_view okLabel = "OK";
// Build 6 (the user, 9:39: "OK is hard to select"): A on either controller closes it too (the left
// controller's A is its X), and says so beside OK. That press never reaches play (holdsA).
inline constexpr std::string_view closeHint = "Press A to close";
inline constexpr float closeHintHeight = .026f, closeHintGap = .03f; // right of OK, inside the rim
inline constexpr float laserWidth = .004f, laserDot = .016f; // build 6: the aiming hand's laser
#if defined(SPIDY_TEST_BUILD) && SPIDY_TEST_BUILD
inline constexpr bool testBuild = true;
#else
inline constexpr bool testBuild = false;
#endif

// The panel (its own frame: x right, y up, the text facing +z, the centre at the origin), metres.
inline constexpr float width = 1.10f, height = .60f, textHeight = .034f, lineGap = .022f, margin = .07f;
inline constexpr float buttonWidth = .24f, buttonHeight = .09f, buttonY = -height / 2 + .085f;
inline constexpr float pokeDepth = .05f, pokeSlack = .03f; // a hand within 5 cm of the face over the button
inline constexpr float settleSeconds = 1.0f;               // play shown this long before it appears
inline constexpr float armSeconds = .75f;                  // and this long before it takes an OK
inline constexpr float fadeSeconds = .35f, autoHideSeconds = 120.f;
inline constexpr float triggerOn = .75f, triggerOff = .35f;

// The notice's lines at `textHeight` within width - 2 margin (words kept whole).
std::vector<std::string_view> lines(std::string_view text, float textHeight, float most);

struct Hand {
    bool tracked{};
    Pose grip{}, aim{}; // tracking space; aim -z forward
    float trigger{};
    bool a{}; // build 6: A (the left hand's X)
};
struct Input {
    double seconds{};
    bool enabled = testBuild; // a test build (tests turn it on)
    bool immersive{};         // VR shows play (not paused, not flat screen)
    bool calibrating{};       // a calibration shows (any phase but idle)
    bool calibrationDue{};    // one will start this session (calibrationWanted || the tab's CALIBRATE)
    Pose head{};              // tracking space
    Hand hands[2]{};
};
enum class State : uint8_t { waiting, shown, gone };
// How it went (the session report and the tests).
enum class Ended : uint8_t { no, poked, triggered, timedOut, off, pressedA };

class Notice {
  public:
    void update(const Input&);
    State state() const { return state_; }
    Ended ended() const { return ended_; }
    bool visible() const { return state_ == State::shown || (state_ == State::gone && fade_ > 0); }
    float opacity() const { return fade_; }
    bool hover() const { return hover_; }
    Pose panel() const { return panel_; } // tracking space
    // The button's centre and the panel's facing (tracking space).
    Vec3 buttonCentre() const;
    Vec3 facing() const;
    // Build 6: whether A / X belong to the notice this frame (it shows, or A closed it and is still
    // held): the caller takes them from play so the closing press is never a jump.
    bool holdsA() const { return state_ == State::shown || aLatch_; }
    // Build 6: the laser of hand i (its aim ray onto the panel; tracking space). false: none.
    bool laser(unsigned i, Vec3& from, Vec3& to) const;

  private:
    State state_ = State::waiting;
    Ended ended_ = Ended::no;
    Pose panel_{};
    double settledSince_ = -1, shownAt_ = 0, goneAt_ = 0, last_ = 0;
    float fade_ = 0;
    bool hover_{}, released_[2]{}, wasNear_[2]{};
    bool aReleased_[2]{}, aLatch_{}, laserOn_[2]{};
    Vec3 laserFrom_[2]{}, laserTo_[2]{};
};

// What a frame draws (NativeEyeFrame::notice), the panel in this frame's world (opacity 0: none).
struct View {
    float opacity{};
    bool hover{};
    Pose panel{};
    bool laser[2]{};       // build 6: each hand's laser (world)
    Vec3 laserFrom[2]{}, laserTo[2]{};
};
View view(const Notice&, Pose worldFromTracking);
// The panel: its dark plate and gold rim, the lines, the OK button (brighter, outlined in white under a
// hover), "Press A to close" beside it, and the lasers (build 6). Appends
// nothing for opacity 0 or a pose that is not finite. `out` blends (drawn without depth, last).
void append(std::vector<Vertex>& out, const View&);
} // namespace spidy::test_notice
