// Update 0.0.72 GAME BODY (EXPERIMENTAL): the multiplayer team's spec, offline
// (game_body.hpp). A synthetic 237-joint hero rig in the game's own byte
// layout (16-byte joints, 48-byte rest entries, the game's name hash) is
// parsed, named, checked for protected joints, converted to model space and
// solved; no game needed. tools/probe_game_body.py --phases find,solve is
// the same proof in the game.
#include "spidy/game_body.hpp"
#include "spidy/body_ik.hpp"
#include "spidy/body_calibration.hpp"
#include "spidy/glove_overlay.hpp"
#include "spidy/native_body.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace spidy;
namespace gb = spidy::game_body;

namespace {
void check(bool ok, const std::string& what) {
    if (!ok)
        throw std::runtime_error(what);
}
struct J {
    std::string name;
    int parent;
    Vec3 p;
};
// The hero: a T-pose facing +z, its left on +x (the joint places core_tests'
// testBody(heroHand) uses), named as the game's rig is, then its other roots
// (fx_root, sync, the IK targets, cameraTarget, aimTarget) and helper joints
// up to 237.
std::vector<J> heroJoints() {
    std::vector<J> j = {
        {"root", -1, {0, 0, 0}},           {"a_body", 0, {0, 1.f, 0}},     {"spine_1", 1, {0, 1.12f, 0}},
        {"spine_2", 2, {0, 1.25f, 0}},     {"chest", 3, {0, 1.38f, 0}},     {"neck", 4, {0, 1.52f, 0}},
        {"head", 5, {0, 1.62f, 0}},        {"LF_middle_eye_deform", 6, {.032f, 1.7f, .08f}},
        {"RT_middle_eye_deform", 6, {-.032f, 1.7f, .08f}},
    };
    const char* side[2] = {"LF_", "RT_"};
    for (int s = 0; s < 2; ++s) {
        const float x = s ? -1.f : 1.f;
        const std::string n = side[s];
        const int clav = static_cast<int>(j.size());
        j.push_back({n + "clavicle", 4, {.03f * x, 1.47f, .02f}});
        j.push_back({n + "uparm", clav, {.18f * x, 1.46f, 0}});
        j.push_back({n + "loarm", clav + 1, {.44f * x, 1.46f, 0}});
        j.push_back({n + "wrist", clav + 2, {.74f * x, 1.46f, 0}});
        const int wrist = clav + 3;
        const Vec3 fingers[4][5] = {
            {{.7547f, 1.4682f, .0254f}, {.8261f, 1.4679f, .0223f}, {.8739f, 1.4709f, .0272f}, {.9036f, 1.4727f, .0302f}, {.931f, 1.4743f, .0329f}},
            {{.7521f, 1.4625f, .008f}, {.8226f, 1.46f, 0}, {.8791f, 1.4514f, -.0046f}, {.9141f, 1.4459f, -.0075f}, {.9464f, 1.441f, -.0102f}},
            {{.7507f, 1.4533f, -.0112f}, {.8093f, 1.4475f, -.0189f}, {.8603f, 1.4276f, -.0306f}, {.8931f, 1.4147f, -.0382f}, {.9241f, 1.4026f, -.0453f}},
            {{.7469f, 1.4398f, -.021f}, {.7989f, 1.4339f, -.0358f}, {.8333f, 1.4134f, -.0501f}, {.8545f, 1.4007f, -.0589f}, {.8741f, 1.3891f, -.0671f}}};
        const Vec3 thumb[4] = {{.7509f, 1.4536f, .0366f}, {.7913f, 1.4578f, .0674f}, {.8164f, 1.4634f, .0953f}, {.8359f, 1.4701f, .1244f}};
        const char* letters[4] = {"A", "B", "C", "D"};
        const char* parts[5] = {"_A", "_B", "_C", "_D", "_D_end"};
        for (int f = 0; f < 4; ++f) {
            const int first = static_cast<int>(j.size());
            for (int k = 0; k < 5; ++k)
                j.push_back({n + "finger_" + letters[f] + parts[k], k ? first + k - 1 : wrist,
                             {fingers[f][k].x * x, fingers[f][k].y, fingers[f][k].z}});
        }
        const char* thumbs[4] = {"thumb_A", "thumb_B", "thumb_C", "thumb_C_end"};
        const int first = static_cast<int>(j.size());
        for (int k = 0; k < 4; ++k)
            j.push_back({n + thumbs[k], k ? first + k - 1 : wrist, {thumb[k].x * x, thumb[k].y, thumb[k].z}});
    }
    for (int s = 0; s < 2; ++s) {
        const float x = s ? -1.f : 1.f;
        const std::string n = side[s];
        const int up = static_cast<int>(j.size());
        j.push_back({n + "upleg", 1, {.1f * x, .95f, 0}});
        j.push_back({n + "loleg", up, {.1f * x, .5f, .02f}});
        j.push_back({n + "foot", up + 1, {.1f * x, .08f, 0}});
        j.push_back({n + "toe", up + 2, {.1f * x, 0, .13f}});
    }
    for (const char* r : {"fx_root", "sync", "cameraTarget", "aimTarget", "LF_handIK", "RT_handIK", "LF_footIK",
                          "RT_footIK"})
        j.push_back({r, -1, {0, 1, .5f}});
    // Helper joints (twists, deformers) under the spine, arms, legs and head.
    const int hosts[] = {2, 3, 4, 5, 6, 10, 11, 37, 38, 75, 76, 79, 80};
    for (int k = 0; static_cast<int>(j.size()) < 237; ++k) {
        const int h = hosts[k % (sizeof(hosts) / sizeof(hosts[0]))];
        const Vec3 at = j[h].p + Vec3{.01f * std::sin(k * 1.f), .01f * std::cos(k * 1.f), .005f};
        j.push_back({"helper_" + std::to_string(k), h, at});
    }
    return j;
}
// Each joint's model rotation (turned frames, so nothing leans on the axes).
Quat frameOf(int k) {
    return body::unit({std::sin(k * 1.3f), std::cos(k * .7f), std::sin(k * .31f + 1), 1.5f});
}
struct Bytes {
    std::vector<uint8_t> joints, rest;
};
Bytes layout(const std::vector<J>& js) {
    Bytes b;
    b.joints.assign(js.size() * gb::jointBytes, 0);
    b.rest.assign(js.size() * gb::restBytes, 0);
    for (size_t k = 0; k < js.size(); ++k) {
        const int16_t parent = static_cast<int16_t>(js[k].parent);
        const uint32_t hash = gb::nameHash(js[k].name.c_str()), offset = static_cast<uint32_t>(k * 32);
        std::memcpy(&b.joints[k * 16], &parent, 2);
        std::memcpy(&b.joints[k * 16 + 8], &hash, 4);
        std::memcpy(&b.joints[k * 16 + 12], &offset, 4);
        // Local to the parent: rotation and translation in its frame.
        const Quat q = frameOf(static_cast<int>(k));
        Quat local = q;
        Vec3 t = js[k].p;
        if (parent >= 0) {
            const Quat pq = frameOf(parent);
            local = pq.conjugate() * q;
            t = pq.conjugate().rotate(js[k].p - js[parent].p);
        }
        const float e[12] = {1, 1, 1, 0, local.x, local.y, local.z, local.w, t.x, t.y, t.z, 1};
        std::memcpy(&b.rest[k * 48], e, sizeof(e));
    }
    return b;
}
Vec3 at(const std::vector<float>& pose, int k) {
    return {pose[k * 16 + 12], pose[k * 16 + 13], pose[k * 16 + 14]};
}
Quat gripFacing(Vec3 knuckles) {
    const Vec3 y = normalized(knuckles) * -1.f, x{-1, 0, 0};
    return body::fromAxes(x, y, cross(x, y));
}
Vec3 wristOf(const body::Targets::Hand& h, int side, const body::Config& c = {}) {
    return h.grip + h.orientation.rotate(
                        {side == 0 ? -c.wristFromGrip.x : c.wristFromGrip.x, c.wristFromGrip.y, c.wristFromGrip.z});
}
} // namespace

