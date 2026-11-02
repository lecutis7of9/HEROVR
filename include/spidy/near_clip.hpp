#pragma once

// NEAR CLIP (vr_settings::Values::nearClipMm): how near the eyes the headset
// still shows things in immersive VR. The game's own camera, behind the hero,
// clips at 10 cm, which cut off hands held up before the face.
namespace spidy::near_clip {
// From the VR settings (game_xr.cpp), as they start and whenever they change.
void set(int millimetres);
// 0.0.76 roll fix (fly076::RollFrame::nearClip): while the flight is rolled far over the near plane steps out to
// at least this, metres (0: NEAR CLIP's own), so the chest never fills the view. Set every frame by the XR worker.
void boost(float metres);
// For the head's and the eyes' lenses (stereo_probe.cpp), metres: the larger of NEAR CLIP's and the boost.
float metres();
} // namespace spidy::near_clip
