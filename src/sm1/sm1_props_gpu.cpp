// HERO VR 0.0.76 SM1 HERO PROPS: the D3D12 side (sm1_props_gpu.hpp). Made by Agent Mulder.
#include "sm1_props_gpu.hpp"

#include "spidy/mesh_model.hpp"

#include <objbase.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>

namespace spidy::sm1::propsgpu {
namespace {

using CompileFn = HRESULT(WINAPI*)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR, UINT,
                                   UINT, ID3DBlob**, ID3DBlob**);

Xr xr;
Log logFn = nullptr;
mesh_model::Decoder decoder = nullptr;

void say(const char* fmt, ...) {
    if (!logFn)
        return;
    char text[640];
    va_list a;
    va_start(a, fmt);
    std::vsnprintf(text, sizeof text, fmt, a);
    va_end(a);
    logFn(text);
}
template <class T> void release(T*& p) {
    if (p)
        p->Release();
    p = nullptr;
}

// --- The shader -------------------------------------------------------------------------------------------------
// Root constants (b0, 44 values): the eye's view-projection (row-major, column vectors), the model's 3x4 and its
// normals' 3x3 (rows), and mode: x 1 = steel (the claws), y the steel's reach, z 1 = encode sRGB by hand.
const char* const shaderText = R"(
cbuffer C : register(b0) {
    row_major float4x4 viewProj;
    float4 model0, model1, model2;
    float4 normal0, normal1, normal2;
    float4 mode;
};
Texture2DArray pictures : register(t0);
SamplerState linearWrap : register(s0);
struct VI { float3 p : POSITION; float3 n : NORMAL; float2 uv : TEXCOORD0; float layer : TEXCOORD1; float3 c : COLOR; };
struct VO {
    float4 pos : SV_Position; float3 n : NORMAL; float2 uv : TEXCOORD0; float layer : TEXCOORD1; float3 c : COLOR;
    float3 m : TEXCOORD2;
};
VO vs(VI i) {
    VO o;
    const float4 mp = float4(i.p, 1);
    const float3 w = float3(dot(model0, mp), dot(model1, mp), dot(model2, mp));
    o.pos = mul(viewProj, float4(w, 1));
    o.n = float3(dot(normal0.xyz, i.n), dot(normal1.xyz, i.n), dot(normal2.xyz, i.n));
    o.uv = i.uv; o.layer = i.layer; o.c = i.c; o.m = i.p;
    return o;
}
float3 encode(float3 c) {
    if (mode.z < .5) return c;
    return c <= .0031308 ? c * 12.92 : 1.055 * pow(abs(c), 1 / 2.4) - .055;
}
float4 ps(VO i) : SV_Target {
    float3 n = i.n;
    const float len = length(n);
    n = len > 1e-6 ? n / len : float3(0, 1, 0);
    const float3 light = normalize(float3(.35, .85, .4));
    const float lit = .32 + .68 * abs(dot(n, light));
    if (mode.x > .5) {
        // hero007_visual's steel: brighter toward the tip, a hard highlight
        const float t = saturate(-i.m.z / max(mode.y, .05));
        const float shine = pow(abs(dot(n, normalize(float3(.2, .9, .3)))), 24);
        const float3 steel = lerp(float3(.50, .52, .56), float3(.82, .84, .88), t) * lit + shine * .55;
        return float4(encode(saturate(steel)), 1);
    }
    float3 c = i.c;
    if (i.layer >= 0) {
        float layer = i.layer;
        const bool cut = layer >= 1023.5;
        if (cut) layer -= 1024;
        const float4 t = pictures.Sample(linearWrap, float3(i.uv, layer));
        if (cut && t.a < .5) discard;
        c *= t.rgb;
    }
    return float4(encode(saturate(c * lit)), 1);
}
)";
constexpr UINT constantCount = 44;

// --- A loaded set of models (one hero's) ----------------------------------------------------------------------------
struct Range {
    UINT first = 0, count = 0;
};
struct Set {
    uint32_t hero = UINT32_MAX;
    ID3D12Device* device = nullptr; // AddRef'd
    ID3D12Resource* vertices = nullptr;
    ID3D12Resource* pictures = nullptr;
    ID3D12DescriptorHeap* views = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    Range range[props::modelCount];
    props::Bounds bounds[props::modelCount];
    UINT64 lastUse = 0; // the props' fence value of the last frame that drew it
    ~Set() {
        release(views);
        release(pictures);
        release(vertices);
        release(device);
    }
};

std::mutex setLock; // pending, wantedKey, loading
Set* pending = nullptr;   // loaded, not yet taken by the frame
std::wstring wantedKey;   // hero + root of the newest request
std::atomic<uint64_t> generation{0};