int main() {
    int total = 0, failed = 0;
    auto test = [&](const char* name, const std::function<void()>& f) {
        ++total;
        try {
            f();
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& e) {
            ++failed;
            std::cerr << "FAIL " << name << ": " << e.what() << '\n';
        }
    };
    test("the game's name hash: 'pelvis' is 0x8e59b33f", [] {
        check(gb::nameHash("pelvis") == 0x8e59b33fu, "pelvis hash");
        check(gb::nameHash("a_body") != gb::nameHash("pelvis"), "distinct");
    });
    const auto js = heroJoints();
    const auto bytes = layout(js);
    test("a 237-joint rig parses: parents before children, hashes as named", [&] {
        check(js.size() == 237, "237 joints");
        gb::Table t;
        check(gb::parseJoints(bytes.joints, 237, t), "parse");
        for (size_t k = 0; k < js.size(); ++k) {
            check(t.parent[k] == js[k].parent, "parent " + js[k].name);
            check(t.hash[k] == gb::nameHash(js[k].name.c_str()), "hash " + js[k].name);
        }
        auto bad = bytes.joints;
        const int16_t later = 200;
        std::memcpy(&bad[5 * 16], &later, 2);
        check(!gb::parseJoints(bad, 237, t), "a parent after its child was accepted");
        check(!gb::parseJoints(bytes.joints, 0, t) && !gb::parseJoints(bytes.joints, 238, t), "count checks");
    });
    test("the rest entries convert to the model-space T-pose", [&] {
        gb::Table t;
        gb::parseJoints(bytes.joints, 237, t);
        std::vector<float> rest;
        check(gb::restToModel(bytes.rest, t, rest), "convert");
        for (size_t k = 0; k < js.size(); ++k)
            check(length(at(rest, static_cast<int>(k)) - js[k].p) < 1e-4f, "place of " + js[k].name);
    });
    test("names give the roles; protected roots stay clear; a missing name is refused", [&] {
        gb::Table t;
        gb::parseJoints(bytes.joints, 237, t);
        body::Rig rig;
        std::vector<std::string> missing;
        check(gb::identify(t, rig, &missing), "identify");
        check(rig.pelvis == 1 && rig.head == 6 && rig.spine == std::vector<int16_t>{2, 3, 4} &&
                  rig.neck == std::vector<int16_t>{5},
              "trunk roles");
        check(rig.arms[0].hand == gb::byName(t, "LF_wrist") && rig.arms[1].finger == gb::byName(t, "RT_finger_B_B"),
              "arm roles");
        check(rig.arms[0].fingers.size() == 4 && rig.arms[1].thumbChain.size() == 4, "fingers and thumbs");
        check(rig.legs[1].foot == gb::byName(t, "RT_foot"), "leg roles");
        std::string hit;
        check(gb::protectedClear(t, rig, &hit), "protected: " + hit);
        // fx_root hung under the body would move with it: refused.
        auto under = t;
        under.parent[gb::byName(t, "fx_root")] = 1;
        check(!gb::protectedClear(under, rig, &hit) && hit == "fx_root", "fx_root under the body accepted");
        // An IK root (no name rule) hung under a hand: refused as a root under the body.
        auto ik = t;
        ik.parent[gb::byName(t, "LF_handIK")] = static_cast<int16_t>(rig.arms[0].hand);
        check(!gb::protectedClear(ik, rig, &hit), "an IK target under the hand accepted");
        auto lost = t;
        lost.hash[gb::byName(t, "LF_wrist")] = gb::nameHash("LF_wrist_renamed");
        lost.hash[gb::byName(t, "chest")] = gb::nameHash("torso");
        check(!gb::identify(lost, rig, &missing), "missing names accepted");
        bool wrist = false, chest = false;
        for (const auto& m : missing) {
            wrist |= m == "LF_wrist";
            chest |= m == "chest";
        }
        check(wrist && chest, "the missing names are named");
    });
    test("the solve: wrists on target, head joint in place, head hidden, feet down, protected joints untouched", [&] {
        gb::Table t;
        gb::parseJoints(bytes.joints, 237, t);
        std::vector<float> rest;
        gb::restToModel(bytes.rest, t, rest);
        body::Rig rig;
        check(gb::identify(t, rig), "identify");
        check(body::prepare(rig, rest), "prepare");
        body::Config c; // the spec's constants are body_ik's defaults
        check(c.blendSpeed == 4 && c.hideHead && c.lean == .25f && c.maxLean == .5f && c.neckShare == .5f &&
                  c.forearmRoll == .5f && c.groundedFoot == .3f && c.footReach == .15f,
              "the spec's constants");
        for (const float pitch : {0.f, -0.8726646f}) { // ahead, and looking 50 degrees down
            body::Targets tg;
            tg.head = true;
            tg.eyes = {0, 1.7f, .08f};
            tg.facing = Quat::yaw(3.14159265f) * Quat::around({1, 0, 0}, pitch);
            tg.hands[0] = {true, {.32f, 1.2f, .3f}, gripFacing({.2f, 0, 1}), 0, 0};
            tg.hands[1] = {true, {-.32f, 1.2f, .3f}, gripFacing({-.2f, 0, 1}), 0, 0};
            body::State s;
            std::vector<float> pose;
            body::Result r;
            // The game hands its pose over each frame; 4/s blends fully in by 0.25 s.
            for (int i = 0; i < 30; ++i) {
                pose = rest;
                body::Pose view(pose.data(), 237);
                r = body::solve(view, rig, tg, c, s, true, 1.f / 90);
            }
            check(r.solved && r.weight >= .999f, "blended in");
            for (int i = 0; i < 2; ++i) {
                const float err = length(at(pose, rig.arms[i].hand) - wristOf(tg.hands[i], i, c));
                std::printf("  pitch %.0f deg wrist %d error %.2f mm (solver %.2f mm)\n", pitch * 57.29578f, i,
                            err * 1000, r.handError[i] * 1000);
                check(err < .005f, "a wrist is off its target");
            }
            std::printf("  pitch %.0f deg head error %.3f mm\n", pitch * 57.29578f, r.headError * 1000);
            check(r.headError >= 0 && r.headError < .001f, "the head joint is off its place");
            const float headScale = length(Vec3{pose[rig.head * 16], pose[rig.head * 16 + 1], pose[rig.head * 16 + 2]});
            check(headScale < .002f, "the head is not shrunk");
            for (int i = 0; i < 2; ++i)
                check(at(pose, rig.legs[i].foot).y < .2f, "a foot left the ground");
            for (const char* name : {"root", "fx_root", "sync", "cameraTarget", "aimTarget", "LF_handIK",
                                     "RT_handIK", "LF_footIK", "RT_footIK"}) {
                const int k = gb::byName(t, name);
                check(!std::memcmp(&pose[k * 16], &rest[k * 16], 64), std::string("moved ") + name);
            }
        }
    });
    test("the head shrinks at once and the body blends at 4/s", [&] {
        gb::Table t;
        gb::parseJoints(bytes.joints, 237, t);
        std::vector<float> rest;
        gb::restToModel(bytes.rest, t, rest);
        body::Rig rig;
        gb::identify(t, rig);
        body::prepare(rig, rest);
        body::Targets tg;
        tg.head = true;
        tg.eyes = {0, 1.7f, .08f};
        tg.facing = Quat::yaw(3.14159265f);
        body::State s;
        auto pose = rest;
        body::Pose view(pose.data(), 237);
        const auto r = body::solve(view, rig, tg, {}, s, true, .1f);
        check(std::abs(r.weight - .4f) < .01f, "4/s: 0.4 after 0.1 s");
        const float headScale = length(Vec3{pose[rig.head * 16], pose[rig.head * 16 + 1], pose[rig.head * 16 + 2]});
        check(headScale < .002f, "the head did not shrink at once");
    });
    test("solve time on the 237-joint rig", [&] {
        gb::Table t;
        gb::parseJoints(bytes.joints, 237, t);
        std::vector<float> rest;
        gb::restToModel(bytes.rest, t, rest);
        body::Rig rig;
        gb::identify(t, rig);
        body::prepare(rig, rest);
        body::Targets tg;
        tg.head = true;
        tg.eyes = {0, 1.7f, .08f};
        tg.facing = Quat::yaw(3.14159265f);
        tg.hands[0] = {true, {.32f, 1.2f, .3f}, gripFacing({.2f, 0, 1}), 0, 0};
        tg.hands[1] = {true, {-.32f, 1.2f, .3f}, gripFacing({-.2f, 0, 1}), 0, 0};
        body::State s;
        std::vector<float> pose;
        const int frames = 2000;
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < frames; ++i) {
            pose = rest;
            body::Pose view(pose.data(), 237);
            body::solve(view, rig, tg, {}, s, true, 1.f / 90);
        }
        const double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / frames;
        std::printf("  %.4f ms per frame\n", ms);
        check(ms < 1.0, "the solve is too slow");
    });
    test("byte searches: PE sections, prologues, the writer's call, RTTI vtables", [] {
        // A tiny image: headers, a code section with a writer that calls a
        // conversion, a data section with an RTTI locator and its vtable.
        std::vector<uint8_t> image(0x8000, 0xCC);
        auto put32 = [&](size_t at, uint32_t v) { std::memcpy(&image[at], &v, 4); };
        auto put16 = [&](size_t at, uint16_t v) { std::memcpy(&image[at], &v, 2); };
        image[0] = 'M';
        image[1] = 'Z';
        put32(0x3c, 0x80);
        put32(0x80, 0x4550);
        put16(0x86, 2);    // sections
        put16(0x94, 0xf0); // optional header size
        const size_t sec = 0x80 + 24 + 0xf0;
        put32(sec + 8, 0x3000);
        put32(sec + 12, 0x1000);
        put32(sec + 36, 0x60000020); // code
        put32(sec + 40 + 8, 0x3000);
        put32(sec + 40 + 12, 0x4000);
        put32(sec + 40 + 36, 0x40000040); // data
        const auto secs = gb::sections(std::span(image).first(0x1000));
        check(secs.size() == 2 && secs[0].code && !secs[1].code && secs[1].rva == 0x4000, "sections");
        const uintptr_t w = 0x1100, m = 0x2200;
        std::memcpy(&image[w], gb::writerPrologue.data(), gb::writerPrologue.size());
        std::memcpy(&image[m], gb::toModelPrologue.data(), gb::toModelPrologue.size());
        image[w + 0x40] = 0xE8;
        put32(w + 0x41, static_cast<uint32_t>(static_cast<int32_t>(m - (w + 0x45))));
        // A decoy writer prologue that calls nothing.
        std::memcpy(&image[0x1800], gb::writerPrologue.data(), gb::writerPrologue.size());
        check(gb::calls(image, w, m) && !gb::calls(image, 0x1800, m), "calls");
        std::vector<gb::Section> code{secs[0]};
        gb::Functions f;
        check(gb::findFunctions(image, code, f) && f.writer == w && f.toModel == m && f.refound, "found again");
        // A second writer calling the conversion: not unique, refused.
        image[0x1800 + 0x30] = 0xE8;
        put32(0x1800 + 0x31, static_cast<uint32_t>(static_cast<int32_t>(m - (0x1800 + 0x35))));
        check(!gb::findFunctions(image, code, f), "two writers accepted");
        // RTTI: type descriptor at 0x5000 (name at +0x10), locator at 0x5100, vtable at 0x5208.
        const uint64_t base = 0x140000000ull;
        const char* name = ".?AVHeroLocal@Hero@@";
        std::memcpy(&image[0x5010], name, std::strlen(name) + 1);
        put32(0x5100, 1);
        put32(0x5104, 0);
        put32(0x5108, 0);
        put32(0x510c, 0x5000);
        put32(0x5110, 0);
        put32(0x5114, 0x5100);
        const uint64_t col = base + 0x5100;
        std::memcpy(&image[0x5200], &col, 8);
        check(gb::vtableByRtti(image, base, name) == 0x5208, "vtable by RTTI");
        const gb::Reader read = [&](uintptr_t rva, void* out, size_t n) {
            if (rva + n > image.size())
                return false;
            std::memcpy(out, &image[rva], n);
            return true;
        };
        check(gb::rttiNamed(read, base, 0x5208, ".?AVHeroLocal@Hero@@"), "RTTI names the vtable");
        check(!gb::rttiNamed(read, base, 0x5208, ".?AVMoverStandard@"), "another class accepted");
        check(!gb::rttiNamed(read, base, 0x5210, ".?AVHeroLocal@Hero@@"), "a wrong vtable accepted");
    });
    test("the gate: three passes in a row on one record; a failure or another record drops it", [] {
        gb::Gate g;
        check(!g.feed(0x10, nullptr) && !g.feed(0x10, nullptr) && g.feed(0x10, nullptr), "three passes");
        check(g.open(0x10) && !g.open(0x20) && !g.open(0), "open for its record only");
        check(!g.feed(0x10, "registry") && !g.open(0x10) && g.failed == "registry", "a failure drops it");
        g.feed(0x10, nullptr);
        g.feed(0x10, nullptr);
        g.feed(0x10, nullptr);
        check(!g.feed(0x20, nullptr) && !g.open(0x20) && !g.open(0x10), "another record starts over");
        check(!g.feed(0, nullptr), "no player");
    });
    // 0.0.72 audit of the multiplayer team's spec (t853u) against the merged build.
    test("spec 3: a command expires 250 ms after it came (longer leases are cut)", [] {
        static_assert(native_body::maxLeaseMs == 250);
        check(native_body::Command{}.leaseMs == 250, "the default lease");
        check(native_body::leaseOf(250) == 250 && native_body::leaseOf(500) == 250 && native_body::leaseOf(100) == 100 &&
                  native_body::leaseOf(0) == 0 && native_body::leaseOf(0xffffffffu) == 250,
              "the lease cap");
    });
    test("spec 3a: standing eye height up at once, down at most 1 cm/s", [] {
        namespace bc = body_calibration;
        check(bc::followEye(0, 1.7f, 1.f / 90) == 1.7f, "the first height");
        float e = 1.7f;
        for (int i = 0; i < 90; ++i) // one second crouched at 1.2 m
            e = bc::followEye(e, 1.2f, 1.f / 90);
        std::printf("  after 1 s crouched: %.4f m (from 1.7)\n", e);
        check(std::abs(e - 1.69f) < 1e-4f, "falls 1 cm in a second");
        for (int i = 0; i < 180; ++i) // two seconds standing on tiptoe at 1.8 m
            e = bc::followEye(e, 1.8f, 1.f / 90);
        check(std::abs(e - 1.8f) < 1e-3f, "rises at once");
        check(bc::followEye(1.7f, 3.f, .1f) == 1.7f && bc::followEye(1.7f, .5f, .1f) == 1.7f &&
                  bc::followEye(1.7f, std::nanf(""), .1f) == 1.7f && bc::followEye(1.7f, 1.6f, std::nanf("")) == 1.7f &&
                  bc::followEye(1.7f, 1.6f, -1) == 1.7f,
              "a bad height or frame time keeps it");
        check(bc::followEye(1.7f, 1.69999f, 1) == 1.69999f, "never below the headset");
    });
    test("spec 3a: arms scale 0.8-1.25 about the shoulder; uncalibrated body 0.85-1.2", [&] {
        check(body_calibration::minArmScale == .8f && body_calibration::maxArmScale == 1.25f, "the arm scale clamp");
        check(body_calibration::minUncalibratedScale == .85f && body_calibration::maxUncalibratedScale == 1.2f,
              "the uncalibrated clamp");
    });
    test("spec 3e: a rig without eye joints puts the eyes 0.1115 m ahead of the head joint, 0.0505 m up", [&] {
        gb::Table t;
        gb::parseJoints(bytes.joints, 237, t);
        std::vector<float> rest;
        gb::restToModel(bytes.rest, t, rest);
        body::Rig rig;
        check(gb::identify(t, rig), "identify");
        rig.eyes[0] = rig.eyes[1] = -1;
        check(body::prepare(rig, rest), "prepare");
        std::printf("  eyes from head: %.4f ahead, %.4f up, %.4f left\n", rig.eyesFromHead.x, rig.eyesFromHead.y,
                    rig.eyesFromHead.z);
        check(std::abs(rig.eyesFromHead.x - .1115f) < 1e-4f && std::abs(rig.eyesFromHead.y - .0505f) < 1e-4f &&
                  std::abs(rig.eyesFromHead.z) < 1e-4f,
              "the fallback eye point");
    });
    test("spec 3g: the palm faces from the rest-pose knuckles, ~30 degrees off down toward the thumb", [&] {
        gb::Table t;
        gb::parseJoints(bytes.joints, 237, t);
        std::vector<float> rest;
        gb::restToModel(bytes.rest, t, rest);
        body::Rig rig;
        check(gb::identify(t, rig), "identify");
        check(body::prepare(rig, rest), "prepare");
        std::vector<float> copy = rest;
        const body::Pose view(copy.data(), 237);
        for (int i = 0; i < 2; ++i) {
            const auto& a = rig.arms[i];
            const Vec3 palm = normalized(view.direction(a.hand, rig.handPalm[i]));
            // The knuckle line (index to little) across the four palm bones, as the spec says.
            Vec3 bones{};
            for (const auto& f : a.fingers)
                bones += normalized(view.position(f[1]) - view.position(f[0]));
            const Vec3 knuckles = view.position(a.fingers.back()[1]) - view.position(a.fingers.front()[1]);
            const Vec3 fingers = normalized(view.position(a.finger) - view.position(a.hand));
            Vec3 byKnuckles = cross(normalized(knuckles), normalized(bones));
            byKnuckles = normalized(byKnuckles - fingers * dot(byKnuckles, fingers));
            if (dot(byKnuckles, palm) < 0)
                byKnuckles = byKnuckles * -1.f;
            const float offDown = std::acos(std::clamp(-palm.y, -1.f, 1.f)) * 57.29578f;
            const Vec3 thumb = view.position(a.thumb) - view.position(a.hand);
            std::printf("  hand %d: palm %.1f deg off down, %.2f deg from the knuckles'\n", i, offDown,
                        std::acos(std::clamp(dot(palm, byKnuckles), -1.f, 1.f)) * 57.29578f);
            check(dot(palm, byKnuckles) > .9999f, "the palm is not the knuckles'");
            check(offDown > 15 && offDown < 45, "the palm is not ~30 degrees off down");
            check(dot(palm - Vec3{0, -1, 0} * -palm.y, thumb) > 0, "the palm does not lean toward the thumb");
        }
    });
    test("spec 3 fallback: a glove on each controller while the hero is not drawn", [] {
        namespace go = glove_overlay;
        check(go::wanted(true, true, false, false), "GAME BODY on, body not drawn: gloves");
        check(!go::wanted(true, true, false, true) && !go::wanted(false, true, false, false) &&
                  !go::wanted(true, false, false, false) && !go::wanted(true, true, true, false),
              "no gloves when drawn, OFF, another hero's look or the flat screen");
        std::vector<Vertex> v;
        const Pose grip{{.2f, 1.2f, -.3f}, Quat::yaw(.4f)};
        const size_t open = go::append(v, grip, false, 0, {0, 1.6f, 0}, .001f);
        check(open > 0 && open == v.size(), "an open glove");
        for (const auto& x : v)
            check(std::isfinite(x.position.x) && std::isfinite(x.position.y) && std::isfinite(x.position.z) &&
                      length(x.position - grip.position) < .2f,
                  "a glove vertex is off the hand");
        std::vector<Vertex> closed;
        check(go::append(closed, grip, true, 1, {0, 1.6f, 0}, .001f) > 0, "a fist");
        std::vector<Vertex> none;
        check(go::append(none, Pose{{0, 1, 0}, Quat{0, 0, 0, 0}}, false, 0, {0, 1.6f, 0}, .001f) == 0 &&
                  go::append(none, Pose{{std::nanf(""), 1, 0}, Quat{}}, false, 0, {0, 1.6f, 0}, .001f) == 0 &&
                  none.empty(),
              "a bad pose draws nothing");
        check(go::append(none, grip, false, std::nanf(""), {0, 1.6f, 0}, std::nanf("")) == 0,
              "a bad pixel size draws nothing");
        check(go::append(none, grip, false, std::nanf(""), {0, 1.6f, 0}, .001f) > 0, "a bad fist is open");
    });
    std::cout << total - failed << '/' << total << " game body tests passed\n";
    return failed ? 1 : 0;
}
