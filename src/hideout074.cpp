// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
// Version 0.0.74 test 1 HIDEOUTS (hideout074.hpp).
#include "spidy/hideout074.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace spidy::hideout074 {
namespace {
constexpr float pi = 3.14159265358979f;
bool fine(float f) { return std::isfinite(f); }
bool fine(Vec3 v) { return finite(v); }
float angleOf(Vec3 v) { return std::atan2(-v.z, v.x); } // Quat::yaw(angleOf(v)) takes +x to v's direction
} // namespace

Action actionOf(const std::string& name) {
    if (name.rfind("return_to_city", 0) == 0 || name.rfind("exit_to", 0) == 0 || name == "to_vehicle")
        return Action::toVehicle;
    if (name.empty() || name == "none")
        return Action::none;
    return Action::comingSoon;
}

bool Room::ok() const {
    return cols > 0 && rows > 0 && floorCm.size() == static_cast<size_t>(cols) * static_cast<size_t>(rows) && fine(cell) && cell > .01f &&
           fine(gridX) && fine(gridZ) && fine(spawn) && fine(spawnYaw) && fine(killY);
}

bool Room::floorAt(float x, float z, float& y) const {
    if (!fine(x) || !fine(z) || cols <= 0 || rows <= 0)
        return false;
    const float fi = (x - gridX) / cell, fj = (z - gridZ) / cell;
    if (!(fi >= 0 && fj >= 0 && fi < static_cast<float>(cols) && fj < static_cast<float>(rows)))
        return false;
    const int16_t h = floorCm[static_cast<size_t>(fj) * static_cast<size_t>(cols) + static_cast<size_t>(fi)];
    if (h == blocked)
        return false;
    y = h / 100.f;
    return true;
}

namespace {
bool parseInto(const std::string& text, Room& out, std::string& error) {
    out = {};
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line.rfind("HEROVR-HIDEOUT 1", 0) != 0) {
        error = "not a HERO VR hideout room (room.txt)";
        return false;
    }
    bool spawn = false, grid = false;
    int row = 0;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::istringstream l(line);
        std::string key;
        l >> key;
        if (key == "name") {
            std::getline(l >> std::ws, out.name);
        } else if (key == "spawn") {
            l >> out.spawn.x >> out.spawn.y >> out.spawn.z >> out.spawnYaw;
            spawn = !l.fail();
        } else if (key == "point") {
            Point p;
            std::string action;
            l >> p.id >> action >> p.at.x >> p.at.y >> p.at.z >> p.radius;
            if (l.fail() || !fine(p.at) || !fine(p.radius) || p.radius <= 0 || p.radius > 20) {
                error = "a bad point line";
                return false;
            }
            std::getline(l >> std::ws, p.prompt);
            p.action = actionOf(action);
            if (out.points.size() < 32)
                out.points.push_back(std::move(p));
        } else if (key == "killy") {
            l >> out.killY;
        } else if (key == "grid") {
            l >> out.gridX >> out.gridZ >> out.cell >> out.cols >> out.rows;
            if (l.fail() || out.cols <= 0 || out.rows <= 0 || out.cols > 2000 || out.rows > 2000) {
                error = "a bad grid line";
                return false;
            }
            out.floorCm.assign(static_cast<size_t>(out.cols) * static_cast<size_t>(out.rows), blocked);
            grid = true;
        } else if (key == "row") {
            if (!grid || row >= out.rows) {
                error = "a row outside the grid";
                return false;
            }
            for (int i = 0; i < out.cols; ++i) {
                std::string v;
                if (!(l >> v)) {
                    error = "a short grid row";
                    return false;
                }
                if (v != ".") {
                    char* end = nullptr;
                    const long cm = std::strtol(v.c_str(), &end, 10);
                    if (!end || *end || cm < -2000 || cm > 2000) {
                        error = "a bad grid cell";
                        return false;
                    }
                    out.floorCm[static_cast<size_t>(row) * static_cast<size_t>(out.cols) + static_cast<size_t>(i)] = static_cast<int16_t>(cm);
                }
            }
            ++row;
        }
        // bounds and anything unknown: skipped (newer tools may add lines)
    }
    if (!spawn || !grid || row != out.rows || !out.ok()) {
        error = !spawn ? "no spawn" : !grid ? "no walk grid" : row != out.rows ? "the grid's rows are missing" : "bad numbers";
        return false;
    }
    float y = 0;
    if (!out.floorAt(out.spawn.x, out.spawn.z, y)) {
        error = "the spawn is not on the walk grid";
        return false;
    }
    return true;
}
} // namespace