// Frame-thread state.
struct Gpu {
    bool off = false;
    bool tried = false; // the pipeline
    ID3D12RootSignature* root = nullptr;
    ID3DBlob* vs = nullptr;
    ID3DBlob* ps = nullptr;
    ID3D12PipelineState* state = nullptr;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    bool encode = false; // a UNORM target: the shader writes sRGB itself
    ID3D12CommandAllocator* allocators[3]{};
    UINT64 allocatorUse[3]{};
    uint32_t nextAllocator = 0;
    ID3D12GraphicsCommandList* list = nullptr;
    ID3D12Fence* fence = nullptr;
    UINT64 fenceValue = 0;
    HANDLE event = nullptr;
    // the swapchain (both eyes side by side) and its depth
    XrSwapchain swapchain = XR_NULL_HANDLE;
    std::vector<ID3D12Resource*> images;
    int eyeWidth = 0, height = 0;
    bool waiting = false; // acquired, its wait timed out: wait again next frame
    uint32_t acquired = 0;
    ID3D12Resource* depth = nullptr;
    ID3D12DescriptorHeap* targets = nullptr; // RTVs, one an image
    ID3D12DescriptorHeap* depthView = nullptr;
    UINT targetStep = 0;
    XrSession session = XR_NULL_HANDLE;
    ID3D12Device* device = nullptr;
    Set* current = nullptr;
    std::vector<Set*> retired;
    XrCompositionLayerProjectionView views[2]{};
    bool loggedFirst = false;
} gpu;

std::atomic<bool> enabled{true};

bool fail(const char* why) {
    if (!gpu.off)
        say("PROPS: off for this session (%s); the game and the rest of the add-on go on", why);
    gpu.off = true;
    enabled = false;
    return false;
}

// The models' folder as the narrow path mesh_model reads (its short 8.3 form when the name is not in the code page).
std::string narrowPath(const std::wstring& w) {
    const auto convert = [](const std::wstring& s, std::string& out) {
        BOOL lossy = FALSE;
        const int n = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, s.c_str(), static_cast<int>(s.size()), nullptr, 0,
                                          nullptr, &lossy);
        if (n <= 0 || lossy)
            return false;
        out.assign(static_cast<size_t>(n), '\0');
        return WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, s.c_str(), static_cast<int>(s.size()), out.data(), n,
                                   nullptr, &lossy) == n &&
               !lossy;
    };
    std::string out;
    if (convert(w, out))
        return out;
    wchar_t buf[1024]{};
    const DWORD n = GetShortPathNameW(w.c_str(), buf, 1024);
    if (n && n < 1024 && convert(std::wstring(buf, n), out))
        return out;
    return {};
}

// --- The loader thread ----------------------------------------------------------------------------------------------
struct Job {
    uint64_t generation = 0;
    uint32_t hero = 0;
    std::wstring root;
    ID3D12Device* device = nullptr; // AddRef'd, released by the job
};

