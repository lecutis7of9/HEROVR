#pragma once
// HERO VR 0.0.77 CUT6 SWING START (the owner on cut5: "Still cant swing or even do a charged jump after getting
// out of immersive cutscene" and "Still stuck in that spot on that save").
//
// CONFIRMED (report 20261103-234713): the native swing was started on the one VR-play frame the title screen's
// player record had (json sample 402, presentation immersive for a frame after the title's scene), that record was
// gone a frame later (sample 403 no_player), SpidySwingStart answered 1002 (console line 119, "Native swinging is
// off ... (1002)") and swingFailed stayed set for the whole session: no swing on the ledge save's record or on the
// record after the scene, swingState zeroed (grounded 0 in all 15 JUMP lines), so no RT web swing in the air, no
// charged jump, and none of what starts with the swing (game_warp's crawl at rest on a wall, game_offers' wall
// interactions: the json's offers hooks "off"). The start logic was unchanged from 0.0.76; the title's one-frame
// release (cut3 onwards) is what reached it.
//
// The start now waits for one player record held in VR play for settleSeconds, a refusal that says "not now"
// (1000 running already, 1002 the mover not live / its module not answering yet, 1004 retarget before start) is
// tried again retrySeconds later and at once on another record, and only a refusal of the bytes or hooks
// themselves turns the swing off for the session (as in 0.0.76: VR goes on without it).
// Pure and allocation-free; nothing here reads or writes the game.
#include <cstdint>

namespace spidy::swing077 {
constexpr double settleSeconds = .5f;
constexpr double retrySeconds = 1.f;
// A refusal worth trying again: the player was not there yet, not the bridge's bytes or hooks.
inline bool transient(uint32_t code) {
    return code == 1000 || code == 1002 || code == 1004;
}
class StartGate {
  public:
    // ready: VR play this frame with a hero (rays started, motion active, not flat, controls visible). Returns
    // true when the swing should be started this frame.
    bool update(double now, bool ready, uint64_t record, uint64_t mover) {
        if (started_ || failed_)
            return false;
        if (!ready || !record || !mover) {
            since_ = -1;
            return false;
        }
        if (record != record_) {
            record_ = record;
            since_ = now;
            retryAt_ = -1; // another player: no wait for the last one's refusal
        } else if (since_ < 0) {
            since_ = now;
        }
        if (now - since_ < settleSeconds)
            return false;
        return retryAt_ < 0 || now >= retryAt_;
    }
    // The start's answer. Returns true if the swing is off for the session (a real failure).
    bool result(double now, uint32_t code) {
        if (!code || code == 1000) {
            started_ = true;
            ++starts_;
            return false;
        }
        ++refusals_;
        if (transient(code)) {
            retryAt_ = now + retrySeconds;
            return false;
        }
        failed_ = true;
        return true;
    }
    // The swing stopped for a reason that is not its own (another player its retarget refused): start again.
    void restart() {
        started_ = false;
        since_ = -1;
        record_ = 0;
        retryAt_ = -1;
    }
    // A failure of the running swing (a bad hit record, the bridge switched off): transient codes restart,
    // anything else turns it off for the session.
    bool stopped(uint32_t code) {
        if (transient(code)) {
            restart();
            return false;
        }
        started_ = false;
        failed_ = true;
        return true;
    }
    bool started() const {
        return started_;
    }
    bool failed() const {
        return failed_;
    }
    uint64_t refusals() const {
        return refusals_;
    }
    uint64_t starts() const {
        return starts_;
    }
    uint64_t record() const {
        return record_;
    }

  private:
    uint64_t record_{}, refusals_{}, starts_{};
    double since_{-1}, retryAt_{-1};
    bool started_{}, failed_{};
};
// The charged jump's "standing" (0.0.76's expression, unchanged): the swing's own ground (its data are real only
// while it runs; without it no charge and A is the game's plain jump, as 0.0.76 had it).
struct Footing {
    bool steering, padInstalled;
    uint32_t swingStatus;
    bool grounded, owned, mounting, takeoff, takeoffPhase, webLeft, webRight, calibrating;
};
inline bool standing(const Footing& f) {
    return f.steering && f.padInstalled && f.swingStatus && f.grounded && !f.owned && !f.mounting && !f.takeoff &&
           !f.takeoffPhase && !f.webLeft && !f.webRight && !f.calibrating;
}
} // namespace spidy::swing077