bool parseRoom(const std::string& text, Room& out, std::string& error) {
    try {
        if (parseInto(text, out, error))
            return true;
    } catch (...) {
        error = "the room did not read";
    }
    out = {}; // a refused room is empty (never half read)
    return false;
}

Vec3 Place::world(Vec3 r) const { return Quat::yaw(yaw).rotate(r) + at; }
Vec3 Place::room(Vec3 w) const { return Quat::yaw(-yaw).rotate(w - at); }
Vec3 Place::worldDir(Vec3 d) const { return Quat::yaw(yaw).rotate(d); }

void Visit::toSpawn(Vec3 head, Vec3 forward) {
    Vec3 f{forward.x, 0, forward.z};
    if (!fine(f) || length(f) < 1e-4f)
        f = {0, 0, -1};
    const float s = room_->spawnYaw * pi / 180.f;
    place_.yaw = angleOf(f) - s;
    // the spawn under the eye, its floor on the real floor
    const Vec3 sp = Quat::yaw(place_.yaw).rotate(room_->spawn);
    place_.at = Vec3{head.x - sp.x, floorY_ - room_->spawn.y, head.z - sp.z};
    feet_ = room_->spawn;
    floorShown_ = room_->spawn.y;
}

bool Visit::begin(Which which, const Room& room, Vec3 head, Vec3 forward, float floorY) {
    end();
    if (which == Which::none || !room.ok() || !fine(head) || !fine(floorY))
        return false;
    room_ = &room;
    floorY_ = floorY;
    toSpawn(head, forward);
    if (!fine(place_.at) || !fine(place_.yaw)) {
        end();
        return false;
    }
    which_ = which;
    turnHeld_ = pressWas_ = false;
    return true;
}

void Visit::end() {
    which_ = Which::none;
    room_ = nullptr;
    place_ = {};
    feet_ = {};
}