// Uploads the packed vertices and the textures on a copy queue of the set's own; waits for it (10 s at most).
bool upload(Set& set, const std::vector<props::GpuVertex>& verts, const mesh_model::TextureArray& tex, std::string& why) {
    ID3D12Device* d = set.device;
    const UINT layers = static_cast<UINT>(std::max<size_t>(1, tex.layers.size()));
    const UINT levels = tex.levels;
    D3D12_HEAP_PROPERTIES gpuHeap{D3D12_HEAP_TYPE_DEFAULT};
    D3D12_HEAP_PROPERTIES cpuHeap{D3D12_HEAP_TYPE_UPLOAD};
    const UINT64 vertexBytes = verts.size() * sizeof(props::GpuVertex);
    D3D12_RESOURCE_DESC vb{};
    vb.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vb.Width = vertexBytes;
    vb.Height = 1;
    vb.DepthOrArraySize = 1;
    vb.MipLevels = 1;
    vb.SampleDesc.Count = 1;
    vb.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_RESOURCE_DESC td{};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = tex.size;
    td.Height = tex.size;
    td.DepthOrArraySize = static_cast<UINT16>(layers);
    td.MipLevels = static_cast<UINT16>(levels);
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    td.SampleDesc.Count = 1;
    if (FAILED(d->CreateCommittedResource(&gpuHeap, D3D12_HEAP_FLAG_NONE, &vb, D3D12_RESOURCE_STATE_COMMON, nullptr,
                                          IID_PPV_ARGS(&set.vertices))) ||
        FAILED(d->CreateCommittedResource(&gpuHeap, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COMMON, nullptr,
                                          IID_PPV_ARGS(&set.pictures)))) {
        why = "the models' GPU buffers could not be made";
        return false;
    }
    const UINT subresources = layers * levels;
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> foot(subresources);
    std::vector<UINT> rows(subresources);
    std::vector<UINT64> rowBytes(subresources);
    UINT64 texBytes = 0;
    d->GetCopyableFootprints(&td, 0, subresources, 0, foot.data(), rows.data(), rowBytes.data(), &texBytes);
    const UINT64 vertexAt = (texBytes + 511) & ~UINT64{511};
    D3D12_RESOURCE_DESC ub = vb;
    ub.Width = vertexAt + vertexBytes;
    ID3D12Resource* staging = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12GraphicsCommandList* list = nullptr;
    ID3D12Fence* fence = nullptr;
    HANDLE event = nullptr;
    bool ok = false;
    do {
        if (FAILED(d->CreateCommittedResource(&cpuHeap, D3D12_HEAP_FLAG_NONE, &ub, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                              IID_PPV_ARGS(&staging)))) {
            why = "the models' upload buffer could not be made";
            break;
        }
        uint8_t* p = nullptr;
        D3D12_RANGE none{0, 0};
        if (FAILED(staging->Map(0, &none, reinterpret_cast<void**>(&p))) || !p) {
            why = "the models' upload buffer could not be mapped";
            break;
        }
        for (UINT layer = 0; layer < layers; ++layer) {
            const std::vector<uint8_t>* src = layer < tex.layers.size() ? &tex.layers[layer] : nullptr;
            size_t at = 0;
            for (UINT level = 0; level < levels; ++level) {
                const UINT s = std::max(1u, tex.size >> level);
                const auto& f = foot[layer * levels + level];
                for (UINT y = 0; y < s; ++y) {
                    uint8_t* row = p + f.Offset + UINT64{y} * f.Footprint.RowPitch;
                    if (src && at + size_t{s} * 4 <= src->size())
                        std::memcpy(row, src->data() + at, size_t{s} * 4);
                    else
                        std::memset(row, 0xff, size_t{s} * 4); // a set without textures: one white layer
                    at += size_t{s} * 4;
                }
            }
        }
        std::memcpy(p + vertexAt, verts.data(), static_cast<size_t>(vertexBytes));
        staging->Unmap(0, nullptr);
        D3D12_COMMAND_QUEUE_DESC qd{D3D12_COMMAND_LIST_TYPE_COPY, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0};
        if (FAILED(d->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue))) ||
            FAILED(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&allocator))) ||
            FAILED(d->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, allocator, nullptr, IID_PPV_ARGS(&list))) ||
            FAILED(d->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))) ||
            !(event = CreateEventW(nullptr, FALSE, FALSE, nullptr))) {
            why = "the models' copy queue could not be made";
            break;
        }
        list->CopyBufferRegion(set.vertices, 0, staging, vertexAt, vertexBytes);
        for (UINT i = 0; i < subresources; ++i) {
            D3D12_TEXTURE_COPY_LOCATION dst{};
            dst.pResource = set.pictures;
            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = i;
            D3D12_TEXTURE_COPY_LOCATION src{};
            src.pResource = staging;
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint = foot[i];
            list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        }
        if (FAILED(list->Close())) {
            why = "the models' copy could not be recorded";
            break;
        }
        ID3D12CommandList* lists[] = {list};
        queue->ExecuteCommandLists(1, lists);
        if (FAILED(queue->Signal(fence, 1)) || FAILED(fence->SetEventOnCompletion(1, event)) ||
            WaitForSingleObject(event, 10000) != WAIT_OBJECT_0) {
            why = "the models' copy did not finish in 10 s";
            // the copy may still run: keep the staging buffer and the queue alive (a small leak, never a fault)
            staging = nullptr;
            queue = nullptr;
            allocator = nullptr;
            list = nullptr;
            fence = nullptr;
            event = nullptr;
            break;
        }
        D3D12_DESCRIPTOR_HEAP_DESC h{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0};
        if (FAILED(d->CreateDescriptorHeap(&h, IID_PPV_ARGS(&set.views)))) {
            why = "the models' texture view could not be made";
            break;
        }
        D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
        sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Texture2DArray.MipLevels = levels;
        sv.Texture2DArray.ArraySize = layers;
        d->CreateShaderResourceView(set.pictures, &sv, set.views->GetCPUDescriptorHandleForHeapStart());
        set.vbv.BufferLocation = set.vertices->GetGPUVirtualAddress();
        set.vbv.SizeInBytes = static_cast<UINT>(vertexBytes);
        set.vbv.StrideInBytes = sizeof(props::GpuVertex);
        ok = true;
    } while (false);
    if (event)
        CloseHandle(event);
    release(fence);
    release(list);
    release(allocator);
    release(queue);
    release(staging);
    return ok;
}

