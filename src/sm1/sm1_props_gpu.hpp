#pragma once
// HERO VR 0.0.76 SM1 HERO PROPS: the D3D12 side (Windows only, the SM1 layer's). The heroes' models (the SM2 build's,
// THIRD-PARTY-MODELS.md) are read from HERO VR's Models folder and put on the GPU by a thread of their own; each frame
// the props of sm1_props.hpp's scene are drawn into one swapchain of the layer's own (both eyes side by side, with a
// depth buffer of its own so a prop hides its own back), and handed back as ONE extra projection layer for xrEndFrame,
// after his. Not his depth: the props always draw over the game's picture (a wall never hides them).
// Every D3D12 / OpenXR call is checked: a failure logs one line and the props stop drawing (his frame goes on as is).
#include "spidy/sm1_props.hpp"

#include <windows.h>
#include <d3d12.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <cstdint>
#include <string>
#include <vector>

namespace spidy::sm1::propsgpu {

// The next layer's swapchain functions (the props' swapchain never goes through the layer's own hooks).
struct Xr {
    PFN_xrEnumerateSwapchainFormats enumerateFormats = nullptr;
    PFN_xrCreateSwapchain createSwapchain = nullptr;
    PFN_xrDestroySwapchain destroySwapchain = nullptr;
    PFN_xrEnumerateSwapchainImages enumerateImages = nullptr;
    PFN_xrAcquireSwapchainImage acquireImage = nullptr;
    PFN_xrWaitSwapchainImage waitImage = nullptr;
    PFN_xrReleaseSwapchainImage releaseImage = nullptr;
};
using Log = void (*)(const char* line);
// Once, before anything else: the functions, the log, the image decoder (WIC, the caller's).
void setup(const Xr& xr, Log log, mesh_model::Decoder decoder);

// The hero's models, from modelsRoot (HERO VR's Models folder, "...\Models\"), loaded on a thread of their own; a new
// hero's set replaces the old one when it is ready. Cheap when nothing changed (call it every frame).
void want(uint32_t hero, const std::wstring& modelsRoot, ID3D12Device* device);
// The loaded set's bounds (ok false where a model is not on the GPU yet, or failed).
void bounds(props::Bounds (&out)[props::modelCount]);
// True when the model is ready to draw (its set loaded, the renderer not turned off).
bool ready(props::Model m);

// Draws `items` for his projection layer `his` (its views' poses and fovs, its space). True: `out` (and the views it
// points at, kept here until the next call) is the layer to append after his. False: nothing to add this frame.
bool render(XrSession session, ID3D12Device* device, ID3D12CommandQueue* queue, const XrCompositionLayerProjection& his,
            const std::vector<props::Item>& items, XrCompositionLayerProjection& out);
// The runtime refused the props' layer (or a fault): no more props this session.
void turnOff(const char* why);
bool on();
// The session ended: waits for the GPU (bounded), releases everything (the next session starts afresh).
void drop();

} // namespace spidy::sm1::propsgpu