Out Visit::step(const In& in) {
    Out o;
    if (!active() || !room_)
        return o;
    const Room& r = *room_;
    const float dt = fine(in.dt) ? std::clamp(in.dt, 0.f, .1f) : 0.f;
    if (!fine(in.head) || !fine(place_.at) || !fine(place_.yaw)) {
        if (fine(in.head)) {
            toSpawn(in.head, in.headForward);
            o.respawned = true;
        }
        return o;
    }
    // the right stick: snap turns about the eye
    const float tx = fine(in.turnX) ? in.turnX : 0.f;
    if (!turnHeld_ && std::abs(tx) > turnPress) {
        turnHeld_ = true;
        const float a = (tx > 0 ? -1.f : 1.f) * snapDegrees * pi / 180.f; // right: clockwise from above
        const Vec3 off = in.head - place_.at;
        place_.at = in.head - Quat::yaw(a).rotate(off);
        place_.yaw += a;
        if (place_.yaw > pi)
            place_.yaw -= 2 * pi;
        if (place_.yaw < -pi)
            place_.yaw += 2 * pi;
    } else if (turnHeld_ && std::abs(tx) < turnRelease)
        turnHeld_ = false;
    // where the eye stands in the room now (real walking moves it)
    Vec3 q = place_.room(in.head);
    float y = 0;
    const auto standable = [&](float x, float z, float from) {
        float h = 0;
        return r.floorAt(x, z, h) && h - from <= stepUp ? h : -1e9f;
    };
    // real walking into a wall: the room is pushed back so the eye stays where it last could
    if (!r.floorAt(q.x, q.z, y) || y - feet_.y > stepUp) {
        q.x = feet_.x;
        q.z = feet_.z;
    }
    // the left stick, along the eye's heading
    float mx = fine(in.moveX) ? std::clamp(in.moveX, -1.f, 1.f) : 0.f, my = fine(in.moveY) ? std::clamp(in.moveY, -1.f, 1.f) : 0.f;
    const float m = std::sqrt(mx * mx + my * my);
    if (m > stickDead && dt > 0) {
        const float share = std::min(1.f, (m - stickDead) / (1 - stickDead)) / m;
        Vec3 f{in.headForward.x, 0, in.headForward.z};
        f = fine(f) && length(f) > 1e-4f ? normalized(f) : Vec3{0, 0, -1};
        const Vec3 right{-f.z, 0, f.x};
        const Vec3 wmove = (f * my + right * mx) * (share * walkSpeed * dt);
        const Vec3 rm = Quat::yaw(-place_.yaw).rotate(wmove);
        const float from = r.floorAt(q.x, q.z, y) ? y : feet_.y;
        // slide: both axes, then each alone
        if (standable(q.x + rm.x, q.z + rm.z, from) > -1e8f)
            q.x += rm.x, q.z += rm.z;
        else if (standable(q.x + rm.x, q.z, from) > -1e8f)
            q.x += rm.x;
        else if (standable(q.x, q.z + rm.z, from) > -1e8f)
            q.z += rm.z;
    }
    if (!r.floorAt(q.x, q.z, y)) { // lost (should not be): the spawn
        toSpawn(in.head, in.headForward);
        o.respawned = true;
        return o;
    }
    // the floor followed (steps, ramps), eased so a step is not a jolt
    floorShown_ += (y - floorShown_) * std::min(1.f, dt * 12.f);
    feet_ = Vec3{q.x, y, q.z};
    // put the eye at q: the room moves under him
    const Vec3 wq = Quat::yaw(place_.yaw).rotate(Vec3{q.x, 0, q.z});
    place_.at = Vec3{in.head.x - wq.x, floorY_ - floorShown_, in.head.z - wq.z};
    if (!fine(place_.at) || place_.room(in.head).y < r.killY) {
        toSpawn(in.head, in.headForward);
        o.respawned = true;
        return o;
    }
    // the point in reach (the nearest)
    float best = 1e9f;
    for (size_t k = 0; k < r.points.size(); ++k) {
        const Point& p = r.points[k];
        const float d = std::hypot(p.at.x - q.x, p.at.z - q.z);
        if (d <= p.radius && d < best) {
            best = d;
            o.point = static_cast<int>(k);
        }
    }
    if (in.press && o.point >= 0) {
        o.act = r.points[static_cast<size_t>(o.point)].action;
        o.leave = o.act == Action::toVehicle;
    }
    if (in.exit)
        o.leave = true;
    return o;
}

