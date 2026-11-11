// HERO VR 0.0.78 cut13: the model's and the calibration art's root signatures, serialized and read back with no
// GPU. cut12 (a823c99) said 3 parameters of an array of 2 in the model's root: "Serialize model root signature
// HRESULT=2147942487" (console line 325 of the 161552 report) the first time the holo opened, and every model went
// UNTEXTURED for the session (initials for the portraits, no suits, the flat-lit van). This fails on that build.
// cut14: the roots as before the PBR change (cut8's model root: 2 parameters, the 3-descriptor heap), the see-through
// models' root (4 parameters, WORLD SHADOW's 20 constants at b1) and the vertex the three model pipelines read.
#include "spidy/d3d12_renderer.hpp"
#include "spidy/vertex.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace spidy;
static int failures = 0;
static void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    failures += ok ? 0 : 1;
}
static const D3D12_ROOT_SIGNATURE_DESC* readBack(HRESULT (*serialize)(ComPtr<ID3DBlob>&, ComPtr<ID3DBlob>&), const char* name,
                                                 ComPtr<ID3D12RootSignatureDeserializer>& reader) {
    ComPtr<ID3DBlob> signature, error;
    const HRESULT r = serialize(signature, error);
    char line[200];
    std::snprintf(line, sizeof line, "%s root signature serializes (HRESULT 0x%08lX%s%s)", name, static_cast<unsigned long>(r),
                  error ? ": " : "", error ? static_cast<const char*>(error->GetBufferPointer()) : "");
    check(SUCCEEDED(r) && signature, line);
    if (FAILED(r) || !signature)
        return nullptr;
    const HRESULT d = D3D12CreateRootSignatureDeserializer(signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&reader));
    check(SUCCEEDED(d), std::string(name) + " root signature reads back");
    return SUCCEEDED(d) ? reader->GetRootSignatureDesc() : nullptr;
}
static bool tablesFit(const D3D12_ROOT_SIGNATURE_DESC& rs, unsigned heap) {
    for (UINT p = 0; p < rs.NumParameters; ++p) {
        if (rs.pParameters[p].ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
            continue;
        UINT next = 0;
        for (UINT k = 0; k < rs.pParameters[p].DescriptorTable.NumDescriptorRanges; ++k) {
            const auto& range = rs.pParameters[p].DescriptorTable.pDescriptorRanges[k];
            const UINT at = range.OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND ? next : range.OffsetInDescriptorsFromTableStart;
            if (at + range.NumDescriptors > heap)
                return false;
            next = at + range.NumDescriptors;
        }
    }
    return true;
}
int main() {
    // The vertex the overlay, model and see-through model pipelines read (their input layouts: POSITION float3 at 0,
    // COLOR float4 at 12 - the colour and the opacity -, TEXCOORD0 float4 at 28 - the texture's u, v, layer, w - in a
    // 44-byte stride). cut11's PBR vertex carried a tangent: the old layout must read every byte of this one.
    check(sizeof(Vertex) == 44, "vertex: 44 bytes (the model pipelines' stride)");
    check(offsetof(Vertex, position) == 0, "vertex: POSITION float3 at 0");
    check(offsetof(Vertex, color) == 12 && offsetof(Vertex, alpha) == 24, "vertex: COLOR float4 at 12 (rgb, then the opacity at 24)");
    check(offsetof(Vertex, shape) == 28 && sizeof(Shape) == 16, "vertex: TEXCOORD0 float4 at 28 (u, v, layer, w)");
    ComPtr<ID3D12RootSignatureDeserializer> modelReader, artReader, occludedReader, bodyCutReader;
    if (const auto* rs = readBack(&serializeModelRoot, "model", modelReader)) {
        check(rs->NumParameters == 2, "model root: 2 parameters (the draw sets 0 the eye, 1 the textures)");
        if (rs->NumParameters == 2) {
            const auto& eye = rs->pParameters[0];
            check(eye.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS && eye.Constants.Num32BitValues == 16 &&
                      eye.Constants.ShaderRegister == 0,
                  "model root 0: 16 constants at b0 (the eye's view-projection)");
            const auto& table = rs->pParameters[1];
            const bool t0 = table.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE &&
                            table.DescriptorTable.NumDescriptorRanges == 1 &&
                            table.DescriptorTable.pDescriptorRanges[0].BaseShaderRegister == 0 &&
                            table.DescriptorTable.pDescriptorRanges[0].NumDescriptors == 1 &&
                            table.DescriptorTable.pDescriptorRanges[0].OffsetInDescriptorsFromTableStart == 0;
            check(t0, "model root 1: t0 the colours' array at model heap 0");
        }
        check(modelHeapDescriptors == 3, "model heap: 3 descriptors (the colours, each eye's scene depth)");
        check(tablesFit(*rs, modelHeapDescriptors), "model root: its table fits the model heap");
        check(rs->NumStaticSamplers == 1 && rs->pStaticSamplers[0].AddressU == D3D12_TEXTURE_ADDRESS_MODE_WRAP,
              "model root: the wrapping sampler (the models' UVs tile)");
    }
    if (const auto* rs = readBack(&serializeOccludedRoot, "see-through model", occludedReader)) {
        check(rs->NumParameters == 4, "see-through root: 4 parameters (the eye, the colours, the scene depth, the match + sun)");
        if (rs->NumParameters == 4) {
            check(rs->pParameters[0].ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS &&
                      rs->pParameters[0].Constants.Num32BitValues == 16 && rs->pParameters[0].Constants.ShaderRegister == 0,
                  "see-through root 0: 16 constants at b0");
            const auto& skins = rs->pParameters[1], &scene = rs->pParameters[2], &sun = rs->pParameters[3];
            check(skins.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && skins.DescriptorTable.NumDescriptorRanges == 1 &&
                      skins.DescriptorTable.pDescriptorRanges[0].BaseShaderRegister == 0,
                  "see-through root 1: t0 the colours");
            check(scene.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && scene.DescriptorTable.NumDescriptorRanges == 1 &&
                      scene.DescriptorTable.pDescriptorRanges[0].BaseShaderRegister == 1 &&
                      scene.DescriptorTable.pDescriptorRanges[0].NumDescriptors == 1,
                  "see-through root 2: t1 one eye's scene depth (set at model heap 1 or 2)");
            check(sun.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS && sun.Constants.ShaderRegister == 1 &&
                      sun.Constants.Num32BitValues == 20,
                  "see-through root 3: 20 constants at b1 (the depth match and WORLD SHADOW's sun; the draw sets 20)");
        }
        check(tablesFit(*rs, modelHeapDescriptors), "see-through root: its tables fit the model heap");
    }
    if (const auto* rs = readBack(&serializeArtRoot, "art", artReader)) {
        check(rs->NumParameters == 2, "art root: 2 parameters (the eye, the atlas)");
        check(tablesFit(*rs, artHeapDescriptors), "art root: its table fits the art heap (1 descriptor) - cut12 reached 3..5");
        check(rs->NumStaticSamplers == 1 && rs->pStaticSamplers[0].AddressU == D3D12_TEXTURE_ADDRESS_MODE_CLAMP &&
                  rs->pStaticSamplers[0].MaxLOD == 4,
              "art root: the clamped sampler, 4 levels (the atlas's pictures sit side by side)");
    }
    // UA EXPANSION fix 3: the hero's cut (the eye, one eye's scene depth at heap 1 or 2, 12 constants at b1).
    if (const auto* rs = readBack(&serializeBodyCutRoot, "body cut", bodyCutReader)) {
        check(rs->NumParameters == 3, "body cut root: 3 parameters (the eye, the scene depth, the cut)");
        if (rs->NumParameters == 3) {
            check(rs->pParameters[0].ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS &&
                      rs->pParameters[0].Constants.Num32BitValues == 16 && rs->pParameters[0].Constants.ShaderRegister == 0,
                  "body cut root 0: 16 constants at b0");
            const auto& scene = rs->pParameters[1];
            check(scene.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && scene.DescriptorTable.NumDescriptorRanges == 1 &&
                      scene.DescriptorTable.pDescriptorRanges[0].BaseShaderRegister == 0 &&
                      scene.DescriptorTable.pDescriptorRanges[0].NumDescriptors == 1,
                  "body cut root 1: t0 one eye's scene depth");
            check(rs->pParameters[2].ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS &&
                      rs->pParameters[2].Constants.ShaderRegister == 1 && rs->pParameters[2].Constants.Num32BitValues == 12,
                  "body cut root 2: 12 constants at b1 (the draw sets 12)");
        }
        check(rs->NumStaticSamplers == 0, "body cut root: no sampler (the depth is loaded)");
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
