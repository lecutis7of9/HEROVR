#pragma once
// 0.0.76 POSE CHECK (owner, far shake): each eye's picture against the pose the headset is told it has.
//
// An eye image is made from its command's world eye (game_tracking: rig.toWorld of the tracked eye) and given to
// the headset with that command's tracking eye (game_xr: NativeEyeFrame::trackingEyes). The runtime turns the
// image from that tracking pose to the head at display. Any turn or shift between the camera the game really drew
// the eye from (its view's pose, view +0x00, read when the eye's job is copied) and the submitted tracking eye
// carried into the image's world (saved worldFromTracking, then the player's travel to the rendered frame) is
// left in the image: it would move the whole city, the far buildings most visibly.
//
// SpidyPoseCheckData (game_xr.cpp) counts it per eye at every submit, in degrees and millimetres, with how much
// the image's world turned against the headset's room between its command and the submit (the rig's own turns,
// which no reprojection undoes) and how old the image was. Read-only: nothing here changes a pose.
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::pose_check {
inline constexpr uint32_t magic = 0x4b434850; // PHCK
inline constexpr uint32_t version = 1;
// Over these an eye's submit is counted as differing (float rounding of world positions near 2 km stays under).
inline constexpr float overDegrees = .05f, overMm = 2.f;

struct Delta {
    float degrees{}, mm{};
    bool valid{};
};
// The turn between two eye frames given as the game's camera matrices (row-major: x right, y the eye's down,
// z its forward in rows 0..2, the position in row 3, as game_tracking's worldPose writes them), and the distance
// between their positions once `travel` (the player's travel the native views added to `rendered`,
// native_eyes::renderOffset) is taken off. Rows are normalised; a degenerate or non-finite one: not valid.
inline Delta betweenMatrices(const Mat4& rendered, const Mat4& expected, Vec3 travel) {
    Delta out{};
    // |Ra - Rb|^2 (Frobenius) = 8 sin^2(turn / 2): exact for small turns, where acos of the trace is not.
    double squared = 0;
    for (unsigned r = 0; r < 3; ++r) {
        const double x = rendered[r * 4], y = rendered[r * 4 + 1], z = rendered[r * 4 + 2];
        const double ex = expected[r * 4], ey = expected[r * 4 + 1], ez = expected[r * 4 + 2];
        const double n = std::sqrt(x * x + y * y + z * z), en = std::sqrt(ex * ex + ey * ey + ez * ez);
        if (!(n > 1e-6) || !(en > 1e-6) || !std::isfinite(n) || !std::isfinite(en))
            return out;
        const double dx = x / n - ex / en, dy = y / n - ey / en, dz = z / n - ez / en;
        squared += dx * dx + dy * dy + dz * dz;
    }
    const double s = std::clamp(std::sqrt(squared / 8), 0.0, 1.0);
    const double dx = static_cast<double>(rendered[12]) - expected[12] - travel.x,
                 dy = static_cast<double>(rendered[13]) - expected[13] - travel.y,
                 dz = static_cast<double>(rendered[14]) - expected[14] - travel.z;
    const double mm = std::sqrt(dx * dx + dy * dy + dz * dz) * 1000;
    if (!std::isfinite(s) || !std::isfinite(mm))
        return out;
    out.degrees = static_cast<float>(2 * std::asin(s) * 180 / 3.14159265358979323846);
    out.mm = static_cast<float>(mm);
    out.valid = true;
    return out;
}
// The game camera matrix of a world eye pose (game_tracking.cpp worldPose).
inline Mat4 cameraOf(Pose pose) {
    const Vec3 x = pose.orientation.rotate({1, 0, 0}), y = pose.orientation.rotate({0, -1, 0}),
               z = pose.orientation.rotate({0, 0, -1}), p = pose.position;
    return {x.x, x.y, x.z, 0, y.x, y.y, y.z, 0, z.x, z.y, z.z, 0, p.x, p.y, p.z, 1};
}
// `rendered` against a submitted eye `expected` carried into the image's world.
inline Delta between(const Mat4& rendered, Pose expected, Vec3 travel) {
    return betweenMatrices(rendered, cameraOf(expected), travel);
}
// The turn between two orientations, in degrees (0..180).
inline float turnDegrees(Quat a, Quat b) {
    const double d = std::abs(static_cast<double>(a.x) * b.x + static_cast<double>(a.y) * b.y +
                              static_cast<double>(a.z) * b.z + static_cast<double>(a.w) * b.w);
    const double na = std::sqrt(static_cast<double>(a.x) * a.x + a.y * a.y + a.z * a.z + a.w * a.w),
                 nb = std::sqrt(static_cast<double>(b.x) * b.x + b.y * b.y + b.z * b.z + b.w * b.w);
    if (!(na > 1e-6) || !(nb > 1e-6) || !std::isfinite(d))
        return 0;
    // The relative turn's vector part (b * conj(a)) against its scalar: exact for small turns.
    const double w = d / (na * nb);
    const Quat r = b * a.conjugate();
    const double v = std::sqrt(static_cast<double>(r.x) * r.x + static_cast<double>(r.y) * r.y +
                               static_cast<double>(r.z) * r.z) / (na * nb);
    return static_cast<float>(2 * std::atan2(v, std::clamp(w, 0.0, 1.0)) * 180 / 3.14159265358979323846);
}

struct Eye {
    uint64_t checked{}, unread{}, overDegrees{}, overMm{}; // unread: the image's camera was not recorded
    float lastDegrees{}, maxDegrees{}, lastMm{}, maxMm{};
    double sumDegrees{}, sumMm{};
};
static_assert(sizeof(Eye) == 64);
struct Data {
    uint32_t magic = pose_check::magic, version = pose_check::version, bytes = 256, flags{};
    Eye eyes[2]{};
    uint64_t submits{}, reused{}; // immersive submits checked; of them, an image already submitted before
    float worldTurnLast{}, worldTurnMax{}; // degrees the image's world turned in the room up to its submit
    double worldTurnSum{};
    uint64_t worldTurnOver{}; // submits whose world had turned more than overDegrees
    float travelMaxMm{}, ageLastMs{}, ageMaxMs{}, reservedF{};
    double ageSumMs{};
    int64_t sequence{}; // odd while game_xr writes (InterlockedIncrement64)
    // The rendered camera against the eye the game was commanded (stereo_probe's placeEyes, before travel):
    // a difference in `eyes` that shows here too is the game moving its camera after SetPose; one only in `eyes`
    // is the command differing from the eye the headset was given.
    float commandMaxDegrees[2]{}, commandMaxMm[2]{};
    uint64_t reserved[3]{};
};
static_assert(sizeof(Data) == 256, "SpidyPoseCheckData is 256 bytes");

inline void note(Eye& e, Delta d) {
    if (!d.valid) {
        ++e.unread;
        return;
    }
    ++e.checked;
    e.lastDegrees = d.degrees;
    e.lastMm = d.mm;
    e.maxDegrees = std::max(e.maxDegrees, d.degrees);
    e.maxMm = std::max(e.maxMm, d.mm);
    e.sumDegrees += d.degrees;
    e.sumMm += d.mm;
    e.overDegrees += d.degrees > pose_check::overDegrees;
    e.overMm += d.mm > pose_check::overMm;
}
inline void noteCommand(Data& data, unsigned eye, Delta d) {
    if (!d.valid || eye > 1)
        return;
    data.commandMaxDegrees[eye] = std::max(data.commandMaxDegrees[eye], d.degrees);
    data.commandMaxMm[eye] = std::max(data.commandMaxMm[eye], d.mm);
}
// One submit's world turn, the image's age and the player's travel in it.
inline void noteSubmit(Data& data, bool reused, float worldTurn, float ageMs, float travelMm) {
    ++data.submits;
    data.reused += reused;
    if (std::isfinite(worldTurn)) {
        data.worldTurnLast = worldTurn;
        data.worldTurnMax = std::max(data.worldTurnMax, worldTurn);
        data.worldTurnSum += worldTurn;
        data.worldTurnOver += worldTurn > pose_check::overDegrees;
    }
    if (std::isfinite(ageMs) && ageMs >= 0) {
        data.ageLastMs = ageMs;
        data.ageMaxMs = std::max(data.ageMaxMs, ageMs);
        data.ageSumMs += ageMs;
    }
    if (std::isfinite(travelMm))
        data.travelMaxMm = std::max(data.travelMaxMm, travelMm);
}
} // namespace spidy::pose_check
