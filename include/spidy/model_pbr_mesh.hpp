#pragma once
// HERO VR 0.0.78 PBR MATERIALS: the glue between a loaded model (mesh_model::Mesh: its Surface per triangle, its
// tangents) and the corner Surface the model emitters write (model_pbr.hpp). CONFIRMED by tests/model_pbr_tests.cpp.
#include "spidy/mesh_model.hpp"
#include "spidy/model_pbr.hpp"

namespace spidy::model_pbr {

// Whether this frame's model corners take PBR: the switch on and the game's world light (STUDIO: the 0.0.78 look).
inline bool drawsPbr(const world_light::Light& l) { return enabled() && l.world; }

// Triangle t's material with its layers resolved: the maps only when the renderer bound them (mapsReady), the HQ
// copies only when it bound those (hqReady). A model without surfaces: the glTF defaults (rough 1, dielectric).
inline Material material(const mesh_model::Mesh& m, uint32_t t) {
    Material out;
    if (m.hasSurfaces() && t < m.surface.size() && m.surface[t] < m.surfaces.size()) {
        const auto& s = m.surfaces[m.surface[t]];
        out.roughness = std::clamp(s.roughness, 0.f, 1.f);
        out.metallic = std::clamp(s.metallic, 0.f, 1.f);
        out.normalScale = s.normalScale;
        out.occlusionStrength = std::clamp(s.occlusionStrength, 0.f, 1.f);
        if (mapsReady()) {
            out.normalLayer = s.normalLayer;
            out.ormLayer = s.ormLayer;
            out.occlusion = s.ormLayer >= 0 && s.occlusionImage >= 0;
        }
        if (hqReady() && mapsReady())
            out.hqNormalLayer = s.hqNormalLayer;
    }
    if (hqReady() && t < m.image.size()) {
        const int16_t im = m.image[t];
        if (im >= 0 && static_cast<size_t>(im) < m.images.size() && m.images[static_cast<size_t>(im)].layer >= 0)
            out.hqBaseLayer = m.images[static_cast<size_t>(im)].hqLayer;
    }
    return out;
}

// Corner c's model-space tangent (xyz) and its sign; false where it has none.
inline bool tangentOf(const mesh_model::Mesh& m, size_t c, Vec3& t, float& w) {
    if (!m.hasTangents() || c * 4 + 3 >= m.tangents.size())
        return false;
    const float* q = &m.tangents[c * 4];
    if (q[3] == 0)
        return false;
    t = {q[0], q[1], q[2]};
    w = q[3] < 0 ? -1.f : 1.f;
    return true;
}

// A corner's Surface. `n` the world normal it is shaded with (unit; `turned`: turned round to the side he sees),
// `tw` its world tangent (any length; zero: none) with sign `w` already in world handedness.
inline VertexSurface corner(const Material& mat, Vec3 tint, Vec3 n, Vec3 tw, float w, bool turned) {
    const float l = length(tw);
    const bool has = l > 1e-12f && std::isfinite(l);
    return surface(mat, tint, n, has ? tw * (1.f / l) : Vec3{}, turned ? -w : w, hqReady());
}

// The handedness of a model-to-world map's axes (its determinant's sign).
inline float handedness(Vec3 ex, Vec3 ey, Vec3 ez) { return dot(cross(ex, ey), ez) < 0 ? -1.f : 1.f; }

} // namespace spidy::model_pbr