DWORD WINAPI loadThread(void* raw) {
    std::unique_ptr<Job> job(static_cast<Job*>(raw));
    const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED); // WIC (the decoder)
    auto set = std::make_unique<Set>();
    set->hero = job->hero;
    set->device = job->device; // the job's reference moves to the set
    job->device = nullptr;
    std::string why;
    bool ok = false;
    try {
        const std::string root = narrowPath(job->root);
        auto library = std::make_unique<mesh_model::Library>();
        const std::vector<props::Model> models = props::modelsFor(job->hero);
        std::string names;
        if (root.empty())
            why = "the Models folder's path cannot be read by the model loader";
        for (const props::Model m : models) {
            if (root.empty())
                break;
            const uint32_t slot = props::slotOf(m);
            const mesh_model::Status s = library->load(slot, root);
            if (s != mesh_model::loaded)
                say("PROPS: %s (Models\\%s) not loaded: %s", props::modelNames[m], mesh_model::slotFolders[slot],
                    library->error[slot].empty() ? "missing" : library->error[slot].c_str());
        }
        const mesh_model::TextureArray tex =
            mesh_model::prepareTextures(*library, decoder, props::textureSize, props::textureLevels);
        std::vector<props::GpuVertex> verts;
        for (const props::Model m : models) {
            const mesh_model::Mesh* mesh = library->model(props::slotOf(m));
            if (!mesh)
                continue;
            const size_t first = verts.size();
            if (!props::pack(*mesh, verts))
                continue;
            set->range[m] = {static_cast<UINT>(first), static_cast<UINT>(verts.size() - first)};
            set->bounds[m] = props::boundsOf(m, *mesh);
            if (!set->bounds[m].ok)
                set->range[m] = {};
            names += names.empty() ? "" : ", ";
            names += props::modelNames[m];
            names += " (" + std::to_string(mesh->triangles()) + " triangles)";
        }
        if (verts.empty()) {
            if (why.empty())
                why = "none of the hero's models could be read from Models";
        } else if (generation.load() == job->generation) {
            ok = upload(*set, verts, tex, why);
            if (ok)
                say("PROPS: ready: %s; %zu texture layers (%u px)", names.c_str(), tex.layers.size(), props::textureSize);
        }
    } catch (...) {
        why = "out of memory reading the models";
        ok = false;
    }
    if (!ok && !why.empty())
        say("PROPS: hero %u's props not drawn: %s", job->hero, why.c_str());
    {
        std::lock_guard<std::mutex> g(setLock);
        if (ok && generation.load() == job->generation) {
            delete pending; // a newer request's set never waits behind an older one's
            pending = set.release();
        }
    }
    if (SUCCEEDED(co))
        CoUninitialize();
    return 0;
}

// --- The frame side -----------------------------------------------------------------------------------------------
bool waitFence(UINT64 value, DWORD ms) {
    if (!gpu.fence || gpu.fence->GetCompletedValue() >= value)
        return true;
    if (!gpu.event || FAILED(gpu.fence->SetEventOnCompletion(value, gpu.event)))
        return false;
    return WaitForSingleObject(gpu.event, ms) == WAIT_OBJECT_0;
}
void retireDone() {
    const UINT64 done = gpu.fence ? gpu.fence->GetCompletedValue() : UINT64_MAX;
    for (auto it = gpu.retired.begin(); it != gpu.retired.end();) {
        if ((*it)->lastUse <= done) {
            delete *it;
            it = gpu.retired.erase(it);
        } else {
            ++it;
        }
    }
}
void takePending() {
    Set* fresh = nullptr;
    {
        std::lock_guard<std::mutex> g(setLock);
        fresh = pending;
        pending = nullptr;
    }
    if (!fresh)
        return;
    if (fresh->device != gpu.device) { // a set for an earlier session's device
        delete fresh;
        return;
    }
    if (gpu.current)
        gpu.retired.push_back(gpu.current);
    gpu.current = fresh;
}

