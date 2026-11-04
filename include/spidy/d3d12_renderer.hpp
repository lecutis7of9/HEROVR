#pragma once
#include "math.hpp"
#include "vertex.hpp"
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <span>
#include <string>
#include <vector>
#include <windows.h>
#include <wrl/client.h>

namespace spidy {
template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;
// HERO VR 0.0.78 cut13: the model's and the calibration art's root signatures, serialized without a device so the
// tests check them (cut12's model root said 3 parameters of an array of 2: E_INVALIDARG, every model UNTEXTURED).
// The model heap: 0 the colours, 1-2 the eye depth, 3 the maps, 4 the HQ colours, 5 the HQ maps; the art heap: its atlas.
inline constexpr unsigned modelHeapDescriptors = 6, artHeapDescriptors = 1;
HRESULT serializeModelRoot(ComPtr<ID3DBlob>& signature, ComPtr<ID3DBlob>& error) noexcept;
HRESULT serializeArtRoot(ComPtr<ID3DBlob>& signature, ComPtr<ID3DBlob>& error) noexcept;
class D3D12Renderer {
  public:
    ~D3D12Renderer();
    void initialize(LUID adapter, D3D_FEATURE_LEVEL minimum);
    void initialize(ID3D12Device* device, ID3D12CommandQueue* queue);
    void render(ID3D12Resource* target, DXGI_FORMAT format, unsigned width, unsigned height,
                const Mat4& viewProjection, std::span<const Vertex> vertices, bool preserveColor = false,
                std::span<const Vertex> translucent = {});
    struct ViewTarget {
        ID3D12Resource* texture{};
        DXGI_FORMAT format{};
        unsigned width{}, height{};
        Mat4 viewProjection{};
    };
    // Slow motion's look (slow_motion.hpp), recoloured into each view's image
    // under the overlay: colour drained toward grey, a cool cast and a darker
    // rim by `amount`, and a ring of light that bends the image a little as it
    // crosses the view outward after slow motion starts or ends.
    struct Grade {
        float amount{};         // 0..1
        float ripple = -1;      // 0..1 of the ring's way out; below 0 none
        float rippleStrength{}; // 0..1
        bool entering{};        // the ring follows a start (cool) or an end (pale)
        // Each view's lens: the tangents of its field of view, left, right,
        // up and down (left and down negative).
        std::array<std::array<float, 4>, 2> tangents{};
        bool visible() const {
            return amount > .002f || (ripple >= 0 && ripple < 1 && rippleStrength > 0);
        }
    };
    // One upload, command list, and submission for a complete eye pair. When
    // asynchronous, the next reuse waits for this submission's own fence only.
    // `translucent` is drawn after the other vertices, in its own order, each
    // vertex blended by its opacity: nearer overlay geometry hides it, but it
    // never hides anything itself (it leaves the depth buffer alone). A
    // visible `grade` recolours each preserved image before the overlay.
    // Update 36: `art` is drawn between the two, like the translucent
    // vertices, but each vertex's colour and opacity multiplied by the art
    // texture (setArt) at its shape.xy; without the texture it is left out.
    // Version 0.0.72: `textured` (the drop-in models) right after the depth-writing vertices, depth
    // tested and written, each colour times the model texture array (setModelTextures) at shape.xyz
    // (u, v, layer; layer < 0: the colour alone), texels under half alpha cut out; without the
    // array it is left out.
    void renderViews(std::span<const ViewTarget>, std::span<const Vertex>, bool preserveColor,
                     bool waitForCompletion = true, std::span<const Vertex> translucent = {},
                     const Grade* grade = nullptr, std::span<const Vertex> art = {},
                     std::span<const Vertex> textured = {});
    // Update 36: the art texture (the calibration panel's pictures), each
    // level half the last, RGBA8 rows from the top, display-encoded (sRGB).
    // Uploaded once, waiting for the GPU. False, with artFault() saying why,
    // if it can't be used; nothing else changes.
    struct ArtLevel {
        unsigned width{}, height{};
        const unsigned char* rgba{};
    };
    bool setArt(std::span<const ArtLevel> levels) noexcept;
    bool artReady() const {
        return art_ && artFault_.empty();
    }
    const std::string& artFault() const {
        return artFault_;
    }
    // Version 0.0.72 REAL MODELS: the drop-in models' base-colour textures, one sRGB Texture2DArray
    // (`layers` each `levels` mips of size >> level squared RGBA8, level 0 first). Uploaded once,
    // waiting for the GPU. False, with modelFault() saying why, if it can't be used; then renderViews
    // leaves its `textured` vertices out and the caller draws the models untextured.
    bool setModelTextures(unsigned size, unsigned levels, std::span<const std::vector<unsigned char>> layers) noexcept;
    bool modelReady() const {
        return model_ && modelFault_.empty();
    }
    const std::string& modelFault() const {
        return modelFault_;
    }
    // HERO VR 0.0.78 PBR MATERIALS (model_pbr.hpp): the models' material maps beside the base colours, uploaded
    // once after setModelTextures (which drops them), waiting for the GPU. `linear` (UNORM: the normal maps and the
    // ORM layers, `levels` mips of `size`), and the HQ copies (`hqSize`, `hqLevels`; `hq` sRGB base colours,
    // `hqLinear` normal maps; either may be empty). A set that does not fit is left out, with modelMapsFault() saying
    // why; the base colours are never touched. True when `linear` is bound.
    struct ModelMaps {
        unsigned size{}, levels{};
        std::span<const std::vector<unsigned char>> linear;
        unsigned hqSize{}, hqLevels{};
        std::span<const std::vector<unsigned char>> hq, hqLinear;
    };
    bool setModelMaps(const ModelMaps& maps) noexcept;
    bool modelMapsReady() const {
        return model_ && modelLinear_;
    }
    bool modelHqReady() const {
        return model_ && (modelHq_ || modelHqLinear_);
    }
    const std::string& modelMapsFault() const {
        return modelMapsFault_;
    }
    // Empty, or why the PBR model shaders did not compile (the 0.0.78 shaders draw the models then).
    const std::string& modelPbrFault() const {
        return modelPbrFault_;
    }
    // The light for the next renderViews' PBR models (model_pbr::LightConstants, 24 floats; its eye is filled per
    // view from the view's projection). toSun.w 0: the 0.0.78 look for every model.
    void setModelLight(const float (&constants)[24]) noexcept;
    // The adapter's free local video memory (its budget minus what this process uses), bytes; 0 when unknown.
    std::uint64_t videoMemoryFree() const noexcept;
    // 0.0.74 test 10 SEE-THROUGH: the game's scene depth for the next renderViews only. The models
    // textured[first, first + count) (world-placed, whole triangles) are then hidden per pixel
    // where the game's scene is nearer: overlay distance > game * scale + bias metres. `eyes`
    // (R32_TYPELESS in PIXEL_SHADER_RESOURCE, one per view) hold the game's reversed-Z depth made
    // with near `gameNear` and far `gameFar` (0: infinite); the views' own projection uses
    // overlayNear / overlayFar (standard Z). Anything that does not fit (sizes, formats, a range
    // outside the models, the shader) draws those models as without it. Never throws.
    struct SceneDepth {
        std::array<ComPtr<ID3D12Resource>, 2> eyes;
        unsigned width{}, height{};
        float gameNear{}, gameFar{}, overlayNear{}, overlayFar{}, scale = 1.02f, bias = .05f;
        std::size_t first{}, count{};
        // HERO VR 0.0.78 WORLD SHADOW (world_light.hpp): toward the game's sun (world space, unit) and how much of
        // a world-lit model's sun light (its vertex alpha) is taken where the game's scene depth blocks the sun;
        // 0: no sun shadow (count may then not be 0). The march: its reach and steps (metres; 1..12).
        std::array<float, 3> toSun{};
        float sunShadow = 0;
        float marchMetres = 24.f, marchSteps = 12.f;
    };
    void setSceneDepth(SceneDepth depth) noexcept;
    // Empty, or why the occluded models' pass is off for the session.
    const std::string& sceneFault() const {
        return sceneFault_;
    }
    // Frames drawn with the scene depth; frames given one that did not fit.
    std::uint64_t sceneDrawn() const {
        return sceneDrawn_;
    }
    std::uint64_t sceneRefused() const {
        return sceneRefused_;
    }
    // Scales `source` (read as `sourceView`, in PIXEL_SHADER_RESOURCE state)
    // over the whole target, which enters and leaves in RENDER_TARGET state.
    // A display-encoded source reaches an sRGB target with its stored values
    // unchanged; `linearSource` holds light values and is encoded. Asynchronous
    // like renderViews.
    void blit(ID3D12Resource* source, DXGI_FORMAT sourceView, bool linearSource, const ViewTarget& target);
    // HERO VR 0.0.77 PROMPT WINDOW (CANDIDATE): an image drawn by the next renderViews into a
    // rectangle of each view (after the grade, under the overlay's drawings), with blit's shader.
    // `rect[eye]` = {x0, y0, x1, y1} in the view's pixels; a rectangle under 8 pixels is skipped.
    // The source must be in PIXEL_SHADER_RESOURCE state; the renderer keeps it until the next
    // renderViews. A failure turns the inset off for the session (insetFault), never the overlay.
    struct Inset {
        ComPtr<ID3D12Resource> source;
        DXGI_FORMAT view = DXGI_FORMAT_UNKNOWN;
        bool linear{};
        float rect[2][4]{};
        // HERO VR 0.0.77 CUTSCENES: the part of the source drawn {u0, v0, u1, v1} (all of it by default),
        // and keyed: a see-through panel (a glass of `backdrop` darkness, the source's bright colourless
        // pixels on it, cutscene077.hpp keyOf) blended over the image instead of covering it.
        float crop[4]{0, 0, 1, 1};
        bool keyed{};
        float backdrop{};
    };
    void setInset(Inset inset) noexcept;
    void clearInset() noexcept;
    const std::string& insetFault() const {
        return insetFault_;
    }
    std::uint64_t insetsDrawn() const {
        return insetsDrawn_;
    }
    void waitIdle();
    // Diagnostic RGBA8 readback; target enters and exits in RENDER_TARGET state.
    std::vector<unsigned char> readback(ID3D12Resource* target);
    // Nonblocking diagnostic copy of an RGBA8 target that is in RENDER_TARGET
    // state (as renderViews leaves it), queued after the work already
    // submitted. Returns a ticket, or 0 when no copy was queued: a previous
    // copy is still running, or the target cannot be copied. Never throws.
    std::uint64_t capture(ID3D12Resource* target) noexcept;
    struct Captured {
        const unsigned char* pixels{};
        unsigned width{}, height{}, rowPitch{};
    };
    // The rows of `ticket` once the GPU has finished its copy. They stay
    // unchanged until the next capture() that returns a ticket.
    bool captured(std::uint64_t ticket, Captured& out) const noexcept;
    // Update 24.1: empty, or why the overlay's full shader did not compile
    // (the compiler's message); then everything is drawn in plain colours.
    const std::string& shaderFault() const {
        return shaderFault_;
    }
    ID3D12Device* device() const {
        return device_.Get();
    }
    ID3D12CommandQueue* queue() const {
        return queue_.Get();
    }