bool appendRoom(std::vector<Vertex>& sink, const mesh_model::Mesh& model, const Place& place, size_t budget) {
    if (!model.ok() || !fine(place.at) || !fine(place.yaw) || sink.size() + size_t{model.triangles()} * 3 > budget)
        return false;
    const size_t from = sink.size();
    try {
        const Quat q = Quat::yaw(place.yaw);
        const Vec3 ex = q.rotate({1, 0, 0}), ey = q.rotate({0, 1, 0}), ez = q.rotate({0, 0, 1}), o = place.at;
        const bool uvs = model.hasUvs();
        const Vec3 light = normalized(Vec3{.3f, 1.f, .25f});
        sink.reserve(from + size_t{model.triangles()} * 3);
        for (uint32_t t = 0; t < model.triangles(); ++t) {
            const int16_t im = uvs && t < model.image.size() ? model.image[t] : int16_t{-1};
            const int32_t at = im >= 0 && static_cast<size_t>(im) < model.images.size() ? model.images[static_cast<size_t>(im)].layer : -1;
            const float layer = at >= 0 ? static_cast<float>(at) : -1.f;
            const Vec3 a0 = model.corners[t * 3], b0 = model.corners[t * 3 + 1], c0 = model.corners[t * 3 + 2];
            Vec3 colour;
            if (at >= 0)
                colour = t < model.tints.size() ? model.tints[t] : Vec3{1, 1, 1}; // baked: unlit
            else {
                const Vec3 n = normalized(cross(b0 - a0, c0 - a0));
                colour = model.colors[t] * (.55f + .45f * std::abs(dot(n, light)));
            }
            const Vec3 p[3]{a0, b0, c0};
            for (uint32_t k = 0; k < 3; ++k) {
                const Vec3 w = o + ex * p[k].x + ey * p[k].y + ez * p[k].z;
                const float u = uvs ? model.uvs[t * 6 + k * 2] : 0.f, v = uvs ? model.uvs[t * 6 + k * 2 + 1] : 0.f;
                sink.push_back(Vertex{w, colour, 1.f, {u, v, layer, 1.f}});
            }
        }
        for (size_t i = from; i < sink.size(); ++i)
            if (!fine(sink[i].position) || !fine(sink[i].color) || !fine(sink[i].shape.x) || !fine(sink[i].shape.y)) {
                sink.resize(from);
                return false;
            }
        return true;
    } catch (...) {
        sink.resize(from);
        return false;
    }
}

namespace {
// A box's six faces (inward and outward: any winding), opaque black.
void box(std::vector<Vertex>& out, const Vec3 c[8], Vec3 colour) {
    static const int f[6][4] = {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1}, {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}};
    for (const auto& q : f) {
        for (const int i : {q[0], q[1], q[2], q[0], q[2], q[3]})
            out.push_back(Vertex{c[i], colour, 1.f, {}});
    }
}
} // namespace

bool appendBackdrop(std::vector<Vertex>& out, const mesh_model::Mesh& mesh, const Place& place, Vec3 eye, float margin, float most) {
    if (!mesh.ok() || !fine(place.at) || !fine(eye) || !fine(margin) || !fine(most))
        return false;
    const Vec3 lo = mesh.low - Vec3{margin, margin, margin}, hi = mesh.high + Vec3{margin, margin, margin};
    Vec3 c[8];
    for (int i = 0; i < 8; ++i) {
        const Vec3 r{i & 1 ? hi.x : lo.x, i & 2 ? hi.y : lo.y, i & 4 ? hi.z : lo.z};
        c[i] = place.world(r);
        const Vec3 d = c[i] - eye;
        const float l = length(d);
        if (!fine(c[i]))
            return false;
        if (l > most) // pulled in toward the eye: the far plane would cut it
            c[i] = eye + d * (most / l);
    }
    box(out, c, Vec3{0, 0, 0});
    return true;
}

void appendFallbackBox(std::vector<Vertex>& out, Vec3 eye, float floorY, float half) {
    if (!fine(eye) || !fine(floorY) || !fine(half))
        return;
    Vec3 c[8];
    for (int i = 0; i < 8; ++i)
        c[i] = Vec3{eye.x + (i & 1 ? half : -half), i & 2 ? floorY + 2 * half : floorY - .02f, eye.z + (i & 4 ? half : -half)};
    box(out, c, Vec3{.004f, .004f, .005f});
    const Vec3 g{.05f, .05f, .05f};
    const Vec3 a{eye.x - half, floorY, eye.z - half}, b{eye.x + half, floorY, eye.z - half}, d{eye.x + half, floorY, eye.z + half},
        e{eye.x - half, floorY, eye.z + half};
    for (const Vec3 p : {a, b, d, a, d, e})
        out.push_back(Vertex{p, g, 1.f, {}});
}

} // namespace spidy::hideout074