DXGI_FORMAT pickFormat(XrSession s, bool& encode) {
    uint32_t n = 0;
    if (!xr.enumerateFormats || xr.enumerateFormats(s, 0, &n, nullptr) != XR_SUCCESS || !n || n > 256)
        return DXGI_FORMAT_UNKNOWN;
    std::vector<int64_t> f(n);
    if (xr.enumerateFormats(s, n, &n, f.data()) != XR_SUCCESS)
        return DXGI_FORMAT_UNKNOWN;
    const DXGI_FORMAT order[] = {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM,
                                 DXGI_FORMAT_B8G8R8A8_UNORM};
    for (const DXGI_FORMAT want : order)
        for (uint32_t i = 0; i < n; ++i)
            if (f[i] == static_cast<int64_t>(want)) {
                encode = want == DXGI_FORMAT_R8G8B8A8_UNORM || want == DXGI_FORMAT_B8G8R8A8_UNORM;
                return want;
            }
    return DXGI_FORMAT_UNKNOWN;
}

bool preparePipeline(ID3D12Device* d, XrSession s) {
    if (gpu.state)
        return true;
    if (gpu.tried)
        return false;
    gpu.tried = true;
    gpu.format = pickFormat(s, gpu.encode);
    if (gpu.format == DXGI_FORMAT_UNKNOWN)
        return fail("the runtime offers no 8-bit colour swapchain");
    HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
    const auto compile = compiler ? reinterpret_cast<CompileFn>(GetProcAddress(compiler, "D3DCompile")) : nullptr;
    if (!compile)
        return fail("d3dcompiler_47.dll (D3DCompile) is not there");
    ID3DBlob* error = nullptr;
    const size_t n = std::strlen(shaderText);
    if (FAILED(compile(shaderText, n, "herovr_sm1_props", nullptr, nullptr, "vs", "vs_5_0", 1u << 11, 0, &gpu.vs, &error)) ||
        FAILED(compile(shaderText, n, "herovr_sm1_props", nullptr, nullptr, "ps", "ps_5_0", 1u << 11, 0, &gpu.ps, &error))) {
        if (error)
            say("PROPS shader: %.400s", static_cast<const char*>(error->GetBufferPointer()));
        release(error);
        return fail("the props' shader did not compile");
    }
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0};
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].Constants.Num32BitValues = constantCount;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable = {1, &range};
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rs{2, parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
    ID3DBlob* signature = nullptr;
    const bool rootOk = SUCCEEDED(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)) &&
                        SUCCEEDED(d->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                                         IID_PPV_ARGS(&gpu.root)));
    release(signature);
    release(error);
    if (!rootOk)
        return fail("the props' root signature could not be made");
    const D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
    p.pRootSignature = gpu.root;
    p.VS = {gpu.vs->GetBufferPointer(), gpu.vs->GetBufferSize()};
    p.PS = {gpu.ps->GetBufferPointer(), gpu.ps->GetBufferSize()};
    p.InputLayout = {layout, 5};
    p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    p.NumRenderTargets = 1;
    p.RTVFormats[0] = gpu.format;
    p.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    p.SampleDesc.Count = 1;
    p.SampleMask = UINT_MAX;
    p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // the SM2 models' winding is not one way everywhere
    p.RasterizerState.DepthClipEnable = TRUE;
    p.DepthStencilState.DepthEnable = TRUE;
    p.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    p.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    auto& blend = p.BlendState.RenderTarget[0];
    blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(d->CreateGraphicsPipelineState(&p, IID_PPV_ARGS(&gpu.state))))
        return fail("the props' pipeline could not be made");
    for (auto*& a : gpu.allocators)
        if (FAILED(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&a))))
            return fail("the props' command allocators could not be made");
    if (FAILED(d->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, gpu.allocators[0], nullptr, IID_PPV_ARGS(&gpu.list))) ||
        FAILED(gpu.list->Close()) || FAILED(d->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gpu.fence))))
        return fail("the props' command list or fence could not be made");
    gpu.event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!gpu.event)
        return fail("the props' fence event could not be made");
    say("PROPS: renderer ready (D3D12 on his queue, format %d%s)", static_cast<int>(gpu.format),
        gpu.encode ? ", sRGB written by the shader" : "");
    return true;
}

void dropSwapchain() {
    if (gpu.swapchain && gpu.waiting && xr.releaseImage) {
        XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        xr.releaseImage(gpu.swapchain, &ri);
    }
    gpu.waiting = false;
    if (gpu.swapchain && xr.destroySwapchain)
        xr.destroySwapchain(gpu.swapchain);
    gpu.swapchain = XR_NULL_HANDLE;
    gpu.images.clear();
    release(gpu.depth);
    release(gpu.targets);
    release(gpu.depthView);
    gpu.eyeWidth = gpu.height = 0;
}

