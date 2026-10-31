// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder. 0.0.74 test 10 MIRROR PROPS: what of the 0.0.72 look the mirror shows
// (mirror_props074.hpp heroLook), that it draws (the hammer into the model sink, sheathed too), and the
// model sink's miss counter (hero007::modelMisses) with its log pacing (mirror_props::MissLog).
#include "spidy/construct_fist.hpp"
#include "spidy/hero007_visual.hpp"
#include "spidy/hero072_visual.hpp"
#include "spidy/mirror_props074.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

using namespace spidy;

namespace {
int failures = 0, checks = 0;
void check(bool ok, const char* what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what);
    }
}

hero072::Look thorLook(const mesh_model::Mesh* model) {
    hero072::Look l{};
    l.any = true;
    l.hammer = true;
    l.hammerModel = model;
    l.hammerAt = {0, 1.2f, -1.f};
    l.hammerForward = {0, 1, 0};
    l.hammerUp = {0, 0, -1};
    return l;
}
} // namespace

int main() {
    const mesh_model::Mesh& fist = construct_fist::mesh(); // any small real model stands in for the hammer
    check(fist.ok(), "the stand-in model is usable");
    const Vec3 viewer{0, 1.6f, 0};

    { // what is kept and what is left out
        hero072::Look l = thorLook(&fist);
        l.hammerSheathed = true;
        l.bolts = 3;
        l.boltFrom[2] = {1, 2, 3};
        l.boltTo[2] = {4, 5, 6};
        l.boltSeed[2] = .5f;
        l.ring[1] = true;
        l.ringAt[1] = {.2f, 1.f, -.3f};
        l.beam = l.bubble = l.chain = l.fist = true;
        l.fistAlpha = .7f;
        l.bubbleRadius = 1.5f;
        l.hud = l.lock = l.aimHint = l.charge = l.van = l.spray = l.shock = l.hook = l.perchLight = l.rope = true;
        l.vanModel = &fist;
        l.handleMarkAlpha = 1.f;
        const hero072::Look m = mirror_props::heroLook(l);
        check(m.any && m.hammer && m.hammerModel == &fist, "the hammer kept");
        check(!m.hammerSheathed, "a sheathed hammer is drawn where it hangs in the mirror");
        check(m.bolts == 3 && m.boltFrom[2].x == 1 && m.boltTo[2].z == 6 && m.boltSeed[2] == .5f, "the lightning kept");
        check(m.ring[1] && !m.ring[0] && m.ringAt[1].z == -.3f, "the lantern ring kept");
        check(m.beam && m.bubble && m.chain && m.fist && m.fistAlpha == .7f && m.bubbleRadius == 1.5f, "the constructs kept");
        check(!m.hud && !m.lock && !m.aimHint && !m.charge, "the player's own HUD bits left out");
        check(!m.van && !m.vanModel && m.handleMarkAlpha == 0, "the van left out");
        check(!m.spray && !m.shock && !m.hook && !m.perchLight && !m.rope && !m.blood, "the rest left out");
    }
    { // nothing to show: nothing
        check(!mirror_props::heroLook(hero072::Look{}).any, "an empty look stays empty");
        hero072::Look l = thorLook(nullptr); // MODEL MISSING: no stand-in
        check(!mirror_props::heroLook(l).any, "a hammer without its model is not shown");
        hero072::Look v{};
        v.any = v.van = v.hud = true;
        v.vanModel = &fist;
        check(!mirror_props::heroLook(v).any, "only a van and a HUD: nothing in the mirror");
        hero072::Look b{};
        b.any = true;
        b.bolts = 99;
        check(mirror_props::heroLook(b).bolts == hero072::boltsMost, "the bolts clamped");
    }
    { // drawn: the hammer into the model sink, also sheathed (first person still skips a sheathed one)
        std::vector<Vertex> sink, soft;
        hero007::setModelSink(&sink);
        hero072::Look l = thorLook(&fist);
        l.hammerSheathed = true;
        hero072::appendLook(soft, l, {}, viewer, .001f);
        check(sink.empty(), "first person: no sheathed hammer");
        hero072::appendLook(soft, mirror_props::heroLook(l), {}, viewer, .001f);
        check(!sink.empty(), "the mirror: the sheathed hammer drawn");
        const size_t one = sink.size();
        sink.clear();
        hero072::Look held = thorLook(&fist);
        held.hammerInHand = true;
        hero072::appendLook(soft, mirror_props::heroLook(held), {}, viewer, .001f);
        check(sink.size() == one, "the hammer in the fist drawn the same");
        hero007::setModelSink(nullptr);
    }
    { // the miss counter: a full sink and a bad place are each counted once
        std::vector<Vertex> sink;
        hero007::setModelSink(&sink);
        const hero007::ModelMisses a = hero007::modelMisses();
        sink.resize(hero007::modelSinkBudget - 1);
        const bool drew = hero007::appendPlacedModel(sink, fist, {0, 1.2f, -1.f}, {0, 0, -1}, {0, 1, 0}, viewer, 1.f);
        const hero007::ModelMisses b = hero007::modelMisses();
        check(!drew && sink.size() == hero007::modelSinkBudget - 1, "over budget: not drawn");
        check(b.full == a.full + 1 && b.badPlace == a.badPlace, "over budget: one 'full' miss");
        sink.clear();
        sink.shrink_to_fit();
        check(hero007::appendPlacedModel(sink, fist, {0, 1.2f, -1.f}, {0, 0, -1}, {0, 1, 0}, viewer, 1.f), "room: drawn");
        const hero007::ModelMisses c = hero007::modelMisses();
        check(c.full == b.full && c.badPlace == b.badPlace, "a drawn model is no miss");
        hero007::setModelSink(nullptr);
    }
    { // the log pacing: the first rise at once, then at most every gap, the count since the last line
        mirror_props::MissLog log;
        check(!log.due(0, 1000), "no misses: no line");
        check(log.due(2, 1000) && log.take() == 2, "the first miss logged at once");
        check(!log.due(2, 1500), "no new miss: no line");
        check(!log.due(5, 5000), "a rise within the gap waits");
        check(log.due(7, 11001) && log.take() == 5, "after the gap: the rise since the last line");
        check(!log.due(7, 30000), "nothing new after it");
    }
    std::printf("mirror props tests: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