  private:
    void pipeline(DXGI_FORMAT format);
    void resources();
    void waitForSubmission();
    // blit's root signature and pipeline for a target format, and its draw
    // into list_ (open, after any copy the draw reads).
    void prepareBlit(DXGI_FORMAT targetFormat);
    void recordBlit(ID3D12Resource* source, DXGI_FORMAT sourceView, bool linearSource, const ViewTarget& target);
    // HERO VR 0.0.77 PROMPT WINDOW: the inset's draw into one view through `rtv` (list_ open).
    bool recordInset(const ViewTarget& target, D3D12_CPU_DESCRIPTOR_HANDLE rtv, unsigned eye);
    // The grade's pipeline for a target format, and its pass over one view
    // into list_ (open): the image copied aside, then drawn back recoloured
    // through the render target view at `rtv`.
    void prepareGrade(DXGI_FORMAT targetFormat);
    void recordGrade(const ViewTarget& target, D3D12_CPU_DESCRIPTOR_HANDLE rtv, const Grade& grade, unsigned view);
    // The art pass's root signature and pipeline for a target format.
    void prepareArt(DXGI_FORMAT targetFormat);
    // The model pass's root signature and pipeline for a target format.
    void prepareModel(DXGI_FORMAT targetFormat);
    // 0.0.74 test 10 SEE-THROUGH: the occluded models' root signature and pipeline; and whether
    // `scene` fits these views and models.
    void prepareOccluded(DXGI_FORMAT targetFormat);
    bool sceneFits(const SceneDepth& scene, std::span<const ViewTarget> views, std::size_t models) const;
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<ID3D12CommandAllocator> allocator_;
    ComPtr<ID3D12GraphicsCommandList> list_;
    ComPtr<ID3D12DescriptorHeap> rtv_, dsv_;
    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12PipelineState> pipeline_, translucent_;
    ComPtr<ID3D12Resource> depth_, vertices_;
    ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_{};
    std::uint64_t fenceValue_{};
    // capture(): its own command list, and a readback buffer mapped for its lifetime.
    ComPtr<ID3D12CommandAllocator> captureAllocator_;
    ComPtr<ID3D12GraphicsCommandList> captureList_;
    ComPtr<ID3D12Resource> captureBuffer_;
    const unsigned char* captureMapped_{};
    std::uint64_t captureBytes_{}, captureFence_{}, captureTicket_{};
    Captured captureLayout_{};
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
    unsigned width_{}, height_{};
    std::string shaderFault_;
    std::size_t capacity_{};
    // blit(): a full-screen triangle sampling one shader-visible descriptor.
    ComPtr<ID3D12DescriptorHeap> srv_;
    ComPtr<ID3D12RootSignature> blitRoot_;
    ComPtr<ID3D12PipelineState> blitPipeline_;
    ComPtr<ID3D12PipelineState> keyPipeline_; // HERO VR 0.0.77 CUTSCENES: the see-through inset
    DXGI_FORMAT blitFormat_ = DXGI_FORMAT_UNKNOWN;
    // HERO VR 0.0.77 PROMPT WINDOW: the inset, the one it drew last (kept until the GPU is done), its
    // own shader-visible descriptor and whether it failed.
    Inset inset_{}, insetDrawn_{};
    ComPtr<ID3D12DescriptorHeap> insetHeap_;
    std::string insetFault_;
    std::uint64_t insetsDrawn_{};
    // The grade: its own descriptor, root signature and pipeline, and the
    // copy of a view's image it reads (made like the targets, reused).
    ComPtr<ID3D12DescriptorHeap> gradeHeap_;
    ComPtr<ID3D12RootSignature> gradeRoot_;
    ComPtr<ID3D12PipelineState> gradePipeline_;
    DXGI_FORMAT gradeFormat_ = DXGI_FORMAT_UNKNOWN;
    ComPtr<ID3D12Resource> gradeCopy_;
    D3D12_RESOURCE_STATES gradeCopyState_ = D3D12_RESOURCE_STATE_COPY_DEST;
    DXGI_FORMAT gradeViewFormat_ = DXGI_FORMAT_UNKNOWN;
    // Update 36: the art texture, its descriptor, root signature and pipeline.
    ComPtr<ID3D12Resource> art_;
    ComPtr<ID3D12DescriptorHeap> artHeap_;
    ComPtr<ID3D12RootSignature> artRoot_;
    ComPtr<ID3D12PipelineState> artPipeline_;
    DXGI_FORMAT artFormat_ = DXGI_FORMAT_UNKNOWN;
    std::string artFault_;
    // Version 0.0.72: the models' texture array, its descriptor, root signature and pipeline.
    ComPtr<ID3D12Resource> model_;
    ComPtr<ID3D12DescriptorHeap> modelHeap_;
    ComPtr<ID3D12RootSignature> modelRoot_;
    ComPtr<ID3D12PipelineState> modelPipeline_;
    DXGI_FORMAT modelFormat_ = DXGI_FORMAT_UNKNOWN;
    std::string modelFault_;
    // HERO VR 0.0.78 PBR MATERIALS: the maps (heap 3: linear, 4: HQ sRGB, 5: HQ linear), the frame's light, and
    // whether the PBR shaders compiled (false: the 0.0.78 shaders).
    ComPtr<ID3D12Resource> modelLinear_, modelHq_, modelHqLinear_;
    std::string modelMapsFault_, modelPbrFault_;
    std::array<float, 24> modelLight_{};
    bool modelPbr_ = false, occludedPbr_ = false;
    ComPtr<ID3D12Resource> uploadModelArray(unsigned size, unsigned levels,
                                            std::span<const std::vector<unsigned char>> layers, DXGI_FORMAT format,
                                            const char* what);
    void writeModelViews();
    // 0.0.74 test 10 SEE-THROUGH: the depth for the next frame, the one the last frame read (kept
    // until its work is done), and the pass.
    SceneDepth scene_, sceneKept_;
    ComPtr<ID3D12RootSignature> occludedRoot_;
    ComPtr<ID3D12PipelineState> occludedPipeline_;
    DXGI_FORMAT occludedFormat_ = DXGI_FORMAT_UNKNOWN;
    std::string sceneFault_;
    std::uint64_t sceneDrawn_{}, sceneRefused_{};
};
void addBox(std::vector<Vertex>& out, Vec3 min, Vec3 max, Vec3 color);
void addBeam(std::vector<Vertex>& out, Vec3 a, Vec3 b, float radius, Vec3 color);
} // namespace spidy