// The props' swapchain: both eyes side by side, each .65 of his eye's size (16..2048 px), and its depth buffer.
bool prepareSwapchain(ID3D12Device* d, XrSession s, int hisWidth, int hisHeight) {
    const int w = std::clamp(static_cast<int>(std::lround(hisWidth * .65)), 16, 2048);
    const int h = std::clamp(static_cast<int>(std::lround(hisHeight * .65)), 16, 2048);
    if (gpu.swapchain && w == gpu.eyeWidth && h == gpu.height)
        return true;
    if (gpu.swapchain) {
        if (!waitFence(gpu.fenceValue, 2000))
            return fail("the GPU did not finish the props' last frame in 2 s");
        dropSwapchain();
    }
    XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    ci.format = static_cast<int64_t>(gpu.format);
    ci.sampleCount = 1;
    ci.width = static_cast<uint32_t>(2 * w);
    ci.height = static_cast<uint32_t>(h);
    ci.faceCount = 1;
    ci.arraySize = 1;
    ci.mipCount = 1;
    if (!xr.createSwapchain || xr.createSwapchain(s, &ci, &gpu.swapchain) != XR_SUCCESS) {
        gpu.swapchain = XR_NULL_HANDLE;
        return fail("the runtime would not make the props' swapchain");
    }
    uint32_t n = 0;
    if (xr.enumerateImages(gpu.swapchain, 0, &n, nullptr) != XR_SUCCESS || !n || n > 8)
        return fail("the props' swapchain has no images");
    std::vector<XrSwapchainImageD3D12KHR> imgs(n, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
    if (xr.enumerateImages(gpu.swapchain, n, &n, reinterpret_cast<XrSwapchainImageBaseHeader*>(imgs.data())) != XR_SUCCESS)
        return fail("the props' swapchain images could not be read");
    D3D12_DESCRIPTOR_HEAP_DESC t{D3D12_DESCRIPTOR_HEAP_TYPE_RTV, n, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
    D3D12_DESCRIPTOR_HEAP_DESC z{D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
    if (FAILED(d->CreateDescriptorHeap(&t, IID_PPV_ARGS(&gpu.targets))) ||
        FAILED(d->CreateDescriptorHeap(&z, IID_PPV_ARGS(&gpu.depthView))))
        return fail("the props' target views could not be made");
    gpu.targetStep = d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (uint32_t i = 0; i < n; ++i) {
        if (!imgs[i].texture)
            return fail("a props swapchain image is empty");
        gpu.images.push_back(imgs[i].texture);
        D3D12_RENDER_TARGET_VIEW_DESC rv{};
        rv.Format = gpu.format;
        rv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        D3D12_CPU_DESCRIPTOR_HANDLE at = gpu.targets->GetCPUDescriptorHandleForHeapStart();
        at.ptr += SIZE_T{i} * gpu.targetStep;
        d->CreateRenderTargetView(imgs[i].texture, &rv, at);
    }
    D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT};
    D3D12_RESOURCE_DESC dd{};
    dd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    dd.Width = static_cast<UINT64>(2 * w);
    dd.Height = static_cast<UINT>(h);
    dd.DepthOrArraySize = 1;
    dd.MipLevels = 1;
    dd.Format = DXGI_FORMAT_D32_FLOAT;
    dd.SampleDesc.Count = 1;
    dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    D3D12_CLEAR_VALUE clear{};
    clear.Format = DXGI_FORMAT_D32_FLOAT;
    clear.DepthStencil.Depth = 1;
    if (FAILED(d->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &dd, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear,
                                          IID_PPV_ARGS(&gpu.depth))))
        return fail("the props' depth buffer could not be made");
    d->CreateDepthStencilView(gpu.depth, nullptr, gpu.depthView->GetCPUDescriptorHandleForHeapStart());
    gpu.eyeWidth = w;
    gpu.height = h;
    say("PROPS: swapchain %dx%d (two eyes of %dx%d, his %dx%d), %u images", 2 * w, h, w, h, hisWidth, hisHeight, n);
    return true;
}

bool usableView(const XrCompositionLayerProjectionView& v) {
    const auto& q = v.pose.orientation;
    const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    return std::isfinite(n) && std::abs(n - 1.f) < .02f && std::isfinite(v.pose.position.x) && std::isfinite(v.pose.position.y) &&
           std::isfinite(v.pose.position.z) && v.fov.angleLeft < v.fov.angleRight && v.fov.angleDown < v.fov.angleUp &&
           v.subImage.imageRect.extent.width > 0 && v.subImage.imageRect.extent.height > 0;
}

} // namespace

void setup(const Xr& x, Log log, mesh_model::Decoder d) {
    xr = x;
    logFn = log;
    decoder = d;
}

void want(uint32_t hero, const std::wstring& modelsRoot, ID3D12Device* device) {
    if (!device || !enabled.load() || !props::hasProps(hero))
        return;
    const std::wstring key = std::to_wstring(hero) + L"|" + std::to_wstring(reinterpret_cast<uintptr_t>(device)) + L"|" + modelsRoot;
    {
        std::lock_guard<std::mutex> g(setLock);
        if (key == wantedKey)
            return;
        wantedKey = key;
    }
    if (gpu.current && gpu.current->hero == hero && gpu.current->device == device)
        return; // already drawn (a hero picked again)
    auto job = std::make_unique<Job>();
    job->generation = ++generation;
    job->hero = hero;
    job->root = modelsRoot;
    job->device = device;
    device->AddRef();
    Job* raw = job.release();
    HANDLE t = CreateThread(nullptr, 0, loadThread, raw, 0, nullptr);
    if (t) {
        SetThreadPriority(t, THREAD_PRIORITY_BELOW_NORMAL);
        CloseHandle(t);
    } else {
        release(raw->device);
        delete raw;
        say("PROPS: the models' loading thread did not start");
    }
}

void bounds(props::Bounds (&out)[props::modelCount]) {
    for (auto& b : out)
        b = {};
    if (gpu.off)
        return;
    takePending();
    if (!gpu.current)
        return;
    for (uint32_t m = 0; m < props::modelCount; ++m)
        if (gpu.current->range[m].count)
            out[m] = gpu.current->bounds[m];
}

bool ready(props::Model m) {
    return !gpu.off && enabled.load() && gpu.current && m < props::modelCount && gpu.current->range[m].count > 0;
}

bool on() { return enabled.load() && !gpu.off; }

void turnOff(const char* why) { fail(why); }

bool render(XrSession session, ID3D12Device* d, ID3D12CommandQueue* queue, const XrCompositionLayerProjection& his,
            const std::vector<props::Item>& items, XrCompositionLayerProjection& out) {
    if (gpu.off || !d || !queue || !session)
        return false;
    if (gpu.device != d || gpu.session != session) {
        if (gpu.device || gpu.session)
            drop();
        gpu.device = d;
        gpu.session = session;
    }
    retireDone();
    takePending();
    Set* set = gpu.current;
    if (!set || items.empty() || his.viewCount < 2 || !his.views || !usableView(his.views[0]) || !usableView(his.views[1]))
        return false;
    if (!preparePipeline(d, session))
        return false;
    const auto& e0 = his.views[0].subImage.imageRect.extent;
    if (!prepareSwapchain(d, session, e0.width, e0.height))
        return false;
    // a command allocator the GPU is done with (never block the frame long: skip this frame's props instead)
    const uint32_t slot = gpu.nextAllocator;
    if (!waitFence(gpu.allocatorUse[slot], 4))
        return false;
    if (!gpu.waiting) {
        XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        if (xr.acquireImage(gpu.swapchain, &ai, &gpu.acquired) != XR_SUCCESS || gpu.acquired >= gpu.images.size())
            return fail("the props' swapchain image could not be acquired");
    }
    XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    wi.timeout = 20'000'000; // 20 ms
    const XrResult w = xr.waitImage(gpu.swapchain, &wi);
    if (w == XR_TIMEOUT_EXPIRED) {
        gpu.waiting = true; // still acquired: wait for it again next frame
        return false;
    }
    gpu.waiting = false;
    if (w != XR_SUCCESS)
        return fail("the props' swapchain image wait failed");
    ID3D12CommandAllocator* a = gpu.allocators[slot];
    bool recorded = SUCCEEDED(a->Reset()) && SUCCEEDED(gpu.list->Reset(a, gpu.state));
    if (recorded) {
        ID3D12GraphicsCommandList* l = gpu.list;
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = gpu.targets->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += SIZE_T{gpu.acquired} * gpu.targetStep;
        const D3D12_CPU_DESCRIPTOR_HANDLE dsv = gpu.depthView->GetCPUDescriptorHandleForHeapStart();
        const float clear[4] = {0, 0, 0, 0}; // premultiplied: nothing where no prop is
        l->ClearRenderTargetView(rtv, clear, 0, nullptr);
        l->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
        l->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        l->SetGraphicsRootSignature(gpu.root);
        ID3D12DescriptorHeap* heaps[] = {set->views};
        l->SetDescriptorHeaps(1, heaps);
        l->SetGraphicsRootDescriptorTable(1, set->views->GetGPUDescriptorHandleForHeapStart());
        l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        l->IASetVertexBuffers(0, 1, &set->vbv);
        for (uint32_t eye = 0; eye < 2; ++eye) {
            const XrCompositionLayerProjectionView& v = his.views[eye];
            const Pose pose{{v.pose.position.x, v.pose.position.y, v.pose.position.z},
                            {v.pose.orientation.x, v.pose.orientation.y, v.pose.orientation.z, v.pose.orientation.w}};
            const Mat4 vp = props::eyeViewProjection(pose, v.fov.angleLeft, v.fov.angleRight, v.fov.angleUp, v.fov.angleDown);
            D3D12_VIEWPORT port{static_cast<float>(eye * gpu.eyeWidth), 0, static_cast<float>(gpu.eyeWidth),
                                static_cast<float>(gpu.height), 0, 1};
            D3D12_RECT scissor{static_cast<LONG>(eye * gpu.eyeWidth), 0, static_cast<LONG>((eye + 1) * gpu.eyeWidth),
                               static_cast<LONG>(gpu.height)};
            l->RSSetViewports(1, &port);
            l->RSSetScissorRects(1, &scissor);
            float c[constantCount]{};
            std::memcpy(c, vp.data(), 16 * sizeof(float));
            for (const props::Item& it : items) {
                if (it.model >= props::modelCount || !set->range[it.model].count)
                    continue;
                float model[12], normal[9];
                if (!props::modelMatrices(it.place, model, normal))
                    continue;
                std::memcpy(c + 16, model, sizeof model);
                for (int r = 0; r < 3; ++r) {
                    c[28 + r * 4] = normal[r * 3];
                    c[29 + r * 4] = normal[r * 3 + 1];
                    c[30 + r * 4] = normal[r * 3 + 2];
                    c[31 + r * 4] = 0;
                }
                c[40] = it.steel ? 1.f : 0.f;
                c[41] = it.reach;
                c[42] = gpu.encode ? 1.f : 0.f;
                c[43] = 0;
                l->SetGraphicsRoot32BitConstants(0, constantCount, c, 0);
                l->DrawInstanced(set->range[it.model].count, 1, set->range[it.model].first, 0);
            }
        }
        recorded = SUCCEEDED(l->Close());
    }
    if (recorded) {
        ID3D12CommandList* lists[] = {gpu.list};
        queue->ExecuteCommandLists(1, lists);
        ++gpu.fenceValue;
        if (FAILED(queue->Signal(gpu.fence, gpu.fenceValue)))
            recorded = false;
        gpu.allocatorUse[slot] = gpu.fenceValue;
        set->lastUse = gpu.fenceValue;
        gpu.nextAllocator = (slot + 1) % 3;
    }
    XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    const bool released = xr.releaseImage(gpu.swapchain, &ri) == XR_SUCCESS;
    if (!recorded)
        return fail("the props' frame could not be recorded");
    if (!released)
        return fail("the props' swapchain image could not be released");
    for (uint32_t eye = 0; eye < 2; ++eye) {
        XrCompositionLayerProjectionView& v = gpu.views[eye];
        v = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        v.pose = his.views[eye].pose;
        v.fov = his.views[eye].fov;
        v.subImage.swapchain = gpu.swapchain;
        v.subImage.imageRect = {{static_cast<int32_t>(eye * gpu.eyeWidth), 0}, {gpu.eyeWidth, gpu.height}};
        v.subImage.imageArrayIndex = 0;
    }
    out = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    out.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT; // premultiplied alpha
    out.space = his.space;
    out.viewCount = 2;
    out.views = gpu.views;
    if (!gpu.loggedFirst) {
        gpu.loggedFirst = true;
        say("PROPS: first frame drawn (%zu props), one projection layer after his", items.size());
    }
    return true;
}

void drop() {
    const bool idle = waitFence(gpu.fenceValue, 2000);
    if (!idle)
        say("PROPS: the GPU did not finish in 2 s; the props' resources are kept (not freed while in use)");
    {
        std::lock_guard<std::mutex> g(setLock);
        delete pending;
        pending = nullptr;
        wantedKey.clear();
        ++generation;
    }
    if (idle) {
        dropSwapchain();
        delete gpu.current;
        for (Set* s : gpu.retired)
            delete s;
        release(gpu.state);
        release(gpu.root);
        release(gpu.vs);
        release(gpu.ps);
        for (auto*& a : gpu.allocators)
            release(a);
        release(gpu.list);
        release(gpu.fence);
        if (gpu.event)
            CloseHandle(gpu.event);
    } else if (gpu.swapchain && xr.destroySwapchain) {
        xr.destroySwapchain(gpu.swapchain); // the runtime's; the session is ending anyway
    }
    gpu = Gpu{};
    enabled = true; // a new session tries again
}

} // namespace spidy::sm1::propsgpu
